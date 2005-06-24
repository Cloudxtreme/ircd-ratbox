/*
 *  ircd-ratbox: A slightly useful ircd.
 *  resv.c: Functions to reserve(jupe) a nick/channel.
 *
 *  Copyright (C) 2001-2002 Hybrid Development Team
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
#include "tools.h"
#include "restart.h"
#include "common.h"
#include "fdlist.h"
#include "ircd.h"
#include "send.h"
#include "s_debug.h"
#include "numeric.h"
#include "s_log.h"
#include "client.h"
#include "memory.h"
#include "resv.h"
#include "hash.h"
#include "ircd_defs.h"
#include "s_conf.h"

dlink_list resv_channel_list;
dlink_list resv_nick_list;

struct ResvEntry *
create_resv(char *name, char *reason, int flags)
{
	struct ResvEntry *resv_p;

	if(flags & RESV_CHANNEL)
	{
		if(find_channel_resv(name))
			return NULL;

		if(strlen(name) > CHANNELLEN)
			name[CHANNELLEN] = '\0';
	}
	else
	{
		if(find_nick_resv(name))
			return NULL;

		if(strlen(name) > NICKLEN * 2)
			name[NICKLEN * 2] = '\0';

		if((strchr(name, '*') != NULL) || (strchr(name, '?') != NULL))
			flags |= RESV_NICKWILD;
	}

	if(strlen(reason) > TOPICLEN)
		reason[TOPICLEN] = '\0';

	resv_p = (struct ResvEntry *) MyMalloc(sizeof(struct ResvEntry));

	DupString(resv_p->name, name);
	DupString(resv_p->reason, reason);
	resv_p->flags = flags;

	if(flags & RESV_CHANNEL)
	{
		dlinkAddAlloc(resv_p, &resv_channel_list);
		add_to_resv_hash_table(resv_p->name, resv_p);
	}
	else
	{
		dlinkAddAlloc(resv_p, &resv_nick_list);
	}

	return resv_p;
}

void
clear_resv(void)
{
	struct ResvEntry *resv_p;
	dlink_node *ptr;
	dlink_node *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, resv_channel_list.head)
	{
		resv_p = ptr->data;

		dlinkDestroy(ptr, &resv_channel_list);
		del_from_resv_hash_table(resv_p->name, resv_p);
		MyFree((char *) resv_p);
	}

	DLINK_FOREACH_SAFE(ptr, next_ptr, resv_nick_list.head)
	{
		resv_p = ptr->data;

		dlinkDestroy(ptr, &resv_nick_list);
		MyFree((char *) resv_p);
	}
}

int
delete_resv(struct ResvEntry *resv_p)
{
	if(resv_p == NULL)
		return 0;

	if(resv_p->flags & RESV_CHANNEL)
	{
		del_from_resv_hash_table(resv_p->name, resv_p);
		dlinkFindDestroy(&resv_channel_list, resv_p);
		MyFree((char *) resv_p);
	}
	else
	{
		dlinkFindDestroy(&resv_nick_list, resv_p);
		MyFree((char *) resv_p);
	}

	return 1;
}

int
find_channel_resv(const char *name)
{
	struct ResvEntry *resv_p;

	resv_p = (struct ResvEntry *) hash_find_resv(name);

	if(resv_p == NULL)
		return 0;

	return 1;
}

struct ResvEntry *
get_nick_resv(char *name)
{
	struct ResvEntry *resv_p;
	dlink_node *ptr;

	DLINK_FOREACH(ptr, resv_nick_list.head)
	{
		resv_p = ptr->data;

		if(irccmp(resv_p->name, name) == 0)
			return resv_p;
	}

	return NULL;
}

int
find_nick_resv(char *name)
{
	struct ResvEntry *resv_p;
	dlink_node *ptr;

	DLINK_FOREACH(ptr, resv_nick_list.head)
	{
		resv_p = ptr->data;

		if(resv_p->flags & RESV_NICKWILD)
		{
			if(match(resv_p->name, name))
				return 1;
		}
		else
		{
			if(irccmp(resv_p->name, name) == 0)
				return 1;
		}
	}

	return 0;
}

void
report_resv(struct Client *source_p)
{
	struct ResvEntry *resv_p;
	dlink_node *ptr;

	DLINK_FOREACH(ptr, resv_channel_list.head)
	{
		resv_p = ptr->data;

		sendto_one(source_p, form_str(RPL_STATSQLINE),
			   me.name, source_p->name, 'Q', resv_p->name, resv_p->reason);
	}

	DLINK_FOREACH(ptr, resv_nick_list.head)
	{
		resv_p = ptr->data;

		sendto_one(source_p, form_str(RPL_STATSQLINE),
			   me.name, source_p->name, 'Q', resv_p->name, resv_p->reason);
	}
}

int
clean_resv_nick(char *nick)
{
	char tmpch;
	int as = 0;
	int q = 0;
	int ch = 0;

	if(*nick == '-' || IsDigit(*nick))
		return 0;

	while ((tmpch = *nick++))
	{
		if(tmpch == '?')
			q++;
		else if(tmpch == '*')
			as++;
		else if(IsNickChar(tmpch))
			ch++;
		else
			return 0;
	}

	if(!ch && as)
		return 0;

	return 1;
}

int
valid_wild_card_simple(const char *data)
{
	const char *p;
	char tmpch;
	int nonwild = 0;

	/* check the string for minimum number of nonwildcard chars */
	p = data;

	while((tmpch = *p++))
	{
		if(!IsMWildChar(tmpch))
		{
			/* if we have enough nonwildchars, return */
			if(++nonwild >= ConfigFileEntry.min_nonwildcard_simple)
				return 1;
		}
	}

	return 0;
}

