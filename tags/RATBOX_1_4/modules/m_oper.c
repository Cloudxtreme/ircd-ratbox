/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_oper.c: Makes a user an IRC Operator.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
#include "handlers.h"
#include "client.h"
#include "common.h"
#include "fdlist.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_user.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"


static struct ConfItem *find_password_aconf(char *name, struct Client *source_p);
static int match_oper_password(char *password, struct ConfItem *aconf);
int oper_up(struct Client *source_p, struct ConfItem *aconf);

extern char *crypt();


static void m_oper(struct Client *, struct Client *, int, char **);
static void ms_oper(struct Client *, struct Client *, int, char **);
static void mo_oper(struct Client *, struct Client *, int, char **);


struct Message oper_msgtab = {
	"OPER", 0, 0, 3, 0, MFLG_SLOW, 0,
	{m_unregistered, m_oper, ms_oper, mo_oper}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&oper_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&oper_msgtab);
}

const char *_version = "$Revision$";
#endif

/*
** m_oper
**      parv[0] = sender prefix
**      parv[1] = oper name
**      parv[2] = oper password
*/
static void
m_oper(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	struct ConfItem *aconf;
	struct ConfItem *oconf = NULL;
	char *name;
	char *password;
	dlink_node *ptr;

	name = parv[1];
	password = parv[2];

	if(EmptyString(password))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "OPER");
		return;
	}

	/* end the grace period */
	if(!IsFloodDone(source_p))
		flood_endgrace(source_p);

	if((aconf = find_password_aconf(name, source_p)) == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOOPERHOST), me.name, source_p->name);
		log_foper(source_p, name);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Failed OPER attempt - host mismatch by %s (%s@%s)",
					     source_p->name, source_p->username, source_p->host);
		}
		return;
	}

	if(match_oper_password(password, aconf))
	{
		/*
		   20001216:
		   detach old iline
		   -einride
		 */
		ptr = source_p->localClient->confs.head;
		if(ptr)
		{

			oconf = ptr->data;
			detach_conf(source_p, oconf);
		}

		if(attach_conf(source_p, aconf) != 0)
		{
			sendto_one(source_p, ":%s NOTICE %s :Can't attach conf!",
				   me.name, source_p->name);
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Failed OPER attempt by %s (%s@%s) can't attach conf!",
					     source_p->name, source_p->username, source_p->host);
			log_foper(source_p, name);
			/* 
			   20001216:
			   Reattach old iline
			   -einride
			 */
			attach_conf(source_p, oconf);
			return;
		}

		oper_up(source_p, aconf);

		ilog(L_TRACE, "OPER %s by %s!%s@%s",
		     name, source_p->name, source_p->username, source_p->host);
		log_oper(source_p, name);
		return;
	}
	else
	{
		sendto_one(source_p, form_str(ERR_PASSWDMISMATCH), me.name, parv[0]);
		log_foper(source_p, name);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Failed OPER attempt by %s (%s@%s)",
					     source_p->name, source_p->username, source_p->host);
		}
	}
}

/*
** mo_oper
**      parv[0] = sender prefix
**      parv[1] = oper name
**      parv[2] = oper password
*/
static void
mo_oper(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	sendto_one(source_p, form_str(RPL_YOUREOPER), me.name, parv[0]);
	SendMessageFile(source_p, &ConfigFileEntry.opermotd);
	return;
}

/*
** ms_oper
**      parv[0] = sender prefix
**      parv[1] = oper name
**      parv[2] = oper password
*/
static void
ms_oper(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	/* if message arrived from server, trust it, and set to oper */

	if(!IsOper(source_p))
	{
		if(source_p->status == STAT_CLIENT)
			source_p->handler = OPER_HANDLER;
		source_p->umodes |= UMODE_OPER;
		Count.oper++;
		sendto_server(client_p, NULL, NOCAPS, NOCAPS, ":%s MODE %s :+o", parv[0], parv[0]);
	}
}

/*
 * find_password_aconf
 *
 * inputs       -
 * output       -
 */

static struct ConfItem *
find_password_aconf(char *name, struct Client *source_p)
{
	struct ConfItem *aconf;

	if(!(aconf = find_conf_exact(name, source_p->username, source_p->host,
				     CONF_OPERATOR)) &&
	   !(aconf = find_conf_exact(name, source_p->username,
				     source_p->localClient->sockhost, CONF_OPERATOR)))
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
match_oper_password(char *password, struct ConfItem *aconf)
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
