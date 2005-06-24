/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_version.c
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
#include "s_misc.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void m_time(struct Client*, struct Client*, int, char**);
static void mo_time(struct Client*, struct Client*, int, char**);

struct Message time_msgtab = {
  "TIME", 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_time, mo_time, mo_time}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&time_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&time_msgtab);
}

char *_version = "20001202";
#endif
/*
 * m_time
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
static void m_time(struct Client *client_p, struct Client *source_p,
                  int parc, char *parv[])
{
  /* This is safe enough to use during non hidden server mode */
  if(!GlobalSetOptions.hide_server)
    {
      if (hunt_server(client_p,source_p,":%s TIME :%s",1,parc,parv) != HUNTED_ISME)
        return;
    }

  sendto_one(source_p, form_str(RPL_TIME), me.name,
             parv[0], me.name, date(0));
}

/*
 * mo_time
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
static void mo_time(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
  if (hunt_server(client_p,source_p,":%s TIME :%s",1,parc,parv) == HUNTED_ISME)
    sendto_one(source_p, form_str(RPL_TIME), me.name,
               parv[0], me.name, date(0));
}
