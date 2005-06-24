/************************************************************************
 *   IRC - Internet Relay Chat, src/hash.c
 *   Copyright (C) 1991 Darren Reed
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
 *  $Id$
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <assert.h>
#include <fcntl.h>     /* O_RDWR ... */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "tools.h"
#include "s_conf.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_debug.h"
#include "fdlist.h"
#include "fileio.h"
#include "memory.h"

/* New hash code */
/*
 * Contributed by James L. Davis
 */

static unsigned int hash_nick_name(const char* name);
static unsigned int hash_channel_name(const char* name);

#ifdef  DEBUGMODE
static struct HashEntry* clientTable = NULL;
static struct HashEntry* channelTable = NULL;
static struct HashEntry* idTable = NULL;
static int clhits;
static int clmiss;
static int chhits;
static int chmiss;
#else

static struct HashEntry clientTable[U_MAX];
static struct HashEntry channelTable[CH_MAX];
static struct HashEntry idTable[U_MAX];

#endif

struct HashEntry hash_get_channel_block(int i)
{
  return channelTable[i];
}

size_t hash_get_channel_table_size(void)
{
  return sizeof(struct HashEntry) * CH_MAX;
}

size_t hash_get_client_table_size(void)
{
  return sizeof(struct HashEntry) * U_MAX;
}

/*
 *
 * look in whowas.c for the missing ...[WW_MAX]; entry
 */

/*
 * Hashing.
 *
 *   The server uses a chained hash table to provide quick and efficient
 * hash table mantainence (providing the hash function works evenly over
 * the input range).  The hash table is thus not susceptible to problems
 * of filling all the buckets or the need to rehash.
 *    It is expected that the hash table would look somehting like this
 * during use:
 *                   +-----+    +-----+    +-----+   +-----+
 *                ---| 224 |----| 225 |----| 226 |---| 227 |---
 *                   +-----+    +-----+    +-----+   +-----+
 *                      |          |          |
 *                   +-----+    +-----+    +-----+
 *                   |  A  |    |  C  |    |  D  |
 *                   +-----+    +-----+    +-----+
 *                      |
 *                   +-----+
 *                   |  B  |
 *                   +-----+
 *
 * A - GOPbot, B - chang, C - hanuaway, D - *.mu.OZ.AU
 *
 * The order shown above is just one instant of the server. 
 */

static unsigned
int hash_nick_name(const char* name)
{
  unsigned int h = 0;

  while (*name)
    {
      h = (h << 4) - (h + (unsigned char)ToLower(*name++));
    }

  return(h & (U_MAX - 1));
}

/*
 * hash_id
 *
 * IDs are a easy to hash -- they're already evenly distributed,
 * and they are always case sensitive.   -orabidoo
 */
static  unsigned int 
hash_id(const char *nname)
{
	unsigned int h = 0;
	
	while (*nname) {
		h = (h << 4) - (h + (unsigned char)*nname++);
	}

	return (h & (U_MAX - 1));
}
/*
 * hash_channel_name
 *
 * calculate a hash value on at most the first 30 characters of the channel
 * name. Most names are short than this or dissimilar in this range. There
 * is little or no point hashing on a full channel name which maybe 255 chars
 * long.
 */
static unsigned
int hash_channel_name(const char* name)
{
  register int i = 30;
  unsigned int h = 0;

  while (*name && --i)
    {
      h = (h << 4) - (h + (unsigned char)ToLower(*name++));
    }

  return (h & (CH_MAX - 1));
}

/*
 * clear_client_hash_table
 *
 * Nullify the hashtable and its contents so it is completely empty.
 */
static void clear_client_hash_table()
{
#ifdef        DEBUGMODE
  clhits = 0;
  clmiss = 0;
  if(!clientTable)
    clientTable = (struct HashEntry*) MyMalloc(U_MAX * sizeof(struct HashEntry));
#endif
  memset(clientTable, 0, sizeof(struct HashEntry) * U_MAX);
}

/*
 * clear_client_hash_table
 *
 * Nullify the hashtable and its contents so it is completely empty.
 */
static void clear_id_hash_table()
{
#ifdef        DEBUGMODE
  /* XXX -
   * idhits = 0;
   * idmiss = 0;
   * -cosine
   */
  if(!idTable)
    idTable = (struct HashEntry*) MyMalloc(U_MAX * sizeof(struct HashEntry));
#endif
  memset(idTable, 0, sizeof(struct HashEntry) * U_MAX);
}

