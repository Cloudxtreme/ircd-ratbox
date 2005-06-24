/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_htm.c: Does HTM output.
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
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "s_serv.h"
#include "packet.h"


static void m_htm(struct Client*, struct Client*, int, char**);

struct Message htm_msgtab = {
  "HTM", 0, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_ignore, m_ignore, m_htm}
};
#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&htm_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&htm_msgtab);
}
const char *_version = "$Revision$";
#endif

/*
** m_htm
**      parv[0] = sender prefix
*/
static void m_htm(struct Client *client_p,
                  struct Client *source_p,
                  int parc,
                  char *parv[])
{
  get_current_bandwidth(client_p, source_p);
}

