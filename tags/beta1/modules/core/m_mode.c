/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_mode.c
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
#include "s_user.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int m_mode(struct Client*, struct Client*, int, char**);

struct Message mode_msgtab = {
  "MODE", 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_mode, m_mode, m_mode}
};

void
_modinit(void)
{
  mod_add_cmd(&mode_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&mode_msgtab);
}


char *_version = "20001122";

/*
 * m_mode - MODE command handler
 * parv[0] - sender
 * parv[1] - channel
 */
static int m_mode(struct Client *cptr, struct Client *sptr,
              int parc, char *parv[])
{
  struct Channel* chptr=NULL;
  struct Channel* vchan;
  struct Channel* root;
  static char     modebuf[MODEBUFLEN];
  static char     parabuf[MODEBUFLEN];

  /* Now, try to find the channel in question */
  if( !IsChanPrefix(parv[1][0]) )
    {
      /* if here, it has to be a non-channel name */
      return user_mode(cptr, sptr, parc, parv);
    }

  if (!check_channel_name(parv[1]))
    { 
      sendto_one(sptr, form_str(ERR_BADCHANNAME),
		 me.name, parv[0], (unsigned char *)parv[1]);
      return 0;
    }
	  
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
      /* only send a mode upstream if a local client sent this request
       * -davidt
       */
      if ( MyClient(sptr) && !ServerInfo.hub && uplink &&
	   IsCapable(uplink, CAP_LL))
	{
	  /* cache the channel if it exists on uplink
	   * If the channel as seen by the uplink, has vchans,
	   * the uplink will have to SJOIN all of those.
	   */
	  sendto_one(uplink, ":%s CBURST %s",
                     me.name, parv[1]);
	  
	  sendto_one(uplink, ":%s MODE %s %s",
		     sptr->name, parv[1], (parv[2] ? parv[2] : ""));
	  return 0;
	}
      else
	{
	  sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
		     me.name, parv[0], parv[1]);
	  return 0;
	}
    }
  
  /* Now known the channel exists */

  root = chptr;

  if (HasVchans(chptr) || IsVchan(chptr))
  {
    if(HasVchans(chptr))
    {
      if ((vchan = map_vchan(chptr,sptr)) != NULL)
        chptr = vchan; /* root = chptr, chptr = vchan */

      /* XXX - else? the user isn't on any vchan, so we
       *       end up giving them the mode of the root
       *       channel.  MODE #vchan !nick ? (ugh)
       */
    }
    else
    {
      if ((vchan = map_bchan(chptr,sptr)) != NULL)
        root = vchan;  /* root = vchan, chptr = chptr */

      /* XXX - else? the user isn't on any vchan,
       *       but they asked for MODE ##vchan_12345
       *       we send MODE #vchan
       */
    }
  }

  if(parc < 3)
    {
      channel_modes(chptr, sptr, modebuf, parabuf);
      sendto_one(sptr, form_str(RPL_CHANNELMODEIS),
		 me.name, parv[0], parv[1],
		 modebuf, parabuf);
      
      /* Let opers see the "true" TS everyone else see's
       * the top root chan TS
       */
      if (!IsOper(sptr))
	sendto_one(sptr, form_str(RPL_CREATIONTIME),
		   me.name, parv[0],
		   parv[1], root->channelts);
      else
	sendto_one(sptr, form_str(RPL_CREATIONTIME),
		   me.name, parv[0],
		   parv[1], chptr->channelts);
    }
  else
    set_channel_mode(cptr, sptr, chptr, parc - 2, parv + 2, 
                     root->chname);
  
  return 0;
}




