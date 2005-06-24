/************************************************************************
 *   IRC - Internet Relay Chat, src/m_join.c
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
#include "channel.h"
#include "vchannel.h"
#include "client.h"
#include "common.h"   /* bleah */
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "send.h"
#include "s_serv.h"
#include "s_conf.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * m_functions execute protocol messages on this server:
 *
 *      cptr    is always NON-NULL, pointing to a *LOCAL* client
 *              structure (with an open socket connected!). This
 *              identifies the physical socket where the message
 *              originated (or which caused the m_function to be
 *              executed--some m_functions may call others...).
 *
 *      sptr    is the source of the message, defined by the
 *              prefix part of the message if present. If not
 *              or prefix not found, then sptr==cptr.
 *
 *              (!IsServer(cptr)) => (cptr == sptr), because
 *              prefixes are taken *only* from servers...
 *
 *              (IsServer(cptr))
 *                      (sptr == cptr) => the message didn't
 *                      have the prefix.
 *
 *                      (sptr != cptr && IsServer(sptr) means
 *                      the prefix specified servername. (?)
 *
 *                      (sptr != cptr && !IsServer(sptr) means
 *                      that message originated from a remote
 *                      user (not local).
 *
 *              combining
 *
 *              (!IsServer(sptr)) means that, sptr can safely
 *              taken as defining the target structure of the
 *              message in this server.
 *
 *      *Always* true (if 'parse' and others are working correct):
 *
 *      1)      sptr->from == cptr  (note: cptr->from == cptr)
 *
 *      2)      MyConnect(sptr) <=> sptr == cptr (e.g. sptr
 *              *cannot* be a local connection, unless it's
 *              actually cptr!). [MyConnect(x) should probably
 *              be defined as (x == x->from) --msa ]
 *
 *      parc    number of variable parameter strings (if zero,
 *              parv is allowed to be NULL)
 *
 *      parv    a NULL terminated list of parameter pointers,
 *
 *                      parv[0], sender (prefix string), if not present
 *                              this points to an empty string.
 *                      parv[1]...parv[parc-1]
 *                              pointers to additional parameters
 *                      parv[parc] == NULL, *always*
 *
 *              note:   it is guaranteed that parv[0]..parv[parc-1] are all
 *                      non-NULL pointers.
 */

/*
** m_join
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = channel password (key)
*/
int     m_join(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  static char   jbuf[BUFSIZE];
  struct SLink  *lp;
  struct Channel *chptr = NULL;
  struct Channel *vchan_chptr = NULL;
  struct Channel *root_chptr = NULL;
  int joining_vchan = 0;
  char  *name, *key = NULL;
  int   i, flags = 0;
#ifdef NO_CHANOPS_WHEN_SPLIT
  int   allow_op=YES;
#endif
  char  *p = NULL, *p2 = NULL;
#ifdef ANTI_SPAMBOT
  int   successful_join_count = 0; /* Number of channels successfully joined */
#endif
  
  if (!(sptr->user))
    {
      /* something is *fucked* - bail */
      return 0;
    }

  if (parc < 2 || *parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "JOIN");
      return 0;
    }

#ifdef NEED_SPLITCODE

  /* Check to see if the timer has timed out, and if so, see if
   * there are a decent number of servers now connected 
   * to consider any possible split over.
   * -Dianora
   */

  if (server_was_split)
    check_still_split();

#endif

  *jbuf = '\0';
  /*
  ** Rebuild list of channels joined to be the actual result of the
  ** JOIN.  Note that "JOIN 0" is the destructive problem.
  */
  for (i = 0, name = strtoken(&p, parv[1], ","); name;
       name = strtoken(&p, (char *)NULL, ","))
    {
      if (!check_channel_name(name))
        {
          sendto_one(sptr, form_str(ERR_BADCHANNAME),
                       me.name, parv[0], (unsigned char*) name);
          continue;
        }
      if (*name == '&' && !MyConnect(sptr))
        continue;
      if (*name == '0' && !atoi(name))
        *jbuf = '\0';
      else if (!IsChannelName(name))
        {
          if (MyClient(sptr))
            sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                       me.name, parv[0], name);
          continue;
        }

      /* We can't do this for interoperability reasons ;-( */
#if 0
      if (strlen(name) > CHANNELLEN-15)
        {
          sendto_one(sptr, form_str(ERR_BADCHANNAME),me.name, parv[0], name);
          continue;
        }
#endif

#ifdef NO_JOIN_ON_SPLIT_SIMPLE
      if (server_was_split && MyClient(sptr) && (*name != '&'))
        {
              sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
                         me.name, parv[0], name);
              continue;
        }
#endif /* NO_JOIN_ON_SPLIT_SIMPLE */

