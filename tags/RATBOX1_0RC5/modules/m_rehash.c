/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  m_rehash.c: Re-reads the configuration file.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"
#include "handlers.h"
#include "client.h"
#include "channel.h"
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
#include "s_gline.h"
#include "numeric.h"
#include "res.h"
#include "s_conf.h"
#include "s_log.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "hostmask.h"

static void mo_rehash(struct Client*, struct Client*, int, char**);
static void clear_temps(dlink_list *, int);
static void clear_pending_glines(void);

struct Message rehash_msgtab = {
  "REHASH", 0, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, mo_rehash}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&rehash_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&rehash_msgtab);
}

const char *_version = "$Revision$";
#endif
/*
 * mo_rehash - REHASH message handler
 *
 */
static void mo_rehash(struct Client *client_p, struct Client *source_p,
                     int parc, char *parv[])
{
  if (!IsOperRehash(source_p))
    {
      sendto_one(source_p,":%s NOTICE %s :You need rehash = yes;", me.name, parv[0]);
      return;
    }

  if (parc > 1)
    {
      if (irccmp(parv[1],"CHANNELS") == 0)
        {
          sendto_realops_flags(UMODE_ALL, L_ALL,
                       "%s is forcing cleanup of channels",parv[0]);
	  cleanup_channels(NULL);
        }
      else if(irccmp(parv[1],"DNS") == 0)
        {
          sendto_one(source_p, form_str(RPL_REHASHING), me.name, parv[0], "DNS");
          sendto_realops_flags(UMODE_ALL, L_ALL,"%s is rehashing DNS",
                               get_oper_name(source_p));
          restart_resolver();   /* re-read /etc/resolv.conf AGAIN?
                                   and close/re-open res socket */
        }
      else if(irccmp(parv[1],"MOTD") == 0)
        {
          sendto_realops_flags(UMODE_ALL, L_ALL,
		       "%s is forcing re-reading of MOTD file",
		       get_oper_name(source_p));
          ReadMessageFile( &ConfigFileEntry.motd );
        }
      else if(irccmp(parv[1],"OMOTD") == 0)
        {
          sendto_realops_flags(UMODE_ALL, L_ALL,
		       "%s is forcing re-reading of OPER MOTD file",
		       get_oper_name(source_p));
          ReadMessageFile( &ConfigFileEntry.opermotd );
        }
      else if(irccmp(parv[1], "GLINES") == 0)
      {
        sendto_realops_flags(UMODE_ALL, L_ALL,
                             "%s is clearing G-lines",
                             source_p->name);
        clear_temps(&glines, CONF_GLINE);
      }
      else if(irccmp(parv[1], "PGLINES") == 0)
      {
        sendto_realops_flags(UMODE_ALL, L_ALL,
                             "%s is clearing pending glines",
                             source_p->name);
        clear_pending_glines();
      }
      else if(irccmp(parv[1], "TKLINES") == 0)
      {
        sendto_realops_flags(UMODE_ALL, L_ALL,
                             "%s is clearing temp klines",
                             source_p->name);
        clear_temps(&temporary_min, CONF_KILL);
        clear_temps(&temporary_hour, CONF_KILL);
        clear_temps(&temporary_day, CONF_KILL);
        clear_temps(&temporary_week, CONF_KILL);
      }
      else if(irccmp(parv[1], "TDLINES") == 0)
      {
        sendto_realops_flags(UMODE_ALL, L_ALL,
                             "%s is clearing temp dlines",
                             source_p->name);
        clear_temps(&temporary_min, CONF_DLINE);
        clear_temps(&temporary_hour, CONF_DLINE);
        clear_temps(&temporary_day, CONF_DLINE);
        clear_temps(&temporary_week, CONF_DLINE);
      }
      else
      {
        sendto_one(source_p,":%s NOTICE %s :rehash one of: "
                   "GLINES PGLINES TKLINES TDLINES CHANNELS DNS MOTD OMOTD",
                   me.name, source_p->name);
        return;
      }
         
      ilog(L_NOTICE, "REHASH %s From %s", parv[1], 
           get_client_name(source_p, HIDE_IP));
      return;
    }
  else
    {
      sendto_one(source_p, form_str(RPL_REHASHING), me.name, parv[0],
                 ConfigFileEntry.configfile);
      sendto_realops_flags(UMODE_ALL, L_ALL,
			   "%s is rehashing server config file",
			   get_oper_name(source_p));
      ilog(L_NOTICE, "REHASH From %s[%s]", get_oper_name(source_p),
           source_p->localClient->sockhost);
      rehash(0);
      return;
    }
}

static void
clear_temps(dlink_list *tlist, int type)
{
  dlink_node *ptr;
  dlink_node *next_ptr;
  struct ConfItem *aconf;

  DLINK_FOREACH_SAFE(ptr, next_ptr, tlist->head)
  {
    aconf = ptr->data;

    if(aconf->status != type)
      continue;

    delete_one_address_conf(aconf->host, aconf);
    dlinkDestroy(ptr, tlist);
  }
}

static void
clear_pending_glines(void)
{
  struct gline_pending *glp_ptr;
  dlink_node *ptr;
  dlink_node *next_ptr;
  
  DLINK_FOREACH_SAFE(ptr, next_ptr, pending_glines.head)
  {
    glp_ptr = ptr->data;

    MyFree(glp_ptr->reason1);
    MyFree(glp_ptr->reason2);
    MyFree(glp_ptr);
    dlinkDestroy(ptr, &pending_glines);
  }
}

