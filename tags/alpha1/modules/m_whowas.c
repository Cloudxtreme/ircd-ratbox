/************************************************************************
*   IRC - Internet Relay Chat, modules/whowas.c
*   Copyright (C) 1990 Markku Savela
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
#include "whowas.h"
#include "handlers.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "ircd_defs.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "memdebug.h"

struct Message whowas_msgtab = {
  MSG_WHOWAS, 0, 0, 0, MFLG_SLOW, 0L,
  {m_unregistered, m_whowas, mo_whowas, m_whowas}
};

void
_modinit(void)
{
  mod_add_cmd(&whowas_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&whowas_msgtab);
}

int whowas_do(struct Client *cptr, struct Client *sptr,
	      int parc, char *parv[]);

char *_version = "20001122";

/*
** m_whowas
**      parv[0] = sender prefix
**      parv[1] = nickname queried
*/
int     m_whowas(struct Client *cptr,
                 struct Client *sptr,
                 int parc,
                 char *parv[])
{
  static time_t last_used=0L;

  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, parv[0]);
      return 0;
    }

  if((last_used + ConfigFileEntry.whois_wait) > CurrentTime)
    {
      return 0;
    }
  else
    {
      last_used = CurrentTime;
    }

  return(whowas_do(cptr,sptr,parc,parv));
}

int     mo_whowas(struct Client *cptr,
                 struct Client *sptr,
                 int parc,
                 char *parv[])
{
  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, parv[0]);
      return 0;
    }

  return(whowas_do(cptr,sptr,parc,parv));
}

int whowas_do(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Whowas *temp;
  int cur = 0;
  int     max = -1, found = 0;
  char    *p, *nick;

  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, parv[0]);
      return 0;
    }
  if (parc > 2)
    max = atoi(parv[2]);
  if (parc > 3)
    if (hunt_server(cptr,sptr,":%s WHOWAS %s %s :%s", 3,parc,parv))
      return 0;


  if((p = strchr(parv[1],',')))
     *p = '\0';

  nick = parv[1];

  temp = WHOWASHASH[hash_whowas_name(nick)];
  found = 0;
  for(;temp;temp=temp->next)
    {
      if (!irccmp(nick, temp->name))
        {
          sendto_one(sptr, form_str(RPL_WHOWASUSER),
                     me.name, parv[0], temp->name,
                     temp->username,
                     temp->hostname,
                     temp->realname);

          if (GlobalSetOptions.hide_server && !IsOper(sptr))
            sendto_one(sptr, form_str(RPL_WHOISSERVER),
                       me.name, parv[0], temp->name,
                       ConfigFileEntry.network_name, myctime(temp->logoff));
          else
	    sendto_one(sptr, form_str(RPL_WHOISSERVER),
                       me.name, parv[0], temp->name,
                       temp->servername, myctime(temp->logoff));
          cur++;
          found++;
        }
      if (max > 0 && cur >= max)
        break;
    }
  if (!found)
    sendto_one(sptr, form_str(ERR_WASNOSUCHNICK),
               me.name, parv[0], nick);

  sendto_one(sptr, form_str(RPL_ENDOFWHOWAS), me.name, parv[0], parv[1]);
  return 0;
}
