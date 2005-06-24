/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_connect.c: Connects to a remote IRC server.
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
#include "client.h"
#include "ircd.h"
#include "irc_string.h"
#include "numeric.h"
#include "commio.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "hash.h"
#include "modules.h"

static int mo_connect(struct Client *, struct Client *, int, const char **);
static int ms_connect(struct Client *, struct Client *, int, const char **);

struct Message connect_msgtab = {
	"CONNECT", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_connect, 4}, {ms_connect, 4}, {mo_connect, 2}}
};

mapi_clist_av1 connect_clist[] = { &connect_msgtab, NULL };
DECLARE_MODULE_AV1(connect, NULL, NULL, connect_clist, NULL, NULL, "$Revision$");

/*
 * mo_connect - CONNECT command handler
 * 
 * Added by Jto 11 Feb 1989
 *
 * m_connect
 *      parv[0] = sender prefix
 *      parv[1] = servername
 *      parv[2] = port number
 *      parv[3] = remote server
 */
static int
mo_connect(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int port;
	int tmpport;
	struct ConfItem *aconf;
	struct Client *target_p;

	/* always privileged with handlers */

	if(MyConnect(source_p) && !IsOperRemote(source_p) && parc > 3)
	{
		sendto_one(source_p, ":%s NOTICE %s :You need remote = yes;", 
			   me.name, source_p->name);
		return 0;
	}

	if(hunt_server(client_p, source_p, ":%s CONNECT %s %s :%s", 3, parc, parv) != HUNTED_ISME)
		return 0;

	if((target_p = find_server(parv[1])))
	{
		sendto_one(source_p, ":%s NOTICE %s :Connect: Server %s already exists from %s.",
			   me.name, parv[0], parv[1], target_p->from->name);
		return 0;
	}

	/*
	 * try to find the name, then host, if both fail notify ops and bail
	 */
	if(!(aconf = find_conf_by_name(parv[1], CONF_SERVER)))
	{
		if(!(aconf = find_conf_by_host(parv[1], CONF_SERVER)))
		{
			sendto_one(source_p,
				   "NOTICE %s :Connect: Host %s not listed in ircd.conf",
				   parv[0], parv[1]);
			return 0;
		}
	}

	/*
	 * Get port number from user, if given. If not specified,
	 * use the default form configuration structure. If missing
	 * from there, then use the precompiled default.
	 */
	tmpport = port = aconf->port;
	if(parc > 2 && !EmptyString(parv[2]))
	{
		if((port = atoi(parv[2])) <= 0)
		{
			sendto_one(source_p, "NOTICE %s :Connect: Illegal port number", parv[0]);
			return 0;
		}
	}
	else if(port <= 0 && (port = PORTNUM) <= 0)
	{
		sendto_one(source_p, ":%s NOTICE %s :Connect: missing port number",
			   me.name, parv[0]);
		return 0;
	}
	/*
	 * Notify all operators about remote connect requests
	 */

	ilog(L_SERVER, "CONNECT From %s : %s %s", parv[0], parv[1], parc > 2 ? parv[2] : "");

	aconf->port = port;
	/*
	 * at this point we should be calling connect_server with a valid
	 * C:line and a valid port in the C:line
	 */
	if(serv_connect(aconf, source_p))
	{
#ifndef HIDE_SERVERS_IPS
		if(IsOperAdmin(source_p))
			sendto_one(source_p, ":%s NOTICE %s :*** Connecting to %s[%s].%d",
				   me.name, parv[0], aconf->host, aconf->name, aconf->port);
		else
#endif
			sendto_one(source_p, ":%s NOTICE %s :*** Connecting to %s.%d",
				   me.name, parv[0], aconf->name, aconf->port);

	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :*** Couldn't connect to %s.%d",
			   me.name, parv[0], aconf->name, aconf->port);

	}
	/*
	 * client is either connecting with all the data it needs or has been
	 * destroyed
	 */
	aconf->port = tmpport;
	return 0;
}

/*
 * ms_connect - CONNECT command handler
 * 
 * Added by Jto 11 Feb 1989
 *
 * m_connect
 *      parv[0] = sender prefix
 *      parv[1] = servername
 *      parv[2] = port number
 *      parv[3] = remote server
 */
static int
ms_connect(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int port;
	int tmpport;
	struct ConfItem *aconf;
	struct Client *target_p;

	if(hunt_server(client_p, source_p, ":%s CONNECT %s %s :%s", 3, parc, parv) != HUNTED_ISME)
		return 0;

	if((target_p = find_server(parv[1])))
	{
		sendto_one_notice(source_p, ":Connect: Server %s already exists from %s.",
				  parv[1], target_p->from->name);
		return 0;
	}

	/*
	 * try to find the name, then host, if both fail notify ops and bail
	 */
	if(!(aconf = find_conf_by_name(parv[1], CONF_SERVER)))
	{
		if(!(aconf = find_conf_by_host(parv[1], CONF_SERVER)))
		{
			sendto_one_notice(source_p, ":Connect: Host %s not listed in ircd.conf",
					  parv[1]);
			return 0;
		}
	}

	/*
	 * Get port number from user, if given. If not specified,
	 * use the default form configuration structure. If missing
	 * from there, then use the precompiled default.
	 */
	tmpport = aconf->port;

	port = atoi(parv[2]);

	/* if someone sends port 0, and we have a config port.. use it */
	if(port == 0 && aconf->port)
		port = aconf->port;
	else if(port <= 0)
	{
		sendto_one_notice(source_p, ":Connect: Illegal port number");
		return 0;
	}

	/*
	 * Notify all operators about remote connect requests
	 */
	sendto_wallops_flags(UMODE_WALLOP, &me,
			     "Remote CONNECT %s %d from %s", 
			     parv[1], port, source_p->name);
	sendto_server(NULL, NULL, CAP_TS6, NOCAPS,
		      ":%s WALLOPS :Remote CONNECT %s %d from %s",
		      me.id, parv[1], port, source_p->name);
	sendto_server(NULL, NULL, NOCAPS, CAP_TS6,
		      ":%s WALLOPS :Remote CONNECT %s %d from %s",
		      me.name, parv[1], port, source_p->name);

	ilog(L_SERVER, "CONNECT From %s : %s %d", source_p->name, parv[1], port);

	aconf->port = port;
	/*
	 * at this point we should be calling connect_server with a valid
	 * C:line and a valid port in the C:line
	 */
	if(serv_connect(aconf, source_p))
		sendto_one_notice(source_p, ":*** Connecting to %s.%d",
				  aconf->name, aconf->port);
	else
		sendto_one_notice(source_p, ":*** Couldn't connect to %s.%d",
				  aconf->name, aconf->port);
	/*
	 * client is either connecting with all the data it needs or has been
	 * destroyed
	 */
	aconf->port = tmpport;
	return 0;
}
