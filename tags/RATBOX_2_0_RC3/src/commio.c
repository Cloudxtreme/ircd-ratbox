/*
 *  ircd-ratbox: A slightly useful ircd.
 *  commio.c: Network/file related functions
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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

#include "stdinc.h"
#include "config.h"
#include "commio.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "event.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "linebuf.h"
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
#include "reject.h"
#include "memory.h"


#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET        0x7f
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

const char *const NONB_ERROR_MSG = "set_non_blocking failed for %s:%s";
const char *const SETBUF_ERROR_MSG = "set_sock_buffers failed for server %s:%s";

static const char *comm_err_str[] = { "Comm OK", "Error during bind()",
	"Error during DNS lookup", "connect timeout",
	"Error during connect()",
	"Comm Error"
};

fde_t *fd_table = NULL;

static void fdlist_update_biggest(int fd, int opening);

/* Highest FD and number of open FDs .. */
int highest_fd = -1;		/* Its -1 because we haven't started yet -- adrian */
int number_fd = 0;


static void comm_connect_callback(int fd, int status);
static PF comm_connect_timeout;
static void comm_connect_dns_callback(void *vptr, adns_answer * reply);
static PF comm_connect_tryconnect;

/* close_all_connections() can be used *before* the system come up! */

void
comm_close_all(void)
{
	int i;
#ifndef NDEBUG
	int fd;
#endif

	/* XXX someone tell me why we care about 4 fd's ? */
	/* XXX btw, fd 3 is used for profiler ! */
#if 0
#ifndef __VMS
	for (i = 0; i < MAXCONNECTIONS; ++i)
#else
	for (i = 3; i < MAXCONNECTIONS; ++i)
#endif
#endif

		for (i = 4; i < MAXCONNECTIONS; ++i)
		{
			if(fd_table[i].flags.open)
				fd_close(i);
			else
				close(i);
		}

	/* XXX should his hack be done in all cases? */
#ifndef NDEBUG
	/* fugly hack to reserve fd == 2 */
	(void) close(2);
	fd = open("stderr.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
	if(fd >= 0)
	{
		dup2(fd, 2);
		close(fd);
	}
#endif
}

/*
 * get_sockerr - get the error value from the socket or the current errno
 *
 * Get the *real* error from the socket (well try to anyway..).
 * This may only work when SO_DEBUG is enabled but its worth the
 * gamble anyway.
 */
int
comm_get_sockerr(int fd)
{
	int errtmp = errno;
#ifdef SO_ERROR
	int err = 0;
	socklen_t len = sizeof(err);

	if(-1 < fd && !getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *) &err, (socklen_t *) & len))
	{
		if(err)
			errtmp = err;
	}
	errno = errtmp;
#endif
	return errtmp;
}

/*
 * set_sock_buffers - set send and receive buffers for socket
 * 
 * inputs	- fd file descriptor
 * 		- size to set
 * output       - returns true (1) if successful, false (0) otherwise
 * side effects -
 */
int
comm_set_buffers(int fd, int size)
{
	if(setsockopt
	   (fd, SOL_SOCKET, SO_RCVBUF, (char *) &size, sizeof(size))
	   || setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *) &size, sizeof(size)))
		return 0;
	return 1;
}

/*
 * set_non_blocking - Set the client connection into non-blocking mode. 
 *
 * inputs	- fd to set into non blocking mode
 * output	- 1 if successful 0 if not
 * side effects - use POSIX compliant non blocking and
 *                be done with it.
 */
int
comm_set_nb(int fd)
{
#ifndef __VMS
	int nonb = 0;
	int res;

	nonb |= O_NONBLOCK;
	res = fcntl(fd, F_GETFL, 0);
	if(-1 == res || fcntl(fd, F_SETFL, res | nonb) == -1)
		return 0;

	fd_table[fd].flags.nonblocking = 1;
	return 1;
#else
	int val = 1;
	int res;

	res = ioctl(fd, FIONBIO, &val);
	if(res == -1)
		return 0;

	fd_table[fd].flags.nonblocking = 1;
	return 1;
#endif
}


