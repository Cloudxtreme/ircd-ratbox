/************************************************************************
 *   IRC - Internet Relay Chat, include/h.h
 *   Copyright (C) 1992 Darren Reed
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
 *
 * "list.h". - Headers file.
 *
 *
 * $Id$
 *
 */

#ifndef INCLUDED_list_h
#define INCLUDED_list_h
#include "memory.h"

struct dlink_node;
struct Client;
struct Class;
struct User;
struct Channel;
struct ConfItem;
struct Ban;

extern void initUser(void);
extern void count_user_memory(int *count,int *user_memory_used);
extern void count_links_memory(int *count,int *links_memory_used);
extern void outofmemory(void);
extern void _free_user (struct User *, struct Client *);
#ifdef MEMDEBUG
extern dlink_node* _make_dlink_node(const char*, int);
#define make_dlink_node() _make_dlink_node(__FILE__, __LINE__);
#else
extern dlink_node *_make_dlink_node (void);
#define make_dlink_node() _make_dlink_node();
#endif
extern void _free_dlink_node(dlink_node *lp);
extern void init_dlink_nodes();

extern struct User     *make_user (struct Client *);
extern struct Server   *make_server (struct Client *);

#endif
