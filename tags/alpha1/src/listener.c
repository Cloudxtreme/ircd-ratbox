/************************************************************************
 *   IRC - Internet Relay Chat, src/listener.c
 *   Copyright (C) 1999 Thomas Helvey <tomh@inxpress.net>
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
#include "listener.h"
#include "client.h"
#include "fdlist.h"
#include "irc_string.h"
#include "ircd.h"
#include "ircd_defs.h"
#include "numeric.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_stats.h"
#include "send.h"
#include "config.h"

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "memdebug.h"

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

static PF accept_connection;

static struct Listener* ListenerPollList = NULL;

struct Listener* make_listener(int port, struct in_addr addr)
{
  struct Listener* listener = 
    (struct Listener*) MyMalloc(sizeof(struct Listener));
  assert(0 != listener);

  listener->name        = me.name;
  listener->fd          = -1;
  listener->port        = port;
  listener->addr.s_addr = addr.s_addr;

#ifdef NULL_POINTER_NOT_ZERO
  listener->next = NULL;
  listener->conf = NULL;
#endif
  return listener;
}

void free_listener(struct Listener* listener)
{
  assert(0 != listener);
  MyFree(listener);
}

#define PORTNAMELEN 6  /* ":31337" */

/*
 * get_listener_name - return displayable listener name and port
 * returns "host.foo.org:6667" for a given listener
 */
const char* get_listener_name(const struct Listener* listener)
{
  static char buf[HOSTLEN + HOSTLEN + PORTNAMELEN + 4];
  assert(0 != listener);
  ircsprintf(buf, "%s[%s/%u]", 
             listener->name, listener->name, listener->port);
  return buf;
}

/*
 * show_ports - send port listing to a client
 * inputs       - pointer to client to show ports to
 * output       - none
 * side effects - show ports
 */
void show_ports(struct Client* sptr)
{
  struct Listener* listener = 0;

  for (listener = ListenerPollList; listener; listener = listener->next)
    {
      sendto_one(sptr, form_str(RPL_STATSPLINE),
                 me.name,
                 sptr->name,
                 'P',
                 listener->port,
                 listener->name,
                 listener->ref_count,
                 (listener->active)?"active":"disabled");
    }
}
  
/*
 * inetport - create a listener socket in the AF_INET or AF_INET6 domain, 
 * bind it to the port given in 'port' and listen to it  
 * returns true (1) if successful false (0) on error.
 *
 * If the operating system has a define for SOMAXCONN, use it, otherwise
 * use HYBRID_SOMAXCONN 
 */
#ifdef SOMAXCONN
#undef HYBRID_SOMAXCONN
#define HYBRID_SOMAXCONN SOMAXCONN
#endif

static int inetport(struct Listener* listener)
{
#ifdef IPV6
  struct sockaddr_in6 lsin6;
#else
  struct sockaddr_in lsin;
#endif
  int                fd;
  int                opt = 1;

  /*
   * At first, open a new socket
   */
#ifdef IPV6
  fd = comm_open(AF_INET6, SOCK_STREAM, 0, "IPv6 Listener socket");
#else
  fd = comm_open(AF_INET, SOCK_STREAM, 0, "Listener socket");
#endif

  if (-1 == fd) {
    report_error("opening listener socket %s:%s", 
                 get_listener_name(listener), errno);
    return 0;
  }
  else if ((HARD_FDLIMIT - 10) < fd) {
    report_error("no more connections left for listener %s:%s", 
                 get_listener_name(listener), errno);
    /* This is ok because we haven't fd_open()ed it yet -- adrian */
    close(fd);
    return 0;
  }
  /* 
   * XXX - we don't want to do all this crap for a listener
   * set_sock_opts(listener);
   */ 
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &opt, sizeof(opt))) {
    report_error("setting SO_REUSEADDR for listener %s:%s", 
                 get_listener_name(listener), errno);
    fd_close(fd);
    return 0;
  }

  /*
   * Bind a port to listen for new connections if port is non-null,
   * else assume it is already open and try get something from it.
   */
#ifdef IPV6
  memset(&lsin6, 0, sizeof(lsin6));
  lsin6.sin6_family = AF_INET6;
  memcpy(&lsin6.sin6_addr.s6_addr, &in6addr_any, sizeof(in6addr_any));
  lsin6.sin6_port = htons(listener->port);
#else
  memset(&lsin, 0, sizeof(lsin));
  lsin.sin_family = AF_INET;
  lsin.sin_addr   = listener->addr;
  lsin.sin_port   = htons(listener->port);
#endif

#ifndef IPV6
  if (INADDR_ANY != listener->addr.s_addr) {
    struct hostent* hp;
    /*
     * XXX - blocking call to gethostbyaddr
     */
    if ((hp = gethostbyaddr((char*) &listener->addr, 
                            sizeof(struct sockaddr_in), AF_INET))) {
      strncpy_irc(listener->vhost, hp->h_name, HOSTLEN);
      listener->name = listener->vhost;
    }
  }
#endif