/*
 * stolen from squid - its a neat (but overused! :) routine which we
 * can use to see whether we can ignore this errno or not. It is
 * generally useful for non-blocking network IO related errnos.
 *     -- adrian
 */
int
ignoreErrno(int ierrno)
{
	switch (ierrno)
	{
	case EINPROGRESS:
	case EWOULDBLOCK:
#if EAGAIN != EWOULDBLOCK
	case EAGAIN:
#endif
	case EALREADY:
	case EINTR:
#ifdef ERESTART
	case ERESTART:
#endif
		return 1;
	default:
		return 0;
	}
}


/*
 * comm_settimeout() - set the socket timeout
 *
 * Set the timeout for the fd
 */
void
comm_settimeout(int fd, time_t timeout, PF * callback, void *cbdata)
{
	fde_t *F;
	s_assert(fd >= 0);
	F = &fd_table[fd];
	s_assert(F->flags.open);

	F->timeout = CurrentTime + (timeout / 1000);
	F->timeout_handler = callback;
	F->timeout_data = cbdata;
}


/*
 * comm_setflush() - set a flush function
 *
 * A flush function is simply a function called if found during
 * comm_timeouts(). Its basically a second timeout, except in this case
 * I'm too lazy to implement multiple timeout functions! :-)
 * its kinda nice to have it seperate, since this is designed for
 * flush functions, and when comm_close() is implemented correctly
 * with close functions, we _actually_ don't call comm_close() here ..
 */
void
comm_setflush(int fd, time_t timeout, PF * callback, void *cbdata)
{
	fde_t *F;
	s_assert(fd >= 0);
	F = &fd_table[fd];
	s_assert(F->flags.open);

	F->flush_timeout = CurrentTime + (timeout / 1000);
	F->flush_handler = callback;
	F->flush_data = cbdata;
}


/*
 * comm_checktimeouts() - check the socket timeouts
 *
 * All this routine does is call the given callback/cbdata, without closing
 * down the file descriptor. When close handlers have been implemented,
 * this will happen.
 */
void
comm_checktimeouts(void *notused)
{
	int fd;
	PF *hdl;
	void *data;
	fde_t *F;
	for (fd = 0; fd <= highest_fd; fd++)
	{
		F = &fd_table[fd];
		if(!F->flags.open)
			continue;
		if(F->flags.closing)
			continue;

		/* check flush functions */
		if(F->flush_handler &&
		   F->flush_timeout > 0 && F->flush_timeout < CurrentTime)
		{
			hdl = F->flush_handler;
			data = F->flush_data;
			comm_setflush(F->fd, 0, NULL, NULL);
			hdl(F->fd, data);
		}

		/* check timeouts */
		if(F->timeout_handler &&
		   F->timeout > 0 && F->timeout < CurrentTime)
		{
			/* Call timeout handler */
			hdl = F->timeout_handler;
			data = F->timeout_data;
			comm_settimeout(F->fd, 0, NULL, NULL);
			hdl(F->fd, F->timeout_data);
		}
	}
}

/*
 * void comm_connect_tcp(int fd, const char *host, u_short port,
 *                       struct sockaddr *clocal, int socklen,
 *                       CNCB *callback, void *data, int aftype, int timeout)
 * Input: An fd to connect with, a host and port to connect to,
 *        a local sockaddr to connect from + length(or NULL to use the
 *        default), a callback, the data to pass into the callback, the
 *        address family.
 * Output: None.
 * Side-effects: A non-blocking connection to the host is started, and
 *               if necessary, set up for selection. The callback given
 *               may be called now, or it may be called later.
 */
