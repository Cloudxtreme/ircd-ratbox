/*
 *  ircd-ratbox: A slightly useful ircd
 *  reject.c: reject users with prejudice
 *
 *  Copyright 2003 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright 2003 ircd-ratbox development team
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
#include "config.h"
#include "patricia.h"
#include "client.h"
#include "s_conf.h"
#include "event.h"
#include "tools.h"
#include "reject.h"

static patricia_tree_t *reject_tree;
static dlink_list delay_exit;
unsigned long reject_count = 0;

struct reject_data
{
	time_t time;
	unsigned int count;
};


static void
reject_exit(void *unused)
{
	struct Client *client_p;
	dlink_node *ptr, *ptr_next;

	DLINK_FOREACH_SAFE(ptr, ptr_next, delay_exit.head)
	{
		client_p = (struct Client *)ptr->data;
		dlinkDestroy(ptr, &delay_exit);
		exit_client(client_p, client_p, &me, "*** Banned (cached)");
	}
}

static void
reject_expires(void *unused)
{
	patricia_node_t *pnode;
	struct reject_data *rdata;
	
	PATRICIA_WALK(reject_tree->head, pnode)
	{
		rdata = pnode->data;
		if(rdata->time + ConfigFileEntry.reject_duration < CurrentTime)
		{
			MyFree(rdata);
			patricia_remove(reject_tree, pnode);
		}
	}
	PATRICIA_WALK_END;
}

void
init_reject(void)
{
#ifdef IPV6
	reject_tree = New_Patricia(128);
#else
	reject_tree = New_Patricia(32);
#endif
	eventAdd("reject_exit", reject_exit, NULL, DELAYED_EXIT_TIME);
	eventAdd("reject_expires", reject_expires, NULL, 60);
}


void
add_reject(struct Client *client_p)
{
	patricia_node_t *pnode;
	struct reject_data *rdata;
#ifdef IPV6
	int len = 128;
#else
	int len = 32;
#endif
	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0)
		return;

	if((pnode = match_ip(reject_tree, &client_p->localClient->ip)) != NULL)
	{
		rdata = pnode->data;
		rdata->time = CurrentTime;
		rdata->count++;	
	}
	else
	{
		pnode = make_and_lookup_ip(reject_tree, &client_p->localClient->ip, len);
		pnode->data = rdata = MyMalloc(sizeof(struct reject_data));
		rdata->time = CurrentTime;
		rdata->count = 1;
	}
}

int
check_reject(struct Client *client_p)
{
	patricia_node_t *pnode;
	struct reject_data *rdata;
	
	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0 ||
	   ConfigFileEntry.reject_duration == 0)
		return 0;
		
	pnode = match_ip(reject_tree, &client_p->localClient->ip);
	if(pnode != NULL)
	{
		rdata = pnode->data;
		rdata->time = CurrentTime;
		if(rdata->count > ConfigFileEntry.reject_after_count)
		{
			reject_count++;
			dlinkAddAlloc(client_p, &delay_exit);
			return 1;
		}
	}	
	/* Caller does what it wants */	
	return 0;
}

void flush_reject(void)
{
	Clear_Patricia(reject_tree, MyFree);
}
