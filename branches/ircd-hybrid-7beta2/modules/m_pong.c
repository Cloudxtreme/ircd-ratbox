/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_pong.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "channel.h"
#include "irc_string.h"
#include "s_debug.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void mr_pong(struct Client*, struct Client*, int, char**);
static void ms_pong(struct Client*, struct Client*, int, char**);

struct Message pong_msgtab = {
  "PONG", 0, 1, 0, MFLG_SLOW | MFLG_UNREG, 0,
  {mr_pong, m_ignore, ms_pong, m_ignore}
};

void
_modinit(void)
{
  mod_add_cmd(&pong_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&pong_msgtab);
}

char *_version = "20001122";

static void ms_pong(struct Client *cptr,
                   struct Client *sptr,
                   int parc,
                   char *parv[])
{
  struct Client *acptr;
  char  *origin, *destination;

  if (parc < 2 || *parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NOORIGIN), me.name, parv[0]);
      return;
    }

  origin = parv[1];
  destination = parv[2];
  cptr->flags &= ~FLAGS_PINGSENT;
  sptr->flags &= ~FLAGS_PINGSENT;

  /* Now attempt to route the PONG, comstud pointed out routable PING
   * is used for SPING.  routable PING should also probably be left in
   *        -Dianora
   * That being the case, we will route, but only for registered clients (a
   * case can be made to allow them only from servers). -Shadowfax
   */
  if (!EmptyString(destination) && irccmp(destination, me.name) != 0
                && IsRegistered(sptr))
    {
      if ((acptr = find_client(destination, NULL)) ||
          (acptr = find_server(destination)))
        sendto_one(acptr,":%s PONG %s %s",
                   parv[0], origin, destination);
      else
        {
          sendto_one(sptr, form_str(ERR_NOSUCHSERVER),
                     me.name, parv[0], destination);
          return;
        }
    }

#ifdef  DEBUGMODE
  else
    Debug((DEBUG_NOTICE, "PONG: %s %s", origin,
           destination ? destination : "*"));
#endif
  return;
}

static void mr_pong(struct Client *cptr,
                    struct Client *sptr,
                    int parc,
                    char *parv[])
{
  struct Client *acptr;
  char  *origin, *destination;

  if (parc < 2 || *parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NOORIGIN), me.name, parv[0]);
      return;
    }

  origin = parv[1];
  destination = parv[2];
  cptr->flags &= ~FLAGS_PINGSENT;
  sptr->flags &= ~FLAGS_PINGSENT;

  /* Now attempt to route the PONG, comstud pointed out routable PING
   * is used for SPING.  routable PING should also probably be left in
   *        -Dianora
   * That being the case, we will route, but only for registered clients (a
   * case can be made to allow them only from servers). -Shadowfax
   */
  if (!EmptyString(destination) && irccmp(destination, me.name) != 0
                && IsRegistered(sptr))
    {
      if ((acptr = find_client(destination, NULL)) ||
          (acptr = find_server(destination)))
        sendto_one(acptr,":%s PONG %s %s",
                   parv[0], origin, destination);
      else
        {
          sendto_one(sptr, form_str(ERR_NOSUCHSERVER),
                     me.name, parv[0], destination);
          return;
        }
    }

#ifdef  DEBUGMODE
  else
    Debug((DEBUG_NOTICE, "PONG: %s %s", origin,
           destination ? destination : "*"));
#endif
  return;
}

