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

static char* confopts(struct Client *source_p);

static void m_version(struct Client*, struct Client*, int, char**);
static void ms_version(struct Client*, struct Client*, int, char**);
static void mo_version(struct Client*, struct Client*, int, char**);

struct Message version_msgtab = {
  "VERSION", 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_version, ms_version, mo_version}
};

#ifndef STATIC_MODULES
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
#endif
/*
 * m_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static void m_version(struct Client* client_p, struct Client* source_p,
                      int parc, char* parv[])
{
  sendto_one(source_p, form_str(RPL_VERSION), me.name,
                parv[0], version, serno, debugmode,
                me.name, confopts(source_p), serveropts);
                
  show_isupport(source_p);
}

/*
 * mo_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static void mo_version(struct Client* client_p, struct Client* source_p,
                      int parc, char* parv[])
{
  if (hunt_server(client_p, source_p, ":%s VERSION :%s", 
		  1, parc, parv) != HUNTED_ISME)
    return;
    
  sendto_one(source_p, form_str(RPL_VERSION), me.name, parv[0], version, 
  	     serno, debugmode, me.name, confopts(source_p), serveropts);
	       
  show_isupport(source_p);
  
  return;
}

/*
 * ms_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static void ms_version(struct Client* client_p, struct Client* source_p,
                      int parc, char* parv[])
{
  if (IsOper(source_p))
     {
       if (hunt_server(client_p, source_p, ":%s VERSION :%s", 
                       1, parc, parv) == HUNTED_ISME)
         {
           sendto_one(source_p, form_str(RPL_VERSION), me.name,
                      parv[0], version, serno, debugmode,
                      me.name, confopts(source_p), serveropts);
           show_isupport(source_p);
         }
     }
   else
     {
       sendto_one(source_p, form_str(RPL_VERSION), me.name,
                  parv[0], version, serno, debugmode,
                  me.name, confopts(source_p), serveropts);
       show_isupport(source_p);
     }

  return;
}

/* confopts()
 * input  - client pointer
 * output - ircd.conf option string
 * side effects - none
 */
static char* confopts(struct Client *source_p)
{
  static char result[4];

  result[0] = '\0';

  if (ConfigFileEntry.glines)
    strcat(result, "G");

  /* might wanna hide this :P */
  if (ServerInfo.hub && 
      (!GlobalSetOptions.hide_server || IsOper(source_p)) )
    {
      strcat(result, "H");
    }

  return result;
}
