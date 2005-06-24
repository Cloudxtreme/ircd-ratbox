/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_version.c
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
#include <string.h>
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static char* confopts(struct Client *sptr);

static void m_version(struct Client*, struct Client*, int, char**);
static void ms_version(struct Client*, struct Client*, int, char**);
static void mo_version(struct Client*, struct Client*, int, char**);

struct Message version_msgtab = {
  "VERSION", 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_version, ms_version, mo_version}
};

void
_modinit(void)
{
  mod_add_cmd(&version_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&version_msgtab);
}

char *_version = "20001223";

/*
 * m_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static void m_version(struct Client* cptr, struct Client* sptr,
                      int parc, char* parv[])
{
  sendto_one(sptr, form_str(RPL_VERSION), me.name,
                parv[0], version, serno, debugmode,
                me.name, confopts(sptr), serveropts);
                
  show_isupport(sptr);
}

/*
 * mo_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static void mo_version(struct Client* cptr, struct Client* sptr,
                      int parc, char* parv[])
{
  if (hunt_server(cptr, sptr, ":%s VERSION :%s", 
		  1, parc, parv) != HUNTED_ISME)
    return;
    
  sendto_one(sptr, form_str(RPL_VERSION), me.name, parv[0], version, 
  	     serno, debugmode, me.name, confopts(sptr), serveropts);
	       
  show_isupport(sptr);
  
  return;
}

/*
 * ms_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static void ms_version(struct Client* cptr, struct Client* sptr,
                      int parc, char* parv[])
{
  if (IsOper(sptr))
     {
       if (hunt_server(cptr, sptr, ":%s VERSION :%s", 
                       1, parc, parv) == HUNTED_ISME)
         sendto_one(sptr, form_str(RPL_VERSION), me.name,
                    parv[0], version, serno, debugmode,
                    me.name, confopts(sptr), serveropts);
     }
   else
     sendto_one(sptr, form_str(RPL_VERSION), me.name,
                parv[0], version, serno, debugmode,
                me.name, confopts(sptr), serveropts);
}

/* confopts()
 * input  - client pointer
 * output - ircd.conf option string
 * side effects - none
 */
static char* confopts(struct Client *sptr)
{
  static char result[4];

  result[0] = '\0';

  if (ConfigFileEntry.glines)
    strcat(result, "G");

  /* might wanna hide this :P */
  if (ServerInfo.hub && 
      (!GlobalSetOptions.hide_server || IsOper(sptr)) )
    {
      strcat(result, "H");
    }

  return result;
}
