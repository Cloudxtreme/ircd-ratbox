/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_post.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   $Id$
 */
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"

static void mr_post(struct Client*, struct Client*, int, char**);

struct Message post_msgtab = {
  "POST", 0, 0, 0, 0, MFLG_SLOW | MFLG_UNREG, 0,
  {mr_post, m_ignore, m_ignore, m_ignore}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&post_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&post_msgtab);
}

char *_version = "20010309";
#endif
/*
** mr_post
**      parv[0] = sender prefix
**      parv[1] = comment
*/
static void mr_post(struct Client *client_p,
                  struct Client *source_p,
                  int parc,
                  char *parv[])
{
  sendto_realops_flags(FLAGS_REJ, L_ALL,
                       "Client rejected for POST command: [%s@%s]",
                       client_p->username, client_p->host);
  exit_client(client_p, source_p, source_p, "Client Exit");
}
