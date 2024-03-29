/* contrib/m_force.c
 * Copyright (C) 2002 Hybrid Development Team
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1.Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  2.Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  3.The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#include "stdinc.h"
#include "handlers.h"
#include "client.h"
#include "common.h"     /* FALSE bleah */
#include "ircd.h"
#include "irc_string.h"
#include "numeric.h"
#include "fdlist.h"
#include "hash.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "channel.h"
#include "channel_mode.h"


static void mo_forcejoin(struct Client *client_p, struct Client *source_p,
                         int parc, char *parv[]);
static void mo_forcepart(struct Client *client_p, struct Client *source_p,
		         int parc, char *parv[]);

struct Message forcejoin_msgtab = {
  "FORCEJOIN", 0, 0, 3, 0, MFLG_SLOW, 0,
  {m_ignore, m_not_oper, mo_forcejoin, mo_forcejoin}
};
struct Message forcepart_msgtab = {
  "FORCEPART", 0, 0, 3, 0, MFLG_SLOW, 0,
  {m_ignore, m_not_oper, mo_forcepart, mo_forcepart}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&forcejoin_msgtab);
  mod_add_cmd(&forcepart_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&forcejoin_msgtab);
  mod_del_cmd(&forcepart_msgtab);
}

char *_version = "$Revision$";
#endif

/*
 * m_forcejoin
 *      parv[0] = sender prefix
 *      parv[1] = user to force
 *      parv[2] = channel to force them into
 */
static void mo_forcejoin(struct Client *client_p, struct Client *source_p,
                         int parc, char *parv[])
{
  struct Client *target_p;
  struct Channel *chptr;
  int type;
  char mode;
  char sjmode;
  char *newch;

  if(!IsAdmin(source_p))
  {
    sendto_one(source_p, ":%s NOTICE %s :You have no A flag", me.name, parv[0]);
    return;
  }

  if((hunt_server(client_p, source_p, ":%s FORCEJOIN %s %s", 1, parc, parv)) != HUNTED_ISME)
    return;

  /* if target_p is not existant, print message
   * to source_p and bail - scuzzy
   */
  if ((target_p = find_client(parv[1])) == NULL)
  {
    sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
	       source_p->name, parv[1]);
    return;
  }

  if(!IsClient(target_p))
    return;

  /* select our modes from parv[2] if they exist... (chanop)*/
  if(*parv[2] == '@')
  {
    type = CHFL_CHANOP;
    mode = 'o';
    sjmode = '@';
  }
#ifdef HALFOPS
  else if(*parv[2] == '%')
  {
    type = CHFL_HALFOP;
    mode = 'h';
    sjmode = '%';
  }