#if defined(PRESERVE_CHANNEL_ON_SPLIT) || defined(NO_JOIN_ON_SPLIT)
      /* If from a cold start, there were never any channels
       * joined, hence all of them must be considered off limits
       * until this server joins the network
       *
       * cold_start is set to NO if SPLITDELAY is set to 0 in m_set()
       */

      if(cold_start && MyClient(sptr) && (*name != '&'))
	{
              sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
                         me.name, parv[0], name);
              continue;
        }
#endif
      if (*jbuf)
        (void)strcat(jbuf, ",");
      (void)strncat(jbuf, name, sizeof(jbuf) - i - 1);
      i += strlen(name)+1;
    }
  /*    (void)strcpy(parv[1], jbuf);*/

  p = NULL;
  if (parv[2])
    key = strtoken(&p2, parv[2], ",");
  parv[2] = NULL;       /* for m_names call later, parv[parc] must == NULL */
  for (name = strtoken(&p, jbuf, ","); name;
       key = (key) ? strtoken(&p2, NULL, ",") : NULL,
         name = strtoken(&p, NULL, ","))
    {
      /*
      ** JOIN 0 sends out a part for all channels a user
      ** has joined.
	  **
	  ** this should be either disabled or selectable in
	  ** config file .. it's abused a lot more than it's
	  ** used these days :/ --is
      */
      if (*name == '0' && !atoi(name))
        {
          if (sptr->user->channel == NULL)
            continue;
          while ((lp = sptr->user->channel))
            {
              chptr = lp->value.chptr;
              sendto_channel_butserv(chptr, sptr, ":%s PART %s",
                                     parv[0], chptr->chname);
              remove_user_from_channel(sptr, chptr, 0);
            }

#ifdef ANTI_SPAMBOT       /* Dianora */

          if( MyConnect(sptr) )
            {
              if(GlobalSetOptions.spam_num &&
		 (sptr->join_leave_count >= GlobalSetOptions.spam_num))
                {
                  sendto_ops_flags(FLAGS_BOTS,
                                     "User %s (%s@%s) is a possible spambot",
                                     sptr->name,
                                     sptr->username, sptr->host);
                  sptr->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
                }
              else
                {
                  int t_delta;

                  if( (t_delta = (CurrentTime - sptr->last_leave_time)) >
                      JOIN_LEAVE_COUNT_EXPIRE_TIME)
                    {
                      int decrement_count;
                      decrement_count = (t_delta/JOIN_LEAVE_COUNT_EXPIRE_TIME);

                      if(decrement_count > sptr->join_leave_count)
                        sptr->join_leave_count = 0;
                      else
                        sptr->join_leave_count -= decrement_count;
                    }
                  else
                    {
                      if((CurrentTime - (sptr->last_join_time)) < 
			 GlobalSetOptions.spam_time)
                        {
                          /* oh, its a possible spambot */
                          sptr->join_leave_count++;
                        }
                    }
                  sptr->last_leave_time = CurrentTime;
                }
            }
#endif
          sendto_match_servs(NULL, cptr, ":%s JOIN 0", parv[0]);
          continue;
        }
      
      if (MyConnect(sptr))
        {
          /*
          ** local client is first to enter previously nonexistent
          ** channel so make them (rightfully) the Channel
          ** Operator.
          */
           /*     flags = (ChannelExists(name)) ? 0 : CHFL_CHANOP; */

          /* To save a redundant hash table lookup later on */
           
           if((chptr = hash_find_channel(name, NullChn)))
             {
               /* there's subchans so check those
                * but not if it was a subchan's realname they specified */
               if (IsVchanTop(chptr))
                 {
                   if( on_sub_vchan(chptr,sptr) )
                     continue;
                   if (key && key[0] == '!')
                     {
                       /* user joined with key "!".  force listing.
                          (this prevents join-invited-chan voodoo) */
                       if (!key[1])
                         {
                           show_vchans(cptr, sptr, chptr);
                           return 0;
                         }

                       /* found a matching vchan? let them join it */
                       if ((vchan_chptr = find_vchan(chptr, key)))
                         {
                           root_chptr = chptr;
                           chptr = vchan_chptr;
                           joining_vchan = 1;
                         }
                       else
                         {
                           sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                                      me.name, parv[0], name);
                           return 0;
                         }
                     }
                   else
                     {
                       /* one special case here i think..
                        * if there's only one vchan, and the root is empty
                        * let them join that vchan */
                       if( (!chptr->members) && (!chptr->next_vchan->next_vchan) )
                         {
                           root_chptr = chptr;
                           chptr = chptr->next_vchan;
                           joining_vchan = 1;
                         }
                       else
                        {
                          /* voodoo to auto-join channel invited to */
                          if ((vchan_chptr=vchan_invites(chptr, sptr)))
                            {
                              root_chptr = chptr;
                              chptr = vchan_chptr;
                              joining_vchan = 1;
                            }
                          /* otherwise, they get a list of channels */
                          else
                            {
                              show_vchans(cptr, sptr, chptr);
                              return 0;
                            }
                        }
                     }
                 }
               /* trying to join a sub chans 'real' name
                * don't allow that */
               else if (IsVchan(chptr))
                 {
                   sendto_one(sptr, form_str(ERR_BADCHANNAME),
                              me.name, parv[0], (unsigned char*) name);
                   return 0;
                 }
               flags = 0;
             }
           else
             {
               flags = CHFL_CHANOP;
               if(!ConfigFileEntry.hub)
                 {
                   /* LazyLinks */
                   if( (*name != '&') && serv_cptr_list
                       && IsCapable( serv_cptr_list, CAP_LL) )
                     {
                       sendto_one(serv_cptr_list,":%s CBURST %s %s %s",
                         me.name,name,sptr->name, key ? key: "" );
                       /* And wait for LLJOIN */
                       return 0;
                     }
                 }
             }

           /* if its not a local channel, or isn't an oper
            * and server has been split
            */

#ifdef NO_CHANOPS_WHEN_SPLIT
          if((*name != '&') && server_was_split)
            {
              allow_op = NO;

              if(!IsRestricted(sptr) && (flags & CHFL_CHANOP))
                sendto_one(sptr,":%s NOTICE %s :*** Notice -- Due to a network split, you can not obtain channel operator status in a new channel at this time.",
                       me.name,
                       sptr->name);
            }
#endif

          if (sptr->user->joined >= MAXCHANNELSPERUSER)
		  {
              sendto_one(sptr, form_str(ERR_TOOMANYCHANNELS),
                         me.name, parv[0], name);
#ifdef ANTI_SPAMBOT
              if(successful_join_count)
                sptr->last_join_time = CurrentTime;
#endif
              return 0;
            }


#ifdef ANTI_SPAMBOT       /* Dianora */
          if(flags == 0)        /* if channel doesn't exist, don't penalize */
            successful_join_count++;
          if( GlobalSetOptions.spam_num &&
	      (sptr->join_leave_count >= GlobalSetOptions.spam_num))
            { 
              /* Its already known as a possible spambot */
 
              if(sptr->oper_warn_count_down > 0)  /* my general paranoia */
                sptr->oper_warn_count_down--;
              else
                sptr->oper_warn_count_down = 0;
 
              if(sptr->oper_warn_count_down == 0)
                {
                  sendto_ops_flags(FLAGS_BOTS,
                    "User %s (%s@%s) trying to join %s is a possible spambot",
                             sptr->name,
                             sptr->username,
                             sptr->host,
                             name);     
                  sptr->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
                }
#ifndef ANTI_SPAMBOT_WARN_ONLY
              return 0; /* Don't actually JOIN anything, but don't let
                           spambot know that */
#endif
            }
#endif
        }
      else
        {
          /*
          ** complain for remote JOINs to existing channels
          ** (they should be SJOINs) -orabidoo
          */
          if (!ChannelExists(name))
            ts_warn("User on %s remotely JOINing new channel", 
                    sptr->user->server);
        }

