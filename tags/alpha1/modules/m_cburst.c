/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_cburst.c
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
#include "tools.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "s_serv.h"       /* captab, send_channel_burst */
#include "s_user.h"
#include "send.h"
#include "msg.h"
#include "handlers.h"
#include "parse.h"
#include "modules.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct Message cburst_msgtab = {
  MSG_CBURST, 0, 1, 0, MFLG_SLOW | MFLG_UNREG, 0L,
  {m_unregistered, m_error, ms_cburst, m_error}
};

void
_modinit(void)
{
  mod_add_cmd(&cburst_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&cburst_msgtab);
}

char *_version = "20001122";

/*
** m_cburst
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = nick if present (!nick indicates cjoin)
**      parv[3] = channel key (EVENTUALLY)
*/
/*
 * This function will "burst" the given channel onto
 * the given LL capable server.
 */

int     ms_cburst(struct Client *cptr,
                 struct Client *sptr,
                 int parc,
                 char *parv[])
{
  char *name;
  char *nick;
  char *key;
  struct Channel *chptr;

  if( parc < 2 || *parv[1] == '\0' )
     return 0;

  name = parv[1];

  if( parc > 2 )
    nick = parv[2];
  else
    nick = NULL;

  if( parc > 3 )
    key = parv[3];
  else
    key = "";

#ifdef DEBUGLL
  sendto_realops_flags(FLAGS_ALL, "CBURST called by %s for %s %s %s",
    cptr->name,
    name,
    nick ? nick : "",
    key ? key : "" );
#endif

  if( (chptr = hash_find_channel(name, NullChn)) == NULL)
  {
    if((!nick) || (nick && *nick!='!'))
    {
      chptr = get_channel(sptr, name, CREATE);
      chptr->channelts = (time_t)(-1); /* ! highest possible TS so its always-                                          * over-ruled
                                        */
      chptr->users_last = CurrentTime;
    }
    else if(nick && *nick=='!')
    {
      sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                 me.name, nick+1, name);
      return 0;
    }
  }

  if(IsCapable(cptr,CAP_LL))
    {
      burst_channel(cptr,chptr);

      if(nick)
	sendto_one(cptr,":%s LLJOIN %s %s %s", me.name, name,
                   nick, key);
    }
  else
    {
      sendto_realops_flags(FLAGS_ALL,
		   "*** CBURST request received from non LL capable server! [%s]",
			   cptr->name);
    }

  return 0;
}
