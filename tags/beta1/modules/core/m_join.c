/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_join.c
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
#include "common.h"   /* bleah */
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "send.h"
#include "s_serv.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int m_join(struct Client*, struct Client*, int, char**);
static int ms_join(struct Client*, struct Client*, int, char**);

struct Message join_msgtab = {
  "JOIN", 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_join, ms_join, m_join}
};

void
_modinit(void)
{
  mod_add_cmd(&join_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&join_msgtab);
}

static void build_list_of_channels( struct Client *sptr,
                                    char *jbuf, char *given_names);
static void do_join_0(struct Client *cptr, struct Client *sptr);
static void check_spambot_warning( struct Client *sptr, char *name );

char *_version = "20001122";

/*
** m_join
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = channel password (key) (or vkey for vchans)
**      parv[3] = vkey
*/
static int m_join(struct Client *cptr,
                  struct Client *sptr,
                  int parc,
                  char *parv[])
{
  static char   jbuf[BUFSIZE];
  struct Channel *chptr = NULL;
  struct Channel *vchan_chptr = NULL;
  struct Channel *root_chptr = NULL;
  int joining_vchan = 0;
  char  *name, *key = NULL;
  char *vkey = NULL; /* !key for vchans */
  int   i, flags = 0;
  char  *p = NULL, *p2 = NULL, *p3 = NULL, *pvc = NULL;
  int   vc_ts;
  int   successful_join_count = 0; /* Number of channels successfully joined */
  
  if (!(sptr->user) || IsServer(sptr))
    {
      /* something is *fucked* - bail */
      return 0;
    }

  if (*parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "JOIN");
      return 0;
    }

  build_list_of_channels( sptr, jbuf , parv[1] );

  p = NULL;

  if (parc > 3)
    {
      key = strtoken(&p2, parv[3], ",");
      vkey = strtoken(&p3, parv[2], ",");
    }
  else if (parc > 2)
    {
      key = strtoken(&p2, parv[2], ",");
      vkey = key;
    }

  for (name = strtoken(&p, jbuf, ","); name;
         key = (key) ? strtoken(&p2, NULL, ",") : NULL,
         vkey = (parc>3) ? ((vkey) ? strtoken(&p3, NULL, ",") : NULL) : key,
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
          if (sptr->user->channel.head == NULL)
            continue;
	  do_join_0(&me,sptr);
	  check_spambot_warning(sptr,"0");
	  continue;
	}

      if( (chptr = hash_find_channel(name, NullChn)) != NULL )
	{
          /* Check if they want to join a subchan or something */
	  vchan_chptr = select_vchan(chptr, cptr, sptr, vkey, name);
          
          if (!vchan_chptr)
            continue;

          if (vchan_chptr != chptr)
          {
            joining_vchan = 1;
            root_chptr = chptr;
            chptr = vchan_chptr;
          }
          else
          {
            joining_vchan = 0;
            root_chptr = chptr;
          }
          
	  if (chptr->users == 0)
	    flags = CHFL_CHANOP;
	  else
	    flags = 0;
	}
      else
	{
	  flags = CHFL_CHANOP;
	  if(!ServerInfo.hub)
	    {
	      /* LazyLinks */
	      if( (*name != '&') && uplink
		  && IsCapable(uplink, CAP_LL) )
		{
		  sendto_one(uplink,":%s CBURST %s %s %s",
			     me.name,name,sptr->name, key ? key: "" );
		  /* And wait for LLJOIN */
		  return 0;
		}
	    }
	}

      if ((sptr->user->joined >= MAXCHANNELSPERUSER) &&
         (!IsOper(sptr) || (sptr->user->joined >= MAXCHANNELSPERUSER*3)))
	{
	  sendto_one(sptr, form_str(ERR_TOOMANYCHANNELS),
		     me.name, parv[0], name);
	  if(successful_join_count)
	    sptr->localClient->last_join_time = CurrentTime;
	  return 0;
	}

      if(flags == 0)        /* if channel doesn't exist, don't penalize */
	successful_join_count++;

      check_spambot_warning(sptr, name);

      if(!chptr)        /* If I already have a chptr, no point doing this */
      {
	chptr = get_channel(sptr, name, CREATE);
        root_chptr = chptr;
      }
      
      if(chptr)
	{
	  if (IsMember(sptr, chptr))    /* already a member, ignore this */
	    continue;
	}
      else
	{
	  sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
		     me.name, parv[0], name);
	  if(successful_join_count > 0)
	    successful_join_count--;
	  continue;
	}
      
      /*
       * can_join checks for +i key, bans.
       */

      if ( (i = can_join(sptr, chptr, key)) )
	{
	  sendto_one(sptr,
		     form_str(i), me.name, parv[0], name);
	  if(successful_join_count > 0)
	    successful_join_count--;
	  continue;
	}

      /*
      **  Complete user entry to the new channel (if any)
      */
      
      add_user_to_channel(chptr, sptr, flags);

      if (joining_vchan)
	{
	  add_vchan_to_client_cache(sptr,root_chptr,chptr);
	}

      /*
      **  Set timestamp if appropriate, and propagate
      */

      if (flags & CHFL_CHANOP)
	{
	  chptr->channelts = CurrentTime;

          /*
           * XXX - this is a rather ugly hack.
           *
           * Unfortunately, there's no way to pass
           * the fact that it is a vchan through SJOIN...
           */

          /* Prevent users creating a fake vchan */
          if (name[0] == '#' && name[1] == '#')
            {
              if ((pvc = strrchr(name+3, '_'))) 
                {
                  /*
                   * OK, name matches possible vchan:
                   * ##channel_blah
                   */
                  pvc++; /*  point pvc after last _ */
                  vc_ts = atol(pvc);
                  
                  /*
                   * if blah is the same as the TS, up the TS
                   * by one, to prevent this channel being
                   * seen as a vchan
                   */
                  if (vc_ts == CurrentTime)
                    chptr->channelts++;
                }
            }
                  
	  sendto_ll_channel_remote(chptr, cptr, sptr,
				   ":%s SJOIN %lu %s + :@%s",
				   me.name,
				   chptr->channelts,
				   chptr->chname,
				   parv[0]);
	}
