/************************************************************************
 *
 * s_bsd_kqueue.c - code implementing a kqueue IO loop
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
#ifdef USE_KQUEUE
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
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/param.h>    /* NOFILE */
#include <arpa/inet.h>


#define KE_LENGTH	128

static void kq_update_events(int, short, PF *);
static int kq;
static struct timespec zero_timespec;

static struct kevent *kqlst;	/* kevent buffer */
static int kqmax;		/* max structs to buffer */
static int kqoff;		/* offset into the buffer */


/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

void
kq_update_events(int fd, short filter, PF * handler)
{
    PF *cur_handler;
#if 0
    int retval;
#endif

    switch (filter) {
    case EVFILT_READ:
        cur_handler = fd_table[fd].read_handler;
        break;
    case EVFILT_WRITE:
        cur_handler = fd_table[fd].write_handler;
        break;
    default:
        /* XXX bad! -- adrian */
        return;
        break;
    }

    if ((cur_handler == NULL && handler != NULL)
        ||
       (cur_handler != NULL && handler == NULL)) {
        struct kevent *kep;

	kep = kqlst + kqoff;

        kep->ident = (u_long) fd;
        kep->filter = filter;

	/* jlemon didn't define this in fbsd 4.2 argh! -db */
#ifdef NOTE_LOWAT
	kep->fflags = NOTE_LOWAT;
	kep->data = 1;
#endif

        if (handler != NULL) {
		if (filter == EVFILT_WRITE)
			kep->flags = (EV_ADD | EV_ONESHOT);
		else
			kep->flags = EV_ADD;
	} else {
		kep->flags = EV_DELETE;
	}
	if (kqoff == kqmax) {
		kevent(kq, kqlst, kqoff, NULL, 0, &zero_timespec);
		kqoff = 0;
	} else {
		kqoff++;
	}
#if 0
        if (retval < 0)
            /* Error! */
        if (ke.flags & EV_ERROR) {
            errno = ke.data;
        }
#endif
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
    kq = kqueue();
    if (kq < 0) {
        log(L_CRIT, "init_netio: Couldn't open kqueue fd!\n");
        exit(115); /* Whee! */
    }
    kqmax = getdtablesize();
    kqlst = MyMalloc(sizeof(*kqlst) * kqmax);
    zero_timespec.tv_sec = 0;
    zero_timespec.tv_nsec = 0;
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
        kq_update_events(fd, EVFILT_READ, handler);
        F->read_handler = handler;
        F->read_data = client_data;
    }
    if (type & COMM_SELECT_WRITE) {
        kq_update_events(fd, EVFILT_WRITE, handler);
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
    static struct kevent ke[KE_LENGTH];
    struct timespec poll_time;

    do {
        /*
         * remember we are doing NANOseconds here, not micro/milli. God knows
         * why jlemon used a timespec, but hey, he wrote the interface, not I
         *   -- Adrian
         */
        poll_time.tv_sec = (time_t) delay;
        poll_time.tv_nsec = (long) 0;
        for (;;) {
            num = kevent(kq, kqlst, kqoff, ke,  KE_LENGTH, &poll_time);
	    kqoff = 0;
            if (num >= 0)
                break;
            if (ignoreErrno(errno))
                break;
            set_time();
            return COMM_ERROR;
            /* NOTREACHED */
        }

        set_time();
        if (num == 0)
            continue;
        callbacks_called += num;
        
        for (i = 0; i < num; i++) {
            int fd = (int) ke[i].ident;
            PF *hdl = NULL;
            fde_t *F = &fd_table[fd];
            if (ke[i].flags & EV_ERROR) {
                errno = ke[i].data;
                /* XXX error == bad! -- adrian */
                continue; /* XXX! */
            }
            switch (ke[i].filter) {
            case EVFILT_READ:
                if ((hdl = F->read_handler) != NULL) {
                    F->read_handler = NULL;
                    hdl(fd, F->read_data);
                }
            case EVFILT_WRITE:
                if ((hdl = F->write_handler) != NULL) {
                    F->write_handler = NULL;
                    hdl(fd, F->write_data);
                }
            default:
                /* Bad! -- adrian */
                break;
            }
        }
        return COMM_OK;
    } while (0); /* XXX should rip this out! -- adrian */
    /* XXX Get here, we broke! */
    return 0;
}

#endif /* USE_KQUEUE */
