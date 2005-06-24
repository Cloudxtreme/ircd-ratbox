/************************************************************************
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
#include "fdlist.h"
#include "s_bsd.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "event.h"
#include "irc_string.h"
#include "ircdauth.h"
#include "ircd.h"
#include "linebuf.h"
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

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET        0x7f
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

const char* const NONB_ERROR_MSG   = "set_non_blocking failed for %s:%s"; 
const char* const OPT_ERROR_MSG    = "disable_sock_options failed for %s:%s";
const char* const SETBUF_ERROR_MSG = "set_sock_buffers failed for server %s:%s";

static const char *comm_err_str[] = { "Comm OK", "Error during bind()",
  "Error during DNS lookup", "connect timeout", "Error during connect()",
  "Comm Error" };

static void comm_connect_callback(int fd, int status);
static PF comm_connect_timeout;
static void comm_connect_dns_callback(void *vptr, adns_answer *reply);
static PF comm_connect_tryconnect;

/* close_all_connections() can be used *before* the system come up! */

void close_all_connections(void)
{
  int i;
  int fd;

  for (i = 0; i < MAXCONNECTIONS; ++i)
    {
      if (fd_table[i].flags.open)
        fd_close(i);
      else
        close(i);
    }

#ifndef NDEBUG
  /* fugly hack to reserve fd == 2 */
  (void)close(2);
  fd = open("stderr.log",O_WRONLY|O_CREAT|O_APPEND,0755);
  if( fd >= 0 )
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
int get_sockerr(int fd)
{
  int errtmp = errno;
#ifdef SO_ERROR
  int err = 0;
  socklen_t len = sizeof(err);

  if (-1 < fd && !getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*) &err, &len)) {
    if (err)
      errtmp = err;
  }
  errno = errtmp;
#endif
  return errtmp;
}

/*
 * report_error - report an error from an errno. 
 * Record error to log and also send a copy to all *LOCAL* opers online.
 *
 *        text        is a *format* string for outputing error. It must
 *                contain only two '%s', the first will be replaced
 *                by the sockhost from the client_p, and the latter will
 *                be taken from sys_errlist[errno].
 *
 *        client_p        if not NULL, is the *LOCAL* client associated with
 *                the error.
 *
 * Cannot use perror() within daemon. stderr is closed in
 * ircd and cannot be used. And, worse yet, it might have
 * been reassigned to a normal connection...
 * 
 * Actually stderr is still there IFF ircd was run with -s --Rodder
 */

void report_error(const char* text, const char* who, int error) 
{
  who = (who) ? who : "";

  sendto_realops_flags(FLAGS_DEBUG, text, who, strerror(error));

  log(L_ERROR, text, who, strerror(error));
}

/*
 * set_sock_buffers - set send and receive buffers for socket
 * 
 * inputs	- fd file descriptor
 * 		- size to set
 * output       - returns true (1) if successful, false (0) otherwise
 * side effects -
 */
int set_sock_buffers(int fd, int size)
{
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*) &size, sizeof(size)) ||
      setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*) &size, sizeof(size)))
    return 0;
  return 1;
}

/*
 * disable_sock_options
 * 
 * inputs	- fd
 * output	- returns true (1) if successful, false (0) otherwise
 * side effects - disable_sock_options - if remote has any socket options set,
 *                disable them 
 */
