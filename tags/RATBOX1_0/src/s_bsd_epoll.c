/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  s_bsd_epoll.c: Linux epoll compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
 *  Copyright (C) 2002 ircd-ratbox development team
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
 *  $Id$
 */

#include "config.h"
#ifdef USE_EPOLL
#include "stdinc.h"

#include <sys/poll.h>

#include "fdlist.h"
#include "s_bsd.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
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


#define EPOLL_LENGTH 256

static int ep; /* epoll file descriptor */



/* XXX: This ifdef needs to be fixed once epoll is rolled into glibc someday */

#ifndef HAVE_EPOLL_LIB 


#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,46)
#error "epoll support requires kernel headers newer than 2.5.46..try rtsigio instead"
#endif


#include <linux/unistd.h>

#define EP_CTL_ADD 1
#define EP_CTL_DEL 2
#define EP_CTL_MOD 3

#define __NR_epoll_create __NR_sys_epoll_create
#define __NR_epoll_ctl __NR_sys_epoll_ctl
#define __NR_epoll_wait __NR_sys_epoll_wait


static _syscall1(int, epoll_create, int, maxfds);
static _syscall4(int, epoll_ctl, int, epfd, int, op, int, fd, unsigned int, events);
static _syscall4(int, epoll_wait, int, epfd, struct pollfd *, pevents, int, maxevents, int, timeout);

#endif /* HAVE_EPOLL_LIB */


/*
 * init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */
void init_netio(void)
{
  ep = epoll_create(MAX_CLIENTS);
  if (ep < 0)
    {
      ilog(L_CRIT, "init_netio: Couldn't open epoll fd!\n");
      exit(115); /* Whee! */
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
  int old_flags = F->pflags;
  int op = 0;
  assert(fd >= 0);
  assert(F->flags.open);

  /* Update the list, even though we're not using it .. */
  F->list = list;

  if (type & COMM_SELECT_READ)
    {
      if(handler != NULL)
        F->pflags |= POLLIN | POLLHUP | POLLERR;
      else 
        F->pflags &= ~(POLLIN|POLLHUP|POLLERR); 
      
      F->read_handler = handler;
      F->read_data = client_data;
    }
  
  if (type & COMM_SELECT_WRITE)
    {
      if(handler != NULL)
        F->pflags = POLLOUT | POLLHUP | POLLERR;
      else
        F->pflags &= ~(POLLOUT|POLLHUP|POLLERR);
      F->write_handler = handler;
      F->write_data = client_data;
    }
  
  if(old_flags != 0)
  {
    if(F->pflags == 0)
      op = EP_CTL_DEL;
    else
      op = EP_CTL_MOD;
  } else {
    op = EP_CTL_ADD;
  }
 
   
  if (timeout)
    F->timeout = CurrentTime + (timeout / 1000);

  /* No changes...do nothing */
  if(old_flags == F->pflags)
    return;

  if(epoll_ctl(ep, op, fd, F->pflags) != 0)
  {
    ilog(L_ERROR, "comm_setselect(): epoll_ctl failed: %s", strerror(errno));
  }
  

}
 
/*
 * comm_select
 *
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * comm_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */

int
comm_select(unsigned long delay)
{
  int num, i;
  static struct pollfd pfd[EPOLL_LENGTH];

  num = epoll_wait(ep, pfd, EPOLL_LENGTH, delay);
  set_time();
  
  if(num < 0 && !ignoreErrno(errno))
  {
	return COMM_ERROR;
  }
  
  if(num == 0)
    return COMM_OK;
    
  for(i = 0; i < num; i++)
  {
    PF *hdl;
    int fd = pfd[i].fd;
    fde_t *F = &fd_table[fd];
    if(pfd[i].revents & (POLLIN | POLLHUP | POLLERR))
    {
      callbacks_called++;
      hdl = F->read_handler;
      if(hdl) 
        hdl(fd, F->read_data); 
    }
    if(pfd[i].revents & (POLLOUT | POLLHUP | POLLERR))
    {
      callbacks_called++;
      hdl = F->write_handler;
      if(hdl)
        hdl(fd, F->write_data);
    } 
  }
  return COMM_OK;
}

#endif /* USE_EPOLL */
