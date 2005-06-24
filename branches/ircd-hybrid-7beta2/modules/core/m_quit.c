/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_quit.c
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
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"

static void m_quit(struct Client*, struct Client*, int, char**);
static void ms_quit(struct Client*, struct Client*, int, char**);

struct Message quit_msgtab = {
  "QUIT", 0, 0, 0, MFLG_SLOW | MFLG_UNREG, 0,
  {m_quit, m_quit, ms_quit, m_quit}
};

void
_modinit(void)
{
  mod_add_cmd(&quit_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&quit_msgtab);
}

char *_version = "20001122";

/*
** m_quit
**      parv[0] = sender prefix
**      parv[1] = comment
*/
static void m_quit(struct Client *cptr,
                  struct Client *sptr,
                  int parc,
                  char *parv[])
{
  char *comment = (parc > 1 && parv[1]) ? parv[1] : cptr->name;
  char reason [TOPICLEN + 1];

  sptr->flags |= FLAGS_NORMALEX;
  if (strlen(comment) > (size_t) TOPICLEN)
    comment[TOPICLEN] = '\0';

  if (ConfigFileEntry.client_exit)
    {
      snprintf(reason, TOPICLEN, "Client Exit: %s", comment);
      comment = reason;
    }
  
  if( !IsServer(sptr) && MyConnect(sptr) && !IsOper(sptr) && 
     (sptr->firsttime + ANTI_SPAM_EXIT_MESSAGE_TIME) > CurrentTime)
    comment = "Client Quit";

  exit_client(cptr, sptr, sptr, comment);
}
/*
** ms_quit
**      parv[0] = sender prefix
**      parv[1] = comment
*/
static void ms_quit(struct Client *cptr,
                   struct Client *sptr,
                   int parc,
                   char *parv[])
{
  char *comment = (parc > 1 && parv[1]) ? parv[1] : cptr->name;

  sptr->flags |= FLAGS_NORMALEX;
  if (strlen(comment) > (size_t) TOPICLEN)
    comment[TOPICLEN] = '\0';

  exit_client(cptr, sptr, sptr, comment);
}

