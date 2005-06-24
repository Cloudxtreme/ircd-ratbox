/************************************************************************
 *   IRC - Internet Relay Chat, include/resv.h
 *   Copyright (C) 1991 Hybrid Development Team
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
 *   $Id$
 */
#ifndef INCLUDED_resv_h
#define INCLUDED_resv_h
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif

struct ResvChannel
{
  struct ResvChannel *next;
  struct ResvChannel *prev;
  struct ResvChannel *hnext;

  char	name[CHANNELLEN];
  char	*reason;
  int	conf;
};

struct ResvNick
{
  struct ResvNick *next;
  struct ResvNick *prev;

  char	name[NICKLEN];
  char	*reason;
  int	conf;
};

extern struct ResvChannel *ResvChannelList;
extern struct ResvNick *ResvNickList;

extern struct ResvChannel *create_channel_resv(char *, char *, int);
extern struct ResvNick *create_nick_resv(char *, char *, int);

extern int delete_channel_resv(struct ResvChannel *);
extern int delete_nick_resv(struct ResvNick *);

extern int clear_conf_resv();

extern int find_channel_resv(char *);
extern int find_nick_resv(char *);

extern void report_resv(struct Client *);
extern struct ResvNick *return_nick_resv(char *);

extern int clean_resv_nick(char *);

#endif  /* INCLUDED_hash_h */



