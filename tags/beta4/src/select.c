/************************************************************************
 *
 * s_bsd_select.c - code implementing a select IO loop
 *   By Adrian Chadd <adrian@creative.net.au>
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
#ifdef USE_SELECT
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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>    /* NOFILE */
#endif
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>

/*
 * Note that this is only a single list - multiple lists is kinda pointless
 * under select because the list size is a function of the highest FD :-)
 *   -- adrian
 */

fd_set select_readfds;
fd_set select_writefds;

/*
 * You know, I'd rather have these local to comm_select but for some
 * reason my gcc decides that I can't modify them at all..
 *   -- adrian
 */
fd_set tmpreadfds;
fd_set tmpwritefds;

static void select_update_selectfds(int fd, short event, PF *handler);

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

/*
 * set and clear entries in the select array ..
 */ 
static void
select_update_selectfds(int fd, short event, PF *handler)
{  
    /* Update the read / write set */
    if (event & COMM_SELECT_READ) {
        if (handler)
            FD_SET(fd, &select_readfds);
        else
            FD_CLR(fd, &select_readfds);
    }
    if (event & COMM_SELECT_WRITE) {
        if (handler)
            FD_SET(fd, &select_writefds);
        else
            FD_CLR(fd, &select_writefds);
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
    FD_ZERO(&select_readfds);
    FD_ZERO(&select_writefds);
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

#ifdef NOTYET
    debug(5, 5) ("commSetSelect: FD %d type %d, %s\n", fd, type, handler ? "SET"
 : "CLEAR");
#endif
    if (type & COMM_SELECT_READ) {
        F->read_handler = handler;
        F->read_data = client_data;
        select_update_selectfds(fd, COMM_SELECT_READ, handler);
    }
    if (type & COMM_SELECT_WRITE) {
        F->write_handler = handler;
        F->write_data = client_data;
        select_update_selectfds(fd, COMM_SELECT_WRITE, handler);
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
 * Do IO events
 */

int
comm_select(time_t delay)
{
    int num;
    int fd;
    PF *hdl;
    fde_t *F;
    struct timeval to;

    /* Copy over the read/write sets so we don't have to rebuild em */
    bcopy(&select_readfds, &tmpreadfds, sizeof(fd_set));
    bcopy(&select_writefds, &tmpwritefds, sizeof(fd_set));

    for (;;) {
        to.tv_sec = delay;
        to.tv_usec = 0;
        num = select(highest_fd + 1, &tmpreadfds, &tmpwritefds, NULL, &to);
        if (num >= 0)
            break;
        if (ignoreErrno(errno))
            continue;
        set_time();
        /* error! */
        return -1;
        /* NOTREACHED */
    }
    callbacks_called += num;
    set_time();

    if (num == 0)
        return 0;

    /* XXX we *could* optimise by falling out after doing num fds ... */
    for (fd = 0; fd < highest_fd + 1; fd++) {
        F = &fd_table[fd];

        if (FD_ISSET(fd, &tmpreadfds)) {
            hdl = F->read_handler;
            F->read_handler = NULL;
            select_update_selectfds(fd, COMM_SELECT_READ, NULL);
            if (!hdl) {
                /* XXX Eek! This is another bad place! */
            } else {
                hdl(fd, F->read_data);
            }
        }
        if (FD_ISSET(fd, &tmpwritefds)) {
            hdl = F->write_handler;
            F->write_handler = NULL;
            select_update_selectfds(fd, COMM_SELECT_WRITE, NULL);
            if (!hdl) {
                /* XXX Eek! This is another bad place! */
            } else {
                hdl(fd, F->write_data);
            }
        }
    }
    return 0;
}

#endif /* USE_SELECT */
