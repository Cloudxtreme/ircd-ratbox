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

pollfd_list_t pollfd_lists[FDLIST_MAX];

static void poll_update_pollfds(int, fdlist_t, short, PF *);

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

/*
 * find a spare slot in the fd list. We can optimise this out later!
 *   -- adrian
 */
static int
poll_findslot(fdlist_t list)
{
    int i;
    pollfd_list_t *pf = &pollfd_lists[list];
    for (i = 0; i < MAXCONNECTIONS; i++) {
        if (pf->pollfds[i].fd == -1) {
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
poll_update_pollfds(int fd, fdlist_t list, short event, PF * handler)
{  
    fde_t *F = &fd_table[fd];
    pollfd_list_t *pf, *npf;
    int comm_index, ncomm_index;

    /* First, see if we have to shift this across to a new list */
    if (list != F->list) {
        npf = &pollfd_lists[list];
        pf = &pollfd_lists[F->list];
        comm_index = F->comm_index;
        ncomm_index = poll_findslot(list);
        F->comm_index = ncomm_index;

        /* Put the client on the new list */
        assert(npf->pollfds[ncomm_index].fd == -1);
        npf->pollfds[ncomm_index].fd = fd;
        npf->pollfds[ncomm_index].revents = 0; /* Being paranoid */
        npf->pollfds[ncomm_index].events = 0;

        if (comm_index >= 0)
        {
          /* Copy over old events, and clear the old entry */
          npf->pollfds[ncomm_index].events = pf->pollfds[comm_index].events;
          pf->pollfds[comm_index].fd = -1;
          pf->pollfds[comm_index].events = 0;
          pf->pollfds[comm_index].revents = 0;
        }

        /* update maxindex here */
        if (comm_index == pf->maxindex)
            while( pf->pollfds[pf->maxindex].fd == -1 && pf->maxindex >= 0 )
              pf->maxindex--;

        if (ncomm_index > npf->maxindex)
            npf->maxindex = ncomm_index;
    }

    /* Reset stuff here so we can keep the code simple for now */
    pf = &pollfd_lists[list];
    comm_index = F->comm_index;
    F->list = list;

    /* Update the events */
    if (handler)
      {
        pf->pollfds[comm_index].events |= event;
        pf->pollfds[comm_index].fd = fd;
        /* update maxindex here */
        if (comm_index > pf->maxindex)
            pf->maxindex = comm_index;
      }
    else
      {
	if (comm_index >= 0)
	  {
	    pf->pollfds[comm_index].events &= ~event;
	    if (pf->pollfds[comm_index].events == 0)
	      {
		pf->pollfds[comm_index].fd = -1;
		pf->pollfds[comm_index].revents = 0;
		F->comm_index = -1;
		F->list = FDLIST_NONE;

		/* update pf->maxindex here */
		if (comm_index == pf->maxindex)
		  while( pf->pollfds[pf->maxindex].fd == -1 && 
			 pf->maxindex >= 0 )
                    pf->maxindex--;
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
    int i, fd;

    for (i = 0; i < FDLIST_MAX; i++) {
        for (fd = 0; fd < MAXCONNECTIONS; fd++) {
            pollfd_lists[i].pollfds[fd].fd = -1;
        }
        pollfd_lists[i].maxindex = 0;
    }
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
#ifndef NDEBUG
    if(list != FDLIST_NONE)
    	assert(handler != NULL);
#endif

#ifdef NOTYET
    debug(5, 5) ("commSetSelect: FD %d type %d, %s\n", fd, type, handler ? "SET"
 : "CLEAR");
#endif
    if (type & COMM_SELECT_READ) {
        F->read_handler = handler;
        F->read_data = client_data;
        poll_update_pollfds(fd, list, POLLRDNORM, handler);
    }
    if (type & COMM_SELECT_WRITE) {
        F->write_handler = handler;
        F->write_data = client_data;
        poll_update_pollfds(fd, list, POLLWRNORM, handler);
    }
    if (timeout)
        F->timeout = CurrentTime + (timeout / 1000);
}
 
/* int comm_select_fdlist(fdlist_t fdlist, unsigned long delay)
 * Input: The list to select, the maximum time to delay on this list.
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
static int
comm_select_fdlist(fdlist_t fdlist, unsigned long delay)
{
 int num;
 int fd;
 int ci;
 PF *hdl;
 pollfd_list_t *pf = &pollfd_lists[fdlist];
  
 for (;;)
 {
  /* XXX kill that +1 later ! -- adrian */
  num = poll(pf->pollfds, pf->maxindex + 1, delay);
  if (num >= 0)
   break;
  if (ignoreErrno(errno))
   continue;
  set_time();
  /* error! */
  return -1;
  /* NOTREACHED */
 }
  
 /* update current time again, eww.. */
 set_time();
 callbacks_called += num;
 
 if (num == 0)
  return 0;
 /* XXX we *could* optimise by falling out after doing num fds ... */
 for (ci = 0; ci < pf->maxindex + 1; ci++)
 {
  fde_t *F;
  int revents;
  if (((revents = pf->pollfds[ci].revents) == 0) ||
      (pf->pollfds[ci].fd) == -1)
   continue;
  fd = pf->pollfds[ci].fd;
  F = &fd_table[fd];
  if (revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR))
  {
   hdl = F->read_handler;
   F->read_handler = NULL;
   poll_update_pollfds(fd, fdlist, POLLRDNORM, NULL);
   if (hdl)
    hdl(fd, F->read_data);
  }
  if (revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR))
  {
   hdl = F->write_handler;
   F->write_handler = NULL;
   poll_update_pollfds(fd, fdlist, POLLWRNORM, NULL);
   if (hdl)
    hdl(fd, F->write_data);
  }
 }
 return 0;
}

/*
 * Note that I haven't really implemented the multiple FD list code in
 * hyb-7 yet, because I haven't got a server doing some decent traffic
 * to profile. I'll get around to it.
 *
 * If you run a big server, you should be running freebsd-stable + kqueue!
 *
 *   -- adrian
 */
int
comm_select(unsigned long delay)
{
    comm_select_fdlist(FDLIST_SERVICE, 0);
    comm_select_fdlist(FDLIST_SERVER, 0);
    /* comm_select_fdlist(FDLIST_BUSYCLIENT, 0); */
    comm_select_fdlist(FDLIST_IDLECLIENT, delay);
    return 0;
}
#endif
