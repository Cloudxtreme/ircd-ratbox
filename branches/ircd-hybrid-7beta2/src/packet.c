/************************************************************************
 *   IRC - Internet Relay Chat, src/packet.c
 *   Copyright (C) 1990  Jarkko Oikarinen and
 *                       University of Oulu, Computing Center
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
 *
 *   $Id$
 */ 

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "tools.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "client.h"
#include "common.h"
#include "ircd.h"
#include "list.h"
#include "parse.h"
#include "fdlist.h"
#include "packet.h"
#include "irc_string.h"
#include "memory.h"


static char               readBuf[READBUF_SIZE];


/*
 * parse_client_queued - parse client queued messages
 */
static
void parse_client_queued(struct Client *cptr)
{ 
    int dolen  = 0;
#if 0
    struct LocalUser *lcptr = cptr->localClient;
    int checkflood = 1; /* Whether we're checking or not */
#endif

#if 0
    if (IsServer(cptr))
#endif
      {
	while ((dolen = linebuf_get(&cptr->localClient->buf_recvq,
				    readBuf, READBUF_SIZE)) > 0)
        {
          if (IsDead(cptr))
	    {
	      linebuf_donebuf(&cptr->localClient->buf_recvq);
	      linebuf_donebuf(&cptr->localClient->buf_sendq);
	      return;
	    }
	  client_dopacket(cptr, readBuf, dolen);
	}
      }
#if 0
    else
      {
	checkflood = 0;
	if (ConfigFileEntry.no_oper_flood && IsOper(cptr))
	  checkflood = 0;

	/*
	 * Handle flood protection here - if we exceed our flood limit on
	 * messages in this loop, we simply drop out of the loop prematurely.
	 *   -- adrian
	 */

	for (;;) {
	  if (checkflood && (lcptr->sent_parsed > lcptr->allow_read))
            break;
	  assert(lcptr->sent_parsed <= lcptr->allow_read);
	  dolen = linebuf_get(&cptr->localClient->buf_recvq, readBuf,
			      READBUF_SIZE);
	  if (!dolen)
            break;
	  client_dopacket(cptr, readBuf, dolen);
	  lcptr->sent_parsed++;
	}
      }
#endif
}

/*
 * flood_recalc
 *
 * recalculate the number of allowed flood lines. this should be called
 * once a second on any given client. We then attempt to flush some data.
 */
void
flood_recalc(int fd, void *data)
{
    struct Client *cptr = data;
    struct LocalUser *lcptr = cptr->localClient;

    assert(cptr != NULL);
    assert(lcptr != NULL);

    /* 
     * If we're a server, skip to the end. Realising here that this call is
     * cheap and it means that if a op is downgraded they still get considered
     * for anti-flood protection ..
     */
    if (!IsPrivileged(cptr))
      {
	/*
	 * ok, we have to recalculate the number of messages we can receive
	 * in this second, based upon what happened in the last second.
	 * If we still exceed the flood limit, don't move the parsed limit.
	 * If we are below the flood limit, increase the flood limit.
	 *   -- adrian
	 */

	if (lcptr->allow_read == 0)
	  /* Give the poor person a go! */
	  lcptr->allow_read = 1;
	else if (lcptr->actually_read < lcptr->allow_read)
	  /* Raise the allowed messages if we flooded under the limit */
	  lcptr->allow_read++;
	else
	  /* Drop the limit to avoid flooding .. */
	  lcptr->allow_read--;

	/* Enforce floor/ceiling restrictions */
	if (lcptr->allow_read < 1)
	  lcptr->allow_read = 1;
	else if (lcptr->allow_read > MAX_FLOOD_PER_SEC)
	  lcptr->allow_read = MAX_FLOOD_PER_SEC;
      }

    /* Reset the sent-per-second count */
    lcptr->sent_parsed = 0;
    lcptr->actually_read = 0;

    parse_client_queued(cptr);
    /* And now, try flushing .. */
    if (!IsDead(cptr))
    {
        /* and finally, reset the flood check */
        comm_setflush(fd, 1, flood_recalc, cptr);
    }
}

