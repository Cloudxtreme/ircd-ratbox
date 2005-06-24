/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  win32_defs.h: Defines for win32
 *
 *  Copyright (C) 2002 Aaron Sethman <androsyn@ratbox.org>
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
 * $Id$
 *
 */
                       

#define FD_SETSIZE 1024
#include <windows.h>
#include <winnt.h>
#include <winsock2.h>
#include <process.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 128
#endif

#define ENOBUFS WSAENOBUFS
#define EINPROGRESS WSAEINPROGRESS
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EMSGSIZE    WSAEMSGSIZE
#define EALREADY    WSAEALREADY
#define EISCONN     WSAEISCONN

#define ioctl(x,y,z)  ioctlsocket(x,y, (u_long *)z)
int gettimeofday(struct timeval *tv, void *tz);
int kill(int pid, int sig);
unsigned int geteuid(void);
                                            