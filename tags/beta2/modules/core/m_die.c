/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_die.c
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
#include "irc_string.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_log.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void mo_die(struct Client*, struct Client*, int, char**);

struct Message die_msgtab = {
  "DIE", 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, mo_die}
};

void
_modinit(void)
{
  mod_add_cmd(&die_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&die_msgtab);
}

char *_version = "20001122";

/*
 * mo_die - DIE command handler
 */
static void mo_die(struct Client *cptr, struct Client *sptr,
                  int parc, char *parv[])
{
  struct Client* acptr;
  dlink_node *ptr;

  if (!IsOperDie(sptr))
    {
      sendto_one(sptr,":%s NOTICE %s :You have no D flag", me.name, parv[0]);
      return;
    }

  if (parc < 2)
    {
      sendto_one(sptr,":%s NOTICE %s :Need server name /die %s",
                 me.name,sptr->name,me.name);
      return;
    }
  else
    {
      if (irccmp(parv[1], me.name))
        {
          sendto_one(sptr,":%s NOTICE %s :Mismatch on /die %s",
                     me.name,sptr->name,me.name);
          return;
        }
    }

  for(ptr = lclient_list.head; ptr; ptr = ptr->next)
    {
      acptr = ptr->data;

      sendto_one(acptr,
		 ":%s NOTICE %s :Server Terminating. %s",
		 me.name, acptr->name,
		 get_client_name(sptr, MASK_IP));
    }

  for(ptr = lclient_list.head; ptr; ptr = ptr->next)
    {
      acptr = ptr->data;

      sendto_one(acptr,
		 ":%s NOTICE %s :Server Terminating. %s",
		 me.name, acptr->name,
		 get_client_name(sptr, HIDE_IP));
    }

  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      acptr = ptr->data;

      sendto_one(acptr, ":%s ERROR :Terminated by %s",
		 me.name, get_client_name(sptr, MASK_IP));
    }

  /*
   * XXX we called flush_connections() here. Read server_reboot()
   * for an explanation as to what we should do.
   *     -- adrian
   */
  log(L_NOTICE, "Server terminated by %s", get_client_name(sptr, HIDE_IP));
  /* 
   * this is a normal exit, tell the os it's ok 
   */
  exit(0);
  /* NOT REACHED */
}

