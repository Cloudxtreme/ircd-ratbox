/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_close.c: Closes all unregistered connections.
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
#include "rpi/client.h"
#include "ircd.h"
#include "numeric.h"
#include "commio.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int mo_close(struct rpi_client *, struct rpi_client *, int, const char **);

struct Message close_msgtab = {
	"CLOSE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_close, 0}}
};

mapi_clist_av1 close_clist[] = { &close_msgtab, NULL };
DECLARE_MODULE_AV1(close, NULL, NULL, close_clist, NULL, NULL, "$Revision$");

/*
 * mo_close - CLOSE message handler
 *  - added by Darren Reed Jul 13 1992.
 */
static int
mo_close(struct rpi_client *client_p, struct rpi_client *source_p, int parc, const char *parv[])
{
	struct rpi_client *target_p;
	dlink_node *ptr;
	dlink_node *ptr_next;
	int closed = 0;

	DLINK_FOREACH_SAFE(ptr, ptr_next, unknown_list.head)
	{
		target_p = ptr->data;

		sendto_one(source_p, form_str(RPL_CLOSING), me->name, source_p->name,
			   target_p->v->get_name(target_p, SHOW_IP), target_p->status);

		(void) exit_client(target_p, target_p, target_p, "Oper Closing");
		closed++;
	}

	sendto_one(source_p, form_str(RPL_CLOSEEND), me->name, source_p->name, closed);
	return 0;
}
