/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  scache.h: A header for the servername cache functions.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
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

#ifndef INCLUDED_scache_h
#define INCLUDED_scache_h

extern void        clear_scache_hash_table(void);
extern const char* find_or_add(const char* name);
extern void        count_scache(int *,unsigned long *);
extern void        list_scache(struct Client *source_p);

#endif