void
comm_connect_tcp(int fd, const char *host, u_short port,
		 struct sockaddr *clocal, int socklen, CNCB * callback,
		 void *data, int aftype, int timeout)
{
	void *ipptr = NULL;
	fde_t *F;
	s_assert(fd >= 0);
	F = &fd_table[fd];
	F->flags.called_connect = 1;
	s_assert(callback);
	F->connect.callback = callback;
	F->connect.data = data;

	memset(&F->connect.hostaddr, 0, sizeof(F->connect.hostaddr));
#ifdef IPV6
	if(aftype == AF_INET6)
	{
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&F->connect.hostaddr;
		SET_SS_LEN(F->connect.hostaddr, sizeof(struct sockaddr_in6));
		in6->sin6_port = htons(port);
		in6->sin6_family = AF_INET6;
		ipptr = &in6->sin6_addr;
	} else
#endif
	{
		struct sockaddr_in *in = (struct sockaddr_in *)&F->connect.hostaddr;
		SET_SS_LEN(F->connect.hostaddr, sizeof(struct sockaddr_in));
		in->sin_port = htons(port);
		in->sin_family = AF_INET;
		ipptr = &in->sin_addr;
	}

	/* Note that we're using a passed sockaddr here. This is because
	 * generally you'll be bind()ing to a sockaddr grabbed from
	 * getsockname(), so this makes things easier.
	 * XXX If NULL is passed as local, we should later on bind() to the
	 * virtual host IP, for completeness.
	 *   -- adrian
	 */
	if((clocal != NULL) && (bind(F->fd, clocal, socklen) < 0))
	{
		/* Failure, call the callback with COMM_ERR_BIND */
		comm_connect_callback(F->fd, COMM_ERR_BIND);
		/* ... and quit */
		return;
	}

	/* Next, if we have been given an IP, get the addr and skip the
	 * DNS check (and head direct to comm_connect_tryconnect().
	 */
	if(inetpton(aftype, host, ipptr) <= 0)
	{
		/* Send the DNS request, for the next level */
		F->dns_query = MyMalloc(sizeof(struct DNSQuery));
		F->dns_query->ptr = F;
		F->dns_query->callback = comm_connect_dns_callback;
		adns_gethost(host, aftype, F->dns_query);
	}
	else
	{
		/* We have a valid IP, so we just call tryconnect */
		/* Make sure we actually set the timeout here .. */
		comm_settimeout(F->fd, timeout * 1000, comm_connect_timeout, NULL);
		comm_connect_tryconnect(F->fd, NULL);
	}
}

/*
 * comm_connect_callback() - call the callback, and continue with life
 */
static void
comm_connect_callback(int fd, int status)
{
	CNCB *hdl;
	fde_t *F = &fd_table[fd];
	/* This check is gross..but probably necessary */
	if(F->connect.callback == NULL)
		return;
	/* Clear the connect flag + handler */
	hdl = F->connect.callback;
	F->connect.callback = NULL;
	F->flags.called_connect = 0;

	/* Clear the timeout handler */
	comm_settimeout(F->fd, 0, NULL, NULL);

	/* Call the handler */
	hdl(F->fd, status, F->connect.data);
}


/*
 * comm_connect_timeout() - this gets called when the socket connection
 * times out. This *only* can be called once connect() is initially
 * called ..
 */
static void
comm_connect_timeout(int fd, void *notused)
{
	/* error! */
	comm_connect_callback(fd, COMM_ERR_TIMEOUT);
}


/*
 * comm_connect_dns_callback() - called at the completion of the DNS request
 *
 * The DNS request has completed, so if we've got an error, return it,
 * otherwise we initiate the connect()
 */
