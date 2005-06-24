/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_locops.c
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
#include "irc_string.h"
#include "numeric.h"
#include "send.h"
#include "s_user.h"
#include "s_conf.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

struct Message locops_msgtab = {
  MSG_LOCOPS, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, m_locops}
};

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

char *_version = "20001122";

/*
 * m_locops - LOCOPS message handler
 * (write to *all* local opers currently online)
 *      parv[0] = sender prefix
 *      parv[1] = message text
 */
int m_locops(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  char *message = NULL;

  message = parv[1];

  if (EmptyString(message))
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "LOCOPS");
      return 0;
    }

  sendto_all_local_opers(sptr, NULL, "LOCOPS - %s", message);

  return 0;
}


