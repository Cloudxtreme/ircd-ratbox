/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_message.c
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
#include "common.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include "channel.h"
#include "vchannel.h"
#include "irc_string.h"
#include "hash.h"
#include "class.h"
#include "msg.h"
#include "packet.h"

#include <string.h>

struct entity {
  void *ptr;
  int type;
  int flags;
};

static int build_target_list(int p_or_n, char *command,
                             struct Client *client_p, struct Client *source_p,
                             char *nicks_channels, struct entity ***targets,
                             char *text);

static int flood_attack_client(int p_or_n, struct Client *source_p, struct Client *target_p);
static int flood_attack_channel(int p_or_n, struct Client *source_p, struct Channel *chptr,
                                char *chname);

#define ENTITY_NONE    0
#define ENTITY_CHANNEL 1
#define ENTITY_CHANOPS_ON_CHANNEL 2
#define ENTITY_CLIENT  3

static struct entity **target_table = NULL;
static int target_table_size = 0;

static int duplicate_ptr( void *, struct entity **, int);

static void m_message(int, char *, struct Client *,
                     struct Client *, int, char **);

static void m_privmsg(struct Client *, struct Client *, int, char **);
static void m_notice(struct Client *, struct Client *, int, char **);

static void msg_channel( int p_or_n, char *command,
                         struct Client *client_p,
                         struct Client *source_p,
                         struct Channel *chptr,
                         char *text);

static void msg_channel_flags( int p_or_n, char *command,
                               struct Client *client_p,
                               struct Client *source_p,
                               struct Channel *chptr,
                               int flags,
                               char *text);

static void msg_client(int p_or_n, char *command,
                       struct Client *source_p, struct Client *target_p,
                       char *text);

static void handle_opers(int p_or_n, char *command,
                         struct Client *client_p,
                         struct Client *source_p,
                         char *nick,
                         char *text);

static void free_target_table(void);

struct Message privmsg_msgtab = {
  "PRIVMSG", 0, 1, 0, MFLG_SLOW | MFLG_UNREG, 0L,
  {m_unregistered, m_privmsg, m_privmsg, m_privmsg}
};

struct Message notice_msgtab = {
  "NOTICE", 0, 1, 0, MFLG_SLOW | MFLG_UNREG, 0L,
  {m_unregistered, m_notice, m_notice, m_notice}
};

#ifndef STATIC_MODULES

void
_modinit(void)
{
  mod_add_cmd(&privmsg_msgtab);
  mod_add_cmd(&notice_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&privmsg_msgtab);
  mod_del_cmd(&notice_msgtab);
}

char *_version = "20001122";
#endif
/* 
 * free_target_table -
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	- free the target_table
 */

static void
free_target_table(void)
{
  int i;
  
  for (i = 0; i < target_table_size; i++) 
    MyFree (target_table[i]);
	
  MyFree(target_table);
  target_table = NULL;
}

/*
** m_privmsg
**
** massive cleanup
** rev argv 6/91
**
**   Another massive cleanup Nov, 2000
** (I don't think there is a single line left from 6/91. Maybe.)
** m_privmsg and m_notice do basically the same thing.
** in the original 2.8.2 code base, they were the same function
** "m_message.c." When we did the great cleanup in conjuncton with bleep
** of ircu fame, we split m_privmsg.c and m_notice.c.
** I don't see the point of that now. Its harder to maintain, its
** easier to introduce bugs into one version and not the other etc.
** Really, the penalty of an extra function call isn't that big a deal folks.
** -db Nov 13, 2000
**
*/

#define PRIVMSG 0
#define NOTICE  1

static void m_privmsg(struct Client *client_p,
                     struct Client *source_p,
                     int parc,
                     char *parv[])
{
  m_message(PRIVMSG,"PRIVMSG",client_p,source_p,parc,parv);
}

static void m_notice(struct Client *client_p,
                    struct Client *source_p,
                    int parc,
                    char *parv[])
{
  m_message(NOTICE,"NOTICE",client_p,source_p,parc,parv);
}