static void clear_channel_hash_table()
{
#ifdef        DEBUGMODE
  chmiss = 0;
  chhits = 0;
  if (!channelTable)
    channelTable = (struct HashEntry*) MyMalloc(CH_MAX *
                                                sizeof(struct HashEntry));
#endif
  memset(channelTable, 0, sizeof(struct HashEntry) * CH_MAX);
}

void init_hash(void)
{
  clear_client_hash_table();
  clear_channel_hash_table();
  clear_id_hash_table();
}

/*
 * add_to_id_hash_table
 */
int     add_to_id_hash_table(char *name, struct Client *cptr)
{
	unsigned int     hashv;

	hashv = hash_id(name);
	cptr->idhnext = (struct Client *)idTable[hashv].list;
	idTable[hashv].list = (void *)cptr;
	idTable[hashv].links++;
	idTable[hashv].hits++;
	return 0;
}

/*
 * add_to_client_hash_table
 */
void add_to_client_hash_table(const char* name, struct Client* cptr)
{
  unsigned int hashv;
  assert(0 != name);
  assert(0 != cptr);

  hashv = hash_nick_name(name);
  cptr->hnext = (struct Client*) clientTable[hashv].list;
  clientTable[hashv].list = (void*) cptr;
  ++clientTable[hashv].links;
  ++clientTable[hashv].hits;
}

/*
 * add_to_channel_hash_table
 */
void add_to_channel_hash_table(const char* name, struct Channel* chptr)
{
  unsigned int hashv;
  assert(0 != name);
  assert(0 != chptr);

  hashv = hash_channel_name(name);
  chptr->hnextch = (struct Channel*) channelTable[hashv].list;
  channelTable[hashv].list = (void*) chptr;
  ++channelTable[hashv].links;
  ++channelTable[hashv].hits;
}

/*
 * del_from_client_hash_table - remove a client/server from the client
 * hash table
 */
void del_from_id_hash_table(const char* id, struct Client* cptr)
{
  struct Client* tmp;
  struct Client* prev = NULL;
  unsigned int   hashv;

  assert(0 != id);
  assert(0 != cptr);

  hashv = hash_id(id);
  tmp = (struct Client*) idTable[hashv].list;

  for ( ; tmp; tmp = tmp->idhnext)
    {
      if (tmp == cptr)
        {
          if (prev)
            prev->idhnext = tmp->idhnext;
          else
            idTable[hashv].list = (void*) tmp->idhnext;
          tmp->idhnext = NULL;

          assert(idTable[hashv].links > 0);
          if (idTable[hashv].links > 0)
            --idTable[hashv].links;
          return;
        }
      prev = tmp;
    }
  Debug((DEBUG_ERROR, "%#x !in tab %s[%s] %#x %#x %#x %d %d %#x",
         cptr, cptr->name, cptr->from ? cptr->from->host : "??host",
         cptr->from, cptr->next, cptr->prev, cptr->fd, 
         cptr->status, cptr->user));
}

/*
 * del_from_client_hash_table - remove a client/server from the client
 * hash table
 */
void del_from_client_hash_table(const char* name, struct Client* cptr)
{
  struct Client* tmp;
  struct Client* prev = NULL;
  unsigned int   hashv;
  assert(0 != name);
  assert(0 != cptr);

  hashv = hash_nick_name(name);
  tmp = (struct Client*) clientTable[hashv].list;

  for ( ; tmp; tmp = tmp->hnext)
    {
      if (tmp == cptr)
        {
          if (prev)
            prev->hnext = tmp->hnext;
          else
            clientTable[hashv].list = (void*) tmp->hnext;
          tmp->hnext = NULL;

          assert(clientTable[hashv].links > 0);
          if (clientTable[hashv].links > 0)
            --clientTable[hashv].links;
          return;
        }
      prev = tmp;
    }
  Debug((DEBUG_ERROR, "%#x !in tab %s[%s] %#x %#x %#x %d %d %#x",
         cptr, cptr->name, cptr->from ? cptr->from->host : "??host",
         cptr->from, cptr->next, cptr->prev, cptr->fd, 
         cptr->status, cptr->user));
}

/*
 * del_from_channel_hash_table
 */
void del_from_channel_hash_table(const char* name, struct Channel* chptr)
{
  struct Channel* tmp;
  struct Channel* prev = NULL;
  unsigned int    hashv;
  assert(0 != name);
  assert(0 != chptr);

  hashv = hash_channel_name(name);
  tmp = (struct Channel*) channelTable[hashv].list;

  for ( ; tmp; tmp = tmp->hnextch)
    {
      if (tmp == chptr)
        {
          if (prev)
            prev->hnextch = tmp->hnextch;
          else
            channelTable[hashv].list = (void*) tmp->hnextch;
          tmp->hnextch = NULL;

          assert(channelTable[hashv].links > 0);
          if (channelTable[hashv].links > 0)
            --channelTable[hashv].links;
          return;
        }
      prev = tmp;
    }
}

