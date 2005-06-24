/************************************************************************
 *   IRC - Internet Relay Chat, src/send.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                    University of Oulu, Computing Center
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
 *   $Id$
 */

#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <errno.h>

#include "tools.h"
#include "send.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
#include "handlers.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_serv.h"
#include "sprintf_irc.h"
#include "s_conf.h"
#include "list.h"
#include "s_debug.h"
#include "s_log.h"
#include "vchannel.h"
#include "memory.h"
#include "debug.h"

#define LOG_BUFSIZE 2048

static  int
_send_message (struct Client *, char *, int);

#define send_message(a,b,c) _send_message((a->from?a->from:a),b,c)

static void
send_message_remote(struct Client *to, struct Client *from,
		    const char *lsendbuf, int len);

/* global for now *sigh* */
unsigned long current_serial=0L;

static void
sendto_list_local(dlink_list *list, const char *lsendbuf, int len);

static void
sendto_list_anywhere(struct Client *one, struct Client *from,
		     dlink_list *list,
		     const char *llocal_sendbuf, int llocal_len,
		     const char *lremote_sendbuf, int lremote_len
		     );

static int
send_format(char *lsendbuf, const char *pattern, va_list args);

static int
send_trim(char *lsendbuf, int len );

/*
** dead_link
**      An error has been detected. The link *must* be closed,
**      but *cannot* call ExitClient (m_bye) from here.
**      Instead, mark it with FLAGS_DEADSOCKET. This should
**      generate ExitClient from the main loop.
**
**      If 'notice' is not NULL, it is assumed to be a format
**      for a message to local opers. I can contain only one
**      '%s', which will be replaced by the sockhost field of
**      the failing link.
**
**      Also, the notice is skipped for "uninteresting" cases,
**      like Persons and yet unknown connections...
*/
/*
 * Can't call exit_client right away. This client might be in the middle
 * of a /list and sendq's out. exiting the client during that list
 * would thoroughly confuse things (and did). Best we can do, is mark
 * the client as known to be thoroughly dead, treat it like a pariah
 * and exit in the main loop at the earliest opportunity. ugh.
 * - Dianora
 * 
 */

static int
dead_link(struct Client *to, char *notice)
{

  SetDead(to);

#if 0
  exit_client(to, to, &me,
              (to->flags & FLAGS_SENDQEX) ?
              "SendQ exceeded" : "Dead socket");

#endif
  /*
   * If because of buffer problem then clean linebuf's now so that
   * notices don't hurt operators below.
   */
  linebuf_donebuf(&to->localClient->buf_recvq);
  linebuf_donebuf(&to->localClient->buf_sendq);
  if (!IsPerson(to) && !IsUnknown(to) && !(to->flags & FLAGS_CLOSING))
    sendto_realops_flags(FLAGS_ALL,
                         notice, get_client_name(to, HIDE_IP));

  Debug((DEBUG_ERROR, notice, get_client_name(to, HIDE_IP)));
  return (-1);
} /* dead_link() */


/*
** send_message
**      Internal utility which delivers one message buffer to the
**      socket. Takes care of the error handling and buffering, if
**      needed.
**
**      if msg is a null pointer, we are flushing connection
*/
static int
_send_message(struct Client *to, char *msg, int len)
{
  int linebuf_flags = 0;
  char *linebuf_key = NULL;
  
#ifdef INVARIANTS
  if (IsMe(to))
    {
      sendto_realops_flags(FLAGS_ALL,
			   "Trying to send to myself! [%s]", msg);
      return 0;
    }
#endif

  if (to->fd < 0)
    return 0; /* Thou shalt not write to closed descriptors */

  if (IsDead(to))
    return 0; /* This socket has already been marked as dead */

  if (linebuf_len(&to->localClient->buf_sendq) > get_sendq(to))
    {
      if (IsServer(to))
        sendto_realops_flags(FLAGS_ALL,
			     "Max SendQ limit exceeded for %s: %u > %lu",
			     get_client_name(to, HIDE_IP),
          linebuf_len(&to->localClient->buf_sendq), get_sendq(to));
      if (IsClient(to))
        to->flags |= FLAGS_SENDQEX;
      return dead_link(to, "Max Sendq exceeded");
    }
  else
    {
#ifdef OPENSSL
      if (IsCryptOut(to))
      {
        linebuf_flags |= LINEBUF_CRYPT;
        linebuf_key = (char *)to->localClient->out_key;
      }
#endif
      if (len)
          linebuf_put(&to->localClient->buf_sendq, msg, len,
                      linebuf_flags, linebuf_key);
    }
    /*
    ** Update statistics. The following is slightly incorrect
    ** because it counts messages even if queued, but bytes
    ** only really sent. Queued bytes get updated in SendQueued.
    */
    to->localClient->sendM += 1;
    me.localClient->sendM += 1;

    /*
     * Now we register a write callback. We *could* try to write some
     * data to the FD, it'd be an optimisation, and we can deal with it
     * later.
     *     -- adrian
     * So I took adrian's comments to heart..whether it works or not remains
     * to been seen...This seems to be a bad idea as other connections die
     * of starvation..
     * -- Aaron
     */
#if 0
    send_queued_write(to->fd, to);	
#endif
#if 1
    comm_setselect(to->fd, FDLIST_IDLECLIENT, COMM_SELECT_WRITE,
      send_queued_write, to, 0);
#endif
    return 0;
} /* send_message() */

/*
 * send_message_remote
 * 
 */
static void
send_message_remote(struct Client *to, struct Client *from,
		    const char *lsendbuf, int len)

