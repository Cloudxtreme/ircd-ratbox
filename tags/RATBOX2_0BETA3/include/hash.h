/*
 *  ircd-ratbox: A slightly useful ircd.
 *  hash.h: A header for the ircd hashtable code.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002 ircd-ratbox development team
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

#ifndef INCLUDED_hash_h
#define INCLUDED_hash_h

/* Client hash table size, used in hash.c/s_debug.c */
#define U_MAX 65536

/* Channel hash table size, hash.c/s_debug.c */
#define CH_MAX 16384

/* hostname hash table size */
#define HOST_MAX 131072

/* RESV/XLINE hash table size, used in hash.c */
#define R_MAX 1024

struct Client;
struct Channel;
struct rxconf;
struct helpfile;

extern void init_hash(void);

extern size_t hash_get_client_table_size(void);
extern size_t hash_get_channel_table_size(void);
extern size_t hash_get_resv_table_size(void);

extern void add_to_client_hash(const char *name, struct Client *client);
extern void del_from_client_hash(const char *name, struct Client *client);
extern struct Client *find_client(const char *name);
extern struct Client *find_server(const char *name);

extern void add_to_id_hash(const char *, struct Client *);
extern void del_from_id_hash(const char *name, struct Client *client);
extern struct Client *find_id(const char *name);

extern struct Channel *get_or_create_channel(struct Client *client_p, const char *chname, int *isnew);
extern void del_from_channel_hash(const char *name, struct Channel *chan);
extern struct Channel *find_channel(const char *name);

extern void add_to_hostname_hash(const char *, struct Client *);
extern void del_from_hostname_hash(const char *, struct Client *);
extern dlink_node *find_hostname(const char *);

extern void add_to_resv_hash(const char *name, struct rxconf *rconf);
extern void del_from_resv_hash(const char *name, struct rxconf *rconf);
extern struct rxconf *hash_find_resv(const char *name);

extern void add_to_xline_hash(const char *name, struct rxconf *xconf);
extern void del_from_xline_hash(const char *name, struct rxconf *xconf);
extern struct rxconf *hash_find_xline(const char *name);

extern void add_to_nd_hash(const char *name);
extern void del_from_nd_hash(const char *name);
extern int find_nd(const char *name);

extern void add_to_help_hash(const char *name, struct helpfile *hptr);
extern void clear_help_hash(void);
extern struct helpfile *hash_find_help(const char *name, int flags);

#endif /* INCLUDED_hash_h */
