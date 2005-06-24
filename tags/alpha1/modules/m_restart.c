/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_restart.c
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
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "restart.h"
#include "s_log.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

struct Message restart_msgtab = {
  MSG_RESTART, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, mo_restart}
};

void
_modinit(void)
{
  mod_add_cmd(&restart_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&restart_msgtab);
}

char *_version = "20001122";

/*
 * mo_restart
 *
 */
int     mo_restart(struct Client *cptr,
                  struct Client *sptr,
                  int parc,
                  char *parv[])
{
  char buf[BUFSIZE]; 
  if (!MyClient(sptr) || !IsOper(sptr))
    {
      sendto_one(sptr, form_str(ERR_NOPRIVILEGES), me.name, parv[0]);
      return 0;
    }

  if ( !IsOperDie(sptr) )
    {
      sendto_one(sptr,":%s NOTICE %s :You have no D flag", me.name, parv[0]);
      return 0;
    }

  if (parc < 2)
  {
	  sendto_one(sptr, ":%s NOTICE %s :Need server name /restart %s",
				 me.name, sptr->name, me.name);
	  return 0;
  }
  else
  {
	  if (irccmp(parv[1], me.name))
	  {
		  sendto_one(sptr, ":%s NOTICE %s :Mismatch on /restart %s",
					 me.name, sptr->name, me.name);
		  return 0;
	  }
  }
  
  log(L_WARN, "Server RESTART by %s\n", get_client_name(sptr, SHOW_IP));
  ircsprintf(buf, "Server RESTART by %s", get_client_name(sptr, SHOW_IP));
  restart(buf);
  return 0; /*NOT REACHED*/
}