{
  if(to->from)
    to = to->from;

  if(ServerInfo.hub && IsCapable(to, CAP_LL))
    {
      if(((from->lazyLinkClientExists &
           to->localClient->serverMask) == 0))
        client_burst_if_needed(to, from);
    }

/* Optimize by checking if (from && to) before everything */
  if (!MyClient(from) && IsPerson(to) && (to->from == from->from))
    {
      if (IsServer(from))
        {
          sendto_realops_flags(FLAGS_ALL,
		      "Send message to %s[%s] dropped from %s(Fake Dir)",
			      to->name, to->from->name, from->name);
          return;
        }

      sendto_realops_flags(FLAGS_ALL,
			   "Ghosted: %s[%s@%s] from %s[%s@%s] (%s)",
			   to->name, to->username, to->host,
			   from->name, from->username, from->host,
			   to->from->name);
      
      sendto_ll_serv_butone(NULL, to, 0,
                            ":%s KILL %s :%s (%s[%s@%s] Ghosted %s)",
                            me.name, to->name, me.name, to->name,
                            to->username, to->host, to->from->name);

      to->flags |= FLAGS_KILLED;

      if (IsPerson(from))
        sendto_one(from, form_str(ERR_GHOSTEDCLIENT),
                   me.name, from->name, to->name, to->username,
                   to->host, to->from);

      exit_client(NULL, to, &me, "Ghosted client");
      
      return;
    } /* if (!MyClient(from) && IsPerson(to) && (to->from == from->from)) */
  
  deprintf("send", "Sending [%s] to %s", lsendbuf, to->name);

  _send_message(to, (char *)lsendbuf, len);
  return;
} /* send_message_remote() */

/*
** send_queued_write
**      This is called when there is a chance the some output would
**      be possible. This attempts to empty the send queue as far as
**      possible, and then if any data is left, a write is rescheduled.
*/
void
send_queued_write(int fd, void *data)
{
  struct Client *to = data;
  int retlen;

  /*
  ** Once socket is marked dead, we cannot start writing to it,
  ** even if the error is removed...
  */
#ifdef INVARIANTS
  if (IsDead(to)) {
    /*
     * Actually, we should *NEVER* get here--something is
     * not working correct if send_queued is called for a
     * dead socket... --msa
     */
    return;
  } /* if (IsDead(to)) */
#endif

  /* Next, lets try to write some data */
  if (linebuf_len(&to->localClient->buf_sendq)) {
    retlen = linebuf_flush(to->fd, &to->localClient->buf_sendq);
    if ((retlen < 0) && (ignoreErrno(errno))) {
      /* we have a non-fatal error, so just continue */
    } else if (retlen < 0) {
      /* We have a fatal error */
      dead_link(to, "Write error to %s, closing link");
      return;
    } else if (retlen == 0) {
      /* 0 bytes is an EOF .. */
      dead_link(to, "EOF during write to %s, closing link");
      return;
    } else {
      /* We have some data written .. update counters */
      to->localClient->sendB += retlen;
      me.localClient->sendB += retlen;
      if (to->localClient->sendB > 1023) { 
        to->localClient->sendK += (to->localClient->sendB >> 10);
        to->localClient->sendB &= 0x03ff;        /* 2^10 = 1024, 3ff = 1023 */
      } else if (me.localClient->sendB > 1023) { 
        me.localClient->sendK += (me.localClient->sendB >> 10);
        me.localClient->sendB &= 0x03ff;
      }
    }
  }

  /* Finally, if we have any more data, reschedule a write */
  if (linebuf_len(&to->localClient->buf_sendq))
      comm_setselect(fd, FDLIST_IDLECLIENT, COMM_SELECT_WRITE,
        send_queued_write, to, 0);
} /* send_queued_write() */

/*
 * sendto_one
 *
 * inputs	- pointer to destination client
 *		- var args message
 * output	- NONE
 * side effects	- send message to single client
 */
void
sendto_one(struct Client *to, const char *pattern, ...)

{
  char sendbuf[IRCD_BUFSIZE*2];
  int len;
  va_list args;

  /* send remote if to->from non NULL */
  if (to->from)
    to = to->from;
  
#ifdef INVARIANTS
  if (to->fd < 0)
    {
      Debug((DEBUG_ERROR,
             "Local socket %s with negative fd... AARGH!",
             to->name));
    }
  else if (IsMe(to))
    {
      sendto_realops_flags(FLAGS_ALL,
			   "Trying to send [%s] to myself!", sendbuf);
      return;
    }
#endif

  va_start(args, pattern);
  len = send_format(sendbuf, pattern, args);
  va_end(args);

  send_message(to, (char *)sendbuf, len);
  deprintf("send", "Sending [%s] to %s", sendbuf, to->name);
} /* sendto_one() */

/*
 * sendto_one_prefix
 *
 * inputs	- pointer to destination client
 *		- pointer to client to form prefix from
 *		- var args message
 * output	- NONE
 * side effects	- send message to single client
 */
void
sendto_one_prefix(struct Client *to, struct Client *prefix,
		  const char *pattern, ...)

{
  int ilen, llen;
  va_list args;
  char sendbuf[IRCD_BUFSIZE*2];
  char ibuf[IRCD_BUFSIZE + 1];
  char lbuf[IRCD_BUFSIZE + 1];
  struct Client *to_sendto;
  
  /* send remote if to->from non NULL */
  if (to->from)
    to_sendto = to->from;
  else
    to_sendto = to;
  
#ifdef INVARIANTS
  if (to->fd < 0)
    {
      Debug((DEBUG_ERROR,
             "Local socket %s with negative fd... AARGH!",
             to->name));
      return;
    }
  else if (IsMe(to))
    {
      sendto_realops_flags(FLAGS_ALL,
			   "Trying to send [%s] to myself!", sendbuf);
      return;
    }
#endif

  va_start(args, pattern);
  send_format(sendbuf, pattern, args);
  va_end(args);

  ilen = ircsprintf(ibuf, ":%s %s", ID(prefix), sendbuf);
  ilen = send_trim(ibuf, ilen);

  llen = ircsprintf(lbuf, ":%s %s", prefix->name, sendbuf);
  llen = send_trim(lbuf, llen);

  if (IsServer(to_sendto) && IsCapable(to->from, CAP_UID))
    send_message(to_sendto, ibuf, ilen);
  else
    send_message(to_sendto, lbuf, llen);
  
  deprintf("send", "Sending [%s] to %s", sendbuf, to->name);
} /* sendto_one() */

