/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_locops.c: Sends a message to all operators on the local server.
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
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "irc_string.h"
#include "numeric.h"
#include "send.h"
#include "s_user.h"
#include "s_conf.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "cluster.h"
#include "s_serv.h"

static void m_locops(struct Client *, struct Client *, int, char **);
static void ms_locops(struct Client *, struct Client *, int, char **);

struct Message locops_msgtab = {
	"LOCOPS", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, ms_locops, m_locops}
};
#ifndef STATIC_MODULES

void
_modinit(void)
{
	mod_add_cmd(&locops_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&locops_msgtab);
}

const char *_version = "$Revision$";
#endif

/*
 * m_locops - LOCOPS message handler
 * (write to *all* local opers currently online)
 *      parv[0] = sender prefix
 *      parv[1] = message text
 */
static void
m_locops(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "LOCOPS");
		return;
	}

	sendto_wallops_flags(UMODE_LOCOPS, source_p, "LOCOPS - %s", parv[1]);

	if(dlink_list_length(&cluster_list) > 0)
		cluster_locops(source_p, parv[1]);
}

static void
ms_locops(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if(parc != 3 || EmptyString(parv[2]))
		return;

	/* parv[0]  parv[1]      parv[2]
	 * oper     target serv  message
	 */
	sendto_match_servs(source_p, parv[1], CAP_CLUSTER, NOCAPS,
				"LOCOPS %s :%s", parv[1], parv[2]);

	if(!match(parv[1], me.name))
		return;

	if(!IsPerson(source_p))
		return;

	if(find_cluster(source_p->user->server, CLUSTER_LOCOPS))
		sendto_wallops_flags(UMODE_LOCOPS, source_p, "SLOCOPS - %s", parv[2]);
}
