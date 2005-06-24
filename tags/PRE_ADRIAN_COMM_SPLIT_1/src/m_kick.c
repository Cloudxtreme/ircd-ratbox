/************************************************************************
 *   IRC - Internet Relay Chat, src/m_kick.c
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
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"

#include <string.h>
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
** m_kick
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = client to kick
**      parv[3] = kick comment
*/
/*
 * I've removed the multi channel kick, and the multi user kick
 * though, there are still remnants left ie..
 * "name = strtoken(&p, parv[1], ",");" in a normal kick
 * it will just be "KICK #channel nick"
 * A strchr() is going to be faster than a strtoken(), so rewritten
 * to use a strchr()
 *
 * It appears the original code was supposed to support 
 * "kick #channel1,#channel2 nick1,nick2,nick3." For example, look at
 * the original code for m_topic(), where 
 * "topic #channel1,#channel2,#channel3... topic" was supported.
 *
 * -Dianora
 */
int     m_kick(struct Client *cptr,
               struct Client *sptr,
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

  if (parc < 3 || *parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "KICK");
      return 0;
    }
  if (IsServer(sptr))
    sendto_ops("KICK from %s for %s %s",
               parv[0], parv[1], parv[2]);
  comment = (BadPtr(parv[3])) ? parv[0] : parv[3];
  if (strlen(comment) > (size_t) TOPICLEN)
    comment[TOPICLEN] = '\0';

  *buf = '\0';
  p = strchr(parv[1],',');
  if(p)
    *p = '\0';
  name = parv[1]; /* strtoken(&p, parv[1], ","); */

  chptr = get_channel(sptr, name, !CREATE);
  if (!chptr)
    {
      sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                 me.name, parv[0], name);
      return(0);
    }

  if (HasVchans(chptr))
    {
      vchan = map_vchan(chptr,sptr);
      if(vchan != 0)
	{
	  chptr = vchan;
	}
    }

  /* You either have chan op privs, or you don't -Dianora */
  /* orabidoo and I discussed this one for a while...
   * I hope he approves of this code, (he did) users can get quite confused...
   *    -Dianora
   */

  if (!IsServer(sptr) && !is_chan_op(sptr, chptr) ) 
    { 
      /* was a user, not a server, and user isn't seen as a chanop here */
      
      if(MyConnect(sptr))
        {
          /* user on _my_ server, with no chanops.. so go away */
          
          sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED),
                     me.name, parv[0], name);
          return(0);
        }

      if(chptr->channelts == 0)
        {
          /* If its a TS 0 channel, do it the old way */
          
          sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED),
                     me.name, parv[0], name);
          return(0);
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

      /*          sendto_one(sptr, form_str(ERR_DESYNC),
       *           me.name, parv[0], chptr->chname);
       */

      /*
       * After more discussion with orabidoo...
       *
       * The code was sound, however, what happens if we have +h (TS4)
       * and some servers don't understand it yet? 
       * we will be seeing servers with users who appear to have
       * no chanops at all, merrily kicking users....
       * -Dianora
       */
    }

  p = strchr(parv[2],',');
  if(p)
    *p = '\0';
  user = parv[2]; /* strtoken(&p2, parv[2], ","); */

  if (!(who = find_chasing(sptr, user, &chasing)))
    {
      return(0);
    }

  if (IsMember(who, chptr))
    {
      sendto_channel_butserv(chptr, sptr,
                             ":%s KICK %s %s :%s", parv[0],
                             name, who->name, comment);
      sendto_match_servs(chptr, cptr,
                         ":%s KICK %s %s :%s",
                         parv[0], chptr->chname,
                         who->name, comment);
      remove_user_from_channel(who, chptr, 1);
    }
  else
    sendto_one(sptr, form_str(ERR_USERNOTINCHANNEL),
               me.name, parv[0], user, name);

  return (0);
}

int     ms_kick(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  struct Client *who;
  struct Channel *chptr;
  int   chasing = 0;
  char  *comment;
  char  *name;
  char  *p = (char *)NULL;
  char  *user;
  static char     buf[BUFSIZE];

  if (parc < 3 || *parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "KICK");
      return 0;
    }
  if (IsServer(sptr))
    sendto_ops("KICK from %s for %s %s",
               parv[0], parv[1], parv[2]);
  comment = (BadPtr(parv[3])) ? parv[0] : parv[3];
  if (strlen(comment) > (size_t) TOPICLEN)
    comment[TOPICLEN] = '\0';

  *buf = '\0';
  p = strchr(parv[1],',');
  if(p)
    *p = '\0';
  name = parv[1]; /* strtoken(&p, parv[1], ","); */

  chptr = get_channel(sptr, name, !CREATE);
  if (!chptr)
    {
      sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                 me.name, parv[0], name);
      return(0);
    }

  /* You either have chan op privs, or you don't -Dianora */
  /* orabidoo and I discussed this one for a while...
   * I hope he approves of this code, (he did) users can get quite confused...
   *    -Dianora
   */

  if (!IsServer(sptr) && !is_chan_op(sptr, chptr) ) 
    { 
      /* was a user, not a server, and user isn't seen as a chanop here */
      
      if(MyConnect(sptr))
        {
          /* user on _my_ server, with no chanops.. so go away */
          
          sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED),
                     me.name, parv[0], chptr->chname);
          return(0);
        }

      if(chptr->channelts == 0)
        {
          /* If its a TS 0 channel, do it the old way */
          
          sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED),
                     me.name, parv[0], chptr->chname);
          return(0);
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

      /*          sendto_one(sptr, form_str(ERR_DESYNC),
       *           me.name, parv[0], chptr->chname);
       */

      /*
       * After more discussion with orabidoo...
       *
       * The code was sound, however, what happens if we have +h (TS4)
       * and some servers don't understand it yet? 
       * we will be seeing servers with users who appear to have
       * no chanops at all, merrily kicking users....
       * -Dianora
       */
    }

  p = strchr(parv[2],',');
  if(p)
    *p = '\0';
  user = parv[2]; /* strtoken(&p2, parv[2], ","); */

  if (!(who = find_chasing(sptr, user, &chasing)))
    {
      return(0);
    }

  if (IsMember(who, chptr))
    {
      sendto_channel_butserv(chptr, sptr,
                             ":%s KICK %s %s :%s", parv[0],
                             name, who->name, comment);
      sendto_match_servs(chptr, cptr,
                         ":%s KICK %s %s :%s",
                         parv[0], name,
                         who->name, comment);
      remove_user_from_channel(who, chptr, 1);
    }
  else
    sendto_one(sptr, form_str(ERR_USERNOTINCHANNEL),
               me.name, parv[0], user, name);

  return (0);
}