static void
comm_connect_dns_callback(void *vptr, adns_answer * reply)
{
	fde_t *F = vptr;

	if(!reply)
	{
		comm_connect_callback(F->fd, COMM_ERR_DNS);
		return;
	}

	if(reply->status != adns_s_ok)
	{
		/* Yes, callback + return */
		comm_connect_callback(F->fd, COMM_ERR_DNS);
		MyFree(reply);
		MyFree(F->dns_query);
		F->dns_query = NULL;
		return;
	}

	/* No error, set a 10 second timeout */
	comm_settimeout(F->fd, 30 * 1000, comm_connect_timeout, NULL);

	/* Copy over the DNS reply info so we can use it in the connect() */
	/*
	 * Note we don't fudge the refcount here, because we aren't keeping
	 * the DNS record around, and the DNS cache is gone anyway.. 
	 *     -- adrian
	 */
#ifdef IPV6
	if(reply->rrs.addr->addr.sa.sa_family == AF_INET6)
	{
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&F->connect.hostaddr;
		memcpy(&in6->sin6_addr, &reply->rrs.addr->addr.inet6.sin6_addr, sizeof(struct in6_addr));
	}
	else
#endif
	{
		struct sockaddr_in *in = (struct sockaddr_in *)&F->connect.hostaddr;
		in->sin_addr.s_addr = reply->rrs.addr->addr.inet.sin_addr.s_addr;
	}

	/* Now, call the tryconnect() routine to try a connect() */
	MyFree(reply);
	comm_setselect(F->fd, FDLIST_SERVER, COMM_SELECT_WRITE,
		       comm_connect_tryconnect, NULL, 0);
}


/* static void comm_connect_tryconnect(int fd, void *notused)
 * Input: The fd, the handler data(unused).
 * Output: None.
 * Side-effects: Try and connect with pending connect data for the FD. If
 *               we succeed or get a fatal error, call the callback.
 *               Otherwise, it is still blocking or something, so register
 *               to select for a write event on this FD.
 */
static void
comm_connect_tryconnect(int fd, void *notused)
{
	int retval;
	fde_t *F = &fd_table[fd];

	if(F->connect.callback == NULL)
		return;
	/* Try the connect() */
	retval = connect(fd,
			 (struct sockaddr *) &fd_table[fd].connect.hostaddr, 
						       GET_SS_LEN(fd_table[fd].connect.hostaddr));
	/* Error? */
	if(retval < 0)
	{
		/*
		 * If we get EISCONN, then we've already connect()ed the socket,
		 * which is a good thing.
		 *   -- adrian
		 */
		if(errno == EISCONN)
			comm_connect_callback(F->fd, COMM_OK);
		else if(ignoreErrno(errno))
			/* Ignore error? Reschedule */
			comm_setselect(F->fd, FDLIST_SERVER, COMM_SELECT_WRITE|COMM_SELECT_RETRY,
				       comm_connect_tryconnect, NULL, 0);
		else
			/* Error? Fail with COMM_ERR_CONNECT */
			comm_connect_callback(F->fd, COMM_ERR_CONNECT);
		return;
	}
	/* If we get here, we've suceeded, so call with COMM_OK */
	comm_connect_callback(F->fd, COMM_OK);
}

/*
 * comm_error_str() - return an error string for the given error condition
 */
const char *
comm_errstr(int error)
{
	if(error < 0 || error >= COMM_ERR_MAX)
		return "Invalid error number!";
	return comm_err_str[error];
}


/*
 * comm_open() - open a socket
 *
 * This is a highly highly cut down version of squid's comm_open() which
 * for the most part emulates socket(), *EXCEPT* it fails if we're about
 * to run out of file descriptors.
 */
int
comm_open(int family, int sock_type, int proto, const char *note)
{
	int fd;
	/* First, make sure we aren't going to run out of file descriptors */
	if(number_fd >= MASTER_MAX)
	{
		errno = ENFILE;
		return -1;
	}

	/*
	 * Next, we try to open the socket. We *should* drop the reserved FD
	 * limit if/when we get an error, but we can deal with that later.
	 * XXX !!! -- adrian
	 */
	fd = socket(family, sock_type, proto);
	if(fd < 0)
		return -1;	/* errno will be passed through, yay.. */

#if defined(IPV6) && defined(IPV6_V6ONLY)
	/* 
	 * Make sure we can take both IPv4 and IPv6 connections
	 * on an AF_INET6 socket
	 */
	if(family == AF_INET6)
	{
		int off = 1;
		if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) == -1)
		{
			ilog(L_IOERROR,
			     "comm_open: Could not set IPV6_V6ONLY option to 1 on FD %d: %s",
			     fd, strerror(errno));
			close(fd);
			return -1;
		}
	}
