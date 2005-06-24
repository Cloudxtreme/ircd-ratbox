/************************************************************************
 *   IRC - Internet Relay Chat, contrib/m_jupe.c
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

/*
 * WARNING: This is unfinished, and not likely to work :P
 */

#include "tools.h"
#include "irc_string.h"
#include "handlers.h"
#include "channel.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_log.h"
#include "s_serv.h"
#include "send.h"
#include "whowas.h"
#include "irc_string.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "class.h"
#include "common.h"
#include "event.h"

#include "fdlist.h"
#include "list.h"
#include "s_conf.h"
#include "scache.h"
#include <string.h>


static void mo_jupe(struct Client *client_p, struct Client *source_p,
		 int parc, char *parv[]);
static int bogus_host(char *host);

struct Message jupe_msgtab = {
  "JUPE", 0, 3, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, mo_jupe, mo_jupe}
};

void
_modinit(void)
{
  mod_add_cmd(&jupe_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&jupe_msgtab);
}

char *_version = "20010104";

/*
** mo_jupe
**      parv[0] = sender prefix
**      parv[1] = server we're juping
**      parv[2] = reason for jupe
*/
static void mo_jupe(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  struct Client *target_p;
  struct Client *ajupe;
  dlink_node *m;
  /* This should be reallen but ircsprintf cores if its too long.. */
  char reason[TOPICLEN];

  if(!ServerInfo.hub)
    return;

  if(!IsSetOperAdmin(source_p))
    {
      sendto_one(source_p, ":%s NOTICE %s :You must be an admin to use this command",
                 me.name, parv[0]);
      return;
    }

  if (bogus_host(parv[1]))
    {
      sendto_one(source_p, ":%s NOTICE %s :Invalid servername: %s",
                 me.name, parv[0], parv[1]);
      return;
    }
    
  sendto_wallops_flags(FLAGS_WALLOP, &me, "JUPE for %s requested by %s!%s@%s: %s",
			 parv[1], source_p->name, source_p->username,
                         source_p->host, parv[2]);
  sendto_ll_serv_butone(NULL, source_p, 1,
			":%s WALLOPS :JUPE for %s requested by %s!%s@%s: %s",
			parv[0], parv[1], source_p->name, 
                        source_p->username, source_p->host, parv[2]);
  log(L_NOTICE, "JUPE for %s requested by %s!%s@%s: %s",
                parv[1], source_p->name, source_p->username, source_p->host, parv[2]);

  target_p= find_server(parv[1]);

  if(target_p)
    exit_client(client_p, target_p, &me, parv[2]);

  sendto_serv_butone(&me, ":%s SERVER %s 1 :Juped: %s",
		     me.name, parv[1], parv[2]);

  sendto_realops_flags(FLAGS_ALL,
                       "Link with %s established: (JUPED) link",
		       parv[1]);

  ajupe = make_client(NULL);

  m = dlinkFind(&unknown_list, ajupe);
  if(m != NULL)
    {
      dlinkDelete(m, &unknown_list);
    }
  free_dlink_node(m);

  make_server(ajupe);

  ajupe->hopcount = 1;
  strncpy_irc(ajupe->name,parv[1],HOSTLEN);
  ircsprintf(reason, "%s %s", "Juped:", parv[2]);
  strncpy_irc(ajupe->info,reason,REALLEN);
  ajupe->serv->up = me.name;
  ajupe->servptr = &me;
  SetServer(ajupe);

  Count.server++;
  Count.myserver++;

  /* Some day, all these lists will be consolidated *sigh* */
  add_client_to_list(ajupe);
  add_to_client_hash_table(ajupe->name, ajupe);
  add_client_to_llist(&(ajupe->servptr->serv->servers), ajupe);
}


/*  
 * bogus_host
 *   
 * inputs       - hostname
 * output       - 1 if a bogus hostname input, 0 if its valid
 * side effects - none
 */
int bogus_host(char *host)
{
  int bogus_server = 0;
  char *s;
  int dots = 0;
 
  for( s = host; *s; s++ )
    {
      if (!IsServChar(*s))  
        {
          bogus_server = 1;
          break;
        }
      if ('.' == *s)
        ++dots;
    }
     
  if (!dots || bogus_server )
    return 1;
     
  return 0;
}
