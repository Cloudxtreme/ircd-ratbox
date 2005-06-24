/************************************************************************
 *
 * s_bsd_select.c - code implementing a select IO loop
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

#error "s_bsd_select.c hasn't been rewritten yet. use poll or contact \
adrian chadd <adrian@creative.net.au>."

#include "tools.h"
#include "s_bsd.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "fdlist.h"
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
#include <sys/param.h>    /* NOFILE */
#include <arpa/inet.h>
#include "memdebug.h"

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET        0x7f
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

/*
 * Stuff for select()
 */

static fd_set readSet;
static fd_set writeSet;

fd_set*  read_set  = &readSet;
fd_set*  write_set = &writeSet;

static void add_fds_from_list(dlink_list *list, unsigned char mask);
static void check_fds_from_list(dlink_list *list, unsigned char mask,
				int nfds);

void init_netio(void)
{
  if( MAXCONNECTIONS > FD_SETSIZE )
    {
      fprintf(stderr, "FD_SETSIZE = %d MAXCONNECTIONS = %d\n",
             FD_SETSIZE, MAXCONNECTIONS);
      fprintf(stderr,  
        "Make sure your kernel supports a larger FD_SETSIZE then " \
        "recompile with -DFD_SETSIZE=%d\n", MAXCONNECTIONS);
      exit(-1);
    }
  printf("Value of FD_SETSIZE is %d\n", FD_SETSIZE);

  printf("AIEE! This code needs to be converted to use the new-style net " \
         "IO. Go grab adrian.\n");
  exit(59);

  read_set  = &readSet;
  write_set = &writeSet;
  init_resolver();
}
 
/*
 * Check all connections for new connections and input data that is to be
 * processed. Also check for connections with data queued and whether we can
 * write it out.
 */
int read_message(time_t delay, unsigned char mask)        /* mika */

     /* Don't ever use ZERO here, unless you mean to poll
        and then you have to have sleep/wait somewhere 
        else in the code.--msa
      */
{
  struct Client*      cptr;
  int                 nfds;
  struct timeval      wait;
  time_t              delay2 = delay;
  time_t              now;
  u_long              usec = 0;
  int                 res;
  int                 length;
  struct AuthRequest* auth = 0;
  struct AuthRequest* auth_next = 0;
  struct Listener*    listener = 0;
  int                 i;

  now = CurrentTime;

  for (res = 0;;)
    {
      FD_ZERO(read_set);
      FD_ZERO(write_set);

#ifdef USE_IAUTH
      if (iAuth.socket != NOSOCK)
      {
      	if (IsIAuthConnect(iAuth))
      		FD_SET(iAuth.socket, write_set);
      	else
        	FD_SET(iAuth.socket, read_set);
      }
#endif

      for (auth = AuthPollList; auth; auth = auth->next) {
        assert(-1 < auth->fd);
        if (IsAuthConnect(auth))
          FD_SET(auth->fd, write_set);
        else /* if(IsAuthPending(auth)) */
          FD_SET(auth->fd, read_set);
      }
      for (listener = ListenerPollList; listener; listener = listener->next) {
        assert(-1 < listener->fd);
        FD_SET(listener->fd, read_set);
      }

      add_fds_from_list(&lclient_list, mask);
      add_fds_from_list(&serv_list, mask);
      add_fds_from_list(&unknown_list, mask);

      if (ResolverFileDescriptor >= 0)
        {
          FD_SET(ResolverFileDescriptor, read_set);
        }
      wait.tv_sec = IRCD_MIN(delay2, delay);
      wait.tv_usec = usec;

      nfds = select(MAXCONNECTIONS, read_set, write_set, 0, &wait);

      if ((CurrentTime = time(NULL)) == -1)
        {
          log(L_CRIT, "Clock Failure");
          restart("Clock failure");
        }   

      if (nfds == -1 && errno == EINTR)
        {
          return -1;
        }
      else if( nfds >= 0)
        break;

      res++;
      if (res > 5)
        restart("too many select errors");
      sleep(10);
    }

  /*
   * Check the name resolver
   */

  if (-1 < ResolverFileDescriptor && 
      FD_ISSET(ResolverFileDescriptor, read_set)) {
    get_res();
    --nfds;
  }
  /*
   * Check the auth fd's
   */
  for (auth = AuthPollList; auth; auth = auth_next) {
    auth_next = auth->next;
    assert(-1 < auth->fd);
    if (IsAuthConnect(auth) && FD_ISSET(auth->fd, write_set)) {
      send_auth_query(auth);
      if (0 == --nfds)
        break;
    }
    else if (FD_ISSET(auth->fd, read_set)) {
      read_auth_reply(auth);
      if (0 == --nfds)
        break;
    }
  }
  for (listener = ListenerPollList; listener; listener = listener->next) {
    assert(-1 < listener->fd);
    if (FD_ISSET(listener->fd, read_set))
      accept_connection(listener);
  }

#ifdef USE_IAUTH
  /*
   * Check IAuth
   */
  if (iAuth.socket != NOSOCK)
  {
  	if (IsIAuthConnect(iAuth) && FD_ISSET(iAuth.socket, write_set))
  	{
  		/*FD_CLR(iAuth.socket, write_set);*/

  		/*
  		 * Complete the connection to the IAuth server
  		 */
  		if (!CompleteIAuthConnection())
  		{
  			close(iAuth.socket);
  			iAuth.socket = NOSOCK;
  		}
  	}
  	else if (FD_ISSET(iAuth.socket, read_set))
    {
      if (!ParseIAuth())
      {
        /*
         * IAuth server closed the connection
         */
        close(iAuth.socket);
        iAuth.socket = NOSOCK;
      }
    }
  }
#endif

  check_fds_from_list(&lclient_list, mask, nfds);
  check_fds_from_list(&serv_list, mask, nfds);
  check_fds_from_list(&unknown_list, mask, nfds);

  return 0;
}

