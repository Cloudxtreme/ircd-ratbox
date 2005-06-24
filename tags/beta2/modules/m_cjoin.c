/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_cjoin.c
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

static void m_cjoin(struct Client*, struct Client*, int, char**);

struct Message cjoin_msgtab = {
  "CJOIN", 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_cjoin, m_error, m_cjoin}
};


void
_modinit(void)
{
  mod_add_cmd(&cjoin_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&cjoin_msgtab);
}

char *_version = "20001122";

/*
** m_cjoin
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = channel password (key)
*/
static void m_cjoin(struct Client *cptr,
                   struct Client *sptr,
                   int parc,
                   char *parv[])
{
  static char   jbuf[BUFSIZE];
  struct Channel *chptr = NULL;
  struct Channel *vchan_chptr = NULL;
  struct Channel *root_vchan = NULL;
  char  *name;
  char  *p = NULL;

  if (!(sptr->user))
    {
      /* something is *fucked* - bail */
      return;
    }

  if (ConfigFileEntry.vchans_oper_only && !IsOper(sptr))
    {
      sendto_one(sptr, form_str(ERR_NOPRIVILEGES),
                 me.name, parv[0]);
      return;
    }

  if (*parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "CJOIN");
      return;
    }


  /* Ok, only allowed to CJOIN already existing channels
   * so first part simply verifies the "root" channel exists first
   */

  *jbuf = '\0';

  name = parv[1];
  if ( (p = strchr(name,',')) )
    *p = '\0';

  if (!check_channel_name(name))
    {
      sendto_one(sptr, form_str(ERR_BADCHANNAME),
		 me.name, parv[0], (unsigned char*) name);
      return;
    }

  if (*name == '&')
    {
      sendto_one(sptr, form_str(ERR_BADCHANNAME),
		 me.name, parv[0], (unsigned char*) name);
      return;
    }

  if (!IsChannelName(name))
    {
      sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
		 me.name, parv[0], name);
      return;
    }

  (void)strncpy(jbuf, name, sizeof(jbuf) - 1);

  if( (chptr = hash_find_channel(name, NullChn)) == NULL )
    {
      /* if chptr isn't found locally, it =could= exist
       * on the uplink. So ask.
       */
      if ( !ServerInfo.hub && uplink &&
           IsCapable(uplink, CAP_LL))
        {
          /* cache the channel if it exists on uplink
           * If the channel as seen by the uplink, has vchans,
           * the uplink will have to SJOIN all of those.
           */
          sendto_one(uplink, ":%s CBURST %s !%s",
                     me.name, parv[1], sptr->name);

          return;
        }
      else
        {
          sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                     me.name, sptr->name, name);
        }
      return;
    }

  if (! (vchan_chptr = cjoin_channel(chptr, sptr, name)) )
    return;

  root_vchan = chptr;
  chptr = vchan_chptr;
  
  /*
  **  Complete user entry to the new channel
  */
  add_user_to_channel(chptr, sptr, CHFL_CHANOP);

  sendto_channel_local(ALL_MEMBERS, chptr,
                       ":%s!%s@%s JOIN :%s",
                       sptr->name,
                       sptr->username,
                       sptr->host,
                       root_vchan->chname);

  sendto_channel_remote(chptr, cptr,
			":%s SJOIN %lu %s + :@%s", me.name,
			chptr->channelts,
			chptr->chname,
			sptr->name);

  vchan_chptr->mode.mode |= MODE_TOPICLIMIT;
  vchan_chptr->mode.mode |= MODE_NOPRIVMSGS;

  sendto_channel_local(ALL_MEMBERS,chptr,
                       ":%s MODE %s +nt",
                       me.name, root_vchan->chname);

  sendto_channel_remote(vchan_chptr, sptr, 
			":%s MODE %s +nt",
			me.name,
			vchan_chptr->chname);

  del_invite(vchan_chptr, sptr);
  (void)channel_member_names(sptr, vchan_chptr, root_vchan->chname);
}