#if 0
      /*
       * XXX
       * This is broken.  We check if sptr has CAP_HOPS, instead
       * of the server.  Plus, if the server doesn't support
       * HOPS we should send halfops as ops -- we are the users server,
       * so we will do the correct permision checks.
       * There is no way flags can include CHFL_HALFOP anyway,
       * so this code isn't needed anyway
       *
       * -davidt
       */
      else if ((flags & CHFL_HALFOP) && (IsCapable(sptr,CAP_HOPS)))
	{
	  chptr->channelts = CurrentTime;
	  sendto_ll_channel_remote(chptr, cptr, sptr,
				   ":%s SJOIN %lu %s + :%%%s",
				   me.name,
				   chptr->channelts,
				   chptr->chname,
				   parv[0]);
	}
#endif
      else
	{
	  sendto_ll_channel_remote(chptr, cptr, sptr,
				   ":%s SJOIN %lu %s + :%s",
				   me.name,
				   chptr->channelts,
				   chptr->chname,
				   parv[0]);
	}

      /*
      ** notify all other users on the new channel
      */
      sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
			   sptr->name,
			   sptr->username,
			   sptr->host,
			   root_chptr->chname);
      
      if( flags & CHFL_CHANOP )
	{
	  chptr->mode.mode |= MODE_TOPICLIMIT;
	  chptr->mode.mode |= MODE_NOPRIVMSGS;

	  sendto_channel_local(ONLY_CHANOPS_HALFOPS,chptr,
			       ":%s MODE %s +nt",
			       me.name,
			       root_chptr->chname);
	  
	  sendto_ll_channel_remote(chptr, cptr, sptr,
				   ":%s MODE %s +nt",
				   me.name,
				   chptr->chname);
	}

      del_invite(chptr, sptr);
      
      if (chptr->topic[0] != '\0')
	{
	  sendto_one(sptr, form_str(RPL_TOPIC), me.name,
		     parv[0], root_chptr->chname, chptr->topic);

          if (!(chptr->mode.mode & MODE_HIDEOPS) ||
              (flags & CHFL_CHANOP) || (flags & CHFL_HALFOP))
            {
              sendto_one(sptr, form_str(RPL_TOPICWHOTIME),
                         me.name, parv[0], root_chptr->chname,
                         chptr->topic_info,
                         chptr->topic_time);
            }
          else /* Hide from nonops */
            {
               sendto_one(sptr, form_str(RPL_TOPICWHOTIME),
                         me.name, parv[0], root_chptr->chname,
                         me.name,
                         chptr->topic_time);
            }
	}

      (void)channel_member_names(sptr, chptr, root_chptr->chname);
      
      if(successful_join_count)
	sptr->localClient->last_join_time = CurrentTime;
    }
  return 0;
}