/*
 * read_packet - Read a 'packet' of data from a connection and process it.
 */
void
read_packet(int fd, void *data)
{
  struct Client *cptr = data;
  struct LocalUser *lcptr = cptr->localClient;
  int length = 0;

  assert(lcptr != NULL);
  assert(lcptr->allow_read <= MAX_FLOOD_PER_SEC);

  /*
   * Read some data. We *used to* do anti-flood protection here, but
   * I personally think it makes the code too hairy to make sane.
   *     -- adrian
   */
  length = recv(cptr->fd, readBuf, READBUF_SIZE, 0);

  /* XXX If the client is actually dead, read the buffer but throw it out
   * a suggested more optimum fix will be to mark the fd as -1 and close it in 
   * dead_link() in send.c  
   * -Dianora
   */

  if(IsDead(cptr))return;

  if (length <= 0) {
    /*
     * We only get called when data is waiting,
     * so EOF/any error is fatal.
     */
    error_exit_client(cptr, length);
    return;
  }

  if (cptr->lasttime < CurrentTime)
    cptr->lasttime = CurrentTime;
  if (cptr->lasttime > cptr->since)
    cptr->since = CurrentTime;
  cptr->flags &= ~FLAGS_PINGSENT;

  /*
   * Before we even think of parsing what we just read, stick
   * it on the end of the receive queue and do it when its
   * turn comes around.
   */
  lcptr->actually_read += linebuf_parse(&cptr->localClient->buf_recvq,
      readBuf, length);

  /* Check to make sure we're not flooding */
  if (IsPerson(cptr) &&
     (linebuf_alloclen(&cptr->localClient->buf_recvq) > CLIENT_FLOOD)) {
      if (!(ConfigFileEntry.no_oper_flood && IsOper(cptr))) {

#if 0
        exit_client(cptr, cptr, cptr, "Excess Flood");
        return;
#endif

      }
    }

  /* Attempt to parse what we have */
  parse_client_queued(cptr);

  if (!IsDead(cptr))
  {
    /* If we get here, we need to register for another COMM_SELECT_READ */
    if (PARSE_AS_SERVER(cptr)) {
      comm_setselect(cptr->fd, FDLIST_SERVER, COMM_SELECT_READ,
        read_packet, cptr, 0);
    } else {
      comm_setselect(cptr->fd, FDLIST_IDLECLIENT, COMM_SELECT_READ,
        read_packet, cptr, 0);
    }
  }
}




/*
 * client_dopacket - copy packet to client buf and parse it
 *      cptr - pointer to client structure for which the buffer data
 *             applies.
 *      buffer - pointr to the buffer containing the newly read data
 *      length - number of valid bytes of data in the buffer
 *
 * Note:
 *      It is implicitly assumed that dopacket is called only
 *      with cptr of "local" variation, which contains all the
 *      necessary fields (buffer etc..)
 */
void client_dopacket(struct Client *cptr, char *buffer, size_t length)
{
  assert(cptr != NULL);
  assert(buffer != NULL);

  /* 
   * Update messages received
   */
  ++me.localClient->receiveM;
  ++cptr->localClient->receiveM;

  /* 
   * Update bytes received
   */
  cptr->localClient->receiveB += length;

  if (cptr->localClient->receiveB > 1023) {
    cptr->localClient->receiveK += (cptr->localClient->receiveB >> 10);
    cptr->localClient->receiveB &= 0x03ff; /* 2^10 = 1024, 3ff = 1023 */
  }

  me.localClient->receiveB += length;

  if (me.localClient->receiveB > 1023)
    {
      me.localClient->receiveK += (me.localClient->receiveB >> 10);
      me.localClient->receiveB &= 0x03ff;
    }

  parse(cptr, buffer, buffer + length);
}