#if defined(NO_JOIN_ON_SPLIT)
      if(server_was_split && (*name != '&'))
        {
          if( chptr )   /* The channel existed, so I can't join it */
            {
              if (IsMember(sptr, chptr)) /* already a member, ignore this */
                continue;

              /* allow local joins to this channel */
              if( (chptr->users == 0) && !IsAnOper(sptr) )
                {
                  sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
                             me.name, parv[0], name);
                  continue;
                }
            }
          else
            chptr = get_channel(sptr, name, CREATE);        
        }
      else
#endif
      if(!chptr)        /* If I already have a chptr, no point doing this */
        chptr = get_channel(sptr, name, CREATE);

      if(chptr)
        {
          if (IsMember(sptr, chptr))    /* already a member, ignore this */
            continue;
        }
      else
        {
          sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
                     me.name, parv[0], name);
#ifdef ANTI_SPAMBOT
          if(successful_join_count > 0)
            successful_join_count--;
#endif
          continue;
        }

      /*
       * can_join checks for +i key, bans.
       * If a ban is found but an exception to the ban was found
       * flags will have CHFL_EXCEPTION set
       */

      if (MyConnect(sptr) && (i = can_join(sptr, chptr, key, &flags)))
        {
          sendto_one(sptr,
                    form_str(i), me.name, parv[0], name);
#ifdef ANTI_SPAMBOT
          if(successful_join_count > 0)
            successful_join_count--;
#endif
          continue;
        }

      /*
      **  Complete user entry to the new channel (if any)
      */

#ifdef NO_CHANOPS_WHEN_SPLIT
      if(allow_op)
        {
          add_user_to_channel(chptr, sptr, flags);
        }
      else
        {
          add_user_to_channel(chptr, sptr, flags & CHFL_EXCEPTION);
        }
#else
      add_user_to_channel(chptr, sptr, flags);
