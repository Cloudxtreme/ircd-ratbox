/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_motd.c
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
#include "tools.h"
#include "motd.h"
#include "ircd.h"
#include "s_conf.h"
#include "send.h"
#include "numeric.h"
#include "client.h"
#include "handlers.h"
#include "msg.h"
#include "s_serv.h"     /* hunt_server */
#include "parse.h"
#include "modules.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

struct Message motd_msgtab = {
  MSG_MOTD, 0, 0, 1, MFLG_SLOW, 0,
  {m_unregistered, m_motd, mo_motd, mo_motd}
};

void
_modinit(void)
{
  mod_add_cmd(&motd_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&motd_msgtab);
}

char *_version = "20001122";

/*
** m_motd
**      parv[0] = sender prefix
**      parv[1] = servername
*/
int m_motd(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  static time_t last_used = 0;

  /* This is safe enough to use during non hidden server mode */
  if(!GlobalSetOptions.hide_server)
    {
      if (hunt_server(cptr, sptr, ":%s MOTD :%s", 1,parc,parv)!=HUNTED_ISME)
	return 0;
    }

  if((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
    {
      /* safe enough to give this on a local connect only */
      if(MyClient(sptr))
	sendto_one(sptr,form_str(RPL_LOAD2HI),me.name,sptr->name);
      return 0;
    }
  else
    last_used = CurrentTime;

  sendto_realops_flags(FLAGS_SPY, "motd requested by %s (%s@%s) [%s]",
                     sptr->name, sptr->username, sptr->host,
                     sptr->user->server);

  return(SendMessageFile(sptr,&ConfigFileEntry.motd));
}

/*
** mo_motd
**      parv[0] = sender prefix
**      parv[1] = servername
*/
int mo_motd(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  if (hunt_server(cptr, sptr, ":%s MOTD :%s", 1,parc,parv)!=HUNTED_ISME)
    return 0;

  sendto_realops_flags(FLAGS_SPY, "motd requested by %s (%s@%s) [%s]",
                     sptr->name, sptr->username, sptr->host,
                     sptr->user->server);

  return(SendMessageFile(sptr,&ConfigFileEntry.motd));
}

