/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_operwall.c
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
#include "irc_string.h"
#include "numeric.h"
#include "send.h"
#include "s_user.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void mo_operwall(struct Client*, struct Client*, int, char**);
static void ms_operwall(struct Client*, struct Client*, int, char**);

struct Message operwall_msgtab = {
  "OPERWALL", 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, ms_operwall, mo_operwall}
};

void
_modinit(void)
{
  mod_add_cmd(&operwall_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&operwall_msgtab);
}

char *_version = "20010130";

/*
 * mo_operwall - OPERWALL message handler
 *  (write to *all* local opers currently online)
 *      parv[0] = sender prefix
 *      parv[1] = message text
 */

static void mo_operwall(struct Client *cptr, struct Client *sptr,
                       int parc, char *parv[])
{
  char *message = parv[1];

  if (EmptyString(message))
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "OPERWALL");
      return;
    }

  sendto_ll_serv_butone(NULL, sptr, 1,
                        ":%s OPERWALL :%s", parv[0], message);
  sendto_wallops_flags(FLAGS_OPERWALL, sptr, "%s", message);
}

/*
 * ms_operwall - OPERWALL message handler
 *  (write to *all* local opers currently online)
 *      parv[0] = sender prefix
 *      parv[1] = message text
 */

static void ms_operwall(struct Client *cptr, struct Client *sptr,
                       int parc, char *parv[])
{
  char *message = parv[1];

  if (EmptyString(message))
    {
      if (MyClient(sptr))
        sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                   me.name, parv[0], "OPERWALL");
      return;
    }

  sendto_ll_serv_butone(cptr, sptr, 1, ":%s OPERWALL :%s",
                     parv[0], message);
  sendto_wallops_flags(FLAGS_OPERWALL, sptr, "%s", message);
}


