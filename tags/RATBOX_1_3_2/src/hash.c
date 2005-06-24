/*
 *  ircd-ratbox: A slightly useful ircd.
 *  hash.c: Maintains hashtables.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
#include "ircd_defs.h"
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

static unsigned int hash_channel_name(const char *name);


static dlink_list clientTable[U_MAX];
static dlink_list channelTable[CH_MAX];
static dlink_list idTable[U_MAX];
static dlink_list resvTable[R_MAX];
static dlink_list hostTable[HOST_MAX];

/* XXX move channel hash into channel.c or hash channel stuff in channel.c
 * into here eventually -db
 */
extern BlockHeap *channel_heap;


size_t
hash_get_channel_table_size(void)
{
	return sizeof(dlink_list) * CH_MAX;
}

size_t
hash_get_client_table_size(void)
{
	return sizeof(dlink_list) * U_MAX;
}

size_t
hash_get_resv_table_size(void)
{
	return sizeof(dlink_list) * R_MAX;
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

static unsigned int
hash_nick_name(const char *name)
{
	unsigned int h = 0;

	while (*name)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));
	}

	return (h & (U_MAX - 1));
}

/*
 * hash_id
 *
 * IDs are a easy to hash -- they're already evenly distributed,
 * and they are always case sensitive.   -orabidoo
 */
static unsigned int
hash_id(const char *nname)
{
	unsigned int h = 0;

	while (*nname)
	{
		h = (h << 4) - (h + (unsigned char) *nname++);
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
static unsigned int
hash_channel_name(const char *name)
{
	int i = 30;
	unsigned int h = 0;

	while (*name && --i)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));
	}

	return (h & (CH_MAX - 1));
}

static unsigned int
hash_hostname(const char *name)
{
	int i = 30;
	unsigned int h = 0;

	while (*name && --i)
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));

	return (h & (HOST_MAX - 1));
}

/*
 * hash_resv_channel()
 *
 * calculate a hash value on at most the first 30 characters and add
 * it to the resv hash
 */
static unsigned int
hash_resv_channel(const char *name)
{
	int i = 30;
	unsigned int h = 0;

	while (*name && --i)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));
	}

	return (h & (R_MAX - 1));
}

/*
 * clear_client_hash_table
 *
 * Nullify the hashtable and its contents so it is completely empty.
 */
static void
clear_client_hash_table()
{
	memset(clientTable, 0, sizeof(dlink_list) * U_MAX);
}

/*
 * clear_id_hash_table
 *
 * Nullify the hashtable and its contents so it is completely empty.
 */
static void
clear_id_hash_table()
{
	memset(idTable, 0, sizeof(dlink_list) * U_MAX);
}

static void
clear_channel_hash_table(void)
{
	memset(channelTable, 0, sizeof(dlink_list) * CH_MAX);
}

static void
clear_resv_hash_table()
{
	memset(resvTable, 0, sizeof(dlink_list) * R_MAX);
}

void
init_hash(void)
{
	clear_client_hash_table();
	clear_channel_hash_table();
	clear_id_hash_table();
	memset(hostTable, 0, sizeof(dlink_list) * HOST_MAX);
	clear_resv_hash_table();
}

/*
 * add_to_id_hash_table
 */
int
add_to_id_hash_table(char *name, struct Client *client_p)
{
	unsigned int hashv;

	hashv = hash_id(name);
	dlinkAddAlloc(client_p, &idTable[hashv]);

	return 0;
}

/*
 * add_to_client_hash_table
 */
void
add_to_client_hash_table(const char *name, struct Client *client_p)
{
	unsigned int hashv;

	s_assert(name != NULL);
	s_assert(client_p != NULL);
	if(name == NULL || client_p == NULL)
		return;

	hashv = hash_nick_name(name);
	dlinkAddAlloc(client_p, &clientTable[hashv]);

}

void
add_to_hostname_hash_table(const char *hostname, struct Client *client_p)
{
	unsigned int hashv;

	s_assert(hostname != NULL);
	s_assert(client_p != NULL);

	if(hostname == NULL || client_p == NULL)
		return;

	hashv = hash_hostname(hostname);
	dlinkAddAlloc(client_p, &hostTable[hashv]);
}

/*
 * add_to_resv_hash_table
 */
void
add_to_resv_hash_table(const char *name, struct ResvEntry *resv_p)
{
	unsigned int hashv;

	s_assert(name != NULL);
	s_assert(resv_p != NULL);

	if(name == NULL || resv_p == NULL)
		return;

	hashv = hash_resv_channel(name);
	dlinkAddAlloc(resv_p, &resvTable[hashv]);
}


/*
 * del_from_client_hash_table - remove a client/server from the client
 * hash table
 */