/*
 * sendto_channel_butone
 *
 * inputs	- pointer to client(server) to NOT send message to
 *		- pointer to client that is sending this message
 *		- pointer to channel being sent to
 *		- vargs message
 * output	- NONE
 * side effects	- message as given is sent to given channel members.
 */
void
sendto_channel_butone(struct Client *one, struct Client *from,
		      struct Channel *chptr, char *command,
                      const char *pattern, ...)
{
  char buf[IRCD_BUFSIZE*2];
  char local_prefix[NICKLEN+HOSTLEN+USERLEN+CHANNELLEN+10];
  char local_sendbuf[IRCD_BUFSIZE*2];
  char remote_prefix[NICKLEN+HOSTLEN+USERLEN+CHANNELLEN+10];
  char remote_sendbuf[IRCD_BUFSIZE*2];
  int remote_len;
  int local_len;
  va_list    args;

  va_start(args, pattern);
  send_format(buf,pattern,args);
  va_end(args);

  if(IsServer(from))
    {
      (void)ircsprintf(local_prefix,":%s %s %s ",
		       from->name, command,
                       RootChan(chptr)->chname);
    }
  else
    {
      (void)ircsprintf(local_prefix,":%s!%s@%s %s %s ",
		       from->name,
		       from->username,
		       from->host,
                       command,
                       RootChan(chptr)->chname);
    }

  (void)ircsprintf(remote_prefix,":%s %s %s ", from->name,
                   command, chptr->chname);

  remote_len = ircsprintf(remote_sendbuf, "%s%s",remote_prefix,buf);
  remote_len = send_trim(remote_sendbuf, remote_len);

  local_len = ircsprintf(local_sendbuf, "%s%s",local_prefix,buf);
  local_len = send_trim(local_sendbuf, local_len);

  ++current_serial;
  
  sendto_list_anywhere(one, from, &chptr->chanops,
		       (const char *)local_sendbuf, local_len,
		       (const char *)remote_sendbuf, remote_len);

  sendto_list_anywhere(one, from, &chptr->voiced,
		       (const char *)local_sendbuf, local_len,
		       (const char *)remote_sendbuf, remote_len);

  sendto_list_anywhere(one, from, &chptr->halfops,
		       (const char *)local_sendbuf, local_len,
		       (const char *)remote_sendbuf, remote_len);

  sendto_list_anywhere(one, from, &chptr->peons,
		       (const char *)local_sendbuf, local_len,
		       (const char *)remote_sendbuf, remote_len);

} /* sendto_channel_butone() */

/*
 * sendto_list_anywhere
 *
 * inputs	- pointer to client NOT to send back towards
 *		- pointer to client from where message is coming from
 *		- pointer to channel list to send
 *		- pointer to sendbuf to use for local clients
 *		- length of local_sendbuf
 *		- pointer to sendbuf to use for remote clients
 *		- length of remote_sendbuf
 */
void
sendto_list_anywhere(struct Client *one, struct Client *from,
		     dlink_list *list,
		     const char *llocal_sendbuf, int llocal_len,
		     const char *lremote_sendbuf, int lremote_len
		     )
{
  dlink_node *ptr;
  struct Client *target_p;

  for (ptr = list->head; ptr; ptr = ptr->next)
    {
      target_p = ptr->data;
      
      if (target_p->from == one)
        continue;
      
      if (MyConnect(target_p) && IsRegisteredUser(target_p))
        {
          if(target_p->serial != current_serial)
	    {
	      send_message(target_p, (char *)llocal_sendbuf, llocal_len);
	      target_p->serial = current_serial;
	    }
        }
      else
        {
          /*
           * Now check whether a message has been sent to this
           * remote link already
           */
          if(target_p->from->serial != current_serial)
            {
	      send_message_remote(target_p, from, 
				  (char *)lremote_sendbuf, lremote_len);
              target_p->from->serial = current_serial;
            }
        }
    }
}

/*
 * sendto_serv_butone
 *
 * inputs	- pointer to client to not send to
 *		- var arg pattern to send
 * output	- NONE
 * side effects	- Send a message to all connected servers
 *                except the client 'one'.
 */
void
sendto_serv_butone(struct Client *one, const char *pattern, ...)
{
  int len;
  va_list args;
  char sendbuf[IRCD_BUFSIZE*2];
  struct Client *client_p;
  dlink_node *ptr;

  va_start(args, pattern);
  len = send_format(sendbuf,pattern,args);
  va_end(args);
  
  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (one && (client_p == one->from))
        continue;
      
      send_message(client_p, (char *)sendbuf, len);
    }
} /* sendto_serv_butone() */

/*
 * sendto_ll_serv_butone
 *
 * inputs	- pointer to client to not send to
 *		- var arg pattern to send
 * output	- NONE
 * side effects	- Send a message to all connected servers
 *                except the client 'one'. Also deal with
 *		  client being unknown to leaf, as in lazylink...
 */
