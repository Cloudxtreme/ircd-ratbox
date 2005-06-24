/*
 *  ircd-ratbox: A slightly useful ircd
 *  reject.c: reject users with prejudice
 *
 *  Copyright (C) 2003 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2003-2004 ircd-ratbox development team
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
#include "s_stats.h"
#include "msg.h"

static patricia_tree_t *reject_tree;
static dlink_list delay_exit;
static dlink_list reject_list;

struct reject_data
{
	dlink_node rnode;
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
		client_p = ptr->data;
		dlinkDestroy(ptr, &delay_exit);
	  	if(IsDead(client_p))
                	continue;
		if(client_p->localClient->fd >= 0)
			sendto_one(client_p, "ERROR :Closing Link: %s (%s)", client_p->host, "*** Banned (cache)");
 	  	close_connection(client_p);
        	SetDead(client_p);
        	dlinkAddAlloc(client_p, &dead_list);
	}
}

static void
reject_expires(void *unused)
{
	dlink_node *ptr, *next;
	patricia_node_t *pnode;
	struct reject_data *rdata;
	
	DLINK_FOREACH_SAFE(ptr, next, reject_list.head)
	{
		pnode = ptr->data;
		rdata = pnode->data;		

		if(rdata->time + ConfigFileEntry.reject_duration > CurrentTime)
			return;

		dlinkDelete(ptr, &reject_list);
		MyFree(rdata);
		patricia_remove(reject_tree, pnode);
	}
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

	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0)
		return;

	if((pnode = match_ip(reject_tree, &client_p->localClient->ip)) != NULL)
	{
		rdata = pnode->data;
		rdata->time = CurrentTime;
		rdata->count++;
		dlinkMoveTail(&rdata->rnode, &reject_list);
	}
	else
	{
		pnode = make_and_lookup_ip(reject_tree, &client_p->localClient->ip, GET_SS_LEN(client_p->localClient->ip));
		pnode->data = rdata = MyMalloc(sizeof(struct reject_data));
		dlinkAddTail(pnode, &rdata->rnode, &reject_list);
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
			ServerStats->is_rej++;
			SetReject(client_p);
			dlinkMoveNode(&client_p->localClient->tnode, &unknown_list, &delay_exit);
			return 1;
		}
	}	
	/* Caller does what it wants */	
	return 0;
}

void flush_reject(void)
{
	dlink_node *ptr, *next;
	patricia_node_t *pnode;
	struct reject_data *rdata;
	
	DLINK_FOREACH_SAFE(ptr, next, reject_list.head)
	{
		pnode = ptr->data;
		rdata = pnode->data;
		dlinkDelete(ptr, &reject_list);
		MyFree(rdata);
		patricia_remove(reject_tree, pnode);
	}
}
