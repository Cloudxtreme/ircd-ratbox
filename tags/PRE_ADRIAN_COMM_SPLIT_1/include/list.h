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

struct SLink;
struct Client;
struct Class;
struct User;
struct Channel;
struct ConfItem;
struct Ban;

/* general link structure used for chains */

struct SLink
{
  struct        SLink   *next;
  union
  {
    struct Client   *cptr;
    struct Channel  *chptr;
    struct ConfItem *aconf;
    struct Ban   *banptr;
    char      *cp;
  } value;
  int   flags;
};

extern void count_user_memory(int *, int *);
extern void count_links_memory(int *, int *);
extern void count_flud_memory(int *, int *);
extern void     outofmemory(void);
extern  void    _free_link (struct SLink *);
extern  void    _free_user (struct User *, struct Client *);
extern  struct SLink    *make_link (void);
extern  struct User     *make_user (struct Client *);
extern  struct Server   *make_server (struct Client *);
extern  struct SLink    *find_user_link (struct SLink *, struct Client *);
extern  void    initlists (void);
extern  void    block_garbage_collect(void);
extern  void    block_destroy(void);
extern struct SLink *find_channel_link(struct SLink *, struct Channel *);

#endif
