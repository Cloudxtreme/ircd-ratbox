/***********************************************************************
 *   ircd-hybrid project - Internet Relay Chat
 *  hostmask.h: Hostmask header file.
 *  All parts of this program are Copyright(C) 2001(or later).
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * $Id$ 
 */

struct HostMaskEntry
{
  int type;
  unsigned long precedence;
  char *hostmask;
  void *data;
  struct HostMaskEntry *next, *nexthash;
};

void add_hostmask(const char *mask, int type, void *data);
struct HostMaskEntry *match_hostmask(const char *mask, int type);
struct ConfItem *find_matching_conf(const char*, const char*,
                                    struct irc_inaddr*);
void add_conf(struct ConfItem *aconf);
void clear_conf(void);
void report_hostmask_conf_links(struct Client*, int);

#define HOST_CONFITEM 1
#define MAXPREFIX HOSTLEN+USERLEN+15
