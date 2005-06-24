/*
 *  ircd-ratbox: A slightly useful ircd.
 *  whowas.h: Header for the whowas functions.
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
#ifndef INCLUDED_watch_h
#define INCLUDED_watch_h

#include "setup.h"
#include "ircd_defs.h"
#include "client.h"
#include "tools.h"


struct Watch {
	struct Watch *hnext;
	time_t lasttime;
	char watchnick[NICKLEN];
	dlink_list watched_by;
};
            
#define WATCH_BITS 16
#define WATCHHASHSIZE 65536

extern void initwatch(void);
extern int add_to_watch_hash_table(const char *nick, struct Client * cptr);
extern struct Watch * hash_get_watch(const char *name);
extern int del_from_watch_hash_table(const char *nick, struct Client * cptr);
int hash_del_watch_list(struct Client * client_p);
int hash_check_watch(struct Client *client_p, int reply);


#endif /* INCLUDED_whowas_h */