#endif
      /*
      **  Set timestamp if appropriate, and propagate
      */
      if (MyClient(sptr) && (flags & CHFL_CHANOP) )
        {
          if (joining_vchan)
            add_vchan_to_client_cache(sptr,root_chptr,chptr);
          chptr->channelts = CurrentTime;
#ifdef NO_CHANOPS_WHEN_SPLIT
          if(allow_op)
            {
              sendto_match_servs(chptr, cptr,
                                 ":%s SJOIN %lu %s + :@%s", me.name,
                                 chptr->channelts, name, parv[0]);

            }                                
          else
            sendto_match_servs(chptr, cptr,
                               ":%s SJOIN %lu %s + :%s", me.name,
                               chptr->channelts, name, parv[0]);
#else
          sendto_match_servs(chptr, cptr,
                             ":%s SJOIN %lu %s + :@%s", me.name,
                             chptr->channelts, name, parv[0]);
#endif
        }
      else if (MyClient(sptr))
        {
          if (joining_vchan)
            add_vchan_to_client_cache(sptr,root_chptr,chptr);
          sendto_match_servs(chptr, cptr,
                             ":%s SJOIN %lu %s + :%s", me.name,
                             chptr->channelts, name, parv[0]);
        }
      else
        sendto_match_servs(chptr, cptr, ":%s JOIN :%s", parv[0],
                           name);

      /*
      ** notify all other users on the new channel
      */
      sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
                             parv[0], name);

      if (MyClient(sptr))
        {
          if( flags & CHFL_CHANOP )
            {
              chptr->mode.mode |= MODE_TOPICLIMIT;
              chptr->mode.mode |= MODE_NOPRIVMSGS;

              sendto_channel_butserv(chptr, sptr,
                                 ":%s MODE %s +nt",
                                 me.name, chptr->chname);

              sendto_match_servs(chptr, sptr, 
                                 ":%s MODE %s +nt",
                                 me.name, chptr->chname);
            }

          del_invite(sptr, chptr);

          if (chptr->topic[0] != '\0')
            {
              sendto_one(sptr, form_str(RPL_TOPIC), me.name,
                         parv[0], name, chptr->topic);

              sendto_one(sptr, form_str(RPL_TOPICWHOTIME),
                         me.name, parv[0], name,
                         chptr->topic_info,
                         chptr->topic_time);
            }
          if (joining_vchan)
	    (void)names_on_this_channel(sptr, chptr, root_chptr->chname);
          else
            (void)names_on_this_channel(sptr, chptr, name);
        }
    }

#ifdef ANTI_SPAMBOT
  if(MyConnect(sptr) && successful_join_count)
    sptr->last_join_time = CurrentTime;
#endif
  return 0;
}

int     ms_join(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  static char   jbuf[BUFSIZE];
  struct SLink  *lp;
  struct Channel *chptr = NULL;
  char  *name, *key = NULL;
  int   i, flags = 0;
#ifdef NO_CHANOPS_WHEN_SPLIT
  int   allow_op=YES;
#endif
  char  *p = NULL, *p2 = NULL;
#ifdef ANTI_SPAMBOT
  int   successful_join_count = 0; /* Number of channels successfully joined */
#endif
  
  if (!(sptr->user))
    {
      /* something is *fucked* - bail */
      return 0;
    }

  if (parc < 2 || *parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "JOIN");
      return 0;
    }

#ifdef NEED_SPLITCODE

  /* Check to see if the timer has timed out, and if so, see if
   * there are a decent number of servers now connected 
   * to consider any possible split over.
   * -Dianora
   */

  if (server_was_split)
    check_still_split();

#endif

  *jbuf = '\0';
  /*
  ** Rebuild list of channels joined to be the actual result of the
  ** JOIN.  Note that "JOIN 0" is the destructive problem.
  */
  for (i = 0, name = strtoken(&p, parv[1], ","); name;
       name = strtoken(&p, (char *)NULL, ","))
    {
      if (!check_channel_name(name))
        {
          sendto_one(sptr, form_str(ERR_BADCHANNAME),
                       me.name, parv[0], (unsigned char*) name);
          continue;
        }
      if (*name == '&' && !MyConnect(sptr))
        continue;
      if (*name == '0' && !atoi(name))
        *jbuf = '\0';
      else if (!IsChannelName(name))
        {
          if (MyClient(sptr))
            sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                       me.name, parv[0], name);
          continue;
        }

#ifdef NO_JOIN_ON_SPLIT_SIMPLE
      if (server_was_split && MyClient(sptr) && (*name != '&') &&
          !IsAnOper(sptr))
        {
              sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
                         me.name, parv[0], name);
              continue;
        }
#endif /* NO_JOIN_ON_SPLIT_SIMPLE */

