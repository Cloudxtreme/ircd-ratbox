/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_close.c
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
#include "tools.h"
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void mo_close(struct Client*, struct Client*, int, char**);

struct Message close_msgtab = {
  "CLOSE", 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, mo_close}
};
#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&close_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&close_msgtab);
}

char *_version = "20001122";
#endif
/*
 * mo_close - CLOSE message handler
 *  - added by Darren Reed Jul 13 1992.
 */
static void mo_close(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  struct Client  *target_p;
  dlink_node     *ptr;
  int            closed = 0;



  for (ptr = unknown_list.head; ptr; ptr = ptr->next)
    {
      target_p = ptr->data;

  /* Which list would connecting servers be found in? serv_list ? */
#if 0
      if (!IsUnknown(target_p) && !IsConnecting(target_p) &&
          !IsHandshake(target_p) && !IsDoingKauth(target_p))
        continue;
#endif
      sendto_one(source_p, form_str(RPL_CLOSING), me.name, parv[0],
                 get_client_name(target_p, SHOW_IP), target_p->status);
      (void)exit_client(target_p, target_p, target_p, "Oper Closing");
      closed++;
    }
  sendto_one(source_p, form_str(RPL_CLOSEEND), me.name, parv[0], closed);
}

