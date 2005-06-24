/************************************************************************
 *   IRC - Internet Relay Chat, modules/testline.c
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
#include "dline_conf.h"
#include "common.h"
#include "irc_string.h"
#include "ircd_defs.h"
#include "ircd.h"
#include "restart.h"
#include "s_conf.h"
#include "send.h"
#include "msg.h"
#include "hostmask.h"
#include "numeric.h"
#include "parse.h"
#include "modules.h"

#include <string.h>

static void mo_testline(struct Client*, struct Client*, int, char**);

struct Message testline_msgtab = {
  "TESTLINE", 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, mo_testline}
};
 
void
_modinit(void)
{
  mod_add_cmd(&testline_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&testline_msgtab);
}
 
char *_version = "20001124";

/*
 * mo_testline
 *
 * inputs       - pointer to physical connection request is coming from
 *              - pointer to source connection request is comming from
 *              - parc arg count
 *              - parv actual arguments   
 *   
 * output       - always 0
 * side effects - command to test I/K lines on server
 *   
 * i.e. /quote testline user@host,ip
 *
 */  
  
static void mo_testline(struct Client *cptr, struct Client *sptr,
                       int parc, char *parv[])
{
  struct ConfItem *aconf;
  struct irc_inaddr ip;
  unsigned long host_mask;
  char *host, *pass, *user, *name, *classname, *given_host, *given_name, *p;
  int port;
  
  if (parc > 1)
    {
      given_name = parv[1];
      if(!(p = (char*)strchr(given_name,'@')))
        {
#ifndef IPV6 
	  /* XXX: ipv6 doesn't work here */
          IN_ADDR(ip) = 0L;
#endif
          if(is_address(given_name,(unsigned long *)&IN_ADDR(ip),&host_mask))
            {
              aconf = match_Dline(&ip);    
              if( aconf )
                {
                  get_printable_conf(aconf, &name, &host, &pass, &user, &port,&classname);
                  sendto_one(sptr,
                         ":%s NOTICE %s :D-line host [%s] pass [%s]",
                         me.name, parv[0],
                         host,
                         pass);
                }
              else
                sendto_one(sptr, ":%s NOTICE %s :No aconf found",
                         me.name, parv[0]);
            }
          else
          sendto_one(sptr, ":%s NOTICE %s :usage: user@host|ip",
                     me.name, parv[0]);
  
          return;
        }
      
      *p = '\0';
      p++;
      given_host = p;
#ifndef IPV6
      IN_ADDR(ip) = 0L;
#endif
      (void)is_address(given_host,(unsigned long *)&IN_ADDR(ip),&host_mask);
      
      aconf = find_matching_conf(given_host,
                                       given_name,
                                       &ip);
          
      if(aconf)
        {
          get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);
              
          if(aconf->status & CONF_KILL)
            {
              sendto_one(sptr,
                         ":%s NOTICE %s :K-line name [%s] host [%s] pass [%s]",
                         me.name, parv[0],
                         user,
                         host, 
                         pass);
            }
          else if(aconf->status & CONF_CLIENT)
            {
              sendto_one(sptr,
":%s NOTICE %s :I-line mask [%s] prefix [%s] name [%s] host [%s] port [%d] class [%s]",
                         me.name, parv[0],
                         name,
                         show_iline_prefix(sptr,aconf,user),
                         user,
                         host,
                         port,
                         classname);
      
              aconf = find_tkline(given_host, given_name, 0);
              if(aconf)
                {
                  sendto_one(sptr,
                     ":%s NOTICE %s :k-line name [%s] host [%s] pass [%s]",
                             me.name, parv[0],
                             aconf->user,
                             aconf->host,
                             aconf->passwd);
                }
            }
        }
      else
        sendto_one(sptr, ":%s NOTICE %s :No aconf found",
                   me.name, parv[0]);
    }
  else
    sendto_one(sptr, ":%s NOTICE %s :usage: user@host|ip",
               me.name, parv[0]);
}