#if defined(PRESERVE_CHANNEL_ON_SPLIT) || defined(NO_JOIN_ON_SPLIT)
      /* If from a cold start, there were never any channels
       * joined, hence all of them must be considered off limits
       * until this server joins the network
       *
       * cold_start is set to NO if SPLITDELAY is set to 0 in m_set()
       */

      if(cold_start && MyClient(sptr) && (*name != '&') &&
         !IsAnOper(sptr))
        {
              sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
                         me.name, parv[0], name);
              continue;
        }
#endif
      if (*jbuf)
        (void)strcat(jbuf, ",");
      (void)strncat(jbuf, name, sizeof(jbuf) - i - 1);
      i += strlen(name)+1;
    }
  /*    (void)strcpy(parv[1], jbuf);*/

  p = NULL;
  if (parv[2])
    key = strtoken(&p2, parv[2], ",");
  parv[2] = NULL;       /* for m_names call later, parv[parc] must == NULL */
  for (name = strtoken(&p, jbuf, ","); name;
       key = (key) ? strtoken(&p2, NULL, ",") : NULL,
         name = strtoken(&p, NULL, ","))
    {
      /*
      ** JOIN 0 sends out a part for all channels a user
      ** has joined.
      */
      if (*name == '0' && !atoi(name))
        {
          if (sptr->user->channel == NULL)
            continue;
          while ((lp = sptr->user->channel))
            {
              chptr = lp->value.chptr;
              sendto_channel_butserv(chptr, sptr, ":%s PART %s",
                                     parv[0], chptr->chname);
              remove_user_from_channel(sptr, chptr, 0);
            }

#ifdef ANTI_SPAMBOT       /* Dianora */

          if( MyConnect(sptr) && !IsAnOper(sptr) )
            {
              if(GlobalSetOptions.spam_num &&
		 (sptr->join_leave_count >= GlobalSetOptions.spam_num))
                {
                  sendto_ops_flags(FLAGS_BOTS,
                                     "User %s (%s@%s) is a possible spambot",
                                     sptr->name,
                                     sptr->username, sptr->host);
                  sptr->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
                }
              else
                {
                  int t_delta;

                  if( (t_delta = (CurrentTime - sptr->last_leave_time)) >
                      JOIN_LEAVE_COUNT_EXPIRE_TIME)
                    {
                      int decrement_count;
                      decrement_count = (t_delta/JOIN_LEAVE_COUNT_EXPIRE_TIME);

                      if(decrement_count > sptr->join_leave_count)
                        sptr->join_leave_count = 0;
                      else
                        sptr->join_leave_count -= decrement_count;
                    }
                  else
                    {
                      if((CurrentTime - (sptr->last_join_time)) < 
			 GlobalSetOptions.spam_time)
                        {
                          /* oh, its a possible spambot */
                          sptr->join_leave_count++;
                        }
                    }
                  sptr->last_leave_time = CurrentTime;
                }
            }
#endif
          sendto_match_servs(NULL, cptr, ":%s JOIN 0", parv[0]);
          continue;
        }
      
      if (MyConnect(sptr))
        {
          /*
          ** local client is first to enter previously nonexistent
          ** channel so make them (rightfully) the Channel
          ** Operator.
          */
           /*     flags = (ChannelExists(name)) ? 0 : CHFL_CHANOP; */

          /* To save a redundant hash table lookup later on */
           
           if((chptr = hash_find_channel(name, NullChn)))
             flags = 0;
           else
             {
               flags = CHFL_CHANOP;
               if(!ConfigFileEntry.hub)
                 {
                   /* LazyLinks */
                   if( (*name != '&') && serv_cptr_list
                       && IsCapable( serv_cptr_list, CAP_LL) )
                     {
                       sendto_one(serv_cptr_list,":%s CBURST %s %s %s",
                         me.name,name,sptr->name, key ? key: "" );
                       /* And wait for LLJOIN */
                       return 0;
                     }
                 }
             }

           /* if its not a local channel, or isn't an oper
            * and server has been split
            */

#ifdef NO_CHANOPS_WHEN_SPLIT
          if((*name != '&') && !IsAnOper(sptr) && server_was_split)
            {
              allow_op = NO;

              if(!IsRestricted(sptr) && (flags & CHFL_CHANOP))
                sendto_one(sptr,":%s NOTICE %s :*** Notice -- Due to a network split, you can not obtain channel operator status in a new channel at this time.",
                       me.name,
                       sptr->name);
            }
#endif

          if ((sptr->user->joined >= MAXCHANNELSPERUSER) &&
             (!IsAnOper(sptr) || (sptr->user->joined >= MAXCHANNELSPERUSER*3)))
            {
              sendto_one(sptr, form_str(ERR_TOOMANYCHANNELS),
                         me.name, parv[0], name);
#ifdef ANTI_SPAMBOT
              if(successful_join_count)
                sptr->last_join_time = CurrentTime;
#endif
              return 0;
            }


#ifdef ANTI_SPAMBOT       /* Dianora */
          if(flags == 0)        /* if channel doesn't exist, don't penalize */
            successful_join_count++;
          if( GlobalSetOptions.spam_num &&
	      (sptr->join_leave_count >= GlobalSetOptions.spam_num))
            { 
              /* Its already known as a possible spambot */
 
              if(sptr->oper_warn_count_down > 0)  /* my general paranoia */
                sptr->oper_warn_count_down--;
              else
                sptr->oper_warn_count_down = 0;
 
              if(sptr->oper_warn_count_down == 0)
                {
                  sendto_ops_flags(FLAGS_BOTS,
                    "User %s (%s@%s) trying to join %s is a possible spambot",
                             sptr->name,
                             sptr->username,
                             sptr->host,
                             name);     
                  sptr->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
                }
#ifndef ANTI_SPAMBOT_WARN_ONLY
              return 0; /* Don't actually JOIN anything, but don't let
                           spambot know that */
#endif
            }
#endif
        }
      else
        {
          /*
          ** complain for remote JOINs to existing channels
          ** (they should be SJOINs) -orabidoo
          */
          if (!ChannelExists(name))
            ts_warn("User on %s remotely JOINing new channel", 
                    sptr->user->server);
        }

#if defined(NO_JOIN_ON_SPLIT)
      if(server_was_split && (*name != '&'))
        {
          if( chptr )   /* The channel existed, so I can't join it */
            {
              if (IsMember(sptr, chptr)) /* already a member, ignore this */
                continue;

              /* allow local joins to this channel */
              if( (chptr->users == 0) && !IsAnOper(sptr) )
                {
                  sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
                             me.name, parv[0], name);
                  continue;
                }
            }
          else
            chptr = get_channel(sptr, name, CREATE);        
        }
      else
