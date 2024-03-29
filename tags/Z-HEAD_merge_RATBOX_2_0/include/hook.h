/*
 * Copyright (C) 2004 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2004 ircd-ratbox development team
 *
 * $Id$
 */
#ifndef INCLUDED_HOOK_H
#define INCLUDED_HOOK_H

typedef struct
{
	char *name;
	dlink_list hooks;
} hook;

typedef void (*hookfn) (void *data);

extern int h_iosend_id;
extern int h_iorecv_id;
extern int h_iorecvctrl_id;
extern int h_burst_channel_id;
extern int h_client_auth_id;

void init_hook(void);
int register_hook(const char *name);
void add_hook(const char *name, hookfn fn);
void remove_hook(const char *name, hookfn fn);
void call_hook(int id, void *arg);

typedef struct
{
	struct Client *client;
	const void *arg1;
	const void *arg2;
} hook_data;

typedef struct
{
	struct Client *client;
	const void *arg1;
	int arg2;
} hook_data_int;

#endif
