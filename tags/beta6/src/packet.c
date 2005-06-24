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
#include <errno.h>
#include "tools.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_serv.h"
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
static void
parse_client_queued(struct Client *client_p)
{ 
 int dolen = 0, checkflood = 1;
 struct LocalUser *lclient_p = client_p->localClient;
 if (IsServer(client_p))
 {
  while ((dolen = linebuf_get(&client_p->localClient->buf_recvq,
                              readBuf, READBUF_SIZE, 0)) > 0)
  {
   if (!IsDead(client_p))
    client_dopacket(client_p, readBuf, dolen);
   if (IsDead(client_p))
   {
    if (client_p->localClient)
    {
     linebuf_donebuf(&client_p->localClient->buf_recvq);
     linebuf_donebuf(&client_p->localClient->buf_sendq);
    }
    return;
   }
  }
 } else {
  checkflood = 0;
  if (ConfigFileEntry.no_oper_flood && IsOper(client_p))
   checkflood = 0;
    /*
     * Handle flood protection here - if we exceed our flood limit on
     * messages in this loop, we simply drop out of the loop prematurely.
     *   -- adrian
     */
  for (;;)
  {
   if (checkflood && (lclient_p->sent_parsed > lclient_p->allow_read))
    break;
   dolen = linebuf_get(&client_p->localClient->buf_recvq, readBuf,
                       READBUF_SIZE, 0);
   if (!dolen)
    break;
   client_dopacket(client_p, readBuf, dolen);
   lclient_p->sent_parsed++;
  }
 }
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
 struct Client *client_p = data;
 struct LocalUser *lclient_p = client_p->localClient;
 int max_flood_per_sec = MAX_FLOOD_PER_SEC;
 
 /* This can happen in the event that the client detached. */
 if (!lclient_p)
  return;
 /* If we're a server, skip to the end. Realising here that this call is
  * cheap and it means that if a op is downgraded they still get considered
  * for anti-flood protection ..
  */
 if (!IsPrivileged(client_p))
 {
  /* Is the grace period still active? */
  if (client_p->user && !IsFloodDone(client_p))
   max_flood_per_sec = MAX_FLOOD_PER_SEC_I;
  /* ok, we have to recalculate the number of messages we can receive
   * in this second, based upon what happened in the last second.
   * If we still exceed the flood limit, don't move the parsed limit.
   * If we are below the flood limit, increase the flood limit.
   *   -- adrian
   */
  /* Set to 1 to start with, let it rise/fall after that... */
  if (lclient_p->allow_read == 0)
   lclient_p->allow_read = 1;
  else if (lclient_p->actually_read < lclient_p->allow_read)
   /* Raise the allowed messages if we flooded under the limit */
   lclient_p->allow_read++;
  else
   /* Drop the limit to avoid flooding .. */
   lclient_p->allow_read--;
  /* Enforce floor/ceiling restrictions */
  if (lclient_p->allow_read < 1)
   lclient_p->allow_read = 1;
  else if (lclient_p->allow_read > max_flood_per_sec)
   lclient_p->allow_read = max_flood_per_sec;
 }
 /* Reset the sent-per-second count */
 lclient_p->sent_parsed = 0;
 lclient_p->actually_read = 0;
 parse_client_queued(client_p);
 /* And now, try flushing .. */
 if (!IsDead(client_p))
 {
  /* and finally, reset the flood check */
  comm_setflush(fd, 1000, flood_recalc, client_p);
 }
}

/*
 * read_ctrl_packet - Read a 'packet' of data from a servlink control
 *                    link and process it.
 */
void
read_ctrl_packet(int fd, void *data)
{
  struct Client *server = data;
  struct LocalUser *lserver = server->localClient;
  struct SlinkRpl *reply;
  int length = 0;
  unsigned char tmp[2];
  unsigned char *len = tmp;
  struct SlinkRplDef *replydef;

  assert(lserver != NULL);
  reply = &server->localClient->slinkrpl;

  /* if the server died, kill it off now -davidt */
  if(IsDead(server))
  {
    exit_client(server, server, &me,
                (server->flags & FLAGS_SENDQEX) ?
                  "SendQ exceeded" : "Dead socket");
    return;
  }

  if (!reply->command)
  {
    reply->gotdatalen = 0;
    reply->readdata = 0;
    reply->data = NULL;

    length = read(fd, tmp, 1);

    if (length <= 0)
    {
      if((length == -1) && ignoreErrno(errno))
        goto nodata;
      error_exit_client(server, length);
      return;
    }

    reply->command = tmp[0];
  }

  for (replydef = slinkrpltab; replydef->handler; replydef++)
  {
    if (replydef->replyid == reply->command)
      break;
  }

  /* we should be able to trust a local slink process...
   * and if it sends an invalid command, that's a bug.. */
  assert(replydef->handler);

  if ((replydef->flags & SLINKRPL_FLAG_DATA) && (reply->gotdatalen < 2))
  {
    /* we need a datalen u16 which we don't have yet... */
    length = read(fd, len, (2 - reply->gotdatalen));
    if (length <= 0)
    {
      if((length == -1) && ignoreErrno(errno))
        goto nodata;
      error_exit_client(server, length);
      return;
    }

    if (reply->gotdatalen == 0)
    {
      reply->datalen = *len << 8;
      reply->gotdatalen++;
      length--;
      len++;
    }
    if (length && (reply->gotdatalen == 1))
    {
      reply->datalen |= *len;
      reply->gotdatalen++;
      if (reply->datalen > 0)
        reply->data = MyMalloc(reply->datalen);
    }
  }

  if (reply->readdata < reply->datalen) /* try to get any remaining data */
  {
    length = read(fd, (reply->data + reply->readdata),
                  (reply->datalen - reply->readdata));
    if (length <= 0)
    {
      if((length == -1) && ignoreErrno(errno))
        goto nodata;
      error_exit_client(server, length);
      return;
    }

    reply->readdata += length;
    if (reply->readdata < reply->datalen)
      return; /* wait for more data */
  }

  /* we now have the command and any data, pass it off to the handler */
  (*replydef->handler)(reply->command, reply->datalen, reply->data, server);

  /* reset SlinkRpl */                      
  if (reply->datalen > 0)
    MyFree(reply->data);
  reply->command = 0;

  if (IsDead(server))
    return;

nodata:
  /* If we get here, we need to register for another COMM_SELECT_READ */
  comm_setselect(fd, FDLIST_SERVER, COMM_SELECT_READ,
                 read_ctrl_packet, server, 0);
}
  