#endif
      if(!chptr)        /* If I already have a chptr, no point doing this */
        chptr = get_channel(sptr, name, CREATE);

      if(chptr)
        {
          if (IsMember(sptr, chptr))    /* already a member, ignore this */
            continue;
        }
      else
        {
          sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE), me.name, parv[0], name);
#ifdef ANTI_SPAMBOT
          if(successful_join_count > 0)
            successful_join_count--;
#endif
          continue;
        }

      /*
       * can_join checks for +i key, bans.
       * If a ban is found but an exception to the ban was found
       * flags will have CHFL_EXCEPTION set
       */

      if (MyConnect(sptr) && (i = can_join(sptr, chptr, key, &flags)))
        {
          sendto_one(sptr,
                    form_str(i), me.name, parv[0], name);
#ifdef ANTI_SPAMBOT
          if(successful_join_count > 0)
            successful_join_count--;
#endif
          continue;
        }

      /*
      **  Complete user entry to the new channel (if any)
      */

#ifdef NO_CHANOPS_WHEN_SPLIT
      if(allow_op)
        {
          add_user_to_channel(chptr, sptr, flags);
        }
      else
        {
          add_user_to_channel(chptr, sptr, flags & CHFL_EXCEPTION);
        }
#else
      add_user_to_channel(chptr, sptr, flags);
#endif
      /*
      **  Set timestamp if appropriate, and propagate
      */
      if (MyClient(sptr) && (flags & CHFL_CHANOP) )
        {
          chptr->channelts = CurrentTime;
#ifdef NO_CHANOPS_WHEN_SPLIT
          if(allow_op)
            {
              sendto_match_servs(chptr, cptr,
                                 ":%s SJOIN %lu %s + :@%s", me.name,
                                 chptr->channelts, name, parv[0]);

            }                                
          else
            sendto_match_servs(chptr, cptr,
                               ":%s SJOIN %lu %s + :%s", me.name,
                               chptr->channelts, name, parv[0]);
#else
          sendto_match_servs(chptr, cptr,
                             ":%s SJOIN %lu %s + :@%s", me.name,
                             chptr->channelts, name, parv[0]);
#endif
        }
      else if (MyClient(sptr))
        {
          sendto_match_servs(chptr, cptr,
                             ":%s SJOIN %lu %s + :%s", me.name,
                             chptr->channelts, name, parv[0]);
        }
      else
        sendto_match_servs(chptr, cptr, ":%s JOIN :%s", parv[0],
                           name);

      /*
      ** notify all other users on the new channel
      */
      sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
                             parv[0], name);

      if (MyClient(sptr))
        {
          if( flags & CHFL_CHANOP )
            {
              chptr->mode.mode |= MODE_TOPICLIMIT;
              chptr->mode.mode |= MODE_NOPRIVMSGS;

              sendto_channel_butserv(chptr, sptr,
                                 ":%s MODE %s +nt",
                                 me.name, chptr->chname);

              sendto_match_servs(chptr, sptr, 
                                 ":%s MODE %s +nt",
                                 me.name, chptr->chname);
            }

          del_invite(sptr, chptr);

          if (chptr->topic[0] != '\0')
            {
              sendto_one(sptr, form_str(RPL_TOPIC), me.name,
                         parv[0], name, chptr->topic);

              sendto_one(sptr, form_str(RPL_TOPICWHOTIME),
                         me.name, parv[0], name,
                         chptr->topic_info,
                         chptr->topic_time);
            }
          parv[1] = name;
          (void)m_names(cptr, sptr, 2, parv);
        }
    }

