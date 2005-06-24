/*
 *  ircd-ratbox: A slightly useful ircd.
 *  watch.c: Messy hash stuff for the watch command
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1991 Darren Reed
 *  Copyright (C) 2000-2003 TR-IRCD Development
 *  Copyright (C) 2004 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  $Id$
 */

#include "stdinc.h"
#include "tools.h"
#include "hash.h"
#include "send.h"
#include "numeric.h"
#include "s_conf.h"
#include "client.h"
#include "watch.h"

static inline unsigned int
watch_hash_nick(const char *name)
{
	return fnv_hash_upper(name, WATCH_BITS);
}

/*
 * Rough figure of the datastructures for notify:
 *
 * NOTIFY HASH      cptr1
 *   |                |- nick1
 * nick1-|- cptr1     |- nick2
 *   |   |- cptr2                cptr3
 *   |   |- cptr3   cptr2          |- nick1
 *   |                |- nick1
 * nick2-|- cptr2     |- nick2
 *       |- cptr1
 *
 * add-to-notify-hash-table:
 * del-from-notify-hash-table:
 * hash-del-notify-list:
 * hash-check-notify:
 * hash-get-notify:
 */

static struct Watch *watchTable[WATCHHASHSIZE];
static BlockHeap *watch_heap;

void
initwatch(void)
{
	memset((char *) watchTable, '\0', sizeof(watchTable));
	watch_heap = BlockHeapCreate(sizeof(struct Watch), WATCH_HEAP_SIZE);
}


int
hash_check_watch(struct Client *client_p, int reply)
{
	int hashv;
	struct Watch *awptr;
	struct Client *acptr;
	dlink_node *lp;

	hashv = watch_hash_nick(client_p->name);
	if(hashv <= 0)
		return 0;

	if((awptr = (struct Watch *) watchTable[hashv]))
		while(awptr && irccmp(awptr->watchnick, client_p->name))
			awptr = awptr->hnext;

	if(!awptr)
		return 0;	/* This nick isn't on watch */

	awptr->lasttime = CurrentTime;

	DLINK_FOREACH(lp, awptr->watched_by.head)
	{
		acptr = lp->data;
		sendto_one(acptr, form_str(reply), me.name, acptr->name, client_p->name,
				(IsPerson(client_p) ? client_p->username : "<N/A>"),
				(IsPerson(client_p) ? client_p->host : "<N/A>"),
				awptr->lasttime, client_p->info);
	}
	return 0;
}


int
add_to_watch_hash_table(const char *nick, struct Client *client_p)
{
	int hashv;
	struct Watch *awptr;
	dlink_node *lp = NULL;

	if(strlen(nick) > NICKLEN)
		return 0;
	hashv = watch_hash_nick(nick);
	if(hashv <= 0)
		return 0;
	if((awptr = (struct Watch *) watchTable[hashv]))
		while(awptr && irccmp(awptr->watchnick, nick))
			awptr = awptr->hnext;

	if(!awptr)
	{
		awptr = BlockHeapAlloc(watch_heap);
		awptr->lasttime = CurrentTime;
		strlcpy(awptr->watchnick, nick, NICKLEN);

		awptr->hnext = watchTable[hashv];
		watchTable[hashv] = awptr;
	}
	else
	{
		lp = dlinkFind(&awptr->watched_by, client_p);
	}

	if(!lp)
	{
		dlinkAddAlloc(client_p, &awptr->watched_by);
		dlinkAddAlloc(awptr, &client_p->localClient->watchlist);
	}

	return 0;
}

struct Watch *
hash_get_watch(const char *name)
{
	int hashv;
	struct Watch *awptr;

	hashv = watch_hash_nick(name);
	if(hashv <= 0)
		return NULL;
	if((awptr = (struct Watch *) watchTable[hashv]))
		while(awptr && irccmp(awptr->watchnick, name))
			awptr = awptr->hnext;

	return awptr;
}

int
del_from_watch_hash_table(const char *nick, struct Client *client_p)
{
	int hashv;
	struct Watch *awptr, *nlast = NULL;

	hashv = watch_hash_nick(nick);
	if(hashv <= 0)
		return 0;
	if((awptr = (struct Watch *) watchTable[hashv]))
		while(awptr && irccmp(awptr->watchnick, nick))
		{
			nlast = awptr;
			awptr = awptr->hnext;
		}

	if(!awptr)
		return 0;	/* No such watch */

	if(!dlinkFindDestroy(&awptr->watched_by, client_p))
		return 0;

	dlinkFindDestroy(&client_p->localClient->watchlist, awptr);

	/*
	 * In case this header is now empty of notices, remove it 
	 */
	if(!awptr->watched_by.head)
	{
		if(!nlast)
			watchTable[hashv] = awptr->hnext;
		else
			nlast->hnext = awptr->hnext;
		BlockHeapFree(watch_heap, awptr);
	}

	return 0;
}

int
hash_del_watch_list(struct Client *client_p)
{
	int hashv;
	struct Watch *awptr;
	dlink_node *ptr, *next_ptr;

	if(!(client_p->localClient->watchlist.head))
		return 0;	/* Nothing to do */

	DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->localClient->watchlist.head)
	{
		awptr = ptr->data;
		if(awptr)
		{
			dlinkFindDestroy(&awptr->watched_by, client_p);

			if(!dlink_list_length(&awptr->watched_by))
			{
				struct Watch *awptr2, *awptr3;

				hashv = watch_hash_nick(awptr->watchnick);

				awptr3 = NULL;
				awptr2 = watchTable[hashv];
				while(awptr2 && (awptr2 != awptr))
				{
					awptr3 = awptr2;
					awptr2 = awptr2->hnext;
				}

				if(awptr3)
					awptr3->hnext = awptr->hnext;
				else
					watchTable[hashv] = awptr->hnext;
				BlockHeapFree(watch_heap, awptr);
			}
		}
		dlinkDestroy(ptr, &client_p->localClient->watchlist);
	}

	return 0;
}