int disable_sock_options(int fd)
{
#if defined(IP_OPTIONS) && defined(IPPROTO_IP)
  if (setsockopt(fd, IPPROTO_IP, IP_OPTIONS, NULL, 0))
    return 0;
#endif
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
int set_non_blocking(int fd)
{
  int nonb = 0;
  int res;
  nonb |= O_NONBLOCK;

  res = fcntl(fd, F_GETFL, 0);
  if (-1 == res || fcntl(fd, F_SETFL, res | nonb) == -1)
    return 0;

  fd_table[fd].flags.nonblocking = 1;
  return 1;
}

/*
 * close_connection
 *        Close the physical connection. This function must make
 *        MyConnect(client_p) == FALSE, and set client_p->from == NULL.
 */
void close_connection(struct Client *client_p)
{
  struct ConfItem *aconf;
  assert(0 != client_p);

  if (IsServer(client_p))
    {
      ServerStats->is_sv++;
      ServerStats->is_sbs += client_p->localClient->sendB;
      ServerStats->is_sbr += client_p->localClient->receiveB;
      ServerStats->is_sks += client_p->localClient->sendK;
      ServerStats->is_skr += client_p->localClient->receiveK;
      ServerStats->is_sti += CurrentTime - client_p->firsttime;
      if (ServerStats->is_sbs > 2047)
        {
          ServerStats->is_sks += (ServerStats->is_sbs >> 10);
          ServerStats->is_sbs &= 0x3ff;
        }
      if (ServerStats->is_sbr > 2047)
        {
          ServerStats->is_skr += (ServerStats->is_sbr >> 10);
          ServerStats->is_sbr &= 0x3ff;
        }
      /*
       * If the connection has been up for a long amount of time, schedule
       * a 'quick' reconnect, else reset the next-connect cycle.
       */
      if ((aconf = find_conf_exact(client_p->name, client_p->username,
                                   client_p->host, CONF_SERVER)))
        {
          /*
           * Reschedule a faster reconnect, if this was a automatically
           * connected configuration entry. (Note that if we have had
           * a rehash in between, the status has been changed to
           * CONF_ILLEGAL). But only do this if it was a "good" link.
           */
          aconf->hold = time(NULL);
          aconf->hold += (aconf->hold - client_p->since > HANGONGOODLINK) ?
            HANGONRETRYDELAY : ConfConFreq(aconf);
          if (nextconnect > aconf->hold)
            nextconnect = aconf->hold;
        }

    }
  else if (IsClient(client_p))
    {
      ServerStats->is_cl++;
      ServerStats->is_cbs += client_p->localClient->sendB;
      ServerStats->is_cbr += client_p->localClient->receiveB;
      ServerStats->is_cks += client_p->localClient->sendK;
      ServerStats->is_ckr += client_p->localClient->receiveK;
      ServerStats->is_cti += CurrentTime - client_p->firsttime;
      if (ServerStats->is_cbs > 2047)
        {
          ServerStats->is_cks += (ServerStats->is_cbs >> 10);
          ServerStats->is_cbs &= 0x3ff;
        }
      if (ServerStats->is_cbr > 2047)
        {
          ServerStats->is_ckr += (ServerStats->is_cbr >> 10);
          ServerStats->is_cbr &= 0x3ff;
        }
    }
  else
    ServerStats->is_ni++;
  
#if 0
  if (client_p->localClient->dns_reply)
    {
      --client_p->localClient->dns_reply->ref_count;
      client_p->localClient->dns_reply = 0;
    }
#endif
  if (-1 < client_p->fd)
    {
      /* attempt to flush any pending dbufs. Evil, but .. -- adrian */
      if (!IsDead(client_p))
        send_queued_write(client_p->fd, client_p);
      fd_close(client_p->fd);
      client_p->fd = -1;
    }

  linebuf_donebuf(&client_p->localClient->buf_sendq);
  linebuf_donebuf(&client_p->localClient->buf_recvq);
  memset(client_p->localClient->passwd, 0, sizeof(client_p->localClient->passwd));
  /*
   * clean up extra sockets from P-lines which have been discarded.
   */
  if (client_p->localClient->listener)
    {
      assert(0 < client_p->localClient->listener->ref_count);
      if (0 == --client_p->localClient->listener->ref_count &&
	  !client_p->localClient->listener->active) 
	free_listener(client_p->localClient->listener);
      client_p->localClient->listener = 0;
    }

  det_confs_butmask(client_p, 0);
  client_p->from = NULL; /* ...this should catch them! >:) --msa */
}

/*
 * add_connection - creates a client which has just connected to us on 
 * the given fd. The sockhost field is initialized with the ip# of the host.
 * The client is sent to the auth module for verification, and not put in
 * any client list yet.
 */
void add_connection(struct Listener* listener, int fd)
{
  struct Client*     new_client;

  socklen_t len = sizeof(struct irc_sockaddr);
  struct irc_sockaddr   irn;
  assert(0 != listener);

#ifdef USE_IAUTH
  if (iAuth.socket == NOSOCK)
  {
    send(fd,
      "NOTICE AUTH :*** Ircd Authentication Server is temporarily down, please connect later\r\n",
      87,
      0);
    fd_close(fd);
    return;
  }
#endif

  /* 
   * get the client socket name from the socket
   * the client has already been checked out in accept_connection
   */
  new_client = make_client(NULL);
  if (getpeername(fd, (struct sockaddr *)&SOCKADDR(irn), &len))
    {
      report_error("Failed in adding new connection %s :%s", 
		   get_listener_name(listener), errno);
      ServerStats->is_ref++;
      fd_close(fd);
      return;
    }

  /* 
   * copy address to 'sockhost' as a string, copy it to host too
   * so we have something valid to put into error messages...
   */
  new_client->localClient->port = ntohs(S_PORT(irn));
  copy_s_addr(IN_ADDR(new_client->localClient->ip),  S_ADDR(irn));
  inetntop(DEF_FAM, &IN_ADDR(new_client->localClient->ip), new_client->localClient->sockhost, HOSTIPLEN);
#ifdef IPV6
  if(!IN6_IS_ADDR_V4MAPPED(&IN_ADDR2(new_client->localClient->ip) && !IN6_IS_ADDR_V4COMPAT(&IN_ADDR2(new_client->localClient->ip)))
  	new_client->localClient->aftype = AF_INET6;
  else
  {
	memmove(&new_client->localClient->ip.sins.sin.s_addr,&IN_ADDR(new_client->localClient->ip)[12], sizeof(unsigned long));
	new_client->localClient->aftype = AF_INET;  	
  }
#else
  new_client->localClient->aftype = AF_INET;
#endif
  strcpy(new_client->host, new_client->localClient->sockhost);

  new_client->fd        = fd;

  new_client->localClient->listener  = listener;
  ++listener->ref_count;

  if (!set_non_blocking(new_client->fd))
    report_error(NONB_ERROR_MSG, get_client_name(new_client, SHOW_IP), errno);
  if (!disable_sock_options(new_client->fd))
    report_error(OPT_ERROR_MSG, get_client_name(new_client, SHOW_IP), errno);
  start_auth(new_client);
}


void error_exit_client(struct Client* client_p, int error)
{
  /*
   * ...hmm, with non-blocking sockets we might get
   * here from quite valid reasons, although.. why
   * would select report "data available" when there
   * wasn't... so, this must be an error anyway...  --msa
   * actually, EOF occurs when read() returns 0 and
   * in due course, select() returns that fd as ready
   * for reading even though it ends up being an EOF. -avalon
   */
  char errmsg[255];
  int  current_error = get_sockerr(client_p->fd);

  Debug((DEBUG_ERROR, "READ ERROR: fd = %d %d %d",
         client_p->fd, current_error, error));
  if (IsServer(client_p) || IsHandshake(client_p))
    {
      int connected = CurrentTime - client_p->firsttime;
      
      if (error == 0)
        {
	  /* Admins get the real IP */
	  sendto_realops_flags(FLAGS_ADMIN,
				"Server %s closed the connection",
				get_client_name(client_p, SHOW_IP));

	  /* Opers get a masked IP */
	  sendto_realops_flags(FLAGS_NOTADMIN,
				"Server %s closed the connection",
				get_client_name(client_p, MASK_IP));

	  log(L_NOTICE, "Server %s closed the connection",
		get_client_name(client_p, SHOW_IP));
        }
      else
	{
          report_error("Lost connection to %s:%s", 
		get_client_name(client_p, SHOW_IP), current_error);
	}

      sendto_realops_flags(FLAGS_ALL,
			   "%s had been connected for %d day%s, %2d:%02d:%02d",
			   client_p->name, connected/86400,
			   (connected/86400 == 1) ? "" : "s",
			   (connected % 86400) / 3600, (connected % 3600) / 60,
			   connected % 60);
    }
  if (error == 0)
  {
    strcpy(errmsg, "Remote closed the connection");
  }
  else
  {
    ircsprintf(errmsg, "Read error: %d (%s)", 
               current_error, strerror(current_error));
  }
  fd_close(client_p->fd);
  client_p->fd = -1;
  
  detach_client(client_p, errmsg);
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
    switch (ierrno) {
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
    /* NOTREACHED */
       return 0;
}


/*
 * comm_settimeout() - set the socket timeout
 *
 * Set the timeout for the fd
 */
void
comm_settimeout(int fd, time_t timeout, PF *callback, void *cbdata)
{
    assert(fd > -1);
    assert(fd_table[fd].flags.open);

    fd_table[fd].timeout = CurrentTime + timeout;
    fd_table[fd].timeout_handler = callback;
    fd_table[fd].timeout_data = cbdata;
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
comm_setflush(int fd, time_t timeout, PF *callback, void *cbdata)
{
    assert(fd > -1);
    assert(fd_table[fd].flags.open);

    fd_table[fd].flush_timeout = CurrentTime + timeout;
    fd_table[fd].flush_handler = callback;
    fd_table[fd].flush_data = cbdata;
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

    for (fd = 0; fd <= highest_fd; fd++)
      {
        if (!fd_table[fd].flags.open)
            continue;
        if (fd_table[fd].flags.closing)
            continue;

        /* check flush functions */
        if (fd_table[fd].flush_handler &&
            fd_table[fd].flush_timeout > 0 && fd_table[fd].flush_timeout 
            < CurrentTime)
	  {
            hdl = fd_table[fd].flush_handler;
            data = fd_table[fd].flush_data;
            comm_setflush(fd, 0, NULL, NULL);
            hdl(fd, data);
	  }

        /* check timeouts */
        if (fd_table[fd].timeout_handler &&
            fd_table[fd].timeout > 0 && fd_table[fd].timeout < CurrentTime)
	  {
            /* Call timeout handler */
            hdl = fd_table[fd].timeout_handler;
            data = fd_table[fd].timeout_data;
            comm_settimeout(fd, 0, NULL, NULL);
            hdl(fd, fd_table[fd].timeout_data);           
	  }
      }
    /* .. next .. */
    eventAdd("comm_checktimeouts", comm_checktimeouts, NULL, 1, 0);
}

/*
 * comm_connect_tcp() - connect a given socket to a remote address
 *
 * Begin the process of connecting a socket to a remote host/port.
 * Pass a 'local' address to bind to, a remote host/port, and a callback
 * for completion.
 * This routine binds the socket locally if required, and then formulate
 * a DNS request.
 */
void
comm_connect_tcp(int fd, const char *host, u_short port, 
    struct sockaddr *clocal, int socklen, CNCB *callback, void *data, int aftype)
{
    fd_table[fd].flags.called_connect = 1;
    fd_table[fd].connect.callback = callback;
    fd_table[fd].connect.data = data;

    S_FAM(fd_table[fd].connect.hostaddr) = DEF_FAM;
    S_PORT(fd_table[fd].connect.hostaddr) = htons(port);
    /*
     * Note that we're using a passed sockaddr here. This is because
     * generally you'll be bind()ing to a sockaddr grabbed from
     * getsockname(), so this makes things easier.
     * XXX If NULL is passed as local, we should later on bind() to the
     * virtual host IP, for completeness.
     *   -- adrian
     */
    if ((clocal != NULL) && (bind(fd, clocal, socklen) < 0))
      { 
        /* Failure, call the callback with COMM_ERR_BIND */
        comm_connect_callback(fd, COMM_ERR_BIND);
        /* ... and quit */
        return;
      }

    /*
     * Next, if we have been given an IP, get the addr and skip the
     * DNS check (and head direct to comm_connect_tryconnect().
     */
    if(inetpton(DEF_FAM, host, S_ADDR(&fd_table[fd].connect.hostaddr)) <=0)
      {
        /* Send the DNS request, for the next level */
        fd_table[fd].dns_query = MyMalloc(sizeof(struct DNSQuery));
        fd_table[fd].dns_query->ptr = &fd_table[fd];
        fd_table[fd].dns_query->callback = comm_connect_dns_callback;
	adns_gethost(host, aftype, fd_table[fd].dns_query);
      }
    else
      {
        /* We have a valid IP, so we just call tryconnect */
        /* Make sure we actually set the timeout here .. */
        comm_settimeout(fd, 30, comm_connect_timeout, NULL);
        comm_connect_tryconnect(fd, NULL);        
      }
}


/*
 * comm_connect_callback() - call the callback, and continue with life
 */
static void
comm_connect_callback(int fd, int status)
{
    CNCB *hdl;

    /* Clear the connect flag + handler */
    hdl = fd_table[fd].connect.callback;
    fd_table[fd].connect.callback = NULL;
    fd_table[fd].flags.called_connect = 0;

    /* Clear the timeout handler */
    comm_settimeout(fd, 0, NULL, NULL);

    /* Call the handler */
    hdl(fd, status, fd_table[fd].connect.data);

    /* Finish! */
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
comm_connect_dns_callback(void *vptr, adns_answer *reply)
{
    fde_t *F = vptr;

    if(!reply)
      {
	comm_connect_callback(F->fd, COMM_ERR_DNS);
	return;
      }
    
    if (reply->status != adns_s_ok)
      {
        /* Yes, callback + return */
        comm_connect_callback(F->fd, COMM_ERR_DNS);
	MyFree(reply);
	MyFree(F->dns_query);
        return;
      } 

    /* No error, set a 10 second timeout */
    comm_settimeout(F->fd, 30, comm_connect_timeout, NULL);

    /* Copy over the DNS reply info so we can use it in the connect() */
    /*
     * Note we don't fudge the refcount here, because we aren't keeping
     * the DNS record around, and the DNS cache is gone anyway.. 
     *     -- adrian
     */
#ifdef IPV6
    if(reply->rrs.addr->addr.sa.sa_family == AF_INET6)
      {
	copy_s_addr(S_ADDR(F->connect.hostaddr), reply->rrs.addr->addr.inet6.sin6_addr.s6_addr);
      } 
    else
      {
	/* IPv4 mapped address */
	/* This is lazy... */
    	memset(&F->connect.hostaddr.sins.sin6.sin6_addr.s6_addr, 0x0000, 10); 
	memset(&F->connect.hostaddr.sins.sin6.sin6_addr.s6_addr[10], 0xffff, 2);
	memcpy(&F->connect.hostaddr.sins.sin6.sin6_addr.s6_addr[12], &reply->rrs.addr->addr.inet.sin_addr.s_addr, 4);
      }
#else
    F->connect.hostaddr.sins.sin.sin_addr.s_addr = reply->rrs.addr->addr.inet.sin_addr.s_addr;
#endif
    /* Now, call the tryconnect() routine to try a connect() */
    MyFree(reply); 
    comm_connect_tryconnect(F->fd, NULL);
}


/*
 * comm_connect_tryconnect() - called to attempt a connect()
 *
 * Attempt a connect(). If we get a non-fatal error, retry. If we get a fatal
 * error, callback with error. If we suceed, callback with OK.
 */
static void
comm_connect_tryconnect(int fd, void *notused)
{
  int retval;

  /* Try the connect() */

  retval = connect(fd, (struct sockaddr *) &SOCKADDR(fd_table[fd].connect.hostaddr), sizeof(struct irc_sockaddr));
  /* Error? */
  if (retval < 0)
    {
      /*
       * If we get EISCONN, then we've already connect()ed the socket,
       * which is a good thing.
       *   -- adrian
       */
      if (errno == EISCONN)
	comm_connect_callback(fd, COMM_OK);
      else if (ignoreErrno(errno))
	/* Ignore error? Reschedule */
	comm_setselect(fd, FDLIST_SERVER, COMM_SELECT_WRITE,
		       comm_connect_tryconnect, NULL, 0);
      else
	/* Error? Fail with COMM_ERR_CONNECT */
	comm_connect_callback(fd, COMM_ERR_CONNECT);
      return;
    }

    /* If we get here, we've suceeded, so call with COMM_OK */
    comm_connect_callback(fd, COMM_OK);
}


/*
 * comm_error_str() - return an error string for the given error condition
 */
const char *
comm_errstr(int error)
{
    if (error < 0 || error >= COMM_ERR_MAX)
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
  if (number_fd >= MASTER_MAX)
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
  if (fd < 0)
    return -1; /* errno will be passed through, yay.. */

  /* Set the socket non-blocking, and other wonderful bits */
  if (!set_non_blocking(fd))
    {
      log(L_CRIT, "comm_open: Couldn't set FD %d non blocking!", fd);
      close(fd);
      return -1;
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
comm_accept(int fd, struct irc_sockaddr *pn)
{
  int newfd;
  socklen_t addrlen = sizeof(struct irc_sockaddr);
  if (number_fd >= MASTER_MAX)
    {
      errno = ENFILE;
      return -1;
    }

  /*
   * Next, do the accept(). if we get an error, we should drop the
   * reserved fd limit, but we can deal with that when comm_open()
   * also does it. XXX -- adrian
   */
  newfd = accept(fd, (struct sockaddr *)&PSOCKADDR(pn), &addrlen);
  if (newfd < 0)
    return -1;

  /* Set the socket non-blocking, and other wonderful bits */
  if (!set_non_blocking(newfd))
    {
      log(L_CRIT, "comm_accept: Couldn't set FD %d non blocking!", newfd);
      close(newfd);
      return -1;
    }

  /* Next, tag the FD as an incoming connection */
  fd_open(newfd, FD_SOCKET, "Incoming connection");

  /* .. and return */
  return newfd;
}
