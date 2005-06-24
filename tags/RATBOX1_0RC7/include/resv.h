/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  restart.h: A header for the restart functions.
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

#ifndef INCLUDED_resv_h
#define INCLUDED_resv_h

#define RESV_CHANNEL    0x0001
#define RESV_NICK       0x0002
#define RESV_NICKWILD   0x0004

struct ResvEntry
{
  char *name;
  char *reason;
  int flags;
};

extern dlink_list resv_channel_list;
extern dlink_list resv_nick_list;

extern struct ResvEntry *create_resv(char *, char *, int);

extern int delete_resv(struct ResvEntry *);
extern void clear_resv(void);

extern int find_channel_resv(char *);
extern int find_nick_resv(char *);
extern struct ResvEntry *get_nick_resv(char *);

extern void report_resv(struct Client *);

extern int clean_resv_nick(char *);

#endif  /* INCLUDED_hash_h */



