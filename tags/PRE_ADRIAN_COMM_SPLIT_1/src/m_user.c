/************************************************************************
 *   IRC - Internet Relay Chat, src/m_user.c
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
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"

#include <string.h>

static int bot_check(char *host);

/*
 * m_functions execute protocol messages on this server:
 *
 *      cptr    is always NON-NULL, pointing to a *LOCAL* client
 *              structure (with an open socket connected!). This
 *              identifies the physical socket where the message
 *              originated (or which caused the m_function to be
 *              executed--some m_functions may call others...).
 *
 *      sptr    is the source of the message, defined by the
 *              prefix part of the message if present. If not
 *              or prefix not found, then sptr==cptr.
 *
 *              (!IsServer(cptr)) => (cptr == sptr), because
 *              prefixes are taken *only* from servers...
 *
 *              (IsServer(cptr))
 *                      (sptr == cptr) => the message didn't
 *                      have the prefix.
 *
 *                      (sptr != cptr && IsServer(sptr) means
 *                      the prefix specified servername. (?)
 *
 *                      (sptr != cptr && !IsServer(sptr) means
 *                      that message originated from a remote
 *                      user (not local).
 *
 *              combining
 *
 *              (!IsServer(sptr)) means that, sptr can safely
 *              taken as defining the target structure of the
 *              message in this server.
 *
 *      *Always* true (if 'parse' and others are working correct):
 *
 *      1)      sptr->from == cptr  (note: cptr->from == cptr)
 *
 *      2)      MyConnect(sptr) <=> sptr == cptr (e.g. sptr
 *              *cannot* be a local connection, unless it's
 *              actually cptr!). [MyConnect(x) should probably
 *              be defined as (x == x->from) --msa ]
 *
 *      parc    number of variable parameter strings (if zero,
 *              parv is allowed to be NULL)
 *
 *      parv    a NULL terminated list of parameter pointers,
 *
 *                      parv[0], sender (prefix string), if not present
 *                              this points to an empty string.
 *                      parv[1]...parv[parc-1]
 *                              pointers to additional parameters
 *                      parv[parc] == NULL, *always*
 *
 *              note:   it is guaranteed that parv[0]..parv[parc-1] are all
 *                      non-NULL pointers.
 */

#define UFLAGS  (FLAGS_INVISIBLE|FLAGS_WALLOP|FLAGS_SERVNOTICE)

/*
** m_user
**      parv[0] = sender prefix
**      parv[1] = username (login name, account)
**      parv[2] = client host name (used only from other servers)
**      parv[3] = server host name (used only from other servers)
**      parv[4] = users real name info
*/
int m_user(struct Client* cptr, struct Client* sptr, int parc, char *parv[])
{
#define UFLAGS  (FLAGS_INVISIBLE|FLAGS_WALLOP|FLAGS_SERVNOTICE)
  char* username;
  char* host;
  char* server;
  char* realname;
 
  if (parc > 2 && (username = strchr(parv[1],'@')))
    *username = '\0'; 
  if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
      *parv[3] == '\0' || *parv[4] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, BadPtr(parv[0]) ? "*" : parv[0], "USER");
      if (IsServer(cptr))
        sendto_realops("bad USER param count for %s from %s",
                       parv[0], get_client_name(cptr, HIDE_IP));
      else
        return 0;
    }

  /* Copy parameters into better documenting variables */

  username = (parc < 2 || BadPtr(parv[1])) ? "<bad-boy>" : parv[1];
  host     = (parc < 3 || BadPtr(parv[2])) ? "<nohost>" : parv[2];
  server   = (parc < 4 || BadPtr(parv[3])) ? "<noserver>" : parv[3];
  realname = (parc < 5 || BadPtr(parv[4])) ? "<bad-realname>" : parv[4];
  
  if (ConfigFileEntry.botcheck) {
  /* Only do bot checks on local connecting clients */
      if(MyClient(cptr))
        cptr->isbot = bot_check(host);
  }

  return do_user(parv[0], cptr, sptr, username, host, server, realname);
}

/**
 ** bot_check(host)
 **   Reject a bot based on a fake hostname...
 **           -Taner
 **/
static int bot_check(char *host)
{
/*
 * Eggdrop Bots:        "USER foo 1 1 :foo"
 * Vlad, Com, joh Bots: "USER foo null null :foo"
 * Annoy/OJNKbots:      "user foo . . :foo"   (disabled)
 * Spambots that are based on OJNK: "user foo x x :foo"
 */
  if (!strcmp(host,"1")) return 1;
  if (!strcmp(host,"null")) return 2;
  if (!strcmp(host, "x")) return 3;

  return 0;
}
