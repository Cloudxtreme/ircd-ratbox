/*
 *  ircd-ratbox: A slightly useful ircd.
 *  hook.c: Provides a generic event hooking interface.
 *
 *  Copyright (C) 2000-2002 Edward Brocklesby, Hybrid Development Team
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

/* hooks are used by modules to hook into events called by other parts of
   the code.  modules can also register their own events, and call them
   as appropriate. */

#include "stdinc.h"
#include "irc_string.h"

#include "tools.h"
#include "memory.h"
#include "hook.h"

dlink_list hooks;

static hook* find_hook_byname(const char *);

#ifndef NDEBUG
int h_iosend_id;
int h_iorecv_id;
int h_iorecvctrl_id;
#endif
int h_burst_channel_id;
int h_client_auth_id;

void
init_hooks(void)
{
	memset(&hooks, 0, sizeof(hooks));
#ifndef NDEBUG
	hook_add_event("iosend", &h_iosend_id);
	hook_add_event("iorecv", &h_iorecv_id);
	hook_add_event("iorecvctrl", &h_iorecvctrl_id);
#endif
	hook_add_event("burst_channel", &h_burst_channel_id);
	hook_add_event("client_auth", &h_client_auth_id);
}

static int hook_curid = 0;

static hook *
new_hook(const char *name, int *id)
{
	hook *h;

	h = MyMalloc(sizeof(hook));
	DupString(h->name, name);

	h->id = *id = hook_curid++;
	return h;
}

int
hook_add_event(const char *name, int* id)
{
	hook *newhook;
	if ((newhook = find_hook_byname(name)) != NULL)
	{
		*id = newhook->id;
		return 0;
	}

	newhook = new_hook(name, id);

	dlinkAddAlloc(newhook, &hooks);
	return 0;
}

int
hook_del_event(const char *name)
{
	dlink_node *node;
	hook *h;

	DLINK_FOREACH(node, hooks.head)
	{
		h = node->data;

		if(!strcmp(h->name, name))
		{
			dlinkDelete(node, &hooks);
			MyFree(h);
			return 0;
		}
	}
	return 0;
}

static hook *
find_hook_byname(const char *name)
{
	dlink_node *node;
	hook *h;

	DLINK_FOREACH(node, hooks.head)
	{
		h = node->data;

		if(!strcmp(h->name, name))
			return h;
	}
	return NULL;
}

static hook *
find_hook_byid(int id)
{
	dlink_node *node;
	hook *h;

	DLINK_FOREACH(node, hooks.head)
	{
		h = node->data;

		if (h->id == id)
			return h;
	}
	return NULL;
}

int
hook_del_hook(const char *event, hookfn  fn)
{
	hook *h;
	dlink_node *node, *nnode;
	h = find_hook_byname(event);
	if(!h)
		return -1;

	DLINK_FOREACH_SAFE(node, nnode, h->hooks.head)
	{
		if(fn == (hookfn) node->data)
		{
			dlinkDestroy(node, &h->hooks);
		}
	}
	return 0;
}

int
hook_add_hook(const char *event, hookfn fn)
{
	hook *h;

	h = find_hook_byname(event);
	if(!h)
		return -1;

	dlinkAddAlloc(fn, &h->hooks);
	return 0;
}

int
hook_call_event(int id, void *data)
{
	hook *h;
	dlink_node *node;
	hookfn fn;

	h = find_hook_byid(id);
	if(!h)
		return -1;

	DLINK_FOREACH(node, h->hooks.head)
	{
		fn = (hookfn) (uintptr_t) node->data;

		if(fn(data) != 0)
			return 0;
	}
	return 0;
}
