/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  hash.c: Maintains hashtables.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"

#include "tools.h"
#include "s_conf.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "resv.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_debug.h"
#include "fdlist.h"
#include "fileio.h"
#include "memory.h"
#include "msg.h"
#include "handlers.h"

/* New hash code */
/*
 * Contributed by James L. Davis
 */

static unsigned int hash_channel_name(const char* name);

#ifdef FL_DEBUG
struct Message hash_msgtab = {
  "HASH", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_ignore, m_ignore, m_ignore, mo_hash}
};
#endif

static struct HashEntry clientTable[U_MAX];
static struct HashEntry channelTable[CH_MAX];
static struct HashEntry idTable[U_MAX];
static struct HashEntry resvTable[R_MAX];
static struct HashEntry hostTable[HOST_MAX];

/* XXX move channel hash into channel.c or hash channel stuff in channel.c
 * into here eventually -db
 */
extern BlockHeap *channel_heap;

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

size_t hash_get_resv_table_size(void)
{
  return sizeof(struct HashEntry) * R_MAX;
}

/*
 * look in whowas.c for the missing ...[WW_MAX]; entry
 */

/*
 * Hashing.
 *
 *   The server uses a chained hash table to provide quick and efficient
 * hash table maintenance (providing the hash function works evenly over
 * the input range).  The hash table is thus not susceptible to problems
 * of filling all the buckets or the need to rehash.
 *    It is expected that the hash table would look something like this
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
  int i = 30;
  unsigned int h = 0;

  while (*name && --i)
    {
      h = (h << 4) - (h + (unsigned char)ToLower(*name++));
    }

  return (h & (CH_MAX - 1));
}

static unsigned int
hash_hostname(const char *name)
{
  int i = 30;
  unsigned int h = 0;

  while(*name && --i)
    h = (h << 4) - (h + (unsigned char)ToLower(*name++));

  return (h & (HOST_MAX - 1));
}

/*
 * hash_resv_channel()
 *
 * calculate a hash value on at most the first 30 characters and add
 * it to the resv hash
 */
static unsigned
int hash_resv_channel(const char *name)
{
  int i = 30;
  unsigned int h = 0;

  while (*name && --i)
  {
    h = (h << 4) - (h + (unsigned char)ToLower(*name++));
  }

  return (h & (R_MAX -1));
}

/*
 * clear_client_hash_table
 *
 * Nullify the hashtable and its contents so it is completely empty.
 */
static void clear_client_hash_table()
{
  memset(clientTable, 0, sizeof(struct HashEntry) * U_MAX);
}

/*
 * clear_id_hash_table
 *
 * Nullify the hashtable and its contents so it is completely empty.
 */
static void clear_id_hash_table()
{
  memset(idTable, 0, sizeof(struct HashEntry) * U_MAX);
}

static void
clear_channel_hash_table(void)
{
  memset(channelTable, 0, sizeof(struct HashEntry) * CH_MAX);
}

void
clear_hostname_hash_table(void)
{
  memset(hostTable, 0, sizeof(struct HashEntry) * HOST_MAX);
}

static void
clear_resv_hash_table()
{
  memset(resvTable, 0, sizeof(struct HashEntry) * R_MAX);
}

void 
init_hash(void)
{
  clear_client_hash_table();
  clear_channel_hash_table();
  clear_id_hash_table();
  clear_hostname_hash_table();
  clear_resv_hash_table();
}

/*
 * add_to_id_hash_table
 */
int
add_to_id_hash_table(char *name, struct Client *client_p)
{
  unsigned int     hashv;

  hashv = hash_id(name);
  dlinkAddAlloc(client_p, &idTable[hashv].list);

  idTable[hashv].links++;
  idTable[hashv].hits++;
  return 0;
}

/*
 * add_to_client_hash_table
 */
void 
add_to_client_hash_table(const char* name, struct Client* client_p)
{
  unsigned int hashv;

  assert(name != NULL);
  assert(client_p != NULL);
  if(name == NULL || client_p == NULL)
    return;
  
  hashv = hash_nick_name(name);
  dlinkAddAlloc(client_p, &clientTable[hashv].list);

  ++clientTable[hashv].links;
  ++clientTable[hashv].hits;
}

void
add_to_hostname_hash_table(const char *hostname, struct Client *client_p)
{
  unsigned int hashv;

  assert(hostname != NULL);
  assert(client_p != NULL);

  if(hostname == NULL || client_p == NULL)
    return;

  hashv = hash_hostname(hostname);
  dlinkAddAlloc(client_p, &hostTable[hashv].list);
  hostTable[hashv].links++;
  hostTable[hashv].hits++;
}