#endif

	/* Set the socket non-blocking, and other wonderful bits */
	if(!comm_set_nb(fd))
	{
		ilog(L_IOERROR, "comm_open: Couldn't set FD %d non blocking: %s", fd, strerror(errno));
		/* if VMS, we might be opening a file (ircd.conf, resolv.conf).
		   VMS doesn't let us set non-blocking on a file, so it might fail. */
#ifndef __VMS
		close(fd);
		return -1;
#endif
	}

	/* Next, update things in our fd tracking */
	fd_open(fd, FD_SOCKET, note);
	return fd;
}


/*
 * comm_accept() - accept an incoming connection
 *
 * This is a simple wrapper for accept() which enforces FD limits like
 * comm_open() does.
 */
int
comm_accept(int fd, struct sockaddr_storage *pn)
{
	int newfd;
	socklen_t addrlen = sizeof(struct sockaddr_storage);
	if(number_fd >= MASTER_MAX)
	{
		errno = ENFILE;
		return -1;
	}

	/*
	 * Next, do the accept(). if we get an error, we should drop the
	 * reserved fd limit, but we can deal with that when comm_open()
	 * also does it. XXX -- adrian
	 */
	newfd = accept(fd, (struct sockaddr *) pn, (socklen_t *) & addrlen);
	if(newfd < 0)
		return -1;

	/* Set the socket non-blocking, and other wonderful bits */
	if(!comm_set_nb(newfd))
	{
		ilog(L_IOERROR, "comm_accept: Couldn't set FD %d non blocking!", newfd);
		close(newfd);
		return -1;
	}

	/* Next, tag the FD as an incoming connection */
	fd_open(newfd, FD_SOCKET, "Incoming connection");

	/* .. and return */
	return newfd;
}

/*
 * If a sockaddr_storage is AF_INET6 but is a mapped IPv4
 * socket manged the sockaddr.
 */
#ifndef mangle_mapped_sockaddr
void
mangle_mapped_sockaddr(struct sockaddr_storage *in)
{
	struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)in;								

	if(in->ss_family == AF_INET)
		return;

	if(in->ss_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&in6->sin6_addr))
	{
		struct sockaddr_in in4;
		memset(&in4, 0, sizeof(struct sockaddr_in));
		in4.sin_family = AF_INET;
		in4.sin_port = in6->sin6_port;
		in4.sin_addr.s_addr = ((uint32_t *)&in6->sin6_addr)[3];
		memcpy(in, &in4, sizeof(struct sockaddr_in)); 		
	}	
	return;
}
#endif


static void
fdlist_update_biggest(int fd, int opening)
{
	if(fd < highest_fd)
		return;
	s_assert(fd < MAXCONNECTIONS);

	if(fd > highest_fd)
	{
		/*  
		 * s_assert that we are not closing a FD bigger than
		 * our known biggest FD
		 */
		s_assert(opening);
		highest_fd = fd;
		return;
	}
	/* if we are here, then fd == Biggest_FD */
	/*
	 * s_assert that we are closing the biggest FD; we can't be
	 * re-opening it
	 */
	s_assert(!opening);
	while (highest_fd >= 0 && !fd_table[highest_fd].flags.open)
		highest_fd--;
}


void
fdlist_init(void)
{
	static int initialized = 0;

	if(!initialized)
	{
		/* Since we're doing this once .. */
		fd_table = MyMalloc((MAXCONNECTIONS + 1) * sizeof(fde_t));
		initialized = 1;
	}
}

/* Called to open a given filedescriptor */
void
fd_open(int fd, unsigned int type, const char *desc)
{
	fde_t *F = &fd_table[fd];
	s_assert(fd >= 0);

	if(F->flags.open)
	{
		fd_close(fd);
	}
	s_assert(!F->flags.open);
	F->fd = fd;
	F->type = type;
	F->flags.open = 1;
#ifdef NOTYET
	F->defer.until = 0;
	F->defer.n = 0;
	F->defer.handler = NULL;
#endif
	fdlist_update_biggest(fd, 1);
	F->comm_index = -1;
	F->list = FDLIST_NONE;
	if(desc)
		strlcpy(F->desc, desc, sizeof(F->desc));
	number_fd++;
}


