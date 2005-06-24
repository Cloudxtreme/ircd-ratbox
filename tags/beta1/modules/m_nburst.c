/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_nburst.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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
 * $Id$
 */
#include "tools.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "msg.h"
#include "handlers.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static int ms_nburst(struct Client*, struct Client*, int, char**);

struct Message nburst_msgtab = {
  "NBURST", 0, 1, 0, MFLG_SLOW | MFLG_UNREG, 0L,
  {m_unregistered, m_ignore, ms_nburst, m_ignore}
};

void
_modinit(void)
{
  mod_add_cmd(&nburst_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&nburst_msgtab);
}

char *_version = "20010104";

/*
** m_nburst
**      parv[0] = sender prefix
**      parv[1] = nickname to burst
**      parv[2] = new nick (optional)
**      parv[3] = old nick (optional)
*/
/*
 * This function will "burst" the given channel onto
 * the given LL capable server.
 */

static int ms_nburst(struct Client *cptr,
                     struct Client *sptr,
                     int parc,
                     char *parv[])
{
  char *nick;
  char *nick_new = NULL;
  char *nick_old = NULL;
  struct Client *acptr;
  char status;

  if( parc < 2 || *parv[1] == '\0' )
     return 0;

  nick = parv[1];

  if( parc > 2 )
    nick_new = parv[2];

  if( parc > 3 )
    nick_old = parv[3];

  if (!ServerInfo.hub && IsCapable(cptr, CAP_LL))
    return 0;

#ifdef DEBUGLL
  sendto_realops_flags(FLAGS_ALL, "NBURST called by %s for %s %s %s",
    cptr->name,
    nick,
    nick_new ? nick_new : "",
    nick_old ? nick_old : "" );
#endif

  status = 'N';
  if ( (acptr = find_client(nick, NULL)) != NULL )
  {
    /* nick exists.  burst nick back to leaf */
    status = 'Y';
    client_burst_if_needed(cptr, acptr);
  }

  /* Send back LLNICK, if wanted */
  if (parc > 2)
    sendto_one(cptr, ":%s LLNICK %c %s %s", me.name, status, nick_new,
               (nick_old ? nick_old : ""));

  return 0;
}
