/************************************************************************
 *   IRC - Internet Relay Chat, contrib/m_opme.c
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
#include "channel.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_log.h"
#include "s_serv.h"
#include "send.h"
#include "whowas.h"
#include "irc_string.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "vchannel.h"

#include <string.h>

static void mo_opme(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);
static int chan_is_opless(struct Channel *chptr);

struct Message opme_msgtab = {
  "OPME", 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, mo_opme}
};

void
_modinit(void)
{
  mod_add_cmd(&opme_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&opme_msgtab);
}

char *_version = "20010104";

static int chan_is_opless(struct Channel *chptr)
{
  if (chptr->chanops.head)
	  return 0;
  else
	  return 1;
}

/*
** mo_opme
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static void mo_opme(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  struct Channel *chptr, *root_chptr;
  int on_vchan = 0;
  dlink_node *ptr;
  
  /* admins only */
  if (!IsSetOperAdmin(source_p))
    {
      sendto_one(source_p, ":%s NOTICE %s :You have no A flag", me.name, parv[0]);
      return;
    }

  /* XXX - we might not have CBURSTed this channel if we are a lazylink
   * yet. */
  chptr= hash_find_channel(parv[1], NullChn);
  root_chptr = chptr;
  if (chptr && parc > 2 && parv[2][0] == '!')
    {
      chptr = find_vchan(chptr, parv[2]);
      if (root_chptr != chptr)
		  on_vchan++;
    }
  
  if( chptr == NULL )
    {
      sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
		 me.name, parv[0], parv[1]);
      return;
    }

  if (!chan_is_opless(chptr))
  {
    sendto_one(source_p, ":%s NOTICE %s :%s Channel is not opless",
               me.name, parv[0], parv[1]);
    return;
  }

  if ((ptr = find_user_link(&chptr->peons, source_p)))
	  dlinkDelete(ptr, &chptr->peons);
  else if ((ptr = find_user_link(&chptr->voiced, source_p)))
	  dlinkDelete(ptr, &chptr->voiced);
  else if ((ptr = find_user_link(&chptr->halfops, source_p)))
	  dlinkDelete(ptr, &chptr->halfops);
  else if ((ptr = find_user_link(&chptr->chanops, source_p)))
	  dlinkDelete(ptr, &chptr->chanops);
  else
    {
       /* Theyre not even on the channel, bail. */
       return;      
    }
   
  dlinkAdd(source_p, ptr, &chptr->chanops);
  
  if (!on_vchan)
    {
     sendto_wallops_flags(FLAGS_WALLOP, &me,
              "OPME called for [%s] by %s!%s@%s",
              parv[1], source_p->name, source_p->username, source_p->host);
     sendto_ll_serv_butone(NULL, source_p, 1,
            ":%s WALLOPS :OPME called for [%s] by %s!%s@%s",
              me.name, parv[1], source_p->name, source_p->username, source_p->host);
     log(L_NOTICE, "OPME called for [%s] by %s!%s@%s",
                   parv[1], source_p->name, source_p->username, source_p->host);
    }
  else
    {
     sendto_wallops_flags(FLAGS_WALLOP, &me,
               "OPME called for [%s %s] by %s!%s@%s",
               parv[1], parv[2], source_p->name, source_p->username, source_p->host);
     sendto_ll_serv_butone(NULL, source_p, 1,
            ":%s WALLOPS :OPME called for [%s %s] by %s!%s@%s",
              me.name, parv[1], parv[2], source_p->name, source_p->username, source_p->host);
     log(L_NOTICE, "OPME called for [%s %s] by %s!%s@%s",
                   parv[1], parv[2], source_p->name, source_p->username, source_p->host);
    }

  sendto_match_cap_servs(chptr, source_p, CAP_UID, ":%s PART %s",
						 ID(source_p), parv[1]);
  sendto_match_nocap_servs(chptr, source_p, CAP_UID, ":%s PART %s",
						   source_p->name, parv[1]);
  sendto_match_cap_servs(chptr, source_p, CAP_UID, ":%s SJOIN %ld %s + :@%s",
						 me.name, chptr->channelts, parv[1], /* XXX ID(source_p) */ source_p->name);
  sendto_match_nocap_servs(chptr, source_p, CAP_UID, ":%s SJOIN %ld %s + :@%s",
					 me.name, chptr->channelts, parv[1], source_p->name);
  sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +o %s",
					   me.name, parv[1], source_p->name);
  
/*
  sendto_channel_remote(chptr, source_p, ":%s MODE %s +o %s",
						 me.name, parv[1], source_p->name);
*/
}