/*
 * add_to_resv_hash_table
 */
void
add_to_resv_hash_table(const char *name, struct ResvChannel *resv_p)
{
  unsigned int hashv;

  assert(name != NULL);
  assert(resv_p != NULL);
  
  if(name == NULL || resv_p == NULL)
    return;

  hashv = hash_resv_channel(name);
  dlinkAddAlloc(resv_p, &resvTable[hashv].list);
  ++resvTable[hashv].links;
  ++resvTable[hashv].hits;
}


/*
 * del_from_client_hash_table - remove a client/server from the client
 * hash table
 */
void
del_from_id_hash_table(const char* id, struct Client* client_p)
{
  struct Client *target_p;
  unsigned int   hashv;
  dlink_node *ptr;
  dlink_node *tempptr;

  assert(id != NULL);
  assert(client_p != NULL);
  
  if(id == NULL || client_p == NULL)
    return;

  hashv = hash_id(id);

  DLINK_FOREACH_SAFE(ptr, tempptr, idTable[hashv].list.head)
  {
    target_p = ptr->data;

    if (target_p == client_p)
    {
      dlinkDestroy(ptr, &idTable[hashv].list);

      assert(idTable[hashv].links > 0);
      if (idTable[hashv].links > 0)
        --idTable[hashv].links;
      return;
    }
  }

  Debug((DEBUG_ERROR, "%#x !in tab %s[%s] %#x %#x %#x %d %d %#x",
         client_p, client_p->name, client_p->from ? client_p->from->host : "??host",
         client_p->from, client_p->next, client_p->prev, client_p->localClient->fd, 
         client_p->status, client_p->user));
}

/*
 * del_from_client_hash_table - remove a client/server from the client
 * hash table
 */
void
del_from_client_hash_table(const char* name, struct Client* client_p)
{
  struct Client *target_p;
  unsigned int   hashv;
  dlink_node *ptr;
  dlink_node *tempptr;

  /* this can happen when exiting a client who hasnt properly established
   * yet --fl
   */
  if(name == NULL || *name == '\0' || client_p == NULL)
    return;

  hashv = hash_nick_name(name);
  
  DLINK_FOREACH_SAFE(ptr, tempptr, clientTable[hashv].list.head)
  {
    target_p = ptr->data;

    if(client_p == target_p)
    {
      dlinkDestroy(ptr, &clientTable[hashv].list);
      
      assert(clientTable[hashv].links > 0);
      if (clientTable[hashv].links > 0)
        --clientTable[hashv].links;
      return;
    }
  }

  Debug((DEBUG_ERROR, "%#x !in tab %s[%s] %#x %#x %#x %d %d %#x",
         client_p, client_p->name, client_p->from ? client_p->from->host : "??host",
         client_p->from, client_p->next, client_p->prev, client_p->localClient->fd, 
         client_p->status, client_p->user));
}

/*
 * del_from_channel_hash_table
 */
void 
del_from_channel_hash_table(const char* name, struct Channel* chptr)
{
  struct Channel *ch2ptr;
  dlink_node *ptr;
  dlink_node *tempptr;
  unsigned int    hashv;

  assert(name != NULL);
  assert(chptr != NULL);

  if(name == NULL || chptr == NULL)
    return;
    
  hashv = hash_channel_name(name);

  DLINK_FOREACH_SAFE(ptr, tempptr, channelTable[hashv].list.head)
  {
    ch2ptr = ptr->data;

    if(chptr == ch2ptr)
    {
      dlinkDestroy(ptr, &channelTable[hashv].list);

      assert(channelTable[hashv].links > 0);
      if (channelTable[hashv].links > 0)
        --channelTable[hashv].links;
      return;
    }
  }
}

void
del_from_hostname_hash_table(const char *hostname, struct Client *client_p)
{
  struct Client *target_p;
  dlink_node *ptr;
  dlink_node *tempptr;
  unsigned int hashv;

  if(hostname == NULL || client_p == NULL)
    return;

  hashv = hash_hostname(hostname);

  DLINK_FOREACH_SAFE(ptr, tempptr, hostTable[hashv].list.head)
  {
    target_p = ptr->data;
    if(target_p == client_p)
    {
      dlinkDestroy(ptr, &hostTable[hashv].list);

      if(hostTable[hashv].links > 0)
        hostTable[hashv].links--;

      return;
    }
  }
}
  