/*
 * read_packet - Read a 'packet' of data from a connection and process it.
 */
void
read_packet(int fd, void *data)
{
  struct Client *client_p = data;
  struct LocalUser *lclient_p = client_p->localClient;
  int length = 0;
  int lbuf_len;
  int fd_r = client_p->fd;

  /* if the client is dead, kill it off now -davidt */
  if(IsDead(client_p))
  {
    exit_client(client_p, client_p, &me,
                (client_p->flags & FLAGS_SENDQEX) ?
                  "SendQ exceeded" : "Dead socket");
    return;
  }

#ifndef HAVE_SOCKETPAIR
  if (HasServlink(client_p))
  {
    assert(client_p->fd_r > -1);
    fd_r = client_p->fd_r;
  }
#endif
  assert(lclient_p != NULL);

  /*
   * Read some data. We *used to* do anti-flood protection here, but
   * I personally think it makes the code too hairy to make sane.
   *     -- adrian
   */
  length = read(fd_r, readBuf, READBUF_SIZE);

  if (length <= 0) {
    if(ignoreErrno(errno)) {
      comm_setselect(fd_r, FDLIST_IDLECLIENT, COMM_SELECT_READ,
      		read_packet, client_p, 0);
      return;
    }  	
    error_exit_client(client_p, length);
    return;
  }

  if (client_p->lasttime < CurrentTime)
    client_p->lasttime = CurrentTime;
  if (client_p->lasttime > client_p->since)
    client_p->since = CurrentTime;
  client_p->flags &= ~FLAGS_PINGSENT;

  /*
   * Before we even think of parsing what we just read, stick
   * it on the end of the receive queue and do it when its
   * turn comes around.
   */
  lbuf_len = linebuf_parse(&client_p->localClient->buf_recvq,
      readBuf, length, (IsRegistered(client_p) ? 0 : 1));

  if (lbuf_len < 0)
  {
    error_exit_client(client_p, 0);
    return;
  }

  lclient_p->actually_read += lbuf_len;
  
  /* Check to make sure we're not flooding */
  if (IsPerson(client_p) &&
     (linebuf_alloclen(&client_p->localClient->buf_recvq) >
      ConfigFileEntry.client_flood)) {
      if (!(ConfigFileEntry.no_oper_flood && IsOper(client_p)))
      {
       exit_client(client_p, client_p, client_p, "Excess Flood");
       return;
      }
    }

  /* Attempt to parse what we have */
  parse_client_queued(client_p);

  /* server fd may have changed */
  fd_r = client_p->fd;
#ifndef HAVE_SOCKETPAIR
  if (HasServlink(client_p))
  {
    assert(client_p->fd_r > -1);
    fd_r = client_p->fd_r;
  }
#endif

  if (!IsDead(client_p))
  {
    /* If we get here, we need to register for another COMM_SELECT_READ */
    if (PARSE_AS_SERVER(client_p)) {
      comm_setselect(fd_r, FDLIST_SERVER, COMM_SELECT_READ,
        read_packet, client_p, 0);
    } else {
      comm_setselect(fd_r, FDLIST_IDLECLIENT, COMM_SELECT_READ,
        read_packet, client_p, 0);
    }
  }
}




/*
 * client_dopacket - copy packet to client buf and parse it
 *      client_p - pointer to client structure for which the buffer data
 *             applies.
 *      buffer - pointr to the buffer containing the newly read data
 *      length - number of valid bytes of data in the buffer
 *
 * Note:
 *      It is implicitly assumed that dopacket is called only
 *      with client_p of "local" variation, which contains all the
 *      necessary fields (buffer etc..)
 */
void client_dopacket(struct Client *client_p, char *buffer, size_t length)
{
  assert(client_p != NULL);
  assert(buffer != NULL);

  /* 
   * Update messages received
   */
  ++me.localClient->receiveM;
  ++client_p->localClient->receiveM;

  /* 
   * Update bytes received
   */
  client_p->localClient->receiveB += length;

  if (client_p->localClient->receiveB > 1023) {
    client_p->localClient->receiveK += (client_p->localClient->receiveB >> 10);
    client_p->localClient->receiveB &= 0x03ff; /* 2^10 = 1024, 3ff = 1023 */
  }

  me.localClient->receiveB += length;

  if (me.localClient->receiveB > 1023)
    {
      me.localClient->receiveK += (me.localClient->receiveB >> 10);
      me.localClient->receiveB &= 0x03ff;
    }

  parse(client_p, buffer, buffer + length);
}


