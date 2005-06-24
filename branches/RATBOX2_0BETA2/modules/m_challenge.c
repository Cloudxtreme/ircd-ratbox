/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_challenge.c: Allows an IRC Operator to securely authenticate.
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
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "modules.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#ifdef HAVE_LIBCRYPTO
#include "rsa.h"
#endif
#include "msg.h"
#include "parse.h"
#include "irc_string.h"
#include "s_log.h"
#include "s_user.h"

#ifndef HAVE_LIBCRYPTO
/* Maybe this should be an error or something?-davidt */
#ifndef STATIC_MODULES
void
_modinit(void)
{
	return;
}

void
_moddeinit(void)
{
	return;
}

const char *_version = "$Revision$";
#endif
#else

static void m_challenge(struct Client *, struct Client *, int, char **);
void binary_to_hex(unsigned char *bin, char *hex, int length);

/* We have openssl support, so include /CHALLENGE */
struct Message challenge_msgtab = {
	"CHALLENGE", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_challenge, m_ignore, m_challenge}
};
#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&challenge_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&challenge_msgtab);
}

const char *_version = "$Revision$";
#endif
/*
 * m_challenge - generate RSA challenge for wouldbe oper
 * parv[0] = sender prefix
 * parv[1] = operator to challenge for, or +response
 *
 */
static void
m_challenge(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	char *challenge;
	struct ConfItem *aconf, *oconf;
	if(!(source_p->user) || !source_p->localClient)
		return;

	/* if theyre an oper, reprint oper motd and ignore */
	if(IsOper(source_p))
	{
		sendto_one(source_p, form_str(RPL_YOUREOPER), me.name, parv[0]);
		SendMessageFile(source_p, &ConfigFileEntry.opermotd);
		return;
	}

	if(*parv[1] == '+')
	{
		/* Ignore it if we aren't expecting this... -A1kmm */
		if(!source_p->user->response)
			return;

		if(irccmp(source_p->user->response, ++parv[1]))
		{
			sendto_one(source_p, form_str(ERR_PASSWDMISMATCH), me.name, source_p->name);
			log_foper(source_p, source_p->user->auth_oper);

			if(ConfigFileEntry.failed_oper_notice)
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Failed OPER attempt by %s (%s@%s)",
						     source_p->name, source_p->username,
						     source_p->host);
			return;
		}

		if((aconf = find_conf_by_name(source_p->user->auth_oper, CONF_OPERATOR)) == NULL)
		{
			sendto_one(source_p, form_str(ERR_NOOPERHOST), me.name, parv[0]);
			log_foper(source_p, source_p->user->auth_oper);

			if(ConfigFileEntry.failed_oper_notice)
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Failed CHALLENGE attempt - host mismatch by %s (%s@%s)",
						     source_p->name, source_p->username,
						     source_p->host);
			return;
		}

		oconf = source_p->localClient->att_conf;
		detach_conf(source_p);

		if(attach_conf(source_p, aconf) != 0)
		{
			sendto_one(source_p, ":%s NOTICE %s :Can't attach conf!",
				   me.name, source_p->name);
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Failed CHALLENGE attempt by %s (%s@%s) can't attach conf!",
					     source_p->name, source_p->username, source_p->host);
			log_foper(source_p, source_p->user->auth_oper);

			attach_conf(source_p, oconf);
			return;
		}

		oper_up(source_p, aconf);

		ilog(L_TRACE, "OPER %s by %s!%s@%s",
		     source_p->user->auth_oper, source_p->name, source_p->username, source_p->host);
		log_oper(source_p, source_p->user->auth_oper);

		MyFree(source_p->user->response);
		MyFree(source_p->user->auth_oper);
		source_p->user->response = NULL;
		source_p->user->auth_oper = NULL;
		return;
	}

	MyFree(source_p->user->response);
	MyFree(source_p->user->auth_oper);
	source_p->user->response = NULL;
	source_p->user->auth_oper = NULL;

	if(!(aconf = find_conf_exact(parv[1], source_p->username, source_p->host,
				     CONF_OPERATOR)) &&
	   !(aconf = find_conf_exact(parv[1], source_p->username,
				     source_p->localClient->sockhost, CONF_OPERATOR)))
	{
		sendto_one(source_p, form_str(ERR_NOOPERHOST), me.name, parv[0]);
		log_foper(source_p, source_p->user->auth_oper);

		if(ConfigFileEntry.failed_oper_notice)
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Failed CHALLENGE attempt - host mismatch by %s (%s@%s)",
					     source_p->name, source_p->username, source_p->host);
		return;
	}

	if(!aconf->rsa_public_key)
	{
		sendto_one(source_p, ":%s NOTICE %s :I'm sorry, PK authentication "
			   "is not enabled for your oper{} block.", me.name, parv[0]);
		return;
	}

	if(!generate_challenge(&challenge, &(source_p->user->response), aconf->rsa_public_key))
	{
		sendto_one(source_p, form_str(RPL_RSACHALLENGE), me.name, parv[0], challenge);
	}

	DupString(source_p->user->auth_oper, aconf->name);
	MyFree(challenge);
	return;
}

#endif /* HAVE_LIBCRYPTO */
