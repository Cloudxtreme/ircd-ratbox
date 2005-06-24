/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_knock.c
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
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "vchannel.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <string.h>

struct Channel *parse_knock_args(struct Client *,
                                 struct Client *,
                                 int, char **);
void send_knock(struct Client *, struct Client *,
                struct Channel *, char *);

struct Message knock_msgtab = {
  MSG_KNOCK, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_knock, m_ignore, m_knock}
};

void
_modinit(void)
{
  mod_add_cmd(&knock_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&knock_msgtab);
}

char *_version = "20001122";

/* m_knock
**    parv[0] = sender prefix
**    parv[1] = channel
**    parv[2] = 'key' (for vchan)
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

  chptr = parse_knock_args(cptr, sptr, parc, parv);
  
  if (!chptr)
    return 0;

  /* 
   * Flood control redone to allow one KNOCK per knock_delay seconds on each
   * channel.
   * 
   * Unfortunately, because the local server handles KNOCK instead of
   * passing it on, this can be bypassed by KNOCKing from multiple servers.
   * It's still better than allowing any number of clones to flood a channel
   * from anywhere, IMO.
   *
   * -davidt
   */

  if((chptr->last_knock + 30) > CurrentTime)
    {
      sendto_one(sptr, ":%s NOTICE %s :*** Notice -- Wait %d seconds before another knock to %s",
                 me.name, sptr->name,
                 30 - (CurrentTime -
                       chptr->last_knock),
                 parv[1]);
      return 0;
    }

  send_knock(cptr, sptr, chptr, parv[1]);

  return 0;
}

/*
 * parse_knock_args
 *
 * input        - pointer to physical struct cptr
 *              - pointer to source struct sptr
 *              - number of args
 *              - pointer to array of args
 *              
 * output       - returns pointer to channel specified by name/key
 * 
 * side effects - sets name to name of base channel
 *                or sends failure message to sptr
 */

struct Channel *parse_knock_args(struct Client *cptr,
                                        struct Client *sptr,
                                        int parc, char *parv[])
{
  /* We will cut at the first comma reached, however we will not *
   * process anything afterwards.                                */

  struct Channel      *chptr,*vchan_chptr;
  char *p, *name, *key;

  name = parv[1];
  key = (parc > 2) ? parv[2] : NULL;

  if( (p = strchr(name,',')) )
    *p = '\0';

  if (!IsChannelName(name) || !(chptr = hash_find_channel(name, NullChn)))
    {
      sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL), me.name, parv[0],
                 name);
      return NullChn;
    }

  if (IsVchanTop(chptr))
    {
      /* They specified a vchan basename */
      if(on_sub_vchan(chptr,sptr))
        {
          sendto_one(sptr,":%s NOTICE %s :*** Notice -- You are on channel already!",
                     me.name, sptr->name);
          return NullChn;
        }
      if (key && key[0] == '!')
        {
          /* Make "KNOCK #channel !" work like JOIN */
          if (!key[1])
            {
              show_vchans(cptr, sptr, chptr, "knock");
              return NullChn;
            }

          /* Find a matching vchan */
          if ((vchan_chptr = find_vchan(chptr, key)))
            {
              chptr = vchan_chptr;
            }
          else
            {
              sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
              me.name, parv[0], name);
              return NullChn;
            }
        }
      else
        {
          /* No key specified */
 /* XXX FIXME */
#if 0
          if( (!chptr->members) && (!chptr->next_vchan->next_vchan) )
            {
              chptr = chptr->next_vchan;
            }
          else
            {
              /* There's more than one channel, so give them a list */
              show_vchans(cptr, sptr, chptr, "knock");
              return NullChn;
            }
#endif
        }
    }
  else if (IsVchan(chptr))
    {
      /* Don't allow KNOCK'ing a vchans 'real' name */
      sendto_one(sptr, form_str(ERR_BADCHANNAME), me.name, parv[0],
                 name);
      return NullChn;
    }
  else
    {
      /* Normal channel, just be sure they aren't on it */
      if (IsMember(sptr, chptr))
        {
          sendto_one(sptr,":%s NOTICE %s :*** Notice -- You are on channel already!",
                     me.name, sptr->name);
          return NullChn;
        }
    }

  if(!((chptr->mode.mode & MODE_INVITEONLY) ||
       (*chptr->mode.key) ||
       (chptr->mode.limit && chptr->users >= chptr->mode.limit )
       ))
    {
      sendto_one(sptr,":%s NOTICE %s :*** Notice -- Channel is open!",
                 me.name,
                 sptr->name);
      return NullChn;
    }

  /* don't allow a knock if the user is banned, or the channel is paranoid */
  if ((chptr->mode.mode & MODE_PRIVATE) ||
      (is_banned(chptr,sptr) == CHFL_BAN) )
    {
      sendto_one(sptr, form_str(ERR_CANNOTSENDTOCHAN), me.name, parv[0],
                 name);
      return NullChn;
    }

  return chptr;
}

/*
 * send_knock
 *
 * input        - pointer to physical struct cptr
 *              - pointer to source struct sptr
 *              - pointer to channel struct chptr
 *              - pointer to base channel name
 * output       -
 * side effects -
 */

void send_knock(struct Client *cptr, struct Client *sptr,
                       struct Channel *chptr, char *name)
{
  char message[NICKLEN*2+CHANNELLEN+USERLEN+HOSTLEN+30];

  chptr->last_knock = CurrentTime;

  sendto_one(sptr, ":%s NOTICE %s :*** Notice -- Your KNOCK has been delivered",
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

  /* bit of paranoid, be a shame if it cored for this -Dianora */
  if(sptr->user)
    {
      ircsprintf(message,"KNOCK: %s (%s [%s@%s] has asked for an invite)",
                 name, sptr->name, sptr->username, sptr->host);

      /* XXX needs vchan support */
      sendto_channel_local(ONLY_CHANOPS,
			   chptr,
			   ":%s!%s@%s NOTICE %s :%s",
			   sptr->name,
			   sptr->username,
			   sptr->host,
			   chptr->chname,
			   message);

      /* XXX needs remote send or CAP_KNOCK or something */
    }

  return;
}

