/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_bsd_poll.c: POSIX poll() compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
 *  Copyright (C) 2002 ircd-ratbox development team
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
#ifdef USE_POLL
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

/* I hate linux -- adrian */
#ifndef POLLRDNORM
#define POLLRDNORM POLLIN
#endif
#ifndef POLLWRNORM
#define POLLWRNORM POLLOUT
#endif


static struct pollfd *pfds;
static int used_count = 0;
static fde_t **index_to_fde;

static void poll_update_pollfds(int, short, PF *);

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

/*
 * set and clear entries in the pollfds[] array.
 */
static void
poll_update_pollfds(int fd, short event, PF * handler)
{
	fde_t *F = &fd_table[fd];
	struct pollfd *pf;
	int comm_index;

	if(F->comm_index < 0)
	{
		used_count++;
		F->comm_index = used_count - 1;
		index_to_fde[F->comm_index] = F;
	}
	comm_index = F->comm_index;

	pf = &pfds[comm_index];


	/* Update the events */
	if(handler != NULL)
	{
		F->list = FDLIST_IDLECLIENT;
		pf->events |= event;
		pf->fd = fd;
	}
	else
	{
		if(comm_index >= 0)
		{
			pf->events &= ~event;
			if(pf->events == 0)
			{
				pf->fd = -1;
				pf->revents = 0;
				if(comm_index != used_count - 1)
				{
					index_to_fde[used_count - 1]->comm_index = comm_index;
					pfds[comm_index] = pfds[used_count - 1];
				}
				pfds[used_count - 1].fd = -1;
				F->comm_index = -1;
				F->list = FDLIST_NONE;
				used_count--;
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
void
init_netio(void)
{
	pfds = MyMalloc(MAXCONNECTIONS * sizeof(struct pollfd));
	index_to_fde = MyMalloc(MAXCONNECTIONS * sizeof(fde_t *));
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

	if(type & COMM_SELECT_READ)
	{
		F->read_handler = handler;
		F->read_data = client_data;
		poll_update_pollfds(fd, POLLRDNORM, handler);
	}
	if(type & COMM_SELECT_WRITE)
	{
		F->write_handler = handler;
		F->write_data = client_data;
		poll_update_pollfds(fd, POLLWRNORM, handler);
	}
	if(timeout)
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
 * Called to do the new-style IO, courtesy of squid (like most of this
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
	int rused;
	PF *hdl;

	for (;;)
	{
		/* XXX kill that +1 later ! -- adrian */
		num = poll(pfds, used_count, delay);
		if(num >= 0)
			break;
		if(ignoreErrno(errno))
			continue;
		/* error! */
		set_time();
		return -1;
		/* NOTREACHED */
	}

	/* update current time again, eww.. */
	set_time();
	callbacks_called += num;
	if(num == 0)
		return 0;
	rused = used_count;	/* Must safe this here..used_count can fall while looping */

	/* XXX we *could* optimise by falling out after doing num fds ... */
	for (ci = 0; ci < rused; ci++)
	{
		fde_t *F;
		int revents;
		if(((revents = pfds[ci].revents) == 0) || (pfds[ci].fd) == -1)
			continue;
		fd = pfds[ci].fd;
		F = &fd_table[fd];
		if(revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR))
		{
			hdl = F->read_handler;
			F->read_handler = NULL;
			if(hdl)
				hdl(fd, F->read_data);

		}

		if(F->flags.open == 0)
			continue;	/* Read handler closed us..go on */
		if(revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR))
		{
			hdl = F->write_handler;
			F->write_handler = NULL;
			if(hdl)
				hdl(fd, F->write_data);

		}

		if(F->read_handler == NULL)
			poll_update_pollfds(fd, POLLRDNORM, NULL);
		if(F->write_handler == NULL)
			poll_update_pollfds(fd, POLLWRNORM, NULL);

	}
	return 0;
}

#endif
