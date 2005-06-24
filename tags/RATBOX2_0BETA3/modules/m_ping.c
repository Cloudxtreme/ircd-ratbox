/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_ping.c: Requests that a PONG message be sent back.
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
#include "numeric.h"
#include "send.h"
#include "irc_string.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "hash.h"
#include "s_conf.h"
#include "s_serv.h"

static int m_ping(struct Client *, struct Client *, int, const char **);
static int ms_ping(struct Client *, struct Client *, int, const char **);

struct Message ping_msgtab = {
	"PING", 0, 0, 1, 0, MFLG_SLOW, 0,
	{m_unregistered, m_ping, ms_ping, m_ping}
};

mapi_clist_av1 ping_clist[] = { &ping_msgtab, NULL };
DECLARE_MODULE_AV1(ping, NULL, NULL, ping_clist, NULL, NULL, NULL, "$Revision$");

/*
** m_ping
**      parv[0] = sender prefix
**      parv[1] = origin
**      parv[2] = destination
*/
static int
m_ping(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	const char *origin, *destination;

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NOORIGIN), me.name, parv[0]);
		return 0;
	}

	origin = parv[1];
	destination = parv[2];	/* Will get NULL or pointer (parc >= 2!!) */

	if(ConfigServerHide.disable_remote && !IsOper(source_p))
	{
		sendto_one(source_p, ":%s PONG %s :%s", me.name,
			   (destination) ? destination : me.name, origin);
		return 0;
	}

	if(!EmptyString(destination) && irccmp(destination, me.name))
	{
		/* We're sending it across servers.. origin == client_p->name --fl_ */
		origin = client_p->name;

		/* XXX - sendto_server() ? --fl_ */
		if((target_p = find_server(destination)))
		{
			/* use the direct link for LL checking */
			target_p = target_p->from;

			sendto_one(target_p, ":%s PING %s :%s", parv[0], origin, destination);
		}
		else
		{
			sendto_one(source_p, form_str(ERR_NOSUCHSERVER),
				   me.name, parv[0], destination);
			return 0;
		}
	}
	else
		sendto_one(source_p, ":%s PONG %s :%s", me.name,
			   (destination) ? destination : me.name, origin);

	return 0;
}

static int
ms_ping(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	const char *origin, *destination;

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NOORIGIN), me.name, parv[0]);
		return 0;
	}

/* origin == source_p->name, lets not even both wasting effort on it --fl_ */
	origin = source_p->name;
	destination = parv[2];	/* Will get NULL or pointer (parc >= 2!!) */

	if(!EmptyString(destination) && irccmp(destination, me.name))
	{
		if((target_p = find_server(destination)))
			sendto_one(target_p, ":%s PING %s :%s", parv[0], origin, destination);
		else
		{
			sendto_one(source_p, form_str(ERR_NOSUCHSERVER),
				   me.name, parv[0], destination);
			return 0;
		}
	}
	else
		sendto_one(source_p, ":%s PONG %s :%s", me.name,
			   (destination) ? destination : me.name, origin);

	return 0;
}
