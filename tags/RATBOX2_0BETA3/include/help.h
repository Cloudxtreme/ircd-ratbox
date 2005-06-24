/* $Id$ */
#ifndef INCLUDED_HELP_H
#define INCLUDED_HELP_H

#include "tools.h"

#define HELP_MAX	100

#define HELPLINELEN	81
#define HELPFILELEN	30

extern const char emptyline[];

#define HELP_USER	0x001
#define HELP_OPER	0x002

struct helpfile
{
	char helpname[HELPFILELEN];
	char firstline[HELPLINELEN];
	dlink_list contents;
	int flags;
};

struct helpline
{
	char data[HELPLINELEN];
	dlink_node linenode;
};

extern void init_help(void);
extern void load_help(void);
extern void free_help(struct helpfile *);

#endif

