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

struct Message topic_msgtab = {
  MSG_TOPIC, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_topic, ms_topic, m_topic}
};

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

/*
 * m_topic
 *      parv[0] = sender prefix
 *      parv[1] = channel name
 *	parv[2] = new topic, if setting topic
 */
int     m_topic(struct Client *cptr,
                struct Client *sptr,
                int parc,
                char *parv[])
{
  struct Channel *chptr = NullChn;
  struct Channel *vchan;
  char  *p = NULL;
  
  if ((p = strchr(parv[1],',')))
    *p = '\0';

  if (parv[1] && IsChannelName(parv[1]))
    {
      chptr = hash_find_channel(parv[1], NullChn);

      if(chptr == NULL)
      {
        /* if chptr isn't found locally, it =could= exist
         * on the uplink. So ask.
         */

        /* LazyLinks */
        /* this was segfaulting if we had no servers linked.
         *  -pro
         */
        if ( !ConfigFileEntry.hub && uplink &&
           IsCapable(uplink, CAP_LL) )
        {
          /* cache the channel if it exists on uplink
           * If the channel as seen by the uplink, has vchans,
           * the uplink will have to SJOIN all of those.
           */
          sendto_one(uplink, ":%s CBURST %s",
                      me.name, parv[1]);

          sendto_one(uplink, ":%s TOPIC %s %s",
                     sptr->name, parv[1],
                     ((parc > 2) ? parv[2] : ""));
          return 0;
        }
        else
        {
          sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                     me.name, parv[0], parv[1]);
          return 0;
        }
      }

      if (HasVchans(chptr))
	{
	  vchan = map_vchan(chptr,sptr);
	  if(vchan != NULL)
	    chptr = vchan;
	}

      if (parc > 2)
	{ /* setting topic */

	  if (!IsMember(sptr, chptr))
	    {
	      sendto_one(sptr, form_str(ERR_NOTONCHANNEL), me.name, parv[0],
			 parv[1]);
	      return 0;
	    }
	  if ((chptr->mode.mode & MODE_TOPICLIMIT) == 0 ||
	      is_any_op(chptr,sptr))
	    {
	      /* setting a topic */
	      /*
	       * chptr zeroed
	       */
	      strncpy_irc(chptr->topic, parv[2], TOPICLEN);
	      
              MyFree(chptr->topic_info);
	      
	      chptr->topic_info = 
		(char *)MyMalloc(strlen(sptr->name)+
				 strlen(sptr->username)+
				 strlen(sptr->host)+3);
	      ircsprintf(chptr->topic_info, "%s!%s@%s",
			 sptr->name, sptr->username, sptr->host);

	      chptr->topic_time = CurrentTime;
	      
	      sendto_channel_remote(chptr, cptr,":%s TOPIC %s :%s",
				 parv[0], parv[1],
				 chptr->topic);
	      if(chptr->mode.mode & MODE_HIDEOPS)
		{
		  sendto_channel_local(ONLY_CHANOPS,
				       chptr, ":%s!%s@%s TOPIC %s :%s",
				       sptr->name,
				       sptr->username,
				       sptr->host,
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
				       sptr->name,
				       sptr->username,
				       sptr->host,
				       parv[1], chptr->topic);
		}
	    }
	  else
            sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED),
                       me.name, parv[0], parv[1]);
	}
      else  /* only asking  for topic  */
	{
	  if (!IsMember(sptr, chptr) && SecretChannel(chptr))
	    {
	      sendto_one(sptr, form_str(ERR_NOTONCHANNEL), me.name, parv[0],
			 parv[1]);
	      return 0;
	    }
          if (chptr->topic[0] == '\0')
	    sendto_one(sptr, form_str(RPL_NOTOPIC),
		       me.name, parv[0], parv[1]);
          else
	    {
              sendto_one(sptr, form_str(RPL_TOPIC),
                         me.name, parv[0],
                         parv[1], chptr->topic);
              if (!(chptr->mode.mode & MODE_HIDEOPS) ||
                  is_any_op(chptr,sptr))
                {
                  sendto_one(sptr, form_str(RPL_TOPICWHOTIME),
                             me.name, parv[0], parv[1],
                             chptr->topic_info,
                             chptr->topic_time);
                }
	      else /* Hide from nonops */
		{
                  sendto_one(sptr, form_str(RPL_TOPICWHOTIME),
                             me.name, parv[0], parv[1],
                             me.name,
                             chptr->topic_time);
                }
            }
        }
    }
  else
    {
      sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                 me.name, parv[0], parv[1]);
    }
  
  return 0;
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
int     ms_topic(struct Client *cptr,
                struct Client *sptr,
                int parc,
                char *parv[])
{
  struct Channel *chptr = NULL;
  
  if (!IsServer(sptr))
    return m_topic(cptr, sptr, parc, parv);

  if( parc < 5 )
    return 0;

  if (parv[1] && IsChannelName(parv[1]))
    {
      if ( (chptr = hash_find_channel(parv[1], NullChn)) == NULL )
	return 0;

      strncpy_irc(chptr->topic, parv[4], TOPICLEN);
	      
      MyFree(chptr->topic_info);
	      
      DupString(chptr->topic_info,parv[2]);

      chptr->topic_time = atoi(parv[3]);

      if(chptr->mode.mode & MODE_HIDEOPS)
	{
	  sendto_channel_local(ONLY_CHANOPS,
			       chptr, ":%s!%s@%s TOPIC %s :%s",
			       me.name,
			       sptr->username,
			       sptr->host,
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
			       sptr->username,
			       sptr->host,
			       parv[1], chptr->topic);
	}
    }

  return 0;
}
