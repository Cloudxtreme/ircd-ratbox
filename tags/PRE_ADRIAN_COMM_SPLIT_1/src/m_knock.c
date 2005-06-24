/************************************************************************
 *   IRC - Internet Relay Chat, src/m_knock.c
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
#include "client.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"

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

/* m_knock
**    parv[0] = sender prefix
**    parv[1] = channel
**  The KNOCK command has the following syntax:
**   :<sender> KNOCK <channel>
**  If a user is not banned from the channel they can use the KNOCK
**  command to have the server NOTICE the channel operators notifying
**  they would like to join.  Helpful if the channel is invite-only, the
**  key is forgotten, or the channel is full (INVITE can bypass each one
**  of these conditions.  Concept by Dianora <db@db.net> and written by
**  <anonymous>
**
** Just some flood control added here, five minute delay between each
** KNOCK -Dianora
**/

int     m_knock(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  struct Channel      *chptr;
  char  *p, *name;

  /* anti flooding code,
   * I did have this in parse.c with a table lookup
   * but I think this will be less inefficient doing it in each
   * function that absolutely needs it
   *
   * -Dianora
   */
  static time_t last_used=0L;

  /* We will cut at the first comma reached, however we will not *
   * process anything afterwards.                                */

  p = strchr(parv[1],',');
  if(p)
    *p = '\0';
  name = parv[1]; /* strtoken(&p, parv[1], ","); */

  if (!IsChannelName(name) || !(chptr = hash_find_channel(name, NullChn)))
    {
      sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL), me.name, parv[0],
                 name);
      return 0;
    }

  if(!((chptr->mode.mode & MODE_INVITEONLY) ||
       (*chptr->mode.key) ||
       (chptr->mode.limit && chptr->users >= chptr->mode.limit )
       ))
    {
      sendto_one(sptr,":%s NOTICE %s :*** Notice -- Channel is open!",
                 me.name,
                 sptr->name);
      return 0;
    }

  /* don't allow a knock if the user is banned, or the channel is secret */
  if ((chptr->mode.mode & MODE_SECRET) || 
      (is_banned(sptr, chptr) == CHFL_BAN) )
    {
      sendto_one(sptr, form_str(ERR_CANNOTSENDTOCHAN), me.name, parv[0],
                 name);
      return 0;
    }

  /* if the user is already on channel, then a knock is pointless! */
  if (IsMember(sptr, chptr))
    {
      sendto_one(sptr,":%s NOTICE %s :*** Notice -- You are on channel already!",
                 me.name,
                 sptr->name);
      return 0;
    }

  /* flood control server wide, clients on KNOCK
   */

  if((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
	  return 0;
  else
	  last_used = CurrentTime;
 
  /* flood control individual clients on KNOCK
   * the ugly possibility still exists, 400 clones could all KNOCK
   * on a channel at once, flooding all the ops. *ugh*
   * Remember when life was simpler?
   * -Dianora
   */

  /* opers are not flow controlled here */
  if((sptr->last_knock + ConfigFileEntry.knock_delay) > CurrentTime)
    {
      sendto_one(sptr, ":%s NOTICE %s :*** Notice -- Wait %d seconds before another knock",
                 me.name, sptr->name,
                 ConfigFileEntry.knock_delay - (CurrentTime - sptr->last_knock));
      return 0;
    }

  sptr->last_knock = CurrentTime;

  sendto_one(sptr,":%s NOTICE %s :*** Notice -- Your KNOCK has been delivered",
                 me.name, sptr->name);

  /* using &me and me.name won't deliver to clients not on this server
   * so, the notice will have to appear from the "knocker" ick.
   *
   * Ideally, KNOCK would be routable. Also it would be nice to add
   * a new channel mode. Perhaps some day.
   * For now, clients that don't want to see KNOCK requests will have
   * to use client side filtering. 
   *
   * -Dianora
   */

  {
    char message[NICKLEN*2+CHANNELLEN+USERLEN+HOSTLEN+30];

    /* bit of paranoid, be a shame if it cored for this -Dianora */
    if(sptr->user)
      {
        ircsprintf(message,"KNOCK: %s (%s [%s@%s] has asked for an invite)",
                   chptr->chname,
                   sptr->name,
                   sptr->username,
                   sptr->host);
        sendto_channel_type_notice(cptr, chptr, MODE_CHANOP, message);
      }
  }
  return 0;
}

int     mo_knock(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  struct Channel      *chptr;
  char  *p, *name;

  /* We will cut at the first comma reached, however we will not *
   * process anything afterwards.                                */

  p = strchr(parv[1],',');
  if(p)
    *p = '\0';
  name = parv[1]; /* strtoken(&p, parv[1], ","); */

  if (!IsChannelName(name) || !(chptr = hash_find_channel(name, NullChn)))
    {
      sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL), me.name, parv[0],
                 name);
      return 0;
    }

  if(!((chptr->mode.mode & MODE_INVITEONLY) ||
       (*chptr->mode.key) ||
       (chptr->mode.limit && chptr->users >= chptr->mode.limit )
       ))
    {
      sendto_one(sptr,":%s NOTICE %s :*** Notice -- Channel is open!",
                 me.name,
                 sptr->name);
      return 0;
    }

  /* don't allow a knock if the user is banned, or the channel is secret */
  if ((chptr->mode.mode & MODE_SECRET) || 
      (is_banned(sptr, chptr) == CHFL_BAN) )
    {
      sendto_one(sptr, form_str(ERR_CANNOTSENDTOCHAN), me.name, parv[0],
                 name);
      return 0;
    }

  /* if the user is already on channel, then a knock is pointless! */
  if (IsMember(sptr, chptr))
    {
      sendto_one(sptr,":%s NOTICE %s :*** Notice -- You are on channel already!",
                 me.name,
                 sptr->name);
      return 0;
    }

  sptr->last_knock = CurrentTime;

  sendto_one(sptr,":%s NOTICE %s :*** Notice -- Your KNOCK has been delivered",
                 me.name, sptr->name);

  /* using &me and me.name won't deliver to clients not on this server
   * so, the notice will have to appear from the "knocker" ick.
   *
   * Ideally, KNOCK would be routable. Also it would be nice to add
   * a new channel mode. Perhaps some day.
   * For now, clients that don't want to see KNOCK requests will have
   * to use client side filtering. 
   *
   * -Dianora
   */

  {
    char message[NICKLEN*2+CHANNELLEN+USERLEN+HOSTLEN+30];

    /* bit of paranoid, be a shame if it cored for this -Dianora */
    if(sptr->user)
      {
        ircsprintf(message,"KNOCK: %s (%s [%s@%s] has asked for an invite)",
                   chptr->chname, sptr->name, sptr->username, sptr->host);
        sendto_channel_type_notice(cptr, chptr, MODE_CHANOP, message);
      }
  }
  return 0;
}
