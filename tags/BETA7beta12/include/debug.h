/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  debug.h: The debugging functions header.
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

#ifndef __DEBUG_H_INCLUDED_
#define __DEBUG_H_INCLUDED_

#define DEBUG_BUFSIZE 512

typedef struct 
{
  char *name; /* Name of thing to debug */
  int   debugging; /* Are we debugging this? */
} debug_tab;

int        debugging(char *);
int        enable_debug(char *);
int        disable_debug(char *);
int        set_debug(char *, int);
debug_tab *find_debug_tab(char *);
void       deprintf(char *, char *, ...);
void       add_mod_debug(char *);


#endif
