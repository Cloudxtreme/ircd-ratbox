/************************************************************************
 *
 * s_bsd_devpoll.c - code implementing a /dev/poll IO loop
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
#ifdef USE_DEVPOLL
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
#include <sys/types.h>
#include <sys/devpoll.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/param.h>    /* NOFILE */
#include <arpa/inet.h>
/* I don't know if this is right or not.. -AS */
#define POLL_LENGTH	16
static void devpoll_update_events(int, short, PF *);
static int dpfd;

/* STATIC */
static void devpoll_update_events(int, short, PF *);  
static void devpoll_write_update(int, int);

/* static void devpoll_incoming_stats(StoreEntry *); */
/* Does this do anything?? -AS */
#define NOTYET 1

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

/*
 * Write an update to the devpoll filter.
 * See, we end up having to do a seperate (?) remove before we do an
 * add of a new polltype, so we have to have this function seperate from
 * the others.
 */
static void
devpoll_write_update(int fd, int events)
{
    struct pollfd pollfds[1]; /* Just to be careful */
    int retval;
#ifdef NOTYET
    log(L_NOTICE, "devpoll_write_update: FD %d: called with %d\n", fd, events);
#endif
    /* Build the pollfd entry */
    pollfds[0].revents = 0;
    pollfds[0].fd = fd;
    pollfds[0].events = events;
 
    /* Write the thing to our poll fd */
    retval = write(dpfd, &pollfds[0], sizeof(struct pollfd));
#ifdef NOTYET
    if (retval < 0)
        log(L_NOTICE, "devpoll_write_update: dpfd write failed %d: %s\n", errno, strerror(errno));
#endif
    /* Done! */
}

void
devpoll_update_events(int fd, short filter, PF * handler)
{
    int update_required = 0;
    int events = 0;
    PF *cur_handler;
     
    switch (filter) {
    case COMM_SELECT_READ:
        cur_handler = fd_table[fd].read_handler;
        if (handler)
            events |= POLLRDNORM;
        if (fd_table[fd].write_handler)
            events |= POLLWRNORM;
        break;
    case COMM_SELECT_WRITE:
        cur_handler = fd_table[fd].write_handler;
        if (handler)
            events |= POLLWRNORM;
        if (fd_table[fd].read_handler)
            events |= POLLRDNORM;
        break;
    default:
#ifdef NOTYET
        log(L_NOTICE,"devpoll_update_events called with unknown filter: %hd\n",
            filter);
#endif
        return;
        break;
    }

    if (cur_handler == NULL && handler != NULL)
        update_required++;
    else if (cur_handler != NULL && handler == NULL)
        update_required++;

    if (update_required) {
        /*
         * Ok, we can call devpoll_write_update() here now to re-build the
         * fd struct. If we end up with nothing on this fd, it won't write
         * anything.
         */
        if (events)
            devpoll_write_update(fd, events);
        else
            devpoll_write_update(fd, POLLREMOVE);
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
    dpfd = open("/dev/poll", O_RDWR);
    if (dpfd < 0) {
        log(L_CRIT, "init_netio: Couldn't open /dev/poll - %d: %s\n", errno,
	    strerror(errno));
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
    assert(fd >= 0);
    assert(F->flags.open);

    /* Update the list, even though we're not using it .. */
    F->list = list;

    if (type & COMM_SELECT_READ) {
        devpoll_update_events(fd, COMM_SELECT_READ, handler);
        F->read_handler = handler;
        F->read_data = client_data;
    }
    if (type & COMM_SELECT_WRITE) {
        devpoll_update_events(fd, COMM_SELECT_WRITE, handler);
        F->write_handler = handler;
        F->write_data = client_data;
    }
    if (timeout)
        F->timeout = CurrentTime + timeout;

}
 
/*
 * Check all connections for new connections and input data that is to be
 * processed. Also check for connections with data queued and whether we can
 * write it out.
 */

/*
 * comm_select
 *
 * Called to do the new-style IO, courtesy of of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * comm_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */

int
comm_select(time_t delay)
{
    int num, i;
    struct pollfd pollfds[POLL_LENGTH];
    struct dvpoll dopoll;

    do {
        for (;;) {
            dopoll.dp_timeout = delay * 1000;
            dopoll.dp_nfds = POLL_LENGTH;
            dopoll.dp_fds = &pollfds[0];

            num = ioctl(dpfd, DP_POLL, &dopoll);

            if (num >= 0)
                break;
            if (ignoreErrno(errno))
                break;
            return COMM_ERROR;
            /* NOTREACHED */
        }
    
        if (num == 0)
            continue;
        
        for (i = 0; i < num; i++) {
            int fd = dopoll.dp_fds[i].fd;
            PF *hdl = NULL;
            fde_t *F = &fd_table[fd];

            if (dopoll.dp_fds[i].events & 
              (POLLRDNORM | POLLIN | POLLHUP | POLLERR)) {
                if ((hdl = F->read_handler) != NULL) {
                    F->read_handler = NULL;
                    hdl(fd, F->read_data);
                    /*
                     * this call used to be with a NULL pointer, BUT
                     * in the devpoll case we only want to update the
                     * poll set *if* the handler changes state (active ->
                     * NULL or vice versa.)
                     */
                    devpoll_update_events(fd, COMM_SELECT_READ,
                      F->read_handler);
                }
            }
            if (dopoll.dp_fds[i].events &
              (POLLWRNORM | POLLOUT | POLLHUP | POLLERR)) {
                if ((hdl = F->write_handler) != NULL) {
                    F->write_handler = NULL;
                    hdl(fd, F->write_data);
                    /* See above similar code in the read case */
                    devpoll_update_events(fd, COMM_SELECT_WRITE,
                      F->read_handler);
                }
            }
        }
        return COMM_OK;
    } while (0); /* XXX should rip this out! -- adrian */
    /* XXX Get here, we broke! */
    return 0;
}

#endif /* USE_DEVPOLL */
