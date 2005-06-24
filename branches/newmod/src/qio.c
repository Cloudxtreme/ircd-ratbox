/*
 *  ircd-ratbox: A slightly useful ircd.
 *  qio.c: VMS sys$qio() compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2003 Edward Brocklesby <ejb@lythe.org.uk>
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
#ifdef USE_QIO
#include "stdinc.h"

#include "commio.h"
#include "class.h"
#include "rpi/client.h"
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
#include "commio.h"
#include "memory.h"

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

short iosb[4];

static int
ircd$ast_handler_write(int fd)
{
	fde_t *F = &fd_table[fd];
	if (!F->write_handler)
		return 0;
	F->write_handler(fd, F->write_data);
	return 0;
}

static int
ircd$ast_handler_read(int fd)
{
	fde_t *F = &fd_table[fd];

	if (!F->read_handler)
		return 0;

	F->read_handler(fd, F->read_data);
	return 0;
}

static void
ircd$install_ast_write(int fd)
{
	long status;

	if (decc$get_sdc(fd) == 0)
		return;

	status = sys$qiow(0, decc$get_sdc(fd), IO$_SETMODE | IO$M_WRTATTN,
			  iosb, 0, 0, 0, 0, 0, 0, 0, 0);
	if (!$VMS_STATUS_SUCCESS(status))
		sys$exit(status);

	status = sys$qiow(0, decc$get_sdc(fd), IO$_SETMODE | IO$M_WRTATTN,
			  iosb, 0, 0, ircd$ast_handler_write, fd, 0, 0, 0, 0);
	if (!$VMS_STATUS_SUCCESS(status))
		sys$exit(status);
}

static void
ircd$remove_ast_write(int fd)
{
	if (decc$get_sdc(fd) == 0)
		return;

	sys$qiow(0, decc$get_sdc(fd), IO$_SETMODE | IO$M_WRTATTN,
		 iosb, 0, 0, 0, 0, 0, 0, 0, 0);
}

static void
ircd$install_ast_read(int fd)
{
	long status;

	if (decc$get_sdc(fd) == 0)
		return;

	status = sys$qiow(0, decc$get_sdc(fd), IO$_SETMODE | IO$M_READATTN,
			  iosb, 0, 0, 0, 0, 0, 0, 0, 0);
	if (!$VMS_STATUS_SUCCESS(status))
		sys$exit(status);

	status = sys$qiow(0, decc$get_sdc(fd), IO$_SETMODE | IO$M_READATTN,
			  iosb, 0, 0, ircd$ast_handler_read, fd, 0, 0, 0, 0);
	if (!$VMS_STATUS_SUCCESS(status))
		sys$exit(status);
}

static void
ircd$remove_ast_read(int fd)
{
	if (decc$get_sdc(fd) == 0)
		return;

	sys$qiow(0, decc$get_sdc(fd), IO$_SETMODE | IO$M_READATTN,
		iosb, 0, 0, 0, 0, 0, 0, 0, 0);
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
	s_assert(fd >= 0);
	s_assert(F->flags.open);

	F->list = list;

	if(type & COMM_SELECT_READ)
	{
		if (handler)
			ircd$install_ast_read(fd);
		else
			ircd$remove_ast_read(fd);

		F->read_handler = handler;
		F->read_data = client_data;
	}
	if(type & COMM_SELECT_WRITE)
	{
		if (handler)
			ircd$install_ast_write(fd);
		else
			ircd$remove_ast_write(fd);

		F->write_handler = handler;
		F->write_data = client_data;
	}

	if(timeout)
		F->timeout = CurrentTime + (timeout / 1000);
}

/*
 * Check all connections for new connections and input data that is to be
 * processed. Also check for connections with data queued and whether we can
 * write it out.
 */

/*
 * comm_select
 *
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * comm_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */
/*
 * Actually, on VMS this doesn't actually do anything except sleep
 * for a while.  All the callbacks are called from the fd's AST
 * handler.
 */

int
comm_select(unsigned long delay)
{
	sys$hiber();
	/* NOTREACHED */ return 0;
}

#endif /* USE_QIO */
