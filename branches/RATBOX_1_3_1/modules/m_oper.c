/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_oper.c: Makes a user an IRC Operator.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"
#include "tools.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "commio.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_user.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "cache.h"

static int m_oper(struct Client *, struct Client *, int, const char **);

struct Message oper_msgtab = {
	"OPER", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_oper, 3}, mg_ignore, mg_ignore, {m_oper, 3}}
};

mapi_clist_av1 oper_clist[] = { &oper_msgtab, NULL };
DECLARE_MODULE_AV1(oper, NULL, NULL, oper_clist, NULL, NULL, "$Revision$");

static struct ConfItem *find_password_aconf(const char *name, struct Client *source_p);
static int match_oper_password(const char *password, struct ConfItem *aconf);

extern char *crypt();

/*
 * m_oper
 *      parv[0] = sender prefix
 *      parv[1] = oper name
 *      parv[2] = oper password
 */
static int
m_oper(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct ConfItem *aconf;
	struct ConfItem *oconf = NULL;
	const char *name;
	const char *password;

	name = parv[1];
	password = parv[2];

	if(IsOper(source_p))
	{
		sendto_one(source_p, form_str(RPL_YOUREOPER), me.name, source_p->name);
		send_oper_motd(source_p);
		return 0;
	}

	/* end the grace period */
	if(!IsFloodDone(source_p))
		flood_endgrace(source_p);

	if((aconf = find_password_aconf(name, source_p)) == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOOPERHOST), me.name, source_p->name);
		ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s)",
		     name, source_p->name,
		     source_p->username, source_p->host);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Failed OPER attempt - host mismatch by %s (%s@%s)",
					     source_p->name, source_p->username, source_p->host);
		}

		return 0;
	}

	if(match_oper_password(password, aconf))
	{
		oconf = source_p->localClient->att_conf;

		detach_conf(source_p);

		if(attach_conf(source_p, aconf) != 0)
		{
			sendto_one(source_p, ":%s NOTICE %s :Can't attach conf!",
				   me.name, source_p->name);
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Failed OPER attempt by %s (%s@%s) can't attach conf!",
					     source_p->name, source_p->username, source_p->host);

			ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s)",
			     name, source_p->name,
			     source_p->username, source_p->host);

			attach_conf(source_p, oconf);
			return 0;
		}

		oper_up(source_p, aconf);

		ilog(L_OPERED, "OPER %s by %s!%s@%s",
		     name, source_p->name, source_p->username, source_p->host);
		return 0;
	}
	else
	{
		sendto_one(source_p, form_str(ERR_PASSWDMISMATCH),
			   me.name, source_p->name);

		ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s)",
		     name, source_p->name, source_p->username, source_p->host);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Failed OPER attempt by %s (%s@%s)",
					     source_p->name, source_p->username, source_p->host);
		}
	}

	return 0;
}

/*
 * find_password_aconf
 *
 * inputs       -
 * output       -
 */
static struct ConfItem *
find_password_aconf(const char *name, struct Client *source_p)
{
	struct ConfItem *aconf;

	if(!(aconf = find_conf_exact(name, source_p->username, source_p->host,
				     CONF_OPERATOR)) &&
	   !(aconf = find_conf_exact(name, source_p->username,
				     source_p->sockhost, CONF_OPERATOR)))
	{
		return 0;
	}
	return (aconf);
}

/*
 * match_oper_password
 *
 * inputs       - pointer to given password
 *              - pointer to Conf 
 * output       - YES or NO if match
 * side effects - none
 */

static int
match_oper_password(const char *password, struct ConfItem *aconf)
{
	const char *encr;

	if(!aconf->status & CONF_OPERATOR)
		return NO;

	/* passwd may be NULL pointer. Head it off at the pass... */
	if(aconf->passwd == NULL)
		return NO;

	if(IsConfEncrypted(aconf))
	{
		/* use first two chars of the password they send in as salt */
		/* If the password in the conf is MD5, and ircd is linked   
		 * to scrypt on FreeBSD, or the standard crypt library on
		 * glibc Linux, then this code will work fine on generating
		 * the proper encrypted hash for comparison.
		 */
		if(password && *aconf->passwd)
			encr = crypt(password, aconf->passwd);
		else
			encr = "";
	}
	else
	{
		encr = password;
	}

	if(strcmp(encr, aconf->passwd) == 0)
		return YES;
	else
		return NO;
}