/*
 * hash_find_id
 */
struct Client *
hash_find_id(const char *name, struct Client *cptr)
{
	struct Client *tmp;
	unsigned int   hashv;
	
	if (name == NULL)
		return NULL;

	hashv = hash_id(name);
	tmp = (struct Client *)idTable[hashv].list;

	/*
	 * Got the bucket, now search the chain.
	 */
	for (; tmp; tmp = tmp->idhnext) {
		if (tmp->user && strcmp(name, tmp->user->id) == 0)
		{
			return(tmp);
		}
	}
	
	return (cptr);
}


/*
 * hash_find_client
 */
struct Client* hash_find_client(const char* name, struct Client* cptr)
{
  struct Client* tmp;
  unsigned int   hashv;
  assert(0 != name);

  if (*name == '.') /* it's an ID .. */
	  return hash_find_id(name, cptr);

  hashv = hash_nick_name(name);
  tmp = (struct Client*) clientTable[hashv].list;
  /*
   * Got the bucket, now search the chain.
   */
  for ( ; tmp; tmp = tmp->hnext)
    if (irccmp(name, tmp->name) == 0)
      {
#ifdef        DEBUGMODE
        ++clhits;
#endif
        return tmp;
      }
#ifdef        DEBUGMODE
  ++clmiss;
#endif
  
return cptr;

  /*
   * If the member of the hashtable we found isnt at the top of its
   * chain, put it there.  This builds a most-frequently used order into
   * the chains of the hash table, giving speedier lookups on those nicks
   * which are being used currently.  This same block of code is also
   * used for channels and servers for the same performance reasons.
   *
   * I don't believe it does.. it only wastes CPU, lets try it and
   * see....
   */
}

/*
 * Whats happening in this next loop ? Well, it takes a name like
 * foo.bar.edu and proceeds to earch for *.edu and then *.bar.edu.
 * This is for checking full server names against masks although
 * it isnt often done this way in lieu of using matches().
 *
 * Rewrote to do *.bar.edu first, which is the most likely case,
 * also made const correct
 * --Bleep
 */
static struct Client* hash_find_masked_server(const char* name)
{
  char           buf[HOSTLEN + 1];
  char*          p = buf;
  char*          s;
  struct Client* server;

  if ('*' == *name || '.' == *name)
    return 0;

  /*
   * copy the damn thing and be done with it
   */
  strncpy_irc(buf, name, HOSTLEN);
  buf[HOSTLEN] = '\0';

  while ((s = strchr(p, '.')) != 0)
    {
       *--s = '*';
      /*
       * Dont need to check IsServer() here since nicknames cant
       * have *'s in them anyway.
       */
      if ((server = hash_find_client(s, 0)))
        return server;
      p = s + 2;
    }
  return 0;
}

/*
 * hash_find_server
 */
struct Client* hash_find_server(const char* name)
{
  struct Client* tmp;
  unsigned int   hashv;

  assert(0 != name);
  hashv = hash_nick_name(name);
  tmp = (struct Client*) clientTable[hashv].list;

  for ( ; tmp; tmp = tmp->hnext)
    {
      if (!IsServer(tmp) && !IsMe(tmp))
        continue;
      if (irccmp(name, tmp->name) == 0)
        {
#ifdef        DEBUGMODE
          ++clhits;
#endif
          return tmp;
        }
    }
  
#ifndef DEBUGMODE
  return hash_find_masked_server(name);

#else /* DEBUGMODE */
  if (!(tmp = hash_find_masked_server(name)))
    ++clmiss;
  return tmp;
#endif
}

/*
 * hash_find_channel
 * inputs	- pointer to name
 * 		- pointer to channel
 * output	- 
 * side effects	-
 */
struct Channel* hash_find_channel(const char* name, struct Channel* chptr)
{
  struct Channel*    tmp;
  unsigned int hashv;
  
  assert(0 != name);
  hashv = hash_channel_name(name);
  tmp = (struct Channel*) channelTable[hashv].list;

  for ( ; tmp; tmp = tmp->hnextch)
    if (irccmp(name, tmp->chname) == 0)
      {
#ifdef        DEBUGMODE
        ++chhits;
#endif
        return tmp;
      }
#ifdef        DEBUGMODE
  ++chmiss;
#endif
  return chptr;
}