void
sendto_ll_serv_butone(struct Client *one, struct Client *source_p, int add,
		      const char *pattern, ...)
{
  int len;
  va_list args;
  char sendbuf[IRCD_BUFSIZE*2];
  struct Client *client_p;
  dlink_node *ptr;

  va_start(args, pattern);
  len = send_format(sendbuf,pattern,args);
  va_end(args);
  
  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (one && (client_p == one->from))
        continue;
      
      if (IsCapable(client_p,CAP_LL) && ServerInfo.hub)
	{
	  if( ( source_p->lazyLinkClientExists &
		client_p->localClient->serverMask) == 0)
	    {
	      if(add)
		{
                  client_burst_if_needed(client_p,source_p);
		  send_message(client_p, (char *)sendbuf, len);
		}
	    }
	  else
	    send_message(client_p, (char *)sendbuf, len);
	}
      else
	send_message(client_p, (char *)sendbuf, len);
    }
} /* sendto_ll_serv_butone() */

/*
 * sendto_cap_serv_butone
 * 
 * inputs       - int capability mask
 *              - pointer to client NOT to send to or NULL
 *		- var args pattern of message to send
 * output	- NONE
 * side effects - Send a message to all connected servers
 *                except the client 'one'.
 */
void
sendto_cap_serv_butone(int cap, struct Client *one, const char *pattern, ...)
{
  int len;
  va_list args;
  char sendbuf[IRCD_BUFSIZE*2];
  struct Client *client_p;
  dlink_node *ptr;

  va_start(args, pattern);
  len = send_format(sendbuf,pattern,args);
  va_end(args);
  
  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (one && (client_p == one->from))
        continue;
      
      if (IsCapable(client_p,cap))
	send_message(client_p, (char *)sendbuf, len);
    }
} /* sendto_cap_serv_butone() */

/*
 * sendto_nocap_serv_butone
 *
 * inputs       - int capability mask NOT to send to.
 *              - pointer to client NOT to send to or NULL
 *              - var args pattern of message to send
 * output       - NONE
 * side effects - Send a message to all connected servers
 *                except the client 'one'.
 */
void
sendto_nocap_serv_butone(int cap, struct Client *one, const char *pattern, ...)
{
  int len;
  va_list args;
  char sendbuf[IRCD_BUFSIZE*2];
  struct Client *client_p;
  dlink_node *ptr;

  va_start(args, pattern);
  len = send_format(sendbuf,pattern,args);
  va_end(args);

  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (one && (client_p == one->from))
        continue;

      if (!IsCapable(client_p,cap))
        send_message(client_p, (char *)sendbuf, len);
    }
} /* sendto_cap_serv_butone() */

/*
 * sendto_common_channels_local()
 *
 * inputs	- pointer to client
 *		- pattern to send
 * output	- NONE
 * side effects	- Sends a message to all people on local server who are
 * 		  in same channel with user. 
 *		  used by m_nick.c and exit_one_client.
 */
void
sendto_common_channels_local(struct Client *user, const char *pattern, ...)

{
  int len;
  va_list args;
  char sendbuf[IRCD_BUFSIZE*2];
  dlink_node *ptr;
  struct Channel *chptr;

  va_start(args, pattern);
  len = send_format(sendbuf, pattern, args);
  va_end(args);

  ++current_serial;

  if (user->user)
    {
      for (ptr = user->user->channel.head; ptr; ptr = ptr->next)
	{
	  chptr = ptr->data;

	  sendto_list_local(&chptr->chanops, sendbuf, len);
	  sendto_list_local(&chptr->halfops, sendbuf, len);
	  sendto_list_local(&chptr->voiced, sendbuf, len);
	  sendto_list_local(&chptr->peons, sendbuf, len);
	}

      if (MyConnect(user) && (user->serial != current_serial))
	send_message(user, sendbuf, len);
    }
} /* sendto_common_channels() */

/*
 * sendto_channel_local
 *
 * inputs	- int type, i.e. ALL_MEMBERS, NON_CHANOPS,
 *                ONLY_CHANOPS_VOICED, ONLY_CHANOPS
 *              - pointer to channel to send to
 *              - var args pattern
 * output	- NONE
 * side effects - Send a message to all members of a channel that are
 *		  locally connected to this server.
 */
void
sendto_channel_local(int type,
		     struct Channel *chptr,
		     const char *pattern, ...)
{
  int len;
  char sendbuf[IRCD_BUFSIZE*2];
  va_list args;

  va_start(args, pattern);
  len = send_format(sendbuf, pattern, args);
  va_end(args);

  /* Serial number checking isn't strictly necessary, but won't hurt */
  ++current_serial;

  switch(type)
    {
    default:
    case ALL_MEMBERS:
      sendto_list_local(&chptr->chanops, sendbuf, len);
      sendto_list_local(&chptr->halfops, sendbuf, len);
      sendto_list_local(&chptr->voiced,  sendbuf, len);
      sendto_list_local(&chptr->peons,   sendbuf, len);
      break;

    case NON_CHANOPS:
      sendto_list_local(&chptr->voiced,  sendbuf, len);
      sendto_list_local(&chptr->peons,   sendbuf, len);
      break;

    case ONLY_CHANOPS_HALFOPS_VOICED:
      sendto_list_local(&chptr->chanops, sendbuf, len);
      sendto_list_local(&chptr->halfops, sendbuf, len);
      sendto_list_local(&chptr->voiced,  sendbuf, len);
      break;

    case ONLY_CHANOPS_HALFOPS:
      sendto_list_local(&chptr->chanops, sendbuf, len);
      sendto_list_local(&chptr->halfops, sendbuf, len);
      break;

    case ONLY_CHANOPS:
      sendto_list_local(&chptr->chanops, sendbuf, len);
    }
} /* sendto_channel_local() */

/*
 * sendto_list_local
 *
 * inputs	- pointer to all members of this list
 *		- buffer to send
 *		- length of buffer
 * output	- NONE
 * side effects	- all members who are locally on this server on given list
 *		  are sent given message. Right now, its always a channel list
 *		  but there is no reason we could not use another dlink
 *		  list to send a message to a group of people.
 */