#ifdef IPV6
  if (bind(fd, (struct sockaddr*) &lsin6, sizeof(lsin6))) {
#else
  if (bind(fd, (struct sockaddr*) &lsin, sizeof(lsin))) {
#endif
    report_error("binding listener socket %s:%s", 
                 get_listener_name(listener), errno);
    fd_close(fd);
    return 0;
  }

  if (listen(fd, HYBRID_SOMAXCONN)) {
    report_error("listen failed for %s:%s", 
                 get_listener_name(listener), errno);
    fd_close(fd);
    return 0;
  }

  /*
   * XXX - this should always work, performance will suck if it doesn't
   */
  if (!set_non_blocking(fd))
    report_error(NONB_ERROR_MSG, get_listener_name(listener), errno);

  listener->fd = fd;

  /* Listen completion events are READ events .. */
  comm_setselect(fd, FDLIST_SERVICE, COMM_SELECT_READ, accept_connection,
    listener, 0);

  return 1;
}

static struct Listener* find_listener(int port, struct in_addr addr)
{
  struct Listener* listener;
  for (listener = ListenerPollList; listener; listener = listener->next) {
    if (port == listener->port && addr.s_addr == listener->addr.s_addr)
      return listener;
  }
  return 0;
}
 
  
/*
 * add_listener- create a new listener 
 * port - the port number to listen on
 * vhost_ip - if non-null must contain a valid IP address string in
 * the format "255.255.255.255"
 */
void add_listener(int port, const char* vhost_ip) 
{
  struct Listener* listener;
  struct in_addr   vaddr;

  /*
   * if no port in conf line, don't bother
   */
  if (0 == port)
    return;

  vaddr.s_addr = INADDR_ANY;

  if (vhost_ip) {
    vaddr.s_addr = inet_addr(vhost_ip);
    if (INADDR_NONE == vaddr.s_addr)
      return;
  }

  if ((listener = find_listener(port, vaddr))) {
    listener->active = 1;
    return;
  }

  listener = make_listener(port, vaddr);

  if (inetport(listener)) {
    listener->active = 1;
    listener->next   = ListenerPollList;
    ListenerPollList = listener; 
  }
  else
    free_listener(listener);
}

void mark_listeners_closing(void)
{
  struct Listener* listener;
  for (listener = ListenerPollList; listener; listener = listener->next)
    listener->active = 0;
}

/*
 * close_listener - close a single listener
 */
void close_listener(struct Listener* listener)
{
  assert(0 != listener);
  /*
   * remove from listener list
   */
  if (listener == ListenerPollList)
    ListenerPollList = listener->next;
  else {
    struct Listener* prev = ListenerPollList;
    for ( ; prev; prev = prev->next) {
      if (listener == prev->next) {
        prev->next = listener->next;
        break; 
      }
    }
  }
  if (-1 < listener->fd)
    /* This will also remove any pending IO interests  -- Adrian */
    fd_close(listener->fd);
  free_listener(listener);
}
 
/*
 * close_listeners - close and free all listeners that are not being used
 */
void close_listeners()
{
  struct Listener* listener;
  struct Listener* listener_next = 0;
  /*
   * close all 'extra' listening ports we have
   */
  for (listener = ListenerPollList; listener; listener = listener_next) {
    listener_next = listener->next;
    if (0 == listener->active && 0 == listener->ref_count)
      close_listener(listener);
  }
}

static void accept_connection(int pfd, void *data)
{
  static time_t      last_oper_notice = 0;
  struct sockaddr_in addr;
  socklen_t          addrlen = sizeof(struct sockaddr_in);
  int                fd;
  struct Listener *  listener = data;

  assert(0 != listener);

  listener->last_accept = CurrentTime;
  /*
   * There may be many reasons for error return, but
   * in otherwise correctly working environment the
   * probable cause is running out of file descriptors
   * (EMFILE, ENFILE or others?). The man pages for
   * accept don't seem to list these as possible,
   * although it's obvious that it may happen here.
   * Thus no specific errors are tested at this
   * point, just assume that connections cannot
   * be accepted until some old is closed first.
   */
  do {
    fd = comm_accept(listener->fd, (struct sockaddr*) &addr, &addrlen);
    if (fd < 0) {
      if (EAGAIN == errno)
         break;
      /*
       * slow down the whining to opers bit
       */
      if((last_oper_notice + 20) <= CurrentTime) {
        report_error("Error accepting connection %s:%s", 
                   listener->name, errno);
        last_oper_notice = CurrentTime;
      }
      break;
    }
    /*
     * check for connection limit
     */
    if ((MAXCONNECTIONS - 10) < fd) {
      ++ServerStats->is_ref;
      /* 
       * slow down the whining to opers bit 
       */
      if((last_oper_notice + 20) <= CurrentTime) {
        sendto_realops_flags(FLAGS_ALL,"All connections in use. (%s)", 
			     get_listener_name(listener));
        last_oper_notice = CurrentTime;
      }
      send(fd, "ERROR :All connections in use\r\n", 32, 0);
      fd_close(fd);
      break;
    }
    /*
     * check to see if listener is shutting down
     */
    if (!listener->active) {
      ++ServerStats->is_ref;
      send(fd, "ERROR :Use another port\r\n", 25, 0);
      fd_close(fd);
      break;
    }
    /*
     * check conf for ip address access
     */
    if (!conf_connect_allowed(addr.sin_addr)) {
      ServerStats->is_ref++;
#ifdef REPORT_DLINE_TO_USER
       send(fd, "NOTICE DLINE :*** You have been D-lined\r\n", 41, 0);
#endif
      fd_close(fd);
      break;
    }
    ServerStats->is_ac++;

    add_connection(listener, fd);
  } while (0);

  /* Re-register a new IO request for the next accept .. */
  comm_setselect(listener->fd, FDLIST_SERVICE, COMM_SELECT_READ,
    accept_connection, listener, 0);
}

