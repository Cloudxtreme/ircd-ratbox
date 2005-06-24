/************************************************************************
 *
 * s_bsd_poll.c - code implementing a poll IO loop
 *   By Adrian Chadd <adrian@creative.net.au>
 *
 * Based upon:
 *
 *   IRC - Internet Relay Chat, src/s_bsd.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
 *  $Id$
 */
#include "config.h"
#ifdef USE_POLL
#include "fdlist.h"
#include "s_bsd.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "ircdauth.h"
#include "ircd.h"
#include "list.h"
#include "listener.h"
#include "numeric.h"
#include "packet.h"
#include "res.h"
#include "restart.h"
#include "s_auth.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_serv.h"
#include "s_stats.h"
#include "send.h"
#include "s_debug.h"
#include "s_bsd.h"
#include "memory.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>    /* NOFILE */
#endif

#include <arpa/inet.h>
#include <sys/poll.h>

/* I hate linux -- adrian */
#ifndef POLLRDNORM
#define POLLRDNORM POLLIN
#endif
#ifndef POLLWRNORM
#define POLLWRNORM POLLOUT
#endif

struct _pollfd_list {
    struct pollfd pollfds[MAXCONNECTIONS];
    int maxindex;		/* highest FD number */
};

typedef struct _pollfd_list		pollfd_list_t;

pollfd_list_t pollfd_list;
static void poll_update_pollfds(int, short, PF *);

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

/*
 * find a spare slot in the fd list. We can optimise this out later!
 *   -- adrian
 */
static inline int
poll_findslot(void)
{
    int i;
    for (i = 0; i < MAXCONNECTIONS; i++) {
        if (pollfd_list.pollfds[i].fd == -1) {
            /* MATCH!!#$*&$ */
            return i;
        }
    }
    assert(1 == 0);
    /* NOTREACHED */
    return -1;
}
/*
 * set and clear entries in the pollfds[] array.
 */ 
static void
poll_update_pollfds(int fd, short event, PF * handler)
{  
    fde_t *F = &fd_table[fd];
    int comm_index;

    if(F->comm_index < 0)
    {    
    	F->comm_index = poll_findslot();
    }
    comm_index = F->comm_index;

    /* Update the events */
    if (handler)
      {
        F->list = FDLIST_IDLECLIENT;
        pollfd_list.pollfds[comm_index].events |= event;
        pollfd_list.pollfds[comm_index].fd = fd;
        /* update maxindex here */
        if (comm_index > pollfd_list.maxindex)
            pollfd_list.maxindex = comm_index;
      }
    else
      {
	if (comm_index >= 0)
	  {
	    pollfd_list.pollfds[comm_index].events &= ~event;
	    if (pollfd_list.pollfds[comm_index].events == 0)
	      {
		pollfd_list.pollfds[comm_index].fd = -1;
		pollfd_list.pollfds[comm_index].revents = 0;
		F->comm_index = -1;
		F->list = FDLIST_NONE;

		/* update pollfd_list.maxindex here */
		if (comm_index == pollfd_list.maxindex)
		  while (pollfd_list.maxindex >= 0 &&
		  	pollfd_list.pollfds[pollfd_list.maxindex].fd == -1)
                    pollfd_list.maxindex--;
	      }
	  }
      }
}


/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Public functions */


/*
 * init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */
void init_netio(void)
{
    int fd;

    for (fd = 0; fd < MAXCONNECTIONS; fd++) {
       pollfd_list.pollfds[fd].fd = -1;
    }
    pollfd_list.maxindex = 0;
}

/*
 * comm_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
comm_setselect(int fd, fdlist_t list, unsigned int type, PF * handler,
    void *client_data, time_t timeout)
{  
    fde_t *F = &fd_table[fd];
    assert(fd >= 0);
    assert(F->flags.open);

    if (type & COMM_SELECT_READ) {
        F->read_handler = handler;
        F->read_data = client_data;
        poll_update_pollfds(fd, POLLRDNORM, handler);
    }
    if (type & COMM_SELECT_WRITE) {
        F->write_handler = handler;
        F->write_data = client_data;
        poll_update_pollfds(fd, POLLWRNORM, handler);
    }
    if (timeout)
        F->timeout = CurrentTime + (timeout / 1000);
}
 
/* int comm_select_fdlist(unsigned long delay)
 * Input: The maximum time to delay.
 * Output: Returns -1 on error, 0 on success.
 * Side-effects: Deregisters future interest in IO and calls the handlers
 *               if an event occurs for an FD.
 * Comments: Check all connections for new connections and input data
 * that is to be processed. Also check for connections with data queued
 * and whether we can write it out.
 * Called to do the new-style IO, courtesy of of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * comm_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */
int
comm_select(unsigned long delay)
{
 int num;
 int fd;
 int ci;
 PF *hdl;
  
 for (;;)
 {
  /* XXX kill that +1 later ! -- adrian */
  num = poll(pollfd_list.pollfds, pollfd_list.maxindex + 1, delay);
  if (num >= 0)
   break;
  if (ignoreErrno(errno))
   continue;
  /* error! */
  set_time();
  return -1;
  /* NOTREACHED */
 }
  
 /* update current time again, eww.. */
 set_time();
 callbacks_called += num;
 
 if (num == 0)
  return 0;
 /* XXX we *could* optimise by falling out after doing num fds ... */
 for (ci = 0; ci < pollfd_list.maxindex + 1; ci++)
 {
  fde_t *F;
  int revents;
  if (((revents = pollfd_list.pollfds[ci].revents) == 0) ||
      (pollfd_list.pollfds[ci].fd) == -1)
   continue;
  fd = pollfd_list.pollfds[ci].fd;
  F = &fd_table[fd];
  if (revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR))
  {
   hdl = F->read_handler;
   F->read_handler = NULL;
   poll_update_pollfds(fd, POLLRDNORM, NULL);
   if (hdl)
    hdl(fd, F->read_data);
  }
  if (revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR))
  {
   hdl = F->write_handler;
   F->write_handler = NULL;
   poll_update_pollfds(fd, POLLWRNORM, NULL);
   if (hdl)
    hdl(fd, F->write_data);
  }
 }
 return 0;
}

#endif