static void
sendto_list_local(dlink_list *list, const char *lsendbuf, int len)
{
  dlink_node *ptr;
  struct Client *target_p;

  for (ptr = list->head; ptr; ptr = ptr->next)
    {
      if ( (target_p = ptr->data) == NULL )
	continue;

      if (!MyConnect(target_p) || (target_p->fd < 0))
	continue;

      if (target_p->serial == current_serial)
	continue;
      
      target_p->serial = current_serial;

      if (target_p && MyConnect(target_p))
	send_message(target_p, (char *)lsendbuf, len);
    }  
} /* sendto_list() */

/*
 * sendto_channel_remote
 *
 * inputs	- pointer to channel
 *              - from pointer
 *              - var args pattern
 * output       - NONE
 * side effects - send to all servers the channel given, except for "from"
 */
void
sendto_channel_remote(struct Channel *chptr,
		      struct Client *from,
		      const char *pattern, ...)
{
  int len;
  char sendbuf[IRCD_BUFSIZE*2];
  va_list args;
  struct Client *client_p;
  dlink_node *ptr;

  if (chptr != NULL)
    {
      if (*chptr->chname == '&')
        return;
    }
  else
    return;

  va_start(args, pattern);
  len = send_format(sendbuf,pattern,args);
  va_end(args);

  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (client_p == from)
        continue;

      if (ServerInfo.hub && IsCapable(client_p,CAP_LL))
	{
	  if( !(RootChan(chptr)->lazyLinkChannelExists &
		client_p->localClient->serverMask) )
	    continue;
	}

      send_message (client_p, (char *)sendbuf, len);
    }
} /* sendto_channel_remote() */

/*
 * sendto_channel_remote
 *
 * inputs	- pointer to channel
 *              - from pointer
 *              - var args pattern
 * output       - NONE
 * side effects - send to all servers the channel given, except for "from"
 *		  This code is only used in m_join.c,m_sjoin.c
 */
void
sendto_channel_remote_prefix(struct Channel *chptr,
		      struct Client *from,
                      struct Client *prefix,
		      const char *pattern, ...)
{
  int ilen, llen;
  va_list args;
  struct Client *client_p;
  dlink_node *ptr;
  char sendbuf[IRCD_BUFSIZE*2];
  char lbuf[IRCD_BUFSIZE + 1];
  char ibuf[IRCD_BUFSIZE + 1];
  
  if (chptr != NULL)
    {
      if (*chptr->chname == '&')
        return;
    }
  else
	  return;

  va_start(args, pattern);
  send_format(sendbuf,pattern,args);
  va_end(args);

  /* with ID */
  ilen = ircsprintf(ibuf, ":%s %s", ID(prefix), sendbuf);
  /* without ID */
  llen = sprintf(lbuf, ":%s %s", prefix->name, sendbuf);
  
  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (client_p == from)
        continue;

      if (ServerInfo.hub && IsCapable(client_p,CAP_LL))
	{
	  if( !(RootChan(chptr)->lazyLinkChannelExists &
		client_p->localClient->serverMask) )
	    continue;
	}
	  if (IsCapable(client_p, CAP_UID)) 
	    send_message (client_p, ibuf, ilen);
	  else
	    send_message (client_p, lbuf, llen);
	  
    }
} /* sendto_channel_remote() */

/*
 * sendto_ll_channel_remote
 *
 * inputs	- pointer to channel
 *              - from pointer
 *              - var args pattern
 * output       - NONE
 * side effects - send to all servers the channel given, except for "from"
 *		  This code is only used in m_join.c,m_sjoin.c
 *		  It will introduce the client 'source_p' if it is unknown
 *		  to the leaf.
 */
void
sendto_ll_channel_remote(struct Channel *chptr,
			 struct Client *from, struct Client *source_p,
			 const char *pattern, ...)
{
  int len;
  va_list args;
  char sendbuf[IRCD_BUFSIZE*2];
  struct Client *client_p;
  dlink_node *ptr;

  if (chptr != NULL)
    {
      if (*chptr->chname == '&')
        return;
    }
  else
    return;

  va_start(args, pattern);
  len = send_format(sendbuf,pattern,args);
  va_end(args);

  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (client_p == from)
        continue;

      if (IsCapable(client_p,CAP_LL))
	{
	  if(ServerInfo.hub)
	    {
              /* Only tell leafs that already know about the channel */
              if ((RootChan(chptr)->lazyLinkChannelExists &
                   client_p->localClient->serverMask) == 0)
              {
                continue;
              }
	      if (source_p &&
                  ((source_p->lazyLinkClientExists &
                   client_p->localClient->serverMask) == 0))
              {
                sendnick_TS(client_p,source_p);
                add_lazylinkclient(client_p,source_p);
              }
	    }
	}

      send_message (client_p, (char *)sendbuf, len);
    }
} /* sendto_channel_remote() */

/*
 * sendto_match_cap_servs
 *
 * inputs	- channel pointer
 * 		- client to not send back towards (often a server)
 *		- integer capability
 *		- var args message
 * output	- NONE
 * side effects - send to all servers but 'from',
 *                to the given channel only if
 *                they match the capability, the given message.
 */

void
sendto_match_cap_servs(struct Channel *chptr, struct Client *from, int cap, 
                       const char *pattern, ...)
{
  int len;
  char sendbuf[IRCD_BUFSIZE*2];
  va_list args;
  struct Client *client_p;
  dlink_node *ptr;

  if (chptr)
    {
      if (*chptr->chname == '&')
        return;
    }

  va_start(args, pattern);
  len = send_format(sendbuf,pattern,args);
  va_end(args);

  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (client_p == from)
        continue;
      
      if(!IsCapable(client_p, cap))
        continue;

      if (ServerInfo.hub && IsCapable(client_p,CAP_LL))
        {
          if( !(RootChan(chptr)->lazyLinkChannelExists &
                client_p->localClient->serverMask) )
            continue;
        }
      
      send_message (client_p, (char *)sendbuf, len);
    }
} /* sendto_match_cap_servs() */

