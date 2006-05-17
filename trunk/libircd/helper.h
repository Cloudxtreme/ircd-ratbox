/*
 *  ircd-ratbox: A slightly useful ircd
 *  helper.h: Starts and deals with ircd helpers
 *  
 *  Copyright (C) 2006 Aaron Sethman <androsyn@ratbox.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id$
 */

#ifndef IRCD_LIB_H
# error "Do not use helper.h directly"
#endif

#ifndef INCLUDED_helper_h
#define INCLUDED_helper_h

struct _ircd_helper;

typedef void ircd_helper_cb(struct _ircd_helper *);

typedef struct _ircd_helper
{
	char *path;
	buf_head_t sendq;
	buf_head_t recvq;
	int ifd;
	int ofd;
	pid_t pid;
	int fork_count;
	ircd_helper_cb *read_cb;
	ircd_helper_cb *error_cb;
} ircd_helper;


ircd_helper *ircd_helper_start(const char *name, const char *fullpath, ircd_helper_cb *read_cb, ircd_helper_cb *error_cb);

ircd_helper *ircd_helper_child(ircd_helper_cb *read_cb, ircd_helper_cb *error_cb,
			       log_cb *ilog, restart_cb *irestart, die_cb *idie,
                               int maxcon, size_t lb_heap_size, size_t dh_size);

void ircd_helper_restart(ircd_helper *helper);
void ircd_helper_write(ircd_helper *helper, const char *format, ...);
void ircd_helper_read(int, void *);
void ircd_helper_close(ircd_helper *helper);
int ircd_helper_readline(ircd_helper *helper, void *buf, size_t bufsize);

#endif

