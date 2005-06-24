/************************************************************************
 *   IRC - Internet Relay Chat, contrib/m_clearchan.c
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

#define MSG_CLEARCHAN "CLEARCHAN"


static void mo_clearchan(struct Client *cptr, struct Client *sptr,
                         int parc, char *parv[]);

static void kick_list(struct Client *cptr, struct Channel *chptr,
                      dlink_list *list,char *chname);

struct Message clearchan_msgtab = {
  MSG_CLEARCHAN, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, mo_clearchan, mo_clearchan}
};

void
_modinit(void)
{
  mod_add_cmd(&clearchan_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&clearchan_msgtab);
}

char *_version = "20010104";

/*
** mo_clearchan
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static void mo_clearchan(struct Client *cptr, struct Client *sptr,
                        int parc, char *parv[])
{
  struct Channel *chptr, *root_chptr;
  int on_vchan = 0;

  /* admins only */
  if (!IsSetOperAdmin(sptr))
    {
      sendto_one(sptr, ":%s NOTICE %s :You have no A flag", me.name, parv[0]);
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
      sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
		 me.name, parv[0], parv[1]);
      return;
    }

  if (!on_vchan)
    {
     sendto_wallops_flags(FLAGS_WALLOP, &me, 
              "CLEARCHAN called for [%s] by %s!%s@%s",
              parv[1], sptr->name, sptr->username, sptr->host);
     sendto_ll_serv_butone(NULL, sptr, 1,
            ":%s WALLOPS :CLEARCHAN called for [%s] by %s!%s@%s",
              me.name, parv[1], sptr->name, sptr->username, sptr->host);
     log(L_NOTICE, "CLEARCHAN called for [%s] by %s!%s@%s",
                   parv[1], sptr->name, sptr->username, sptr->host);
    }
  else
    {
     sendto_wallops_flags(FLAGS_WALLOP, &me,
              "CLEARCHAN called for [%s %s] by %s!%s@%s",
              parv[1], parv[2], sptr->name, sptr->username, sptr->host);
     sendto_ll_serv_butone(NULL, sptr, 1,
            ":%s WALLOPS :CLEARCHAN called for [%s %s] by %s!%s@%s",
              me.name, parv[1], parv[2], sptr->name, sptr->username, sptr->host);
     log(L_NOTICE, "CLEARCHAN called for [%s %s] by %s!%s@%s",
                   parv[1], parv[2], sptr->name, sptr->username, sptr->host);
    }
  
  add_user_to_channel(chptr, sptr, CHFL_CHANOP);
  kick_list(cptr, chptr, &chptr->chanops, parv[1]);
  kick_list(cptr, chptr, &chptr->voiced, parv[1]);
  kick_list(cptr, chptr, &chptr->halfops, parv[1]);
  kick_list(cptr, chptr, &chptr->peons, parv[1]);

  /* Don't reset channel TS. */
  /* XXX - check this isn't too big above... */
  sptr->user->joined++;
  /* Take the TS down by 1, so we don't see the channel taken over
   * again. */
  if (chptr->channelts)
    chptr->channelts--;
  if (on_vchan)
    add_vchan_to_client_cache(sptr,root_chptr,chptr);
  chptr->mode.mode =
    MODE_SECRET | MODE_TOPICLIMIT | MODE_INVITEONLY | MODE_NOPRIVMSGS;

  MyFree(chptr->topic_info);
  chptr->topic_info = 0;

  *chptr->topic = 0;
  *chptr->mode.key = 0;
  sendto_ll_channel_remote(chptr, cptr, sptr,
      ":%s SJOIN %lu %s +ntsi :@%s", me.name, chptr->channelts,
      chptr->chname, sptr->name);
  sendto_one(sptr, ":%s!%s@%s JOIN %s",
	     sptr->name,
	     sptr->username,
	     sptr->host,
	     root_chptr->chname);
  channel_member_names(sptr, chptr, root_chptr->chname);
}

void kick_list(struct Client *cptr, struct Channel *chptr,
	       dlink_list *list,char *chname)
{
  struct Client *who;
  dlink_node *ptr;
  /* Skip the first entry(our newly added one) if this is the chanops
   * list... */
  for (ptr = (list == &chptr->chanops) ? list->head->next : list->head;
       ptr; ptr = ptr->next)
    {
      who = ptr->data;
      sendto_channel_local(ALL_MEMBERS, chptr,
			   ":%s KICK %s %s :CLEARCHAN",
			   me.name, chname, who->name);

      sendto_channel_remote(chptr, cptr,
			    "KICK %s %s :CLEARCHAN", chname, who->name);

      remove_user_from_channel(chptr, who);

    }
}