#define MAX_CAPS 12

/* eww. */
void
sendto_match_vacap_servs(struct Channel *chptr, struct Client *from, ...)
{
	va_list args;
	int len;
	char sendbuf[IRCD_BUFSIZE*2];
	struct Client *client_p;
	dlink_node *ptr;
	char *pattern;
	int caps[MAX_CAPS];
	int ncaps;
	int cap;
	
	if (chptr)
	{
		if (*chptr->chname == '&')
			return;
	}
	
	va_start(args, from);
	
	/* at this point, we have a list of CAPs followed by a terminating 0 */
	for (ncaps = 0; (cap = va_arg(args, int)); ncaps++)
	{
		/* for each cap, add it to the caps list */
		if (ncaps > MAX_CAPS)
		{
			sendto_realops_flags(FLAGS_ALL, "Warning: too many caps passed to sendto_match_vacap_servs!");
			return;
		}
		
		caps[ncaps] = cap;
	}
	
	/* now, we should be pointer to the format pointer */
	pattern = va_arg(args, char *);
	len = send_format(sendbuf, pattern, args);
	va_end(args);

	/* whew.. now we have a list of caps in caps[],
	   and sendbuf contains the data to send.. */

	for (ptr = serv_list.head; ptr; ptr = ptr->next)
	{
		int i, hascaps = 1;
		
		client_p = ptr->data;
		
		if (client_p == from)
			continue;
		
		/* check it supports *all* the caps.. */
		for (i = 0; i < ncaps && hascaps; i++)
		{
			if (!IsCapable(client_p, caps[i])) {
				hascaps = 0;
				break;
			}
		}
		
		if (!hascaps)
			continue;
		
		if (ServerInfo.hub && IsCapable(client_p, CAP_LL))
		{
			if (!(RootChan(chptr)->lazyLinkChannelExists &
				  client_p->localClient->serverMask))
				continue;
		}
		
		send_message(client_p, sendbuf, len);
	}
}

			
/*
 * sendto_match_cap_servs
 *
 * inputs	- channel pointer
 * 		- client to not send back towards (often a server)
 *		- integer capability
 *		- var args message
 * output	- NONE
 * side effects - send to all servers but 'from',
 *                to the given channel only if
 *                they match the capability, the given message.
 */

void
sendto_match_cap_servs_nocap(struct Channel *chptr, struct Client *from,
                             int cap, int nocap, const char *pattern, ...)
{
  int len;
  char sendbuf[IRCD_BUFSIZE*2];
  va_list args;
  struct Client *client_p;
  dlink_node *ptr;

  if (chptr)
    {
      if (*chptr->chname == '&')
        return;
    }

  va_start(args, pattern);
  len = send_format(sendbuf,pattern,args);
  va_end(args);

  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (client_p == from)
        continue;
      
      if(!IsCapable(client_p, cap) || IsCapable(client_p, nocap))
        continue;

      if (ServerInfo.hub && IsCapable(client_p,CAP_LL))
        {
          if( !(RootChan(chptr)->lazyLinkChannelExists &
                client_p->localClient->serverMask) )
            continue;
        }
      
      send_message (client_p, (char *)sendbuf, len);
    }
} /* sendto_match_cap_servs() */

void
sendto_match_nocap_servs(struct Channel *chptr, struct Client *from, int cap,
                         const char *pattern, ...)
{
  int len;
  va_list args;
  char sendbuf[IRCD_BUFSIZE*2];
  struct Client *client_p;
  dlink_node *ptr;

  if (chptr)
    {
      if (*chptr->chname == '&')
        return;
    }

  va_start(args, pattern);
  len = send_format(sendbuf,pattern,args);
  va_end(args);

  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (client_p == from)
        continue;
      
      if(IsCapable(client_p, cap))
        continue;

      if (ServerInfo.hub && IsCapable(client_p,CAP_LL))
        {
          if( !(RootChan(chptr)->lazyLinkChannelExists &
                client_p->localClient->serverMask) )
            continue;
        }
      
      send_message (client_p, (char *)sendbuf, len);
    }
} /* sendto_match_cap_servs() */


/*
** match_it() and sendto_match_butone() ARE only used
** to send a msg to all ppl on servers/hosts that match a specified mask
** (used for enhanced PRIVMSGs) for opers
**
** addition -- Armin, 8jun90 (gruner@informatik.tu-muenchen.de)
**
*/

/*
 * match_it
 *
 * inputs	- client pointer to match on
 *		- actual mask to match
 *		- what to match on, HOST or SERVER
 * output	- 1 or 0 if match or not
 * side effects	- NONE
 */
static int
match_it(const struct Client *one, const char *mask, int what)

{
  if(what == MATCH_HOST)
    return match(mask, one->host);

  return match(mask, one->user->server);
} /* match_it() */

/*
 * sendto_match_butone
 *
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 *
 * ugh. ONLY used by m_message.c to send an "oper magic" message. ugh.
 */