/*
 * inputs	- flag privmsg or notice
 * 		- pointer to command "PRIVMSG" or "NOTICE"
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 */
static void m_message(int p_or_n,
                     char *command,
                     struct Client *client_p,
                     struct Client *source_p,
                     int parc,
                     char *parv[])
{
  int i;
  int ntargets;

#if 0  /* Allow servers to send notices to individual people */
  if (!IsPerson(source_p))
    return;
#endif

  if (!IsPerson(source_p) && p_or_n != NOTICE)
    return;

  if (parc < 2 || *parv[1] == '\0')
    {
      if(p_or_n != NOTICE)
	sendto_one(source_p, form_str(ERR_NORECIPIENT), me.name, source_p->name,
		   command );
      return;
    }

  if (parc < 3 || *parv[2] == '\0')
    {
      if(p_or_n != NOTICE)
	sendto_one(source_p, form_str(ERR_NOTEXTTOSEND), me.name, source_p->name);
      return;
    }

  /* Finish the flood grace period... */
  SetFloodDone(source_p);

  ntargets = build_target_list(p_or_n,command,
			       client_p,source_p,parv[1],&target_table,parv[2]);
  target_table_size = ntargets;
 
  if(ntargets < 0)
    {
      /* Sigh.  We need to relay this command to the hub */
      sendto_one(uplink, ":%s %s %s :%s",
		 source_p->name,
		 command,
		 parv[1],
		 parv[2]);
      return;
    }

  for(i = 0; i < ntargets ; i++)
    {
      switch (target_table[i]->type)
	{
	case ENTITY_CHANNEL:
          if(!IsPerson(source_p))
            continue;
	  msg_channel(p_or_n,command,
		      client_p,source_p,
		      (struct Channel *)target_table[i]->ptr,
		      parv[2]);
	  break;

	case ENTITY_CHANOPS_ON_CHANNEL:
	  msg_channel_flags(p_or_n,command,
			    client_p,source_p,
			    (struct Channel *)target_table[i]->ptr,
			    target_table[i]->flags,parv[2]);
	  break;

	case ENTITY_CLIENT:
	  msg_client(p_or_n,command,
		     source_p,(struct Client *)target_table[i]->ptr,parv[2]);
	  break;
	}
    }

  free_target_table();
}

/*
 * build_target_list
 *
 * inputs	- pointer to given client_p (server)
 *		- pointer to given source (oper/client etc.)
 *		- pointer to list of nicks/channels
 *		- pointer to table to place results
 *		- pointer to text (only used if source_p is an oper)
 * output	- number of valid entities
 * side effects	- target_table is modified to contain a list of
 *		  pointers to channels or clients
 *		  if source client is an oper
 *		  all the classic old bizzare oper privmsg tricks
 *		  are parsed and sent as is, if prefixed with $
 *		  to disambiguate.
 *
 */

