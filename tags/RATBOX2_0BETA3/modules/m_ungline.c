/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_ungline.c: Unglines a user.
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
#include "handlers.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "fileio.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_log.h"
#include "send.h"
#include "msg.h"
#include "s_gline.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"

static int mo_ungline(struct Client *, struct Client *, int, const char **);
static int remove_temp_gline(const char *, const char *);

struct Message ungline_msgtab = {
	"UNGLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, m_error, mo_ungline}
};

mapi_clist_av1 ungline_clist[] = { &ungline_msgtab, NULL };
DECLARE_MODULE_AV1(ungline, NULL, NULL, ungline_clist, NULL, NULL, NULL, "$Revision$");

/* m_ungline()
 *
 *      parv[0] = sender nick
 *      parv[1] = gline to remove
 */
static int
mo_ungline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *user;
	char *h = LOCAL_COPY(parv[1]);
	char *host;
	char splat[] = "*";

	if(!ConfigFileEntry.glines)
	{
		sendto_one(source_p, ":%s NOTICE %s :UNGLINE disabled", me.name, parv[0]);
		return 0;
	}

	if(!IsOperUnkline(source_p) || !IsOperGline(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need unkline = yes;", me.name, parv[0]);
		return 0;
	}

	if((host = strchr(h, '@')) || *h == '*')
	{
		/* Explicit user@host mask given */

		if(host)	/* Found user@host */
		{
			user = parv[1];	/* here is user part */
			*(host++) = '\0';	/* and now here is host */
		}
		else
		{
			user = splat;	/* no @ found, assume its *@somehost */
			host = h;
		}
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid parameters", me.name, parv[0]);
		return 0;
	}

	if(remove_temp_gline(user, host))
	{
		sendto_one(source_p, ":%s NOTICE %s :Un-glined [%s@%s]",
			   me.name, parv[0], user, host);
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s has removed the G-Line for: [%s@%s]",
				     get_oper_name(source_p), user, host);
		ilog(L_NOTICE, "%s removed G-Line for [%s@%s]",
		     get_oper_name(source_p), user, host);
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :No G-Line for %s@%s",
			   me.name, parv[0], user, host);
	}

	return 0;
}


/* remove_temp_gline()
 *
 * inputs       - username, hostname to ungline
 * outputs      -
 * side effects - tries to ungline anything that matches
 */
static int
remove_temp_gline(const char *user, const char *host)
{
	struct ConfItem *aconf;
	dlink_node *ptr;
	struct sockaddr_storage addr, caddr;
	int bits, cbits;

	parse_netmask(host, &addr, &bits);

	DLINK_FOREACH(ptr, glines.head)
	{
		aconf = (struct ConfItem *) ptr->data;

		parse_netmask(aconf->host, &caddr, &cbits);

		if(user && irccmp(user, aconf->user))
			continue;

		if(!irccmp(aconf->host, host) && bits == cbits &&
		   comp_with_mask_sock(&addr, &caddr, bits))
		{
			dlinkDestroy(ptr, &glines);
			delete_one_address_conf(aconf->host, aconf);
			return YES;
		}
	}

	return NO;
}
