/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_squit.c
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
#include "common.h"      /* FALSE bleah */
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"


#include <assert.h>

struct Message squit_msgtab = {
  MSG_SQUIT, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, ms_squit, mo_squit}
};

void
_modinit(void)
{
  mod_add_cmd(&squit_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&squit_msgtab);
}

struct squit_parms 
{
  char *server_name;
  struct Client *acptr;
};

struct squit_parms *find_squit(struct Client *cptr,
			       struct Client *sptr,
			       char *server);

char *_version = "20001122";

/*
 * mo_squit - SQUIT message handler
 *      parv[0] = sender prefix
 *      parv[1] = server name
 *      parv[2] = comment
 */
int mo_squit(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct squit_parms *found_squit;
  char  *comment = (parc > 2 && parv[2]) ? parv[2] : cptr->name;

  if (!IsOperRemote(sptr))
    {
      sendto_one(sptr,":%s NOTICE %s :You have no R flag",me.name,parv[0]);
      return 0;
    }

  if(parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "SQUIT");
      return -1;
    }

  if( (found_squit = find_squit(cptr,sptr,parv[1])) )
    {
      if(MyConnect(found_squit->acptr))
	{
	  sendto_realops_flags(FLAGS_ALL,
			       "Received SQUIT %s from %s (%s)",
			       found_squit->acptr->name,
			       get_client_name(sptr,FALSE), comment);
          log(L_NOTICE, "Received SQUIT %s from %s (%s)",
              found_squit->acptr->name, get_client_name(sptr,FALSE), comment);
	}
      return exit_client(cptr, found_squit->acptr, sptr, comment);
    }
  return 0;
}

/*
 * ms_squit - SQUIT message handler
 *      parv[0] = sender prefix
 *      parv[1] = server name
 *      parv[2] = comment
 */
int ms_squit(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct squit_parms *found_squit;
  char  *comment = (parc > 2 && parv[2]) ? parv[2] : cptr->name;

  if(parc < 2)
    {
      return -1;
    }

  if( (found_squit = find_squit(cptr, sptr, parv[1])) )
    {
      /*
      **  Notify all opers, if my local link is remotely squitted
      */
      if (MyConnect(found_squit->acptr))
	{
	  sendto_all_local_opers(&me, NULL,
				 "Remote SQUIT %s from %s (%s)",
				 found_squit->server_name,
				 get_client_name(sptr,FALSE), comment);

          sendto_serv_butone(NULL,
			     ":%s WALLOPS :Remote SQUIT %s from %s (%s)",
			     me.name, found_squit->server_name,
			     get_client_name(sptr,FALSE),comment);

	  log(L_TRACE, "SQUIT From %s : %s (%s)", parv[0],
	      found_squit->server_name, comment);

	}
      return exit_client(cptr, found_squit->acptr, sptr, comment);
    }
  return 0;
}


/*
 * find_squit
 * inputs	- local server connection
 *		-
 *		-
 * output	- pointer to struct containing found squit or none if not found
 * side effects	-
 */
struct squit_parms *find_squit(struct Client *cptr, struct Client *sptr,
			     char *server)
{
  static struct squit_parms found_squit;
  static struct Client *acptr;
  struct ConfItem *aconf;

  found_squit.acptr = NULL;
  found_squit.server_name = NULL;

  /*
  ** To accomodate host masking, a squit for a masked server
  ** name is expanded if the incoming mask is the same as
  ** the server name for that link to the name of link.
  */
  while ((*server == '*') && IsServer(cptr))
    {
      aconf = cptr->serv->nline;
      if (!aconf)
	break;

      if (!irccmp(server, my_name_for_link(me.name, aconf)))
	{
	  found_squit.server_name = cptr->name;
	  found_squit.acptr = cptr;
	}

      break; /* WARNING is normal here */
      /* NOTREACHED */
    }

  /*
  ** The following allows wild cards in SQUIT. Only useful
  ** when the command is issued by an oper.
  */
  for (acptr = GlobalClientList; (acptr = next_client(acptr, server));
       acptr = acptr->next)
    {
      if (IsServer(acptr) || IsMe(acptr))
	break;
    }

  found_squit.acptr = acptr;
  found_squit.server_name = server;

  if (acptr && IsMe(acptr))
    {
      found_squit.acptr = acptr;
      found_squit.server_name = cptr->host;
    }

  if(found_squit.acptr != NULL)
    return &found_squit;
  else
    return( NULL );
}
