/************************************************************************
 *   IRC - Internet Relay Chat, include/s_gline.h
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
 * $Id$
 */

#ifndef INCLUDED_s_gline_h
#define INCLUDED_s_gline_h
#ifndef INCLUDED_config_h
#include "config.h"
#endif
#ifndef INCLUDED_ircd_defs_h
#include "ircd_defs.h"
#endif
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif

struct Client;
struct ConfItem;

extern struct ConfItem* find_gkill(struct Client* client, char *);
extern struct ConfItem* find_is_glined(const char* host, const char* name);
extern void   report_glines(struct Client *); 
extern int    remove_gline_match(const char *user, const char *host);
extern void   cleanup_glines(void *notused);
extern void   add_gline(struct ConfItem *);


typedef struct gline_pending
{
  char oper_nick1[NICKLEN + 1];
  char oper_user1[USERLEN + 1];
  char oper_host1[HOSTLEN + 1];
  const char* oper_server1;     /* point to scache */
  char *reason1;
  time_t time_request1;

  char oper_nick2[NICKLEN + 1];
  char oper_user2[USERLEN + 1];
  char oper_host2[HOSTLEN + 1];
  const char* oper_server2;     /* point to scache */
  char *reason2;
  time_t time_request2;
  
  time_t last_gline_time;       /* for expiring entry */
  char user[USERLEN + 1];
  char host[HOSTLEN + 1];
}gline_pending_t;

/* how long a pending G line can be around
 * 10 minutes should be plenty
 */

#define GLINE_PENDING_EXPIRE 600
#define CLEANUP_GLINES_TIME  300

dlink_list pending_glines;

#endif
