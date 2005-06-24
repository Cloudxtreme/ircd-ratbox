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
#include "channel_mode.h"
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
#include "packet.h"

static void m_mode(struct Client*, struct Client*, int, char**);

struct Message mode_msgtab = {
  "MODE", 0, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_mode, m_mode, m_mode}
};
#ifndef STATIC_MODULES

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
#endif
/*
 * m_mode - MODE command handler
 * parv[0] - sender
 * parv[1] - channel
 */
static void m_mode(struct Client *client_p, struct Client *source_p,
              int parc, char *parv[])
{
  struct Channel* chptr=NULL;
  struct Channel* vchan;
  struct Channel* root;
  static char     modebuf[MODEBUFLEN];
  static char     parabuf[MODEBUFLEN];
  int n = 2;
  
  /* Now, try to find the channel in question */
  if( !IsChanPrefix(parv[1][0]) )
    {
      /* if here, it has to be a non-channel name */
      user_mode(client_p, source_p, parc, parv);
      return;
    }
  /* Finish the flood grace period... */
  SetFloodDone(source_p);
  if (!check_channel_name(parv[1]))
    { 
      sendto_one(source_p, form_str(ERR_BADCHANNAME),
		 me.name, parv[0], (unsigned char *)parv[1]);
      return;
    }
	  
  chptr = hash_find_channel(parv[1]);

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
      if ( MyClient(source_p) && !ServerInfo.hub && uplink &&
	   IsCapable(uplink, CAP_LL))
	{
#if 0
	  /* cache the channel if it exists on uplink
	   * If the channel as seen by the uplink, has vchans,
	   * the uplink will have to SJOIN all of those.
	   */
	  /* Lets not for now -db */

	  sendto_one(uplink, ":%s CBURST %s",
                     me.name, parv[1]);
	  
#endif
	  sendto_one(uplink, ":%s MODE %s %s",
		     source_p->name, parv[1], (parv[2] ? parv[2] : ""));
	  return;
	}
      else
	{
	  sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
		     me.name, parv[0], parv[1]);
	  return;
	}
    }
  
  /* Now known the channel exists */

  root = chptr;

  if ((parc > 2) && parv[2][0] == '!')
    {
     struct Client *target_p;
     if (!(target_p = find_client(++parv[2])))
       {
        sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL), me.name,
                   parv[0], root->chname);
        return;
       }
     if (!(chptr = map_vchan(root, target_p)))
       {
        sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL), me.name,
                   parv[0], root->chname);
        return;
       }
     n++;
    }

  else
    {
      if (HasVchans(chptr))
        {
          if ((vchan = map_vchan(chptr,source_p)) != NULL)
            chptr = vchan; /* root = chptr, chptr = vchan */

          /* XXX - else? the user isn't on any vchan, so we
           *       end up giving them the mode of the root
           *       channel.  MODE #vchan !nick ? (ugh)
           */
        }
      else if (IsVchan(chptr))
        {
          vchan = find_bchan(chptr);
          root = vchan;  /* root = vchan, chptr = chptr */

          /* XXX - else? the user isn't on any vchan,
           *       but they asked for MODE ##vchan_12345
           *       we send MODE #vchan
           */
        }
    }

  if(parc < n+1)
    {
      channel_modes(chptr, source_p, modebuf, parabuf);
      sendto_one(source_p, form_str(RPL_CHANNELMODEIS),
		 me.name, parv[0], parv[1],
		 modebuf, parabuf);
      
      /* Let opers see the "true" TS everyone else see's
       * the top root chan TS
       */
      if (!IsOper(source_p))
	sendto_one(source_p, form_str(RPL_CREATIONTIME),
		   me.name, parv[0],
		   parv[1], root->channelts);
      else
	sendto_one(source_p, form_str(RPL_CREATIONTIME),
		   me.name, parv[0],
		   parv[1], chptr->channelts);
    }
  else
    set_channel_mode(client_p, source_p, chptr, parc - n, parv + n, 
                     root->chname);
}




