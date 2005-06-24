/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_users.c
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

static void m_users(struct Client*, struct Client*, int, char**);

struct Message users_msgtab = {
  "USERS", 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_users, m_users, m_users}
};

void
_modinit(void)
{
  mod_add_cmd(&users_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&users_msgtab);
}

char *_version = "20001122";

/*
 * m_users
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
static void m_users(struct Client *cptr, struct Client *sptr,
                   int parc, char *parv[])
{
  if (hunt_server(cptr,sptr,":%s USERS :%s",1,parc,parv) == HUNTED_ISME)
    {
      /* No one uses this any more... so lets remap it..   -Taner */
      
      if (!GlobalSetOptions.hide_server || IsOper(sptr))
        sendto_one(sptr, form_str(RPL_LOCALUSERS), me.name, parv[0],
                   Count.local, Count.max_loc);

      sendto_one(sptr, form_str(RPL_GLOBALUSERS), me.name, parv[0],
                 Count.total, Count.max_tot);
    }
}

