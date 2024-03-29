#ifndef __HOOK_H_INCLUDED
#define __HOOK_H_INCLUDED

#include "tools.h"


typedef struct
{
	char *name;
	dlink_list hooks;
} hook;

/* we don't define the arguments to hookfn, because they can
   vary between different hooks */
typedef int (*hookfn)(void *data);

/* this is used when a hook is called by an m_function
   stand data you'd need in that situation */
struct hook_mfunc_data 
{
	struct Client *client_p;
	struct Client *source_p;
	int parc;
	char **parv;
};

struct hook_stats_data 
{
	struct Client *source_p;
	char statchar;
	char *name;
};

struct hook_links_data
{
	struct Client *client_p;
	struct Client *source_p;
	int parc;
	char **parv;
	char statchar;
	char *mask;
};

struct hook_spy_data
{
	struct Client *source_p;
};

struct hook_io_data
{
        struct Client *connection;
        char *data;
        unsigned int len;
};

int hook_add_event(char *);
int hook_add_hook(char *, hookfn *);
int hook_call_event(char *, void *);
int hook_del_event(char *);
int hook_del_hook(char *event, hookfn *fn);
void init_hooks(void);

#endif