void
sendto_match_butone(struct Client *one, struct Client *from,
		    char *mask, int what,
		    const char *pattern, ...)
{
  int len;
  va_list args;
  struct Client *client_p;
  dlink_node *ptr;
  char sendbuf[IRCD_BUFSIZE*2];
  char lbuf[IRCD_BUFSIZE*2];

  va_start(args, pattern);
  send_format(sendbuf, pattern, args);
  va_end(args);

  if(IsServer(from))
    len = ircsprintf(lbuf,":%s %s", from->name, sendbuf);
  else
    len = ircsprintf(lbuf,":%s!%s@%s %s", from->name,
               from->username, from->host, sendbuf);

  /* scan the local clients */
  for(ptr = lclient_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (client_p == one)  /* must skip the origin !! */
        continue;
      
      if (match_it(client_p, mask, what))
	send_message(client_p, (char *)lbuf, len);
    }

  /* Now scan servers */
  len = ircsprintf(lbuf,":%s %s", from->name, sendbuf);
  for (ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (client_p == one) /* must skip the origin !! */
        continue;

      /*
       * The old code looped through every client on the
       * network for each server to check if the
       * server (client_p) has at least 1 client matching
       * the mask, using something like:
       *
       * for (target_p = GlobalClientList; target_p; target_p = target_p->next)
       *        if (IsRegisteredUser(target_p) &&
       *                        match_it(target_p, mask, what) &&
       *                        (target_p->from == client_p))
       *   vsendto_prefix_one(client_p, from, pattern, args);
       *
       * That way, we wouldn't send the message to
       * a server who didn't have a matching client.
       * However, on a network such as EFNet, that
       * code would have looped through about 50
       * servers, and in each loop, loop through
       * about 50k clients as well, calling match()
       * in each nested loop. That is a very bad
       * thing cpu wise - just send the message
       * to every connected server and let that
       * server deal with it.
       * -wnder
       */

      send_message_remote(client_p, from, lbuf, len);
    }


} /* sendto_match_butone() */


/*
 * sendto_anywhere
 *
 * inputs	- pointer to dest client
 * 		- pointer to from client
 * 		- varags
 * output	- NONE
 * side effects	- less efficient than sendto_remote and sendto_one
 * 		  but useful when one does not know where target "lives"
 */
void
sendto_anywhere(struct Client *to, struct Client *from,
		const char *pattern, ...)
{
  int len;
  va_list args;
  char sendbuf[IRCD_BUFSIZE*2];
  char prefix[NICKLEN+HOSTLEN+USERLEN+5];	/* same as USERHOST_REPLYLEN */
  char lbuf[IRCD_BUFSIZE*2];

  va_start(args, pattern);
  len = send_format(lbuf, pattern, args);
  va_end(args);

  if(MyClient(to))
    {
      if(IsServer(from))
	{
	  ircsprintf(prefix,":%s ",
		     from->name);
	}
      else
	{
	  ircsprintf(prefix,":%s!%s@%s ",
		     from->name,
		     from->username,
		     from->host);
	}
    }
  else
    {
      ircsprintf(prefix,":%s ", from->name);
    }

  len = ircsprintf(sendbuf,"%s%s", prefix,lbuf);
  len = send_trim(sendbuf, len);
      
  if(MyClient(to))
    send_message(to, (char *)sendbuf, len);
  else
    send_message_remote(to, from, (char *)sendbuf, len);
}

/*
 * sendto_realops_flags
 *
 * inputs	- flag types of messages to show to real opers
 *		- var args input message
 * output	- NONE
 * side effects	- Send to *local* ops only but NOT +s nonopers.
 */

void
sendto_realops_flags(int flags, const char *pattern, ...)

{
  int len;
  struct Client *client_p;
  char sendbuf[IRCD_BUFSIZE*2];
  char nbuf[IRCD_BUFSIZE*2];
  dlink_node *ptr;
  va_list args;

  va_start(args, pattern);
  len = send_format(nbuf, pattern, args);
  va_end(args);

  for (ptr = oper_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if((flags == FLAGS_NOTADMIN) && IsAdmin(client_p)) 
        continue;
      if(client_p->umodes & flags)
	{
	  len =ircsprintf(sendbuf, ":%s NOTICE %s :*** Notice -- %s",
			  me.name,
			  client_p->name,
			  nbuf);

	  len = send_trim(sendbuf,len);
	  send_message(client_p, (char *)sendbuf, len);
	}
    }
} /* sendto_realops_flags() */

/*
 * sendto_wallops_flags
 *
 * inputs       - flag types of messages to show to real opers
 *              - client sending request
 *              - var args input message
 * output       - NONE
 * side effects - Send a wallops to local opers
 */

void
sendto_wallops_flags(int flags, struct Client *source_p,
                           const char *pattern, ...)
{
  char prefix[NICKLEN + USERLEN + HOSTLEN + 5];
  int len;
  struct Client *client_p;
  char nbuf[IRCD_BUFSIZE*2];
  char sendbuf[IRCD_BUFSIZE*2];
  dlink_node *ptr;
  va_list args;

  va_start(args, pattern);
  len = send_format(nbuf, pattern, args);
  va_end(args);

  if(IsPerson(source_p))
    (void)ircsprintf(prefix, ":%s!%s@%s",
                     source_p->name, source_p->username, source_p->host);
  else
    (void)ircsprintf(prefix, ":%s", source_p->name);

  for (ptr = oper_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if(client_p->umodes & flags)
        {
          len =ircsprintf(sendbuf, "%s WALLOPS :%s", prefix, nbuf);
          len = send_trim(sendbuf,len);
          send_message(client_p, (char *)sendbuf, len);
        }
     }
}
/*
 * ts_warn
 * inputs	- var args message
 * output	- NONE
 * side effects	- Call sendto_realops_flags, with some flood checking
 *		  (at most 5 warnings every 5 seconds)
 */
 
void
ts_warn(const char *pattern, ...)
{
  va_list args;
  char lbuf[LOG_BUFSIZE];
  static time_t last = 0;
  static int warnings = 0;
 
  /*
  ** if we're running with TS_WARNINGS enabled and someone does
  ** something silly like (remotely) connecting a nonTS server,
  ** we'll get a ton of warnings, so we make sure we don't send
  ** more than 5 every 5 seconds.  -orabidoo
  */

  if (CurrentTime - last < 5)
    {
      if (++warnings > 5)
        return;
    }
  else
    {
      last = CurrentTime;
      warnings = 0;
    }

  va_start(args, pattern);
  (void)send_format(lbuf, pattern, args);
  va_end(args);

  sendto_realops_flags(FLAGS_ALL,"%s",lbuf);
  log(L_CRIT, "%s", lbuf);
} /* ts_warn() */