#ifdef ANTI_SPAMBOT
  if(MyConnect(sptr) && successful_join_count)
    sptr->last_join_time = CurrentTime;
#endif
  return 0;
}

int     mo_join(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  static char   jbuf[BUFSIZE];
  struct SLink  *lp;
  struct Channel *chptr = NULL;
  struct Channel *vchan_chptr = NULL;
  struct Channel *root_chptr = NULL;
  int joining_vchan = 0;
  char  *name, *key = NULL;
  int   i, flags = 0;
  char  *p = NULL, *p2 = NULL;
  
  if (!(sptr->user))
    {
      /* something is *fucked* - bail */
      return 0;
    }

/* not needed --is */
/*  if (parc < 2 || *parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "JOIN");
      return 0;
    } */

  *jbuf = '\0';
  /*
  ** Rebuild list of channels joined to be the actual result of the
  ** JOIN.  Note that "JOIN 0" is the destructive problem.
  */
  for (i = 0, name = strtoken(&p, parv[1], ","); name;
       name = strtoken(&p, (char *)NULL, ","))
    {
      if (!check_channel_name(name))
        {
          sendto_one(sptr, form_str(ERR_BADCHANNAME),
                       me.name, parv[0], (unsigned char*) name);
          continue;
        }
      if (*name == '&' && !MyConnect(sptr))
        continue;
      if (*name == '0' && !atoi(name))
        *jbuf = '\0';
      else if (!IsChannelName(name))
        {
          if (MyClient(sptr))
            sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                       me.name, parv[0], name);
          continue;
        }

      if (*jbuf)
        (void)strcat(jbuf, ",");
      (void)strncat(jbuf, name, sizeof(jbuf) - i - 1);
      i += strlen(name)+1;
    }
  /*    (void)strcpy(parv[1], jbuf);*/

  p = NULL;
  if (parv[2])
    key = strtoken(&p2, parv[2], ",");
  parv[2] = NULL;       /* for m_names call later, parv[parc] must == NULL */
  for (name = strtoken(&p, jbuf, ","); name;
       key = (key) ? strtoken(&p2, NULL, ",") : NULL,
         name = strtoken(&p, NULL, ","))
    {
      /*
      ** JOIN 0 sends out a part for all channels a user
      ** has joined.
      */
      if (*name == '0' && !atoi(name))
        {
          if (sptr->user->channel == NULL)
            continue;
          while ((lp = sptr->user->channel))
            {
              chptr = lp->value.chptr;
              sendto_channel_butserv(chptr, sptr, ":%s PART %s",
                                     parv[0], chptr->chname);
              remove_user_from_channel(sptr, chptr, 0);
            }

          sendto_match_servs(NULL, cptr, ":%s JOIN 0", parv[0]);
          continue;
        }
      
      if (MyConnect(sptr))
        {
          /*
          ** local client is first to enter previously nonexistent
          ** channel so make them (rightfully) the Channel
          ** Operator.
          */
           /*     flags = (ChannelExists(name)) ? 0 : CHFL_CHANOP; */

          /* To save a redundant hash table lookup later on */
           
           if((chptr = hash_find_channel(name, NullChn)))
             {
               /* there's subchans so check those */
               if (IsVchanTop(chptr))
                 {
                   if( on_sub_vchan(chptr,sptr) )    
                     continue;
                   if (key && key[0] == '!')
                     {
                       /* found a matching vchan? let them join it */
                       if ((vchan_chptr = find_vchan(chptr, key)))
                         {
                           root_chptr = chptr;
                           chptr = vchan_chptr;
                           joining_vchan = 1;
                         }
                       else
                         {
                           sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                                      me.name, parv[0], name);
                           return 0;
                         }
                     }
                   else
                     {
                       /* one special case here i think..
                        * if there's only one vchan, and the root is empty
                        * let them join that vchan */
                       if( (!chptr->members) && (!chptr->next_vchan->next_vchan) )
                         {
                           root_chptr = chptr;
                           chptr = chptr->next_vchan;
                           joining_vchan = 1;
                         }
                       /* otherwise, they get a list of inhabited channels */
                       else
                        {
                          show_vchans(cptr, sptr, chptr);
                          return 0;
                        }
                     }   
                 }
               /* trying to join a sub chans 'real' name               
                * don't allow that */
               else if (IsVchan(chptr))
                 {
                   sendto_one(sptr, form_str(ERR_BADCHANNAME),      
                              me.name, parv[0], (unsigned char*) name);   
                   return 0;      
                 }
               flags = 0;
             }
           else
             {
               flags = CHFL_CHANOP;
               if(!ConfigFileEntry.hub)
                 {
                   /* LazyLinks */
                   if( (*name != '&') && serv_cptr_list
                       && IsCapable( serv_cptr_list, CAP_LL) )
                     {
                       sendto_one(serv_cptr_list,":%s CBURST %s %s %s",
                         me.name,name,sptr->name, key ? key: "" );
                       /* And wait for LLJOIN */
                       return 0;
                     }
                 }
             }

          if (sptr->user->joined >= MAXCHANNELSPERUSER*3)
            {
              sendto_one(sptr, form_str(ERR_TOOMANYCHANNELS),
                         me.name, parv[0], name);
              return 0;
            }
        }
      else
        {
          /*
          ** complain for remote JOINs to existing channels
          ** (they should be SJOINs) -orabidoo
          */
          if (!ChannelExists(name))
            ts_warn("User on %s remotely JOINing new channel", 
                    sptr->user->server);
        }

      if(!chptr)        /* If I already have a chptr, no point doing this */
        chptr = get_channel(sptr, name, CREATE);

      if(chptr)
        {
          if (IsMember(sptr, chptr))    /* already a member, ignore this */
            continue;
        }
      else
        {
          sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
		     me.name, parv[0], name);
          continue;
        }

      /*
       * can_join checks for +i key, bans.
       * If a ban is found but an exception to the ban was found
       * flags will have CHFL_EXCEPTION set
       */

      if (MyConnect(sptr) && (i = can_join(sptr, chptr, key, &flags)))
        {
          sendto_one(sptr,
                    form_str(i), me.name, parv[0], name);
          continue;
        }

      /*
      **  Complete user entry to the new channel (if any)
      */

      add_user_to_channel(chptr, sptr, flags);
      /*
      **  Set timestamp if appropriate, and propagate
      */
      if (MyClient(sptr) && (flags & CHFL_CHANOP) )
        {
          if (joining_vchan)
            add_vchan_to_client_cache(sptr,root_chptr,chptr);
          chptr->channelts = CurrentTime;
          sendto_match_servs(chptr, cptr,
                             ":%s SJOIN %lu %s + :@%s", me.name,
                             chptr->channelts, name, parv[0]);
        }
      else if (MyClient(sptr))
        {
          if (joining_vchan)
            add_vchan_to_client_cache(sptr,root_chptr,chptr);
          sendto_match_servs(chptr, cptr,
                             ":%s SJOIN %lu %s + :%s", me.name,
                             chptr->channelts, name, parv[0]);
        }
      else
        sendto_match_servs(chptr, cptr, ":%s JOIN :%s", parv[0],
                           name);

      /*
      ** notify all other users on the new channel
      */
      sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
                             parv[0], name);

      if (MyClient(sptr))
        {
          if( flags & CHFL_CHANOP )
            {
              chptr->mode.mode |= MODE_TOPICLIMIT;
              chptr->mode.mode |= MODE_NOPRIVMSGS;

              sendto_channel_butserv(chptr, sptr,
                                 ":%s MODE %s +nt",
                                 me.name, chptr->chname);

              sendto_match_servs(chptr, sptr, 
                                 ":%s MODE %s +nt",
                                 me.name, chptr->chname);
            }

          del_invite(sptr, chptr);

          if (chptr->topic[0] != '\0')
            {
              sendto_one(sptr, form_str(RPL_TOPIC), me.name,
                         parv[0], name, chptr->topic);

              sendto_one(sptr, form_str(RPL_TOPICWHOTIME),
                         me.name, parv[0], name,
                         chptr->topic_info,
                         chptr->topic_time);
            }
          if (joining_vchan)
            (void)names_on_this_channel(sptr, chptr, root_chptr->chname);
          else
            (void)names_on_this_channel(sptr, chptr, name);
        }
    }
  return 0;
}


#ifdef DBOP
/* ZZZZZZZZZZZZ Q&D debug function */
int     m_dbop(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  int counted_ops=0;
  struct SLink  *l;
  char *name;
  struct Channel *chptr;

  name = parv[1];

  if(!(chptr=hash_find_channel(name, NullChn)))
    {
      sendto_one(sptr,
      ":%s NOTICE %s :*** Notice %s does not exist",
        me.name, sptr->name,  name );
      return -1;
    }

  for (l = chptr->members; l && l->value.cptr; l = l->next)
    if (l->flags & MODE_CHANOP)
      {
        counted_ops++;
      }

  sendto_one(sptr,":%s NOTICE %s :*** Notice %s chptr->opcount %d counted %d",
    me.name, sptr->name, name, chptr->opcount, counted_ops);

  return 0;
}
#endif