static int ms_join(struct Client *cptr,
                   struct Client *sptr,
                   int parc,
                   char *parv[])
{
  char *name;
  
  if (!(sptr->user))
    return 0;
  
  name = parv[1];

  /*
  ** complain for remote JOINs to existing channels
  ** (they should be SJOINs) -orabidoo
  */
  if ((name[0] == '0') && (name[1] == '\0'))
    {
      do_join_0(cptr, sptr);
    }
  else
    {
      ts_warn("User on %s remotely JOINing new channel", 
	      sptr->user->server);
    }

  /* AND ignore it finally. */
  return 0;
}

/*
 * build_list_of_channels
 *
 * inputs	- pointer to client joining
 *		- pointer to scratch buffer
 *		- pointer to list of channel names
 * output	- NONE
 * side effects - jbuf is modified to contain valid list of channel names
 */
static void build_list_of_channels( struct Client *sptr,
				    char *jbuf, char *given_names)
{
  char *name;
  char *p;
  int i;

  *jbuf = '\0';

  /*
  ** Rebuild list of channels joined to be the actual result of the
  ** JOIN.  Note that "JOIN 0" is the destructive problem.
  */
  for (i = 0, name = strtoken(&p, given_names, ","); name;
       name = strtoken(&p, (char *)NULL, ","))
    {
      if (!check_channel_name(name))
        {
          sendto_one(sptr, form_str(ERR_BADCHANNAME),
                       me.name, sptr->name, (unsigned char*) name);
          continue;
        }
      if (*name == '0' && (atoi(name) == 0))
	{
	  strcat(jbuf,"0");
	  continue;
	}
      else if (!IsChannelName(name))
        {
	  sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
		     me.name, sptr->name, name);
          continue;
        }

      if (strlen(name) > CHANNELLEN)
        {
          sendto_one(sptr, form_str(ERR_BADCHANNAME),me.name,sptr->name,name);
          continue;
        }

      if (*jbuf)
        (void)strcat(jbuf, ",");
      (void)strncat(jbuf, name, BUFSIZE - i - 1);
      i += strlen(name)+1;
    }
}

/*
 * do_join_0
 *
 * inputs	- pointer to client doing join 0
 * output	- NONE
 * side effects	- Use has decided to join 0. This is legacy
 *		  from the days when channels were numbers not names. *sigh*
 *		  There is a bunch of evilness necessary here due to
 * 		  anti spambot code.
 */

static void do_join_0(struct Client *cptr, struct Client *sptr)
{
  struct Channel *chptr=NULL;
  dlink_node   *lp;

  sendto_ll_serv_butone(cptr, sptr, 0, ":%s JOIN 0", sptr->name);

  while ((lp = sptr->user->channel.head))
    {
      chptr = lp->data;
      sendto_channel_local(ALL_MEMBERS,chptr, ":%s!%s@%s PART %s",
			   sptr->name,
			   sptr->username,
			   sptr->host,
			   RootChan(chptr)->chname);
      remove_user_from_channel(chptr, sptr);
    }
}

/*
 * check_spambot_warning
 *
 * inputs	- pointer to client to check
 * output	- NONE
 * side effects	- 
 */

static void check_spambot_warning( struct Client *sptr, char *name )
{
  int t_delta;
  int decrement_count;

  if(GlobalSetOptions.spam_num &&
     (sptr->localClient->join_leave_count >= GlobalSetOptions.spam_num))
    {
      if(sptr->localClient->oper_warn_count_down == 0)
	{
	  /* Its already known as a possible spambot */
	  
	  if(sptr->localClient->oper_warn_count_down > 0) 
	    sptr->localClient->oper_warn_count_down--;
	  else
	    sptr->localClient->oper_warn_count_down = 0;

	  sendto_realops_flags(FLAGS_BOTS,
	       "User %s (%s@%s) trying to join %s is a possible spambot",
			       sptr->name,
			       sptr->username,
			       sptr->host,
			       name);     
	  sptr->localClient->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
	}
    }
  else
    {
      if( (t_delta = (CurrentTime - sptr->localClient->last_leave_time)) >
	  JOIN_LEAVE_COUNT_EXPIRE_TIME)
	{
	  decrement_count = (t_delta/JOIN_LEAVE_COUNT_EXPIRE_TIME);

	  if(decrement_count > sptr->localClient->join_leave_count)
	    sptr->localClient->join_leave_count = 0;
	  else
	    sptr->localClient->join_leave_count -= decrement_count;
	}
      else
	{
	  if((CurrentTime - (sptr->localClient->last_join_time)) < 
	     GlobalSetOptions.spam_time)
	    {
	      /* oh, its a possible spambot */
	      sptr->localClient->join_leave_count++;
	    }
	}
      sptr->localClient->last_leave_time = CurrentTime;
    }
}

