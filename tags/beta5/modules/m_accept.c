/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_accept.c
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
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void m_accept(struct Client*, struct Client*, int, char**);

struct Message accept_msgtab = {
  "ACCEPT", 0, 2, 0, MFLG_SLOW | MFLG_UNREG, 0, 
  {m_unregistered, m_accept, m_ignore, m_accept}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&accept_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&accept_msgtab);
}

char *_version = "20001122";
#endif
/*
 * m_accept - ACCEPT command handler
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
static void m_accept(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  char *nick;
  int  add=1;
  struct Client *source;

  nick = parv[1];

  add = 1;

  if (*nick == '-')
    {
      add = -1;
      nick++;
    }
  else if(*nick == '*')
    {
      list_all_accepts(source_p);
      return;
    }

  if ((source = find_client(nick,NULL)) == NULL)
    {
      sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, parv[0], nick);
      return;
    }

  if (!IsPerson(source))
    {
      sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, parv[0], nick);
      return;
    }

  if (add == 1)
    {
      if (accept_message(source, source_p)) {
	/* already on list */
	sendto_one(source_p, ":%s NOTICE %s :%s is already on your accept list",
		   me.name, parv[0], source->name);
	return;
      }
		
      del_from_accept(source,source_p);
      add_to_accept(source,source_p);
      sendto_one(source_p, ":%s NOTICE %s :Now allowing %s", 
		 me.name, parv[0], source->name);
      sendto_anywhere(source, source_p,
		      "NOTICE %s :*** I've put you on my accept list.",
		      source->name);
    }
  else if(add == -1)
    {
		if (!accept_message(source, source_p)) 
		{
			/* not on list */
			sendto_one(source_p, ":%s NOTICE %s :%s is not on your accept list",
					   me.name, parv[0], source->name);
			return;
		}
		
      del_from_accept(source,source_p);
      sendto_one(source_p, ":%s NOTICE %s :Now removed %s from allow list", 
		 me.name, parv[0], source->name);
      sendto_anywhere(source, source_p,
		      "NOTICE %s :*** I've taken you off my accept list.",
		      source->name);
    }
}

