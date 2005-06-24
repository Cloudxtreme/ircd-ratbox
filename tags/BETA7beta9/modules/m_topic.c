/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_topic.c
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
#include "channel_mode.h"
#include "vchannel.h"
#include "client.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <string.h>
#include <stdlib.h>

static void m_topic(struct Client*, struct Client*, int, char**);
static void ms_topic(struct Client*, struct Client*, int, char**);

struct Message topic_msgtab = {
  "TOPIC", 0, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_topic, ms_topic, m_topic}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&topic_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&topic_msgtab);
}

char *_version = "20001122";
#endif
/*
 * m_topic
 *      parv[0] = sender prefix
 *      parv[1] = channel name
 *	parv[2] = new topic, if setting topic
 */
static void m_topic(struct Client *client_p,
                   struct Client *source_p,
                   int parc, char *parv[])
{
  struct Channel *chptr = NULL;
  struct Channel *root_chan, *vchan;
  char  *p = NULL;
  
  if ((p = strchr(parv[1],',')))
    *p = '\0';

  if (parv[1] && IsChannelName(parv[1]))
    {
      chptr = hash_find_channel(parv[1]);

      if(chptr == NULL)
      {
        /* if chptr isn't found locally, it =could= exist
         * on the uplink. so forward reqeuest
         */
        if(!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
        {
          sendto_one(uplink, ":%s TOPIC %s %s",
                     source_p->name, parv[1],
                     ((parc > 2) ? parv[2] : ""));
          return;
        }
        else
        {
          sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                     me.name, parv[0], parv[1]);
          return;
        }
      }

      root_chan = chptr;

      if (HasVchans(chptr))
	{
	  vchan = map_vchan(chptr,source_p);
	  if(vchan != NULL)
	    chptr = vchan;
	}
      else if (IsVchan(chptr))
        root_chan = RootChan(chptr);
          
      /* setting topic */
      if (parc > 2)
	{

	  if (!IsMember(source_p, chptr))
	    {
	      sendto_one(source_p, form_str(ERR_NOTONCHANNEL), me.name, parv[0],
			 parv[1]);
	      return;
	    }
	  if ((chptr->mode.mode & MODE_TOPICLIMIT) == 0 ||
	      is_any_op(chptr,source_p))
	    {
	      /* setting a topic */
	      /*
	       * chptr zeroed
	       */
	      strncpy_irc(chptr->topic, parv[2], TOPICLEN);
	      
	      
	      ircsprintf(chptr->topic_info, "%s!%s@%s",
			 source_p->name, source_p->username, source_p->host);

	      chptr->topic_time = CurrentTime;
	      
	      sendto_server(client_p, NULL, chptr, NOCAPS, NOCAPS, NOFLAGS,
                            ":%s TOPIC %s :%s",
                            parv[0], chptr->chname,
                            chptr->topic);
	      if(chptr->mode.mode & MODE_HIDEOPS)
		{
		  sendto_channel_local(ONLY_CHANOPS_HALFOPS,
				       chptr, ":%s!%s@%s TOPIC %s :%s",
				       source_p->name,
				       source_p->username,
				       source_p->host,
				       root_chan->chname,
				       chptr->topic);

		  sendto_channel_local(NON_CHANOPS,
				       chptr, ":%s TOPIC %s :%s",
				       me.name,
				       root_chan->chname,
				       chptr->topic);
		}
	      else
		{
		  sendto_channel_local(ALL_MEMBERS,
				       chptr, ":%s!%s@%s TOPIC %s :%s",
				       source_p->name,
				       source_p->username,
				       source_p->host,
				       root_chan->chname, chptr->topic);
		}
	    }
	  else
            sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
                       me.name, parv[0], parv[1]);
	}
      else  /* only asking  for topic  */
	{
	  if (!IsMember(source_p, chptr) && SecretChannel(chptr))
	    {
	      sendto_one(source_p, form_str(ERR_NOTONCHANNEL), me.name, parv[0],
			 parv[1]);
	      return;
	    }
          if (chptr->topic[0] == '\0')
	    sendto_one(source_p, form_str(RPL_NOTOPIC),
		       me.name, parv[0], parv[1]);
          else
	    {
              sendto_one(source_p, form_str(RPL_TOPIC),
                         me.name, parv[0],
                         root_chan->chname, chptr->topic);

                if(!(chptr->mode.mode & MODE_HIDEOPS) ||
                  is_any_op(chptr,source_p))
                {
                  sendto_one(source_p, form_str(RPL_TOPICWHOTIME),
                             me.name, parv[0], root_chan->chname,
                             chptr->topic_info,
                             chptr->topic_time);
                }
	        /* client on LL needing the topic - if we have serverhide, say
	         * its the actual LL server that set the topic, not us the
	         * uplink -- fl_
	         */
	        else if(ConfigServerHide.hide_servers && !MyClient(source_p)
	                && IsCapable(client_p, CAP_LL) && ServerInfo.hub)
  	        {
	          sendto_one(source_p, form_str(RPL_TOPICWHOTIME),
	  	             me.name, parv[0], root_chan->chname,
	  		     client_p->name, chptr->topic_time);
                }
  	        /* just normal topic hiding.. */
	        else
	 	{
                  sendto_one(source_p, form_str(RPL_TOPICWHOTIME),
                             me.name, parv[0], root_chan->chname,
                             me.name,
                             chptr->topic_time);
                }
            }
        }
    }
  else
    {
      sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                 me.name, parv[0], parv[1]);
    }
}

/*
 * ms_topic
 *      parv[0] = sender prefix
 *      parv[1] = channel name
 *	parv[2] = topic_info
 *	parv[3] = topic_info time
 *	parv[4] = new channel topic
 *
 * Let servers always set a topic
 */
static void ms_topic(struct Client *client_p,
                    struct Client *source_p,
                    int parc,
                    char *parv[])
{
  struct Channel *chptr = NULL;
  
  if (!IsServer(source_p))
  {
    m_topic(client_p, source_p, parc, parv);
    return;
  }

  if( parc < 5 )
    return;

  if (parv[1] && IsChannelName(parv[1]))
    {
      if ((chptr = hash_find_channel(parv[1])) == NULL)
	return;

      strncpy_irc(chptr->topic, parv[4], TOPICLEN);
	      
      strncpy_irc(chptr->topic_info, parv[2], USERHOST_REPLYLEN);	      

      chptr->topic_time = atoi(parv[3]);

      if(chptr->mode.mode & MODE_HIDEOPS)
	{
	  sendto_channel_local(ONLY_CHANOPS_HALFOPS,
			       chptr, ":%s!%s@%s TOPIC %s :%s",
			       me.name,
			       source_p->username,
			       source_p->host,
			       parv[1],
			       chptr->topic);

	  sendto_channel_local(NON_CHANOPS,
			       chptr, ":%s TOPIC %s :%s",
			       me.name,
			       parv[1],
			       chptr->topic);

	}
      else
	{
	  sendto_channel_local(ALL_MEMBERS,
			       chptr, ":%s!%s@%s TOPIC %s :%s",
			       me.name,
			       source_p->username,
			       source_p->host,
			       parv[1], chptr->topic);
	}
    }
}
