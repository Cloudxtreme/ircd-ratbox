/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_post.c: Exits the user if unregistered, it is a web form.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2012 ircd-ratbox development team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"
#include "struct.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "parse.h"
#include "modules.h"

static int mr_dumb_proxy(struct Client *, struct Client *, int, const char **);

struct Message post_msgtab = {
	"POST", 0, 0, 0, MFLG_SLOW | MFLG_UNREG,
	{{mr_dumb_proxy, 0}, mg_ignore, mg_ignore, mg_ignore, mg_ignore, mg_ignore}
};

struct Message get_msgtab = {
	"GET", 0, 0, 0, MFLG_SLOW | MFLG_UNREG,
	{{mr_dumb_proxy, 0}, mg_ignore, mg_ignore, mg_ignore, mg_ignore, mg_ignore}
};

struct Message put_msgtab = {
	"PUT", 0, 0, 0, MFLG_SLOW | MFLG_UNREG,
	{{mr_dumb_proxy, 0}, mg_ignore, mg_ignore, mg_ignore, mg_ignore, mg_ignore}
};


mapi_clist_av1 post_clist[] = {
	&post_msgtab, &get_msgtab, &put_msgtab, NULL
};

DECLARE_MODULE_AV1(post, NULL, NULL, post_clist, NULL, NULL, "$Revision$");


/*
** mr_dumb_proxy
**      parv[0] = sender prefix
**      parv[1] = comment
*/
static int
mr_dumb_proxy(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	sendto_realops_flags(UMODE_REJ, L_ALL,
			     "HTTP Proxy disconnected: [%s@%s]",
			     client_p->username, client_p->host);
	exit_client(client_p, source_p, source_p, "Client Exit");

	return 0;
}