/* Called to close a given filedescriptor */
void
fd_close(int fd)
{
	fde_t *F = &fd_table[fd];
	s_assert(F->flags.open);
	/* All disk fd's MUST go through file_close() ! */
	s_assert(F->type != FD_FILE);
	if(F->type == FD_FILE)
	{
		s_assert(F->read_handler == NULL);
		s_assert(F->write_handler == NULL);
	}
	comm_setselect(F->fd, FDLIST_NONE, COMM_SELECT_WRITE | COMM_SELECT_READ, NULL, NULL, 0);

	if (F->dns_query != NULL)
	{
		delete_adns_queries(F->dns_query);	
		MyFree(F->dns_query);
		F->dns_query = NULL;
	}
	
	F->flags.open = 0;
	fdlist_update_biggest(fd, 0);
	number_fd--;
	memset(F, '\0', sizeof(fde_t));
	F->timeout = 0;
	/* Unlike squid, we're actually closing the FD here! -- adrian */
	close(fd);
}


/*
 * fd_dump() - dump the list of active filedescriptors
 */
void
fd_dump(struct Client *source_p)
{
	int i;

	for (i = 0; i <= highest_fd; i++)
	{
		if(!fd_table[i].flags.open)
			continue;

		sendto_one_numeric(source_p, RPL_STATSDEBUG, 
				   "F :fd %-3d desc '%s'",
				   i, fd_table[i].desc);
	}
}

/*
 * fd_note() - set the fd note
 *
 * Note: must be careful not to overflow fd_table[fd].desc when
 *       calling.
 */
void
fd_note(int fd, const char *format, ...)
{
	va_list args;

	if(format)
	{
		va_start(args, format);
		ircvsnprintf(fd_table[fd].desc, FD_DESC_SZ, format, args);
		va_end(args);
	}
	else
		fd_table[fd].desc[0] = '\0';
}


/*
 * Wrappers around open() / close() for fileio, since a whole bunch of
 * code that should be using the fbopen() / fbclose() code isn't.
 * Grr. -- adrian
 */

static int
file_open(const char *filename, int mode, int fmode)
{
	int fd;
	fd = open(filename, mode, fmode);
	if(fd == MASTER_MAX)
	{
		fd_close(fd);	/* Too many FDs! */
		errno = ENFILE;
		fd = -1;
	}
	else if(fd >= 0)
		fd_open(fd, FD_FILE, filename);

	return fd;
}

static void
file_close(int fd)
{
	/*
	 * Debug - we set type to FD_FILECLOSE so we can get trapped
	 * in fd_close() with type == FD_FILE. This will allow us to
	 * convert all abusers of fd_close() of a FD_FILE fd over
	 * to file_close() .. mwahaha!
	 */
	s_assert(fd_table[fd].type == FD_FILE);
	fd_table[fd].type = FD_FILECLOSE;
	fd_close(fd);
}

FBFILE *
fbopen(const char *filename, const char *mode)
{
	int openmode = 0;
	int pmode = 0;
	FBFILE *fb = NULL;
	int fd;
	s_assert(filename);
	s_assert(mode);

	if(filename == NULL || mode == NULL)
	{
		errno = EINVAL;
		return NULL;
	}
	while (*mode)
	{
		switch (*mode)
		{
		case 'r':
			openmode = O_RDONLY;
			break;
		case 'w':
			openmode = O_WRONLY | O_CREAT | O_TRUNC;
			pmode = 0644;
			break;
		case 'a':
			openmode = O_WRONLY | O_CREAT | O_APPEND;
			pmode = 0644;
			break;
		case '+':
			openmode &= ~(O_RDONLY | O_WRONLY);
			openmode |= O_RDWR;
			break;
		default:
			break;
		}
		++mode;
	}

	if((fd = file_open(filename, openmode, pmode)) == -1)
	{
		return fb;
	}

	if(NULL == (fb = fdbopen(fd, NULL)))
		file_close(fd);
	return fb;
}