/*
 * send_format
 *
 * inputs	- buffer to format into
 *		- format pattern to use
 *		- var args
 * output	- number of bytes formatted output
 * side effects	- modifies sendbuf
 */
static int
send_format(char *lsendbuf, const char *pattern, va_list args)
{
  int len; /* used for the length of the current message */

  len = vsprintf_irc(lsendbuf, pattern, args);

  return (send_trim(lsendbuf,len));
}

/*
 * send_trim
 *
 * inputs	- pointer to buffer to trim
 *		- length of buffer
 * output	- new length of buffer if modified otherwise original len
 * side effects	- input buffer is trimmed if needed
 */
static int
send_trim(char *lsendbuf,int len)
{
  /*
   * from rfc1459
   *
   * IRC messages are always lines of characters terminated with a CR-LF
   * (Carriage Return - Line Feed) pair, and these messages shall not
   * exceed 512 characters in length,  counting all characters 
   * including the trailing CR-LF.
   * Thus, there are 510 characters maximum allowed
   * for the command and its parameters.  There is no provision for
   * continuation message lines.  See section 7 for more details about
   * current implementations.
   */

  /*
   * We have to get a \r\n\0 onto sendbuf[] somehow to satisfy
   * the rfc. We must assume sendbuf[] is defined to be 513
   * bytes - a maximum of 510 characters, the CR-LF pair, and
   * a trailing \0, as stated in the rfc. Now, if len is greater
   * than the third-to-last slot in the buffer, an overflow will
   * occur if we try to add three more bytes, if it has not
   * already occured. In that case, simply set the last three
   * bytes of the buffer to \r\n\0. Otherwise, we're ok. My goal
   * is to get some sort of vsnprintf() function operational
   * for this routine, so we never again have a possibility
   * of an overflow.
   * -wnder
   */

  if(len > 510)
    {
      lsendbuf[IRCD_BUFSIZE-2] = '\r';
      lsendbuf[IRCD_BUFSIZE-1] = '\n';
      lsendbuf[IRCD_BUFSIZE] = '\0';
      return(IRCD_BUFSIZE);
    }
  return len;
}

/*
 * kill_client
 *
 * inputs	- client to send kill towards
 * 		- pointer to client to kill
 * 		- reason for kill
 * output	- NONE
 * side effects	- NONE
 */

void
kill_client(struct Client *client_p,
	    struct Client *diedie, const char *pattern, ...)
{
  va_list args;
  int len = 0;
  int len_buf = 0;
  char sendbuf[IRCD_BUFSIZE*2];
  char reason[IRCD_BUFSIZE*2];

  va_start(args, pattern);
  len = send_format(reason,"%s",args);
  va_end(args);
  
  if(HasID(diedie) && IsCapable(client_p, CAP_UID))
    {
      len_buf = ircsprintf(sendbuf, ":%s KILL %s :%s",
                           me.name, ID(diedie), reason);
      len_buf = send_trim(sendbuf, len_buf);
    }		    
  else
    {
      len_buf = ircsprintf(sendbuf, ":%s KILL %s :%s",
                           me.name, diedie->name, reason);
      len_buf = send_trim(sendbuf, len_buf);
    }

  send_message(client_p, sendbuf, len_buf);
}


/*
 * kill_client_ll_serv_butone
 *
 * inputs	- pointer to client to not send to
 *		- pointer to client to kill
 * output	- NONE
 * side effects	- Send a KILL for the given client
 *		  message to all connected servers
 *                except the client 'one'. Also deal with
 *		  client being unknown to leaf, as in lazylink...
 */
void
kill_client_ll_serv_butone(struct Client *one, struct Client *source_p,
			   const char *pattern, ...)
{
  va_list args;
  int len;
  int len_uid=0;
  int len_nick=0;
  int have_uid;
  struct Client *client_p;
  dlink_node *ptr;
  char sendbuf_uid[IRCD_BUFSIZE*2];
  char sendbuf_nick[IRCD_BUFSIZE*2];
  char reason[IRCD_BUFSIZE*2];

  va_start(args, pattern);
  len = send_format(reason,"%s",args);
  va_end(args);
  
  have_uid = 0;
  if(HasID(source_p))
    {
      len_uid = ircsprintf(sendbuf_uid,":%s KILL %s :%s",
			    me.name, ID(source_p), reason);
      len_uid = send_trim(sendbuf_uid, len_uid);
      have_uid = 1;
    }		    
  else
    {
      len_nick  = ircsprintf(sendbuf_nick,":%s KILL %s :%s",
			    me.name, source_p->name, reason);
      len_nick  = send_trim(sendbuf_nick, len_nick);
    }
  
  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      client_p = ptr->data;

      if (one && (client_p == one->from))
        continue;
      
      if (IsCapable(client_p,CAP_LL) && ServerInfo.hub)
	{
	  if( ( source_p->lazyLinkClientExists &
		client_p->localClient->serverMask) != 0)
	    {
	      if (have_uid && IsCapable(client_p, CAP_UID))
		send_message(client_p, (char *)sendbuf_uid, len_uid);
	      else
		send_message(client_p, (char *)sendbuf_nick, len_nick);
	    }
	}
      else
	{
	  if (have_uid && IsCapable(client_p, CAP_UID))
	    send_message(client_p, (char *)sendbuf_uid, len_uid);
	  else
	    send_message(client_p, (char *)sendbuf_nick, len_nick);
	}
    }
} 
