/************************************************************************
 *   IRC - Internet Relay Chat, src/c_cburst.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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
 * $Id$
 */
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "s_serv.h"       /* captab */
#include "s_user.h"
#include "send.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>


/*
** m_cburst
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = nick if present
**      parv[3] = channel key (EVENTUALLY)
*/
/*
 * This function will "burst" the given channel onto
 * the given LL capable server.
 * If the nick is given as well, then I also check ot
 * see if that nick can join the given channel. If
 * the nick can join, a LLJOIN message is sent back to leaf
 * stating the nick can join, otherwise a non join message is sent.
 */

int     ms_cburst(struct Client *cptr,
                 struct Client *sptr,
                 int parc,
                 char *parv[])
{
  char *name;
  char *nick;
  char *key;
  struct Client *acptr;
  struct Channel *chptr;

  if( parc < 2 || *parv[1] == '\0' )
     return 0;

  /* If not a server just ignore it */
  if ( !IsServer(cptr) )
    return 0;

  name = parv[1];
  nick = NULL;

  if( parc > 2 )
    nick = parv[2];

  key = NULL;

  if( parc > 3 )
    key = parv[3];

#ifdef DEBUGLL
  sendto_realops("CBURST called by %s for %s %s %s",
    cptr->name,
    name,
    nick ? nick : "",
    key ? key : "" );
#endif

  if(!(chptr=hash_find_channel(name, NullChn)))
    {
     /* I don't know about this channel here, let leaf deal with it */
     if(nick)
       sendto_one(cptr,":%s LLJOIN %s %s :J",
                        me.name, name, nick);
      return 0;
    }

  if(IsCapable(cptr,CAP_LL))
    {
      /* for version 2 of LazyLinks, also have to send nicks on channel */
#ifndef LLVER1
      struct SLink* l;

      for (l = chptr->members; l; l = l->next)
	{
	  acptr = l->value.cptr;
	  if (acptr->from != cptr)
	    sendnick_TS(cptr, acptr);
	}
#endif
      chptr->lazyLinkChannelExists = cptr->serverMask;
      send_channel_modes(cptr, chptr);
       /* Send the topic */
      sendto_one(cptr, ":%s TOPIC %s :%s",
         chptr->topic_info, chptr->chname, chptr->topic);
    }
  else
    {
      sendto_realops("*** CBURST request received from non LL capable server!");
      return 0;
    }

  /* If client attempting to join on a CBURST request
   * is banned or the channel is +i etc. reject client
   * -Dianora
   */

  if ( !nick )
    return 0;

  if( (acptr = hash_find_client(nick,(struct Client *)NULL)) )
    {
      if( (is_banned(acptr, chptr) == CHFL_BAN) )
        {
          sendto_one(cptr,":%s LLJOIN %s %s :B",
                           me.name, name, nick);
          return 0;
	}

      if( (chptr->mode.mode & MODE_INVITEONLY) )
        {
          sendto_one(cptr,":%s LLJOIN %s %s :I",
                          me.name, name, nick);
          return 0;
        }

      if(*chptr->mode.key)
        {
          if(!key)
            {
              sendto_one(cptr,":%s LLJOIN %s %s :K",
                               me.name, name, nick);
              return 0;
	    }

          if(irccmp(chptr->mode.key, key))
            {
              sendto_one(cptr,":%s LLJOIN %s %s :K",
                               me.name, name, nick);
              return 0;
	    }
        }

      if (chptr->mode.limit && chptr->users >= chptr->mode.limit)
        {
          sendto_one(cptr,":%s LLJOIN %s %s :F",
                          me.name, name, nick);
          return 0;
        }

       sendto_one(cptr,":%s LLJOIN %s %s :J",
                        me.name, name, nick);
       return 0;
    }

  /* No client found , no join possible */
  sendto_one(cptr,":%s LLJOIN %s %s :N",
                  me.name, name, nick);
  return 0;
}