static int build_target_list(int p_or_n,
                             char *command,
                             struct Client *client_p,
                             struct Client *source_p,
                             char *nicks_channels,
                             struct entity ***targets,
                             char *text)
{
  int  i = 0;
  int  type;
  char *p;
  char *nick;
  struct Channel *chptr;
  struct Client *target_p;
  char ncbuf[BUFSIZE];
  char *target_list;

  /* Sigh, we can't mutilate parv[1] incase we need it to send to a hub */
  if(!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
    {
      strncpy(ncbuf, nicks_channels, BUFSIZE);
      target_list = ncbuf;
    }
  else
    target_list = nicks_channels; /* skip strcpy for non-lazyleafs */
  
  for (nick = strtoken(&p, target_list, ","); nick; 
       nick = strtoken(&p, (char *)NULL, ","))
    {
      char *with_prefix;
      /*
      ** channels are privmsg'd a lot more than other clients, moved up here
      ** plain old channel msg ?
      */
       
      if( IsChanPrefix(*nick) )
	{
	  if( (chptr = hash_find_channel(nick, NullChn)) )
	    {
	      if( !duplicate_ptr(chptr, *targets, i) ) 
		{
			*targets = MyRealloc(*targets, sizeof(struct entity *) * (i + 1));
			(*targets)[i] = MyMalloc (sizeof (struct entity));
			
		  (*targets)[i]->ptr = (void *)chptr;
		  (*targets)[i++]->type = ENTITY_CHANNEL;
	  
		  if( i >= ConfigFileEntry.max_targets)
		    return(i);
		  continue;
		}
	    }
	  else if(IsOper(source_p))
	    {
	      handle_opers(p_or_n,command, client_p, source_p, nick, text);
	      continue;
	    }
	  else
	    {
              if(!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
              {
                target_table_size = i;
                free_target_table();
                return -1;
              }
              else if(p_or_n != NOTICE)
		sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
			   source_p->name, nick );
	      continue;
	    }
	}

      /* @#channel or +#channel message ? */

      type = 0;
      with_prefix = nick;
      /*  allow %+@ if someone wants to do that */
      for(;;)
      {
        if(*nick == '@') 
          type |= MODE_CHANOP;
        else if(*nick == '%')
          type |= MODE_CHANOP|MODE_HALFOP;
        else if(*nick == '+')
          type |= MODE_CHANOP|MODE_HALFOP|MODE_VOICE;
        else
          break;
        nick++;
      }

      if(type)
	{
	  /* suggested by Mortiis */
	  if(!*nick)   /* if its a '\0' dump it, there is no recipient */
	    {
	      sendto_one(source_p, form_str(ERR_NORECIPIENT),
			 me.name, source_p->name, command);
	      continue;
	    }

	  /* At this point, nick+1 should be a channel name i.e. #foo or &foo
	   * if the channel is found, fine, if not report an error
	   */
	 
	  if ( (chptr = hash_find_channel(nick, NullChn)) )
	    {
		
	      if((type & MODE_CHANOP) && !is_chan_op(chptr, source_p))
	      {
		sendto_one(source_p, form_str(ERR_NOSUCHNICK),
		           me.name, source_p->name, with_prefix);
			continue;			
	      }
	      if( !duplicate_ptr(chptr, *targets, i) )
		{
                  *targets = MyRealloc(*targets, sizeof(struct entity *) * (i + 1));
                  (*targets)[i] = MyMalloc (sizeof (struct entity));
		  (*targets)[i]->ptr = (void *)chptr;
		  (*targets)[i]->type = ENTITY_CHANOPS_ON_CHANNEL;
		  (*targets)[i++]->flags = type;

		  if( i >= ConfigFileEntry.max_targets)
		    return(i);
		  continue;
		}
	    }
	  else
	    {
              if(!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
              {
                target_table_size = i;
                free_target_table();
                return -1;
              }
              else if(p_or_n != NOTICE)
		sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
			   source_p->name, nick );
	      continue;
	    }
	}

      if (IsOper(source_p))
	{
          if(*nick == '$')
            {
              handle_opers(p_or_n, command, client_p,source_p,nick,text);
              continue;
            }
	  if(strchr(nick,'@'))
	    {
              handle_opers(p_or_n, command, client_p,source_p,nick,text);
              continue;
	    }
	}

      /* At this point, its likely its another client */

      if ( (target_p = find_person(nick, NULL)) )
	{
	  if( !duplicate_ptr(target_p, *targets, i) )
	    {
	      *targets = MyRealloc(*targets, sizeof(struct entity *) * (i + 1));
	      (*targets)[i] = MyMalloc (sizeof (struct entity));

	      (*targets)[i]->ptr = (void *)target_p;
	      (*targets)[i]->type = ENTITY_CLIENT;
	      (*targets)[i++]->flags = 0;
	      
	      if( i >= ConfigFileEntry.max_targets)
		return(i);
	      continue;
	    }
	}
      else
	{
          if(!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
            return -1;
          else if(p_or_n != NOTICE)
	    sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
		       source_p->name, nick );
	  continue;
	}

    }
  return i;
}

/*
 * duplicate_ptr
 *
 * inputs	- pointer to check
 *		- pointer to table of entities
 *		- number of valid entities so far
 * output	- YES if duplicate pointer in table, NO if not.
 *		  note, this does the canonize using pointers
 * side effects	- NONE
 */
static int duplicate_ptr( void *ptr, struct entity **ltarget_table, int n)
{
  int i;

  if (!ltarget_table)
	  return NO;
  
  for(i = 0; i < n; i++)
    {
      if (ltarget_table[i]->ptr == ptr)
	return YES;
    }
  return NO;
}

/*
 * msg_channel
 *
 * inputs	- flag privmsg or notice
 * 		- pointer to command "PRIVMSG" or "NOTICE"
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 * output	- NONE
 * side effects	- message given channel
 */
static void msg_channel( int p_or_n, char *command,
                         struct Client *client_p,
                         struct Client *source_p,
                         struct Channel *chptr,
                         char *text)
{
  struct Channel *vchan = NULL;
  char *chname=NULL;
  int result;

  chname = RootChan(chptr)->chname;

  if(HasVchans(chptr))
     vchan = map_vchan(chptr, source_p);

  if(!vchan)
    vchan = chptr;
    
     
  if(MyClient(source_p))
    {
      /* idle time shouldnt be reset by notices --fl */
      if ((p_or_n != NOTICE) && source_p->user)
	source_p->user->last = CurrentTime;
    }

  /* chanops and voiced can flood their own channel with impunity */
  if( (result = can_send(vchan,source_p)) )
    {
      if (result == CAN_SEND_OPV || 
          !flood_attack_channel(p_or_n, source_p, vchan, chname))
	{
          sendto_channel_butone(client_p, source_p, vchan,
                                command, ":%s", text);
	}
    }
  else
    {
      if (p_or_n != NOTICE)
        sendto_one(source_p, form_str(ERR_CANNOTSENDTOCHAN),
                   me.name, source_p->name, chname);
    }
}

/*
 * msg_channel_flags
 *
 * inputs	- flag 0 if PRIVMSG 1 if NOTICE. RFC 
 *		  say NOTICE must not auto reply
 *		- pointer to command, "PRIVMSG" or "NOTICE"
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 *		- flags
 *		- pointer to text to send
 * output	- NONE
 * side effects	- message given channel either chanop or voice
 */
static void msg_channel_flags( int p_or_n, char *command,
                               struct Client *client_p,
                               struct Client *source_p,
                               struct Channel *chptr,
                               int flags,
                               char *text)
{
  struct Channel *vchan = NULL;
  char *chname=NULL;
  int type;
  char c;

  if (flags & MODE_VOICE)
  {
    type = ONLY_CHANOPS_HALFOPS_VOICED;
    c = '+';
  }
  else if (flags & MODE_HALFOP)
  {
    type = ONLY_CHANOPS_HALFOPS;
    c = '%';
  }
  else
  {
    type = ONLY_CHANOPS;
    c = '@';
  }
  
  chname = RootChan(chptr)->chname;

  if (HasVchans(chptr))
    vchan = map_vchan(chptr, source_p);

  if(!vchan)
    vchan = chptr;
      
  if(MyClient(source_p))
    {
      /* idletime shouldnt be reset by notice --fl */
      if((p_or_n != NOTICE) && source_p->user)
	source_p->user->last = CurrentTime;
    }

  sendto_channel_local(type,
		       chptr,
		       ":%s!%s@%s %s %c%s :%s",
		       source_p->name,
		       source_p->username,
		       source_p->host,
		       command,
                       c,
		       chname,
		       text);

  sendto_match_cap_servs(chptr, client_p, CAP_CHW,
			 ":%s %s %c%s :%s",
			 source_p->name,
			 command,
			 c,
			 chptr->chname,
			 text);
}

/*
 * msg_client
 *
 * inputs	- flag 0 if PRIVMSG 1 if NOTICE. RFC 
 *		  say NOTICE must not auto reply
 *		- pointer to command, "PRIVMSG" or "NOTICE"
 * 		- pointer to source_p source (struct Client *)
 *		- pointer to target_p target (struct Client *)
 *		- pointer to text
 * output	- NONE
 * side effects	- message given channel either chanop or voice
 */
static void msg_client(int p_or_n, char *command,
                       struct Client *source_p, struct Client *target_p,
                       char *text)
{
  if(MyClient(source_p))
    {
      /* reset idle time for message only if its not to self 
       * and its not a notice */
      if((p_or_n != NOTICE) && (source_p != target_p) && source_p->user)
	source_p->user->last = CurrentTime;
    }

  if (MyConnect(source_p) && (p_or_n != NOTICE) &&
      target_p->user && target_p->user->away)
    sendto_one(source_p, form_str(RPL_AWAY), me.name,
               source_p->name, target_p->name,
               target_p->user->away);

  if(MyClient(target_p))
    {
      if(!IsServer(source_p) && IsSetCallerId(target_p))
	{
	  /* Here is the anti-flood bot/spambot code -db */
	  if(accept_message(source_p,target_p))
	    {
	      sendto_one(target_p, ":%s!%s@%s %s %s :%s",
			 source_p->name,
			 source_p->username,
			 source_p->host,
			 command, target_p->name, text);
	    }
	  else
	    {
	      /* check for accept, flag recipient incoming message */
              if(p_or_n != NOTICE)
                sendto_anywhere(source_p, target_p,
                  "NOTICE %s :*** I'm in +g mode (server side ignore).",
                  source_p->name);
	      /* XXX hard coded 60 ick fix -db */

	      if((target_p->localClient->last_caller_id_time +
                  ConfigFileEntry.caller_id_wait) < CurrentTime)
		{
		  if(p_or_n != NOTICE)
		    sendto_anywhere(source_p, target_p,
                      "NOTICE %s :*** I've been informed you messaged me.",
                      source_p->name);

		  sendto_one(target_p,
      ":%s NOTICE %s :*** Client %s [%s@%s] is messaging you and you are +g",
				    me.name, target_p->name,
				    source_p->name, source_p->username,
				    source_p->host );

		  target_p->localClient->last_caller_id_time = CurrentTime;
		  
		}
	      /* Only so opers can watch for floods */
	      (void)flood_attack_client(p_or_n,source_p,target_p);
	    }
	}
      else
	{
          /* If the client is remote, we dont perform a special check for flooding.. as we wouldnt
           * block their message anyway.. this means we dont give warnings.. we then check if theyre opered 
           * (to avoid flood warnings), lastly if theyre our client and flooding    -- fl */
          if(!MyClient(source_p) || IsOper(source_p) || (MyClient(source_p) && !flood_attack_client(p_or_n,source_p,target_p)))
	    sendto_anywhere(target_p, source_p, "%s %s :%s",
			    command, target_p->name, text);
	}
    }
  else
    /* The target is a remote user.. same things apply  -- fl */
    if(!MyClient(source_p) || IsOper(source_p) || (MyClient(source_p) && !flood_attack_client(p_or_n,source_p,target_p)))
      sendto_anywhere(target_p, source_p, "%s %s :%s",
		      command, target_p->name, text);
  return;
}
      
/*
 * flood_attack_client
 * inputs       - flag 0 if PRIVMSG 1 if NOTICE. RFC
 *                say NOTICE must not auto reply
 *              - pointer to source Client 
 *		- pointer to target Client
 * output	- 1 if target is under flood attack
 * side effects	- check for flood attack on target target_p
 */
static int flood_attack_client(int p_or_n, struct Client *source_p,struct Client *target_p)
{
  int delta;

  if(GlobalSetOptions.floodcount && MyConnect(target_p) && IsClient(source_p))
    {
      if((target_p->localClient->first_received_message_time+1)
	 < CurrentTime)
	{
	  delta = CurrentTime - target_p->localClient->first_received_message_time;
	  target_p->localClient->received_number_of_privmsgs -= delta;
	  target_p->localClient->first_received_message_time = CurrentTime;
	  if(target_p->localClient->received_number_of_privmsgs <= 0)
	    {
	      target_p->localClient->received_number_of_privmsgs = 0;
	      target_p->localClient->flood_noticed = 0;
	    }
	}

      if((target_p->localClient->received_number_of_privmsgs >= 
	  GlobalSetOptions.floodcount) || target_p->localClient->flood_noticed)
	{
	  if(target_p->localClient->flood_noticed == 0)
	    {
	      sendto_realops_flags(FLAGS_BOTS,
				   "Possible Flooder %s [%s@%s] on %s target: %s",
				   source_p->name, source_p->username,
				   source_p->host,
				   source_p->user->server, target_p->name);
	      target_p->localClient->flood_noticed = 1;
	      /* add a bit of penalty */
	      target_p->localClient->received_number_of_privmsgs += 2;
	    }
	  if(MyClient(source_p) && (p_or_n != NOTICE))
	    sendto_one(source_p, ":%s NOTICE %s :*** Message to %s throttled due to flooding",
		       me.name, source_p->name, target_p->name);
	  return 1;
	}
      else
	target_p->localClient->received_number_of_privmsgs++;
    }

  return 0;
}

/*
 * flood_attack_channel
 * inputs       - flag 0 if PRIVMSG 1 if NOTICE. RFC
 *                says NOTICE must not auto reply
 *              - pointer to source Client 
 *		- pointer to target channel
 * output	- 1 if target is under flood attack
 * side effects	- check for flood attack on target chptr
 */
static int flood_attack_channel(int p_or_n,struct Client *source_p,struct Channel *chptr,
                                char *chname)
{
  int delta;

  if(GlobalSetOptions.floodcount)
    {
      if((chptr->first_received_message_time+1) < CurrentTime)
	{
	  delta = CurrentTime - chptr->first_received_message_time;
	  chptr->received_number_of_privmsgs -= delta;
	  chptr->first_received_message_time = CurrentTime;
	  if(chptr->received_number_of_privmsgs <= 0)
	    {
	      chptr->received_number_of_privmsgs=0;
	      chptr->flood_noticed = 0;
	    }
	}

      if((chptr->received_number_of_privmsgs >= GlobalSetOptions.floodcount)
	 || chptr->flood_noticed)
	{
	  if(chptr->flood_noticed == 0)
	    {
	      sendto_realops_flags(FLAGS_BOTS,
				   "Possible Flooder %s [%s@%s] on %s target: %s",
				   source_p->name, source_p->username,
				   source_p->host,
				   source_p->user->server, chptr->chname);
	      chptr->flood_noticed = 1;

	      /* Add a bit of penalty */
	      chptr->received_number_of_privmsgs += 2;
	    }
	  if(MyClient(source_p) && (p_or_n != NOTICE))
	    sendto_one(source_p, ":%s NOTICE %s :*** Message to %s throttled due to flooding",
		       me.name, source_p->name, chname);
	  return 1;
	}
      else
	chptr->received_number_of_privmsgs++;
    }

  return 0;
}


/*
 * handle_opers
 *
 * inputs	- server pointer
 *		- client pointer
 *		- nick stuff to grok for opers
 *		- text to send if grok
 * output	- none
 * side effects	- all the traditional oper type messages are parsed here.
 *		  i.e. "/msg #some.host."
 *		  However, syntax has been changed.
 *		  previous syntax "/msg #some.host.mask"
 *		  now becomes     "/msg $#some.host.mask"
 *		  previous syntax of: "/msg $some.server.mask" remains
 *		  This disambiguates the syntax.
 */
static void handle_opers(int p_or_n,
                         char *command,
                         struct Client *client_p,
                         struct Client *source_p,
                         char *nick,
                         char *text)
{
  struct Client *target_p;
  char *host;
  char *server;
  char *s;
  int count;

  /*
  ** the following two cases allow masks in NOTICEs
  ** (for OPERs only)
  **
  ** Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
  */
  if ((*nick == '$' || *nick == '#'))
    {
      if (!(s = (char *)strrchr(nick, '.')))
	{
	  sendto_one(source_p, form_str(ERR_NOTOPLEVEL),
		     me.name, source_p->name, nick);
	  return;
	}
      while (*++s)
	if (*s == '.' || *s == '*' || *s == '?')
	  break;
      if (*s == '*' || *s == '?')
	{
	  sendto_one(source_p, form_str(ERR_WILDTOPLEVEL),
		     me.name, source_p->name, nick);
	  return;
	}
      sendto_match_butone(IsServer(client_p) ? client_p : NULL, source_p,
			  nick + 1,
                          (*nick == '#') ? MATCH_HOST : MATCH_SERVER,
			  "PRIVMSG %s :%s",
			  nick,
			  text);
      return;
    }

  /*
  ** user[%host]@server addressed?
  */
  if ((server = (char *)strchr(nick, '@')) &&
      (target_p = find_server(server + 1)))
    {
      count = 0;

      /*
      ** Not destined for a user on me :-(
      */
      if (!IsMe(target_p))
	{
	  sendto_one(target_p,":%s %s %s :%s", source_p->name,
		     "PRIVMSG", nick, text);
	  return;
	}

      *server = '\0';
        
      if ((host = (char *)strchr(nick, '%')))
	*host++ = '\0';

      /* Check if someones msg'ing opers@our.server */
      if (!strcmp(nick,"opers"))
        {
#if 0
          sendto_realops_flags(FLAGS_ALL, "To opers: From: %s!%s@%s: %s", 
                               source_p->name, source_p->username, source_p->host, text);      
#endif
	  sendto_wallops_flags(FLAGS_LOCOPS, source_p, "%s", text);
          return;
        }

      /*
      ** Look for users which match the destination host
      ** (no host == wildcard) and if one and one only is
      ** found connected to me, deliver message!
      */
      target_p = find_userhost(nick, host, NULL, &count);

      if (server)
	*server = '@';
      if (host)
	*--host = '%';
      if (target_p)
	{
	  if (count == 1)
	    sendto_anywhere(target_p, source_p,
			    "%s %s :%s",
			    "PRIVMSG",
			    nick, text);
	  else 
	    sendto_one(source_p,
		       form_str(ERR_TOOMANYTARGETS),
		       me.name, source_p->name, nick);
	}
    }
}
