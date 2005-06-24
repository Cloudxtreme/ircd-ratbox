/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_hash.c
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
#include "s_conf.h"     /* iphash_stats */
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

int mo_hash(struct Client *, struct Client *, int, char **);
void _modinit(void);
void _moddeinit(void);

struct Message hash_msgtab = {
  "HASH", 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, mo_hash, mo_hash}
};

void
_modinit(void)
{
  mod_add_cmd(&hash_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&hash_msgtab);
}

char *_version = "20001122";

/*
 * This function has been gutted for hybrid-7 for now 
 */

/*
 * mo_hash - report hash table statistics
 *
 * NOTE: this command is not supposed to be an offical part of the ircd
 *       protocol.  It is simply here to help debug and to monitor the
 *       performance of the hash functions and table, enabling a better
 *       algorithm to be sought if this one becomes troublesome.
 *       -avalon
 *
 */
int mo_hash(struct Client* cptr, struct Client* sptr,
	    int parc, char* parv[])
{
  struct Channel*  chan;
  struct Client*   client;
  int              client_count  = 0;
  int              missing_count = 0;
  int              channel_count = 0;

  switch (*parv[1])
    {
    case 'V':     /* Verify and report channel hash table stats */
      for (chan = GlobalChannelList; chan; chan = chan->nextch)
	{
	  ++channel_count;
	  if (hash_find_channel(chan->chname, 0) != chan)
	    {
	      sendto_one(sptr,
			 "NOTICE %s :Can't find channel %s in hash table",
			 parv[0], chan->chname);
	      ++missing_count;
	    }
	}
      sendto_one(sptr, "NOTICE %s :Channels: %d Missing Channels: %d",
                 parv[0], channel_count, missing_count);
      break;

  case 'v':     /* verify and report client hash table stats */
    for (client = GlobalClientList; client; client = client->next)
      {
	++client_count;
	if (hash_find_client(client->name, 0) != client)
	  {
	    sendto_one(sptr, "NOTICE %s :Can't find client %s in hash table",
		       parv[0], client->name);
	    ++missing_count;
	  }
      }
    sendto_one(sptr,"NOTICE %s :Clients: %d Missing Clients: %d",
	       parv[0], client_count, missing_count);
    break;

  default:
    break;
  }
  return 0;
}


