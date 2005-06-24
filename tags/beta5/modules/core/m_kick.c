/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_kick.c
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
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "modules.h"
#include "parse.h"

#include <string.h>

static void m_kick(struct Client*, struct Client*, int, char**);
static void ms_kick(struct Client*, struct Client*, int, char**);

struct Message kick_msgtab = {
  "KICK", 0, 3, 0, MFLG_SLOW, 0,
  {m_unregistered, m_kick, ms_kick, m_kick}
};
#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&kick_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&kick_msgtab);
}

char *_version = "20010428";
#endif
/*
** m_kick
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = client to kick
**      parv[3] = kick comment
*/
static void m_kick(struct Client *client_p,
                  struct Client *source_p,
                  int parc,
                  char *parv[])
{
  struct Client *who;
  struct Channel *chptr;
  struct Channel *vchan;
  int   chasing = 0;
  char  *comment;
  char  *name;
  char  *p = (char *)NULL;
  char  *user;
  static char     buf[BUFSIZE];

  if (*parv[2] == '\0')
    {
      sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "KICK");
      return;
    }

  comment = (BadPtr(parv[3])) ? parv[2] : parv[3];
  if (strlen(comment) > (size_t) TOPICLEN)
    comment[TOPICLEN] = '\0';

  *buf = '\0';
  if( (p = strchr(parv[1],',')) )
    *p = '\0';

  name = parv[1];

  chptr = get_channel(source_p, name, !CREATE);
  if (!chptr)
    {
      sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                 me.name, parv[0], name);
      return;
    }

  if (HasVchans(chptr))
    {
      vchan = map_vchan(chptr,source_p);
      if(vchan != 0)
	{
	  chptr = vchan;
	}
    }

  if (!IsServer(source_p) && !is_any_op(chptr, source_p) ) 
    { 
      /* was a user, not a server, and user isn't seen as a chanop here */
      
      if(MyConnect(source_p))
        {
          /* user on _my_ server, with no chanops.. so go away */
          
          sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
                     me.name, parv[0], name);
          return;
        }

      if(chptr->channelts == 0)
        {
          /* If its a TS 0 channel, do it the old way */
          
          sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
                     me.name, parv[0], name);
          return;
        }

      /* Its a user doing a kick, but is not showing as chanop locally
       * its also not a user ON -my- server, and the channel has a TS.
       * There are two cases we can get to this point then...
       *
       *     1) connect burst is happening, and for some reason a legit
       *        op has sent a KICK, but the SJOIN hasn't happened yet or 
       *        been seen. (who knows.. due to lag...)
       *
       *     2) The channel is desynced. That can STILL happen with TS
       *        
       *     Now, the old code roger wrote, would allow the KICK to 
       *     go through. Thats quite legit, but lets weird things like
       *     KICKS by users who appear not to be chanopped happen,
       *     or even neater, they appear not to be on the channel.
       *     This fits every definition of a desync, doesn't it? ;-)
       *     So I will allow the KICK, otherwise, things are MUCH worse.
       *     But I will warn it as a possible desync.
       *
       *     -Dianora
       */
    }

  if( (p = strchr(parv[2],',')) )
    *p = '\0';

  user = parv[2]; /* strtoken(&p2, parv[2], ","); */

  if (!(who = find_chasing(source_p, user, &chasing)))
    {
      return;
    }

  if (IsMember(who, chptr))
    {
      /* half ops cannot kick full chanops */
      if (is_half_op(chptr,source_p) && is_chan_op(chptr,who))
	{
          sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
                     me.name, parv[0], name);
	  return;
	}
      /* jdc
       * - In the case of a server kicking a user (i.e. CLEARCHAN),
       *   the kick should show up as coming from the server which did
       *   the kick.
       * - Personally, flame and I believe that server kicks shouldn't
       *   be sent anyways.  Just waiting for some oper to abuse it...
       */
      if (IsServer(source_p))
      {
        sendto_channel_local(ALL_MEMBERS, chptr, ":%s KICK %s %s :%s",
          source_p->name, name, who->name, comment);
      }
      else if(chptr->mode.mode & MODE_HIDEOPS)
	{
	  /* jdc -- Non-chanops get kicked from me.name, not
	   *        who->name (themselves).
	   */
	  sendto_channel_local(NON_CHANOPS, chptr,
			       ":%s KICK %s %s :%s",
			       me.name,
			       name, who->name, comment);

	  sendto_channel_local(ONLY_CHANOPS_HALFOPS, chptr,
			       ":%s!%s@%s KICK %s %s :%s",
			       source_p->name,
			       source_p->username,
			       source_p->host,
			       name,
			       who->name, comment);
	}
      else
	{
	  sendto_channel_local(ALL_MEMBERS, chptr,
			       ":%s!%s@%s KICK %s %s :%s",
			       source_p->name,
			       source_p->username,
			       source_p->host,
			       name, who->name, comment);
	}

      sendto_channel_remote(chptr, client_p,
			    ":%s KICK %s %s :%s",
			    parv[0], chptr->chname,
			    who->name, comment);
      remove_user_from_channel(chptr, who, 0);
    }
  else
    sendto_one(source_p, form_str(ERR_USERNOTINCHANNEL),
               me.name, parv[0], user, name);
}

static void ms_kick(struct Client *client_p,
                   struct Client *source_p,
                   int parc,
                   char *parv[])
{
  if (*parv[2] == '\0')
    {
      sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "KICK");
      return;
    }

  m_kick(client_p, source_p, parc, parv);
}
