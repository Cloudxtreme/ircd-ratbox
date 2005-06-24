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

/* XXX LazyLinks */
static void m_knock(struct Client*, struct Client*, int, char**);

static struct Channel *parse_knock_args(struct Client *,
                                        struct Client *,
                                        int, char **);

static void send_knock(struct Client *, struct Client *,
                       struct Channel *, char *);

struct Message knock_msgtab = {
  "KNOCK", 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_knock, m_ignore, m_knock}
};
#ifndef STATIC_MODULES

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

char *_version = "20010105";
#endif
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

static void m_knock(struct Client *client_p,
                   struct Client *source_p,
                   int parc,
                   char *parv[])
{
  struct Channel      *chptr;

  if (ConfigChannel.use_knock == 0)
    {
      sendto_one(source_p, ":%s NOTICE %s :*** KNOCK disabled",
		 me.name, source_p->name);
      return;
    }
	

  chptr = parse_knock_args(client_p, source_p, parc, parv);
  
  if (!chptr)
    return;

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

  if((chptr->last_knock + ConfigChannel.knock_delay) > CurrentTime)
    {
      sendto_one(source_p, ":%s NOTICE %s :*** Notice -- Wait %d seconds before another knock to %s",
                 me.name, source_p->name,
                 (int)(ConfigChannel.knock_delay - (CurrentTime - chptr->last_knock)),
                 parv[1]);
      return;
    }

  send_knock(client_p, source_p, chptr, parv[1]);
}

/*
 * parse_knock_args
 *
 * input        - pointer to physical struct client_p
 *              - pointer to source struct source_p
 *              - number of args
 *              - pointer to array of args
 *              
 * output       - returns pointer to channel specified by name/key
 * 
 * side effects - sets name to name of base channel
 *                or sends failure message to source_p
 */

static struct Channel *parse_knock_args(struct Client *client_p,
                                        struct Client *source_p,
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
      sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL), me.name, parv[0],
                 name);
      return NullChn;
    }

  if (IsVchanTop(chptr))
    {
      /* They specified a vchan basename */
      if(on_sub_vchan(chptr,source_p))
        {
          sendto_one(source_p,":%s NOTICE %s :*** Notice -- You are on channel already!",
                     me.name, source_p->name);
          return NullChn;
        }
      if (key && key[0] == '!')
        {
          /* Make "KNOCK #channel !" work like JOIN */
          if (!key[1])
            {
              show_vchans(client_p, source_p, chptr, "knock");
              return NullChn;
            }

          /* Find a matching vchan */
          if ((vchan_chptr = find_vchan(chptr, key)))
            {
              chptr = vchan_chptr;
            }
          else
            {
              sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
              me.name, parv[0], name);
              return NullChn;
            }
        }
      else
        {
          /* No key specified */
          show_vchans(client_p, source_p, chptr, "knock");
          return NullChn;
        }
    }
  else if (IsVchan(chptr))
    {
      /* Don't allow KNOCK'ing a vchans 'real' name */
      sendto_one(source_p, form_str(ERR_BADCHANNAME), me.name, parv[0],
                 name);
      return NullChn;
    }
  else
    {
      /* Normal channel, just be sure they aren't on it */
      if (IsMember(source_p, chptr))
        {
          sendto_one(source_p,":%s NOTICE %s :*** Notice -- You are on channel already!",
                     me.name, source_p->name);
          return NullChn;
        }
    }

  if(!((chptr->mode.mode & MODE_INVITEONLY) ||
       (*chptr->mode.key) ||
       (chptr->mode.limit && chptr->users >= chptr->mode.limit )
       ))
    {
      sendto_one(source_p,":%s NOTICE %s :*** Notice -- Channel is open!",
                 me.name,
                 source_p->name);
      return NullChn;
    }

  /* don't allow a knock if the user is banned, or the channel is paranoid */
  if ((chptr->mode.mode & MODE_PRIVATE) ||
      (is_banned(chptr,source_p) == CHFL_BAN) )
    {
      sendto_one(source_p, form_str(ERR_CANNOTSENDTOCHAN), me.name, parv[0],
                 name);
      return NullChn;
    }

  return chptr;
}

/*
 * send_knock
 *
 * input        - pointer to physical struct client_p
 *              - pointer to source struct source_p
 *              - pointer to channel struct chptr
 *              - pointer to base channel name
 * output       -
 * side effects -
 */

static void send_knock(struct Client *client_p, struct Client *source_p,
                       struct Channel *chptr, char *name)
{
  char message[NICKLEN*2+CHANNELLEN+USERLEN+HOSTLEN+30];

  chptr->last_knock = CurrentTime;

  sendto_one(source_p, ":%s NOTICE %s :*** Notice -- Your KNOCK has been delivered",
             me.name, source_p->name);

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
  if(source_p->user)
    {
      ircsprintf(message,"KNOCK: %s (%s [%s@%s] has asked for an invite)",
                 name, source_p->name, source_p->username, source_p->host);

      sendto_channel_local(ONLY_CHANOPS_HALFOPS,
                          chptr,
                          ":%s!%s@%s NOTICE %s :%s",
                          source_p->name,
                          source_p->username,
                          source_p->host,
                          name,
                          message);

      /* XXX needs remote send or CAP_KNOCK or something */
    }

  return;
}
