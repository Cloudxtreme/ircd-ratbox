/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_userhost.c
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
#include "irc_string.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include <string.h>

static char buf[BUFSIZE];

struct Message userhost_msgtab = {
  MSG_USERHOST, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_userhost, m_ignore, m_userhost}
};

void
_modinit(void)
{
  mod_add_cmd(&userhost_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&userhost_msgtab);
}

char *_version = "20001122";

/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 */
int     m_userhost(struct Client *cptr,
                   struct Client *sptr,
                   int parc,
                   char *parv[])
{
  struct Client *acptr;
  char response[NICKLEN*2+USERLEN+HOSTLEN+30];
  char *t;
  int i;               /* loop counter */
  int cur_len;
  int rl;

  cur_len = ircsprintf(buf,form_str(RPL_USERHOST),me.name, parv[0], "");
  t = buf + cur_len;

  for ( i = 0; i < 5; i++)
    {
      if ((acptr = find_person(parv[i+1], NULL)))
	{
	  if (acptr == sptr) /* show real IP for USERHOST on yourself */
            rl = ircsprintf(response, "%s%s=%c%s@%s ",
			    acptr->name,
			    IsOper(acptr) ? "*" : "",
			    (acptr->user->away) ? '-' : '+',
			    acptr->username,
			    acptr->localClient->sockhost);
          else
            rl = ircsprintf(response, "%s%s=%c%s@%s ",
			    acptr->name,
			    IsOper(acptr) ? "*" : "",
			    (acptr->user->away) ? '-' : '+',
			    acptr->username,
			    acptr->host);

	  if((rl + cur_len) < (BUFSIZE-10))
	    {
	      ircsprintf(t,"%s",response);
	      t += rl;
	      cur_len += rl;
	    }
	  else
	    break;
	}
    }

  sendto_one(sptr, "%s", buf);

  return 0;
}
