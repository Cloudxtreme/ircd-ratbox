/************************************************************************
 *   IRC - Internet Relay Chat, src/m_hash.c
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
#include "s_conf.h"     /* iphash_stats */
#include "send.h"

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

static void report_hash_stats(struct Client* client, const char* name, 
                              const struct HashStats* stats)
{
  int i;
  sendto_one(client, "NOTICE %s :Table Size: %d Buckets Used: %d", 
             name, stats->table_size, stats->buckets_used);
  sendto_one(client, "NOTICE %s :Longest Chain: %d Entries in Table: %d",
             name, stats->longest_chain, stats->entries);
  
  for (i = 0; i < 9; ++i)
    sendto_one(client, "NOTICE %s :Buckets with %d links : %d",
               name, i + 1, stats->link_counts[i]);
  if (0 < stats->link_counts[9])
    sendto_one(client, "NOTICE %s :Buckets with 10 or more links : %d",
               name, stats->link_counts[9]);
}

/*
 * m_hash - report hash table statistics
 *
 * NOTE: this command is not supposed to be an offical part of the ircd
 *       protocol.  It is simply here to help debug and to monitor the
 *       performance of the hash functions and table, enabling a better
 *       algorithm to be sought if this one becomes troublesome.
 *       -avalon
 *
 */
int m_hash(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct HashStats stats;

  if (!MyClient(sptr) || !IsOper(sptr)) {
    sendto_one(sptr, form_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }

  if (parc < 2) {
    sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "HASH");
    return 0;
  }
  if (0 == irccmp(parv[1], "iphash")) {
    iphash_stats(cptr, sptr, parc, parv, -1);
    return 0;
  }

  switch (*parv[1]) {
  case 'V':     /* Verify and report channel hash table stats */
    {
      struct Channel*  chan;
      int              channel_count = 0;
      int              missing_count = 0;
      for (chan = GlobalChannelList; chan; chan = chan->nextch) {
        ++channel_count;
        if (hash_find_channel(chan->chname, 0) != chan) {
          sendto_one(sptr, "NOTICE %s :Can't find channel %s in hash table",
                     parv[0], chan->chname);
          ++missing_count;
        }
      }
      sendto_one(sptr, "NOTICE %s :Channels: %d Missing Channels: %d",
                 parv[0], channel_count, missing_count);
      hash_get_channel_stats(&stats);
      report_hash_stats(sptr, parv[0], &stats);
    }
    break;
  case 'v':     /* verify and report client hash table stats */
    {
      struct Client* client;
      int            client_count  = 0;
      int            missing_count = 0;
        
      for (client = GlobalClientList; client; client = client->next) {
        ++client_count;
        if (hash_find_client(client->name, 0) != client) {
          sendto_one(sptr, "NOTICE %s :Can't find client %s in hash table",
                     parv[0], client->name);
          ++missing_count;
        }
      }
      sendto_one(sptr,"NOTICE %s :Clients: %d Missing Clients: %d",
                 parv[0], client_count, missing_count);
      hash_get_client_stats(&stats);
      report_hash_stats(sptr, parv[0], &stats);
    }
    break;
  case 'P':     /* Report channel hash table stats */
    sendto_one(sptr,"NOTICE %s :Channel Hash Table", parv[0]);
    hash_get_channel_stats(&stats);
    report_hash_stats(sptr, parv[0], &stats);
    break;
  case 'p':     /* report client hash table stats */
    sendto_one(sptr,"NOTICE %s :Client Hash Table", parv[0]);
    hash_get_client_stats(&stats);
    report_hash_stats(sptr, parv[0], &stats);
    break;
  case 'R':     /* Rebuild channel hash table */
    sendto_one(sptr,"NOTICE %s :Rehashing Channel List.", parv[0]);
    hash_rebuild_channel();
    break;
  case 'r':     /* rebuild client hash table */
    sendto_one(sptr,"NOTICE %s :Rehashing Client List.", parv[0]);
    hash_rebuild_client();
    break;
  default:
    break;
  }
  return 0;
}


