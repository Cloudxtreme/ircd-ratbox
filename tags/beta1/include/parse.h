/************************************************************************
 *   IRC - Internet Relay Chat, include/parse.h
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
 * "parse.h". - Headers file.
 *
 *
 * $Id$
 *
 */
#ifndef INCLUDED_parse_h_h
#define INCLUDED_parse_h_h

struct Message;
struct Client;

struct MessageHash
{
  char   *cmd;
  struct Message      *msg;
  struct MessageHash  *next;
}; 

#define MAX_MSG_HASH  387

extern  int     parse (struct Client *, char *, char *);
extern  void    clear_hash_parse (void);
extern  void    mod_add_cmd(struct Message *msg);
extern  void    mod_del_cmd(struct Message *msg);
extern  void    report_messages(struct Client *);

#endif /* INCLUDED_parse_h_h */
