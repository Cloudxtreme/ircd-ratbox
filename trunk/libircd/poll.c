/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_bsd_poll.c: POSIX poll() compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
#include <sys/poll.h>
#include "ircd_lib.h"


/* I hate linux -- adrian */
#ifndef POLLRDNORM
#define POLLRDNORM POLLIN
#endif
#ifndef POLLWRNORM
#define POLLWRNORM POLLOUT
#endif

struct _pollfd_list
{
	struct pollfd *pollfds;
	int maxindex;		/* highest FD number */
	int allocated;		/* number of pollfds allocated */
};

typedef struct _pollfd_list pollfd_list_t;

pollfd_list_t pollfd_list;

int 
ircd_setup_fd(int fd)
{
        return 0;
}
        

/*
 * init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */
void
init_netio(void)
{
	int fd;
	pollfd_list.pollfds = ircd_malloc(ircd_getmaxconnect() * (sizeof(struct pollfd)));
	pollfd_list.allocated = ircd_getmaxconnect();
	for (fd = 0; fd < ircd_getmaxconnect(); fd++)
	{
		pollfd_list.pollfds[fd].fd = -1;
	}
	pollfd_list.maxindex = 0;
}

static inline void 
resize_pollarray(int fd)
{
	if(unlikely(fd >= pollfd_list.allocated))
	{
		int x, old_value = pollfd_list.allocated;
		pollfd_list.allocated += 1024;
		pollfd_list.pollfds = ircd_realloc(pollfd_list.pollfds, pollfd_list.allocated * (sizeof(struct pollfd)));
		memset(&pollfd_list.pollfds[old_value+1], 0, sizeof(struct pollfd) * 1024);
		for(x = old_value + 1; x < pollfd_list.allocated; x++)
		{
			pollfd_list.pollfds[x].fd = -1;
		}
	}	
}

/*
 * ircd_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
ircd_setselect(int fd, unsigned int type, PF * handler,
	       void *client_data)
{
	fde_t *F = find_fd(fd);
	int old_flags;
	
	if(F == NULL)
		return;

	old_flags = F->pflags;

	if(type & IRCD_SELECT_READ)
	{
		F->read_handler = handler;
		F->read_data = client_data;
		if(handler != NULL)
			F->pflags |= POLLRDNORM;
		else
			F->pflags &= ~POLLRDNORM;
	}
	if(type & IRCD_SELECT_WRITE)
	{
		F->write_handler = handler;
		F->write_data = client_data;
		if(handler != NULL)
			F->pflags |= POLLWRNORM;
		else
			F->pflags &= ~POLLWRNORM;
	}

	resize_pollarray(fd);
	
	if(F->pflags <= 0)
	{
		pollfd_list.pollfds[fd].events = 0;
		pollfd_list.pollfds[fd].fd = -1;
		if(fd == pollfd_list.maxindex)
		{
			while (pollfd_list.maxindex >= 0 && pollfd_list.pollfds[pollfd_list.maxindex].fd == -1)
				pollfd_list.maxindex--;	
		}
	} else {
		pollfd_list.pollfds[fd].events = F->pflags;
		pollfd_list.pollfds[fd].fd = fd;
		if(fd > pollfd_list.maxindex)
			pollfd_list.maxindex = fd;
	}
	
}

/* int ircd_select(unsigned long delay)
 * Input: The maximum time to delay.
 * Output: Returns -1 on error, 0 on success.
 * Side-effects: Deregisters future interest in IO and calls the handlers
 *               if an event occurs for an FD.
 * Comments: Check all connections for new connections and input data
 * that is to be processed. Also check for connections with data queued
 * and whether we can write it out.
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * ircd_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */
int
ircd_select(unsigned long delay)
{
	int num;
	int fd;
	int ci;
	PF *hdl;
	void *data;
	for (;;)
	{
		num = poll(pollfd_list.pollfds, pollfd_list.maxindex + 1, delay);
		if(num >= 0)
			break;
		if(ignoreErrno(errno))
			continue;
		/* error! */
		ircd_set_time();
		return -1;
		/* NOTREACHED */
	}

	/* update current time again, eww.. */
	ircd_set_time();

	if(num == 0)
		return 0;
	/* XXX we *could* optimise by falling out after doing num fds ... */
	for (ci = 0; ci < pollfd_list.maxindex + 1; ci++)
	{
		fde_t *F;
		int revents;
		if(((revents = pollfd_list.pollfds[ci].revents) == 0) ||
		   (pollfd_list.pollfds[ci].fd) == -1)
			continue;
		fd = pollfd_list.pollfds[ci].fd;
		F = find_fd(fd);
		if(F == NULL)
			continue;
		
		if(revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR))
		{
			hdl = F->read_handler;
			data = F->read_data;
			F->read_handler = NULL;
			F->read_data = NULL;
			if(hdl)
				hdl(fd, data);
		}
	
		if(revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR))
		{
			hdl = F->write_handler;
			data = F->write_data;
			F->write_handler = NULL;
			F->write_data = NULL;
			if(hdl)
				hdl(fd, data);
		}
	}
	return 0;
}

