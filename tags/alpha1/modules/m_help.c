/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_help.c
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
#include "motd.h"
#include "ircd_handler.h"
#include "msg.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "parse.h"
#include "modules.h"

struct Message help_msgtab = {
  MSG_HELP, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_help, m_ignore, mo_help}
};

void
_modinit(void)
{
  mod_add_cmd(&help_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&help_msgtab);
}

char *_version = "20001122";

/*
 * m_help - HELP message handler
 *      parv[0] = sender prefix
 */
int m_help(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  static time_t last_used = 0;

  /* HELP is always local */
  if ((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
    {
      /* safe enough to give this on a local connect only */
      sendto_one(sptr,form_str(RPL_LOAD2HI),me.name,parv[0]);
      return 0;
    }
  else
    {
      last_used = CurrentTime;
    }

  report_messages(sptr);

  return 0;
}

/*
 * mo_help - HELP message handler
 *      parv[0] = sender prefix
 */
int mo_help(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  SendMessageFile(sptr, &ConfigFileEntry.helpfile);
  return 0;
}