/*
 * add_fds_from_list
 *
 * inputs	- pointer to a dlink_list
 *		- mask
 * output	- NONE
 * side effects	- adds fd's from clients on given list to fd_set's
 */
static void
add_fds_from_list(dlink_list *list, unsigned char mask)
{
  dlink_node *ptr;
  struct Client *cptr;
  int i;

  for(ptr = list->head; ptr; ptr=ptr->next)
    {
      cptr = ptr->data;

      i = cptr->fd;	/* ugh */

      /* XXX */
#if 0
      if (!(fd_table[i].mask & mask))
	continue;
#endif
#if 0
      if (DBufLength(&cptr->localClient->buf_recvq) && delay2 > 2)
	delay2 = 1;
#endif

      if (DBufLength(&cptr->localClient->buf_recvq) < 4088)        
	{
	  FD_SET(i, read_set);
	}

      /* XXX */
      if (DBufLength(&cptr->localClient->buf_sendq || IsConnecting(cptr))
	{
	  FD_SET(i, write_set);
	}
    }
}  

/*
 * check_fds_from_list
 *
 * inputs	- pointer to a dlink_list
 * output	- NONE
 * side effects	- checks fd's from each clients on given list
 *		  does I/O if necessary
 */
static void
check_fds_from_list(dlink_list *list, unsigned char mask, int nfds)
{
  dlink_node *ptr;
  struct Client *cptr;
  int i;
  int length;

  for (ptr = list->head; ptr; ptr = ptr->next)
    {
      cptr = ptr->data;
      i = cptr->fd;

      if (nfds == 0)
	return;

#if 0
      if (!(fd_table[i].mask & mask))
	continue;
#endif

      /*
       * See if we can write...
       */
      if (FD_ISSET(i, write_set)) {
	--nfds;
	if (IsConnecting(cptr)) {
	  if (!completed_connection(cptr)) {
	    exit_client(cptr, cptr, &me, "Lost C/N Line");
	    continue;
	  }
	  send_queued(cptr);
          if (!IsDead(cptr))
            continue;
	}
	else {
	  /*
	   * ...room for writing, empty some queue then...
	   */
	  send_queued(cptr);
	  if (!IsDead(cptr))
	    continue;
	}
	exit_client(cptr, cptr, &me, 
		    (cptr->flags & FLAGS_SENDQEX) ? 
		    "SendQ Exceeded" : strerror(get_sockerr(cptr->fd)));
	continue;
      }
      length = 1;     /* for fall through case */

      if (FD_ISSET(i, read_set)) {
	--nfds;
	length = read_packet(cptr);
      }
      else if (PARSE_AS_CLIENT(cptr) && !NoNewLine(cptr))
	length = parse_client_queued(cptr);

      if (length > 0 || length == CLIENT_EXITED)
	continue;
      if (IsDead(cptr)) {
	exit_client(cptr, cptr, &me,
                    strerror(get_sockerr(cptr->fd)));
	continue;
      }
      error_exit_client(cptr, length);
      errno = 0;
    }
}