/*
 * del_from_resv_hash_table()
 */
void 
del_from_resv_hash_table(const char *name, struct ResvChannel *rptr)
{
  struct ResvChannel *r2ptr;
  dlink_node *ptr;
  dlink_node *tempptr;
  unsigned int hashv;

  assert(name != NULL);
  assert(rptr != NULL);

  if(name == NULL || rptr == NULL)
    return;
    
  hashv = hash_resv_channel(name);

  DLINK_FOREACH_SAFE(ptr, tempptr, resvTable[hashv].list.head)
  {
    r2ptr = ptr->data;
    
    if(rptr == r2ptr)
    {
      dlinkDestroy(ptr, &resvTable[hashv].list);

      assert(resvTable[hashv].links > 0);
      --resvTable[hashv].links;

      return;
    }
  }
}  
 
/*
 * find_id
 */
struct Client *
find_id(const char *name)
{
  struct Client *target_p;
  dlink_node *ptr;
  unsigned int hashv;
	
  if (name == NULL)
    return NULL;

  hashv = hash_id(name);

  DLINK_FOREACH(ptr, idTable[hashv].list.head)
  {
    target_p = ptr->data;

    if(target_p->user && strcmp(name, target_p->user->id) == 0)
    {
      return target_p;
    }
  }

  return NULL;
}

/*
 * find_client
 *
 * inputs	- name of either server or client
 * output	- pointer to client pointer
 * side effects	- none
 */
struct Client* 
find_client(const char* name)
{
  struct Client *target_p;
  dlink_node *ptr;
  unsigned int   hashv;

  assert(name != NULL);
  if(name == NULL)
    return NULL;

  if (*name == '.') /* it's an ID .. */
    return (find_id(name));

  hashv = hash_nick_name(name);

  DLINK_FOREACH(ptr, clientTable[hashv].list.head)
  {
    target_p = ptr->data;
    
    if(irccmp(name, target_p->name) == 0)
    {
      return target_p;
    }
  }

  return NULL;
}

dlink_node *
find_hostname(const char *hostname)
{
  unsigned int hashv;

  if(hostname == NULL)
    return NULL;

  hashv = hash_hostname(hostname);

  return hostTable[hashv].list.head;
#if 0
  DLINK_FOREACH(ptr, hostTable[hashv].list.head)
  {
    target_p = ptr->data;

    if(irccmp(hostname, target_p->host) == 0)
    {
      return target_p;
    }
  }

  return NULL;
#endif
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
static struct Client* 
hash_find_masked_server(const char* name)
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
  strlcpy(buf, name, sizeof(buf));

  while ((s = strchr(p, '.')) != 0)
    {
       *--s = '*';
      /*
       * Dont need to check IsServer() here since nicknames cant
       * have *'s in them anyway.
       */
       if ((server = find_client(s)))
	 return server;
       p = s + 2;
    }
  return 0;
}

/*
 * find_server
 *
 * inputs	- pointer to server name
 * output	- NULL if given name is NULL or
 *		  given server not found
 * side effects	-
 */
struct Client* 
find_server(const char* name)
{
  struct Client *target_p;
  dlink_node *ptr;
  unsigned int hashv;

  if (name == NULL)
    return(NULL);

  hashv = hash_nick_name(name);

  DLINK_FOREACH(ptr, clientTable[hashv].list.head)
  {
    target_p = ptr->data;

    if(IsServer(target_p) || IsMe(target_p))
    {
      if(irccmp(name, target_p->name) == 0)
         return target_p;
    }
  }
  
  return hash_find_masked_server(name);
}

/*
 * hash_find_channel
 * inputs	- pointer to name
 * output	- 
 * side effects	-
 */
struct Channel* 
hash_find_channel(const char* name)
{
  struct Channel *chptr;
  dlink_node *ptr;
  unsigned int hashv;
  
  assert(name != NULL);
  if(name == NULL)
    return NULL;

  hashv = hash_channel_name(name);

  DLINK_FOREACH(ptr, channelTable[hashv].list.head)
  {
    chptr = ptr->data;

    if(irccmp(name, chptr->chname) == 0)
    {
      return chptr;
    }
  }

  return NULL;
}

/*
 * get_or_create_channel
 * inputs       - client pointer
 *              - channel name
 *              - pointer to int flag whether channel was newly created or not
 * output       - returns channel block or NULL if illegal name
 *		- also modifies *isnew
 *
 *  Get Channel block for chname (and allocate a new channel
 *  block, if it didn't exist before).
 */