FBFILE *
fdbopen(int fd, const char *mode)
{
	/*
	 * ignore mode, if file descriptor hasn't been opened with the
	 * correct mode, the first use will fail
	 */
	FBFILE *fb = (FBFILE *) MyMalloc(sizeof(FBFILE));
	if(NULL != fb)
	{
		fb->ptr = fb->endp = fb->buf;
		fb->fd = fd;
		fb->flags = 0;
		fb->pbptr = NULL;
	}
	return fb;
}

void
fbclose(FBFILE * fb)
{
	s_assert(fb);
	if(fb != NULL)
	{
		file_close(fb->fd);
		MyFree(fb);
	}
	else
		errno = EINVAL;

}

static int
fbfill(FBFILE * fb)
{
	int n;
	s_assert(fb);
	if(fb == NULL)
	{
		errno = EINVAL;
		return -1;
	}
	if(fb->flags)
		return -1;
	n = read(fb->fd, fb->buf, BUFSIZ);
	if(0 < n)
	{
		fb->ptr = fb->buf;
		fb->endp = fb->buf + n;
	}
	else if(n < 0)
		fb->flags |= FB_FAIL;
	else
		fb->flags |= FB_EOF;
	return n;
}

int
fbgetc(FBFILE * fb)
{
	s_assert(fb);
	if(fb == NULL)
	{
		errno = EINVAL;
		return -1;
	}
	if(fb->pbptr)
	{
		if((fb->pbptr == (fb->pbuf + BUFSIZ)) || (!*fb->pbptr))
			fb->pbptr = NULL;
	}

	if(fb->ptr < fb->endp || fbfill(fb) > 0)
		return *fb->ptr++;
	return EOF;
}

void
fbungetc(char c, FBFILE * fb)
{
	s_assert(fb);
	if(fb == NULL)
	{
		errno = EINVAL;
		return;
	}
	if(!fb->pbptr)
	{
		fb->pbptr = fb->pbuf + BUFSIZ;
	}

	if(fb->pbptr != fb->pbuf)
	{
		fb->pbptr--;
		*fb->pbptr = c;
	}
}

char *
fbgets(char *buf, size_t len, FBFILE * fb)
{
	char *p = buf;
	s_assert(buf);
	s_assert(fb);
	s_assert(0 < len);

	if(fb == NULL || buf == NULL)
	{
		errno = EINVAL;
		return NULL;
	}
	if(fb->pbptr)
	{
		strlcpy(buf, fb->pbptr, len);
		fb->pbptr = NULL;
		return (buf);
	}

	if(fb->ptr == fb->endp && fbfill(fb) < 1)
		return 0;
	--len;
	while (len--)
	{
		*p = *fb->ptr++;
		if('\n' == *p)
		{
			++p;
			break;
		}
		/*
		 * deal with CR's
		 */
		else if('\r' == *p)
		{
			if(fb->ptr < fb->endp || fbfill(fb) > 0)
			{
				if('\n' == *fb->ptr)
					++fb->ptr;
			}
			*p++ = '\n';
			break;
		}
		++p;
		if(fb->ptr == fb->endp && fbfill(fb) < 1)
			break;
	}
	*p = '\0';
	return buf;
}

int
fbputs(const char *str, FBFILE * fb)
{
	int n = -1;
	s_assert(str);
	s_assert(fb);

	if(str == NULL || fb == NULL)
	{
		errno = EINVAL;
		return -1;
	}
	if(0 == fb->flags)
	{
		n = write(fb->fd, str, strlen(str));
		if(-1 == n)
			fb->flags |= FB_FAIL;
	}
	return n;
}

int
fbstat(struct stat *sb, FBFILE * fb)
{
	s_assert(sb);
	s_assert(fb);
	if(sb == NULL || fb == NULL)
	{
		errno = EINVAL;
		return -1;
	}
	return fstat(fb->fd, sb);
}
