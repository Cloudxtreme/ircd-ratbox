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

struct Message time_msgtab = {
  MSG_TIME, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_time, ms_time, mo_time}
};

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

/*
 * m_time
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
int m_time(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  sendto_one(sptr, form_str(RPL_TIME), me.name,
             parv[0], me.name, date(0));
  return 0;
}

/*
 * mo_time
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
int mo_time(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  if (hunt_server(cptr,sptr,":%s TIME :%s",1,parc,parv) == HUNTED_ISME)
    sendto_one(sptr, form_str(RPL_TIME), me.name,
               parv[0], me.name, date(0));
  return 0;
}

/*
 * ms_time
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
int ms_time(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  if (hunt_server(cptr,sptr,":%s TIME :%s",1,parc,parv) == HUNTED_ISME)
    {
      if(!GlobalSetOptions.hide_server || IsOper(sptr))
	sendto_one(sptr, form_str(RPL_TIME), me.name,
		   parv[0], me.name, date(0));
    }
  return 0;
}