struct Channel *
get_or_create_channel(struct Client *client_p, char *chname, int *isnew)
{
  struct Channel *chptr;
  dlink_node *ptr;
  unsigned int hashv;
  int len;

  if (BadPtr(chname))
    return NULL;

  len = strlen(chname);
  if (len > CHANNELLEN)
    {
      if (IsServer(client_p))
	{
	  sendto_realops_flags(UMODE_DEBUG, L_ALL,
			       "*** Long channel name from %s (%d > %d): %s",
			       client_p->name,
			       len,
			       CHANNELLEN,
			       chname);
	}
      len = CHANNELLEN;
      *(chname + CHANNELLEN) = '\0';
    }

  hashv = hash_channel_name(chname);

  DLINK_FOREACH(ptr, channelTable[hashv].list.head)
  {
    chptr = ptr->data;

    if(irccmp(chname, chptr->chname) == 0)
    {
      if(isnew != NULL)
        *isnew = 0;
      return chptr;
    }
  }
  
  if(isnew != NULL)
    *isnew = 1;

  chptr = BlockHeapAlloc(channel_heap);
  memset(chptr, 0, sizeof(struct Channel));
  strlcpy(chptr->chname, chname, sizeof(chptr->chname));

  dlinkAdd(chptr, &chptr->node, &global_channel_list);

  chptr->channelts = CurrentTime;     /* doesn't hurt to set it here */

  dlinkAddAlloc(chptr, &channelTable[hashv].list);
  ++channelTable[hashv].links;
  ++channelTable[hashv].hits;

  Count.chan++;
  return chptr;
}

/*
 * hash_find_resv()
 */
struct ResvChannel *
hash_find_resv(const char *name)
{
  struct ResvChannel *rptr;
  dlink_node *ptr;
  unsigned int hashv;

  assert(name != NULL);
  if(name == NULL)
    return NULL;

  hashv = hash_resv_channel(name);

  DLINK_FOREACH(ptr, resvTable[hashv].list.head)
  {
    rptr = ptr->data;

    if(irccmp(name, rptr->name) == 0)
    {
      return rptr;
    }
  }

  return NULL;
} 

#ifdef FL_DEBUG
void
mo_hash(struct Client *source_p, struct Client *client_p, 
        int argc,char *argv[])
{
  int i;
  struct Client *target_p;
  u_long used_count;
  int deepest_link;
  u_long average_link;
  int this_link;
  int node[11];

  for(i = 0; i < 11; i++)
    node[i] = 0;

  deepest_link = used_count = this_link = average_link = 0;

  sendto_one(source_p, ":%s %d %s :Hostname hash statistics",
               me.name, RPL_STATSDEBUG, source_p->name);
  
  for(i = 0; i < HOST_MAX; i++)
  {
    this_link = 0;

    for(target_p = hostTable[i].list; target_p; target_p = target_p->hostnext)
    {
      used_count++;
      this_link++;
    }

    if(this_link > deepest_link)
      deepest_link = this_link;

    if(this_link >= 10)
    {
      int j = 0;
      for(target_p = hostTable[i].list; target_p; target_p = target_p->hostnext) 
      {
        sendto_one(source_p, ":%s %d %s :Node[%d][%d] %s",
                   me.name, RPL_STATSDEBUG, source_p->name, i, j,
		   target_p->host);
	j++;
      }

      this_link = 10;
    }

    node[this_link]++;
  }

  for(i = 1; i < 11; i++)
    average_link += node[i] * i;
    
  sendto_one(source_p, ":%s %d %s :Hash Size: %d - Used %lu %f%% - Free %lu %f%%",
             me.name, RPL_STATSDEBUG, source_p->name, HOST_MAX,
	     used_count, (float)((used_count / HOST_MAX) * 100), 
	     HOST_MAX - used_count, 
	     (float)((float)((float)(HOST_MAX - used_count) / HOST_MAX) * 100));
  
  sendto_one(source_p, ":%s %d %s :Deepest Link: %d - Average  %f",
             me.name, RPL_STATSDEBUG, source_p->name, deepest_link,
	     (float)(average_link / used_count));

  for(i = 0; i < 11; i++)
    sendto_one(source_p, ":%s %d %s :Nodes with %d entries: %d",
               me.name, RPL_STATSDEBUG, source_p->name, i, node[i]);
}
#endif
