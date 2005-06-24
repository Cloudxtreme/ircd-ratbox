/************************************************************************
 *   IRC - Internet Relay Chat, include/modules.h
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */

#ifndef INCLUDED_modules_h
#define INCLUDED_modules_h

#include "setup.h"

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include "ircd_handler.h"
#include "msg.h"
#include "memory.h"

struct module {
  char *name;
  char *version;
  void *address;
};

struct module_path
{
	char path[MAXPATHLEN];
};

/* add a path */
void mod_add_path(char *path);

/* load a module */
extern void load_module(char *path);

/* load all modules */
extern void load_all_modules(int check);

extern void _modinit(void);
extern void _moddeinit(void);

extern int unload_one_module (char *, int);
extern int load_one_module (char *);
extern int load_a_module (char *, int);
extern int findmodule_byname (char *);
extern char* irc_basename(char *);
extern void modules_init(void);

#endif