#endif
  else if(*parv[2] == '+')
  {
    type = CHFL_VOICE;
    mode = 'v';
    sjmode = '+';
  }
  else
  {
    type = CHFL_PEON;
    mode = sjmode = '\0';
  }
    
  if(mode != '\0')
    parv[2]++;
    
  if((chptr = hash_find_channel(parv[2])) != NULL)
    {
      if(IsMember(target_p, chptr))
      {
        /* debugging is fun... */
        sendto_one(source_p, ":%s NOTICE %s :*** Notice -- %s is already in %s", me.name,
		   source_p->name, target_p->name, chptr->chname);
	return;
      }

      add_user_to_channel(chptr, target_p, type);

      if (chptr->chname[0] != '&')
        sendto_server(target_p, target_p, chptr, NOCAPS, NOCAPS, LL_ICLIENT,
	              ":%s SJOIN %lu %s + :%c%s",
	              me.name, (unsigned long) chptr->channelts,
	              chptr->chname, type ? sjmode : ' ', target_p->name);

      sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
                             target_p->name, target_p->username,
                             target_p->host, chptr->chname);

      if(type)
        sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +%c %s",
	                     me.name, chptr->chname, mode, target_p->name);
        
      if(chptr->topic != NULL)
      {
	sendto_one(target_p, form_str(RPL_TOPIC), me.name,
	           target_p->name, chptr->chname, chptr->topic);
        sendto_one(target_p, form_str(RPL_TOPICWHOTIME),
	           me.name, source_p->name, chptr->chname,
	           chptr->topic_info, chptr->topic_time);
      }

      channel_member_names(target_p, chptr, chptr->chname, 1);
    }
  else
    {
      newch = parv[2];
      if (!check_channel_name(newch))
      {
        sendto_one(source_p, form_str(ERR_BADCHANNAME), me.name,
		   source_p->name, (unsigned char*)newch);
	return;
      }

      /* channel name must begin with & or # */
      if (!IsChannelName(newch))
      {
        sendto_one(source_p, form_str(ERR_BADCHANNAME), me.name,
		   source_p->name, (unsigned char*)newch);
        return;
      }

     /* it would be interesting here to allow an oper
      * to force target_p into a channel that doesn't exist
      * even more so, into a local channel when we disable
      * local channels... but...
      * I don't want to break anything - scuzzy
      */
      if (ConfigServerHide.disable_local_channels &&
	  (*newch == '&'))
      {
        sendto_one(source_p, ":%s NOTICE %s :No such channel (%s)", me.name,
		   source_p->name, newch);
        return;
      }

      /* newch can't be longer than CHANNELLEN */
      if (strlen(newch) > CHANNELLEN)
      {
	sendto_one(source_p, ":%s NOTICE %s :Channel name is too long", me.name,
		   source_p->name);
        return;
      }

      chptr = get_or_create_channel(target_p, newch, NULL);
      add_user_to_channel(chptr, target_p, CHFL_CHANOP);

      /* send out a join, make target_p join chptr */
      if (chptr->chname[0] != '&')
        sendto_server(target_p, target_p, chptr, NOCAPS, NOCAPS, LL_ICLIENT,
                      ":%s SJOIN %lu %s +nt :@%s", me.name,
		      (unsigned long) chptr->channelts, chptr->chname,
		      target_p->name);

      sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
                             target_p->name, target_p->username,
                             target_p->host, chptr->chname);

      chptr->mode.mode |= MODE_TOPICLIMIT;
      chptr->mode.mode |= MODE_NOPRIVMSGS;

      sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +nt", me.name,
                           chptr->chname);

      target_p->localClient->last_join_time = CurrentTime;
      channel_member_names(target_p, chptr, chptr->chname, 1);

      /* we do this to let the oper know that a channel was created, this will be
       * seen from the server handling the command instead of the server that
       * the oper is on.
       */
      sendto_one(source_p, ":%s NOTICE %s :*** Notice -- Creating channel %s", me.name,
		 source_p->name, chptr->chname);
    }
}


static void mo_forcepart(struct Client *client_p, struct Client *source_p,
		         int parc, char *parv[])
{
  struct Client *target_p;
  struct Channel *chptr;

  if(!IsAdmin(source_p))
  {
    sendto_one(source_p, ":%s NOTICE %s :You have no A flag", me.name, parv[0]);
    return;
  }

  if((hunt_server(client_p, source_p, ":%s FORCEPART %s %s", 1, parc, parv)) != HUNTED_ISME)
    return;

  /* if target_p == NULL then let the oper know */
  if ((target_p = find_client(parv[1])) == NULL)
  {
    sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
               source_p->name, parv[1]);
    return;
  }

  if(!IsClient(target_p))
    return;

  if((chptr = hash_find_channel(parv[2])) == NULL)
  {
    sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
               me.name, parv[0], parv[1]);
    return;
  }

  if (!IsMember(target_p, chptr))
  {
    sendto_one(source_p, form_str(ERR_USERNOTINCHANNEL),
               me.name, parv[0], parv[2], parv[1]);
    return;
  }
  
  if (chptr->chname[0] != '&')
    sendto_server(target_p, target_p, chptr, NOCAPS, NOCAPS, LL_ICLIENT,
		  ":%s PART %s :%s",
		  target_p->name, chptr->chname,
		  target_p->name);

  sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s :%s",
                       target_p->name, target_p->username,
 	               target_p->host,chptr->chname,
		       target_p->name);
  remove_user_from_channel(chptr, target_p);
}