void
del_from_id_hash_table(const char *id, struct Client *client_p)
{
	struct Client *target_p;
	unsigned int hashv;
	dlink_node *ptr;
	dlink_node *tempptr;

	s_assert(id != NULL);
	s_assert(client_p != NULL);

	if(id == NULL || client_p == NULL)
		return;

	hashv = hash_id(id);

	DLINK_FOREACH_SAFE(ptr, tempptr, idTable[hashv].head)
	{
		target_p = ptr->data;

		if(target_p == client_p)
		{
			dlinkDestroy(ptr, &idTable[hashv]);

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
del_from_client_hash_table(const char *name, struct Client *client_p)
{
	struct Client *target_p;
	unsigned int hashv;
	dlink_node *ptr;
	dlink_node *tempptr;

	/* this can happen when exiting a client who hasnt properly established
	 * yet --fl
	 */
	if(name == NULL || *name == '\0' || client_p == NULL)
		return;

	hashv = hash_nick_name(name);

	DLINK_FOREACH_SAFE(ptr, tempptr, clientTable[hashv].head)
	{
		target_p = ptr->data;

		if(client_p == target_p)
		{
			dlinkDestroy(ptr, &clientTable[hashv]);

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
del_from_channel_hash_table(const char *name, struct Channel *chptr)
{
	struct Channel *ch2ptr;
	dlink_node *ptr;
	dlink_node *tempptr;
	unsigned int hashv;

	s_assert(name != NULL);
	s_assert(chptr != NULL);

	if(name == NULL || chptr == NULL)
		return;

	hashv = hash_channel_name(name);

	DLINK_FOREACH_SAFE(ptr, tempptr, channelTable[hashv].head)
	{
		ch2ptr = ptr->data;

		if(chptr == ch2ptr)
		{
			dlinkDestroy(ptr, &channelTable[hashv]);

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

	DLINK_FOREACH_SAFE(ptr, tempptr, hostTable[hashv].head)
	{
		target_p = ptr->data;
		if(target_p == client_p)
		{
			dlinkDestroy(ptr, &hostTable[hashv]);

			return;
		}
	}
}

/*
 * del_from_resv_hash_table()
 */
void
del_from_resv_hash_table(const char *name, struct ResvEntry *resv_p)
{
	struct ResvEntry *r2ptr;
	dlink_node *ptr;
	dlink_node *tempptr;
	unsigned int hashv;

	s_assert(name != NULL);
	s_assert(resv_p != NULL);

	if(name == NULL || resv_p == NULL)
		return;

	hashv = hash_resv_channel(name);

	DLINK_FOREACH_SAFE(ptr, tempptr, resvTable[hashv].head)
	{
		r2ptr = ptr->data;

		if(resv_p == r2ptr)
		{
			dlinkDestroy(ptr, &resvTable[hashv]);

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

	if(name == NULL)
		return NULL;

	hashv = hash_id(name);

	DLINK_FOREACH(ptr, idTable[hashv].head)
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
struct Client *
find_client(const char *name)
{
	struct Client *target_p;
	dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(name == NULL)
		return NULL;

	if(*name == '.')	/* it's an ID .. */
		return (find_id(name));

	hashv = hash_nick_name(name);

	DLINK_FOREACH(ptr, clientTable[hashv].head)
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

	return hostTable[hashv].head;
#if 0
	DLINK_FOREACH(ptr, hostTable[hashv].head)
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
static struct Client *
hash_find_masked_server(const char *name)
{
	char buf[HOSTLEN + 1];
	char *p = buf;
	char *s;
	struct Client *server;

	if('*' == *name || '.' == *name)
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
		if((server = find_client(s)))
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
struct Client *
find_server(const char *name)
{
	struct Client *target_p;
	dlink_node *ptr;
	unsigned int hashv;

	if(name == NULL)
		return (NULL);

	hashv = hash_nick_name(name);

	DLINK_FOREACH(ptr, clientTable[hashv].head)
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
struct Channel *
hash_find_channel(const char *name)
{
	struct Channel *chptr;
	dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(name == NULL)
		return NULL;

	hashv = hash_channel_name(name);

	DLINK_FOREACH(ptr, channelTable[hashv].head)
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

	if(BadPtr(chname))
		return NULL;

	len = strlen(chname);
	if(len > CHANNELLEN)
	{
		if(IsServer(client_p))
		{
			sendto_realops_flags(UMODE_DEBUG, L_ALL,
					     "*** Long channel name from %s (%d > %d): %s",
					     client_p->name, len, CHANNELLEN, chname);
		}
		len = CHANNELLEN;
		*(chname + CHANNELLEN) = '\0';
	}

	hashv = hash_channel_name(chname);

	DLINK_FOREACH(ptr, channelTable[hashv].head)
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

	chptr->channelts = CurrentTime;	/* doesn't hurt to set it here */

	dlinkAddAlloc(chptr, &channelTable[hashv]);

	Count.chan++;
	return chptr;
}

/*
 * hash_find_resv()
 */
struct ResvEntry *
hash_find_resv(const char *name)
{
	struct ResvEntry *resv_p;
	dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(name == NULL)
		return NULL;

	hashv = hash_resv_channel(name);

	DLINK_FOREACH(ptr, resvTable[hashv].head)
	{
		resv_p = ptr->data;

		if(irccmp(name, resv_p->name) == 0)
		{
			return resv_p;
		}
	}

	return NULL;
}

