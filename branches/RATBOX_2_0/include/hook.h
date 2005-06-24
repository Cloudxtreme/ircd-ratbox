/*
 *  ircd-ratbox: A slightly useful ircd.
 *  hook.h: A header for the hooks into parts of ircd.
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

#ifndef __HOOK_H_INCLUDED
#define __HOOK_H_INCLUDED

#include "tools.h"


typedef struct
{
	char *name;
	int id;
	dlink_list hooks;
}
hook;

/* we don't define the arguments to hookfn, because they can
   vary between different hooks */
typedef int (*hookfn) (void *data);

/* this is used when a hook is called by an m_function
   stand data you'd need in that situation */
struct hook_mfunc_data
{
	struct Client *client_p;
	struct Client *source_p;
	int parc;
	char **parv;
};

struct hook_spy_data
{
	struct Client *source_p;
	const char *name;
	char statchar;
};

struct hook_io_data
{
	struct Client *connection;
	const char *data;
	unsigned int len;
};

struct hook_burst_channel
{
	struct Client *client;
	struct Channel *chptr;
};


int hook_add_event(const char *, int *);
int hook_add_hook(const char *, hookfn );
int hook_call_event(int id, void *);
int hook_del_event(const char *);
int hook_del_hook(const char *event, hookfn );
void init_hooks(void);

extern int h_iosend_id;
extern int h_iorecv_id;
extern int h_iorecvctrl_id;
extern int h_burst_channel_id;
extern int h_client_auth_id;
#endif
