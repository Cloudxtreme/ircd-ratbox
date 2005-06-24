/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_ltrace.c: Traces a path to a client/server.
 *
 *  Copyright (C) 2002 Hybrid Development Team
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
#include "class.h"
#include "hook.h"
#include "client.h"
#include "hash.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_serv.h"
#include "s_conf.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"


static void m_ltrace(struct Client *, struct Client *, int, char **);
static void ltrace_spy(struct Client *);

struct Message ltrace_msgtab = {
  "LTRACE", 0, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_ltrace, m_ltrace, m_ltrace}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  hook_add_event("doing_ltrace");
  mod_add_cmd(&ltrace_msgtab);
}

void
_moddeinit(void)
{
  hook_del_event("doing_ltrace");
  mod_del_cmd(&ltrace_msgtab);
}

const char *_version = "$Revision$";
#endif

static int report_this_status(struct Client *source_p, struct Client *target_p,int dow,
                              int link_u_p, int link_u_s);

/*
 * m_ltrace
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
static void m_ltrace(struct Client *client_p, struct Client *source_p,
                     int parc, char *parv[])
{
  struct Client       *target_p = NULL;
  char  *tname;
  int   doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
  int   wilds, dow;
  dlink_node *ptr;
  char *looking_for = parv[0];

  if(!IsClient(source_p))
    return;

  if (parc > 1)
    tname = parv[1];
  else
    tname = me.name;

  if(!IsOper(source_p) && ConfigServerHide.hide_servers)
  {
    if(MyClient(source_p) && (irccmp(tname, source_p->name) == 0))
      report_this_status(source_p, source_p, 0, 0, 0);

    sendto_one(source_p, form_str(RPL_ENDOFTRACE),
              me.name, parv[0], tname);
  }
  if (parc > 2)
  {
    if (hunt_server(client_p, source_p, ":%s LTRACE %s :%s", 2, parc, parv))
      return;
  }

  switch (hunt_server(client_p, source_p, ":%s LTRACE :%s", 1, parc, parv))
    {
    case HUNTED_PASS: /* note: gets here only if parv[1] exists */
      {
        struct Client *ac2ptr;
        dlink_node *cptr;
        
        if((ac2ptr = find_client(tname)) == NULL)
        {
          DLINK_FOREACH(cptr, global_client_list.head)
          {
            ac2ptr = (struct Client *)cptr->data;
            if(match(tname, ac2ptr->name) || match(ac2ptr->name, tname))
               break;
            else
              ac2ptr = NULL;
          }
        }
        if (ac2ptr)
          sendto_one(source_p, form_str(RPL_TRACELINK), me.name, looking_for,
                     ircd_version, debugmode, tname, ac2ptr->from->name);
        else
          sendto_one(source_p, form_str(RPL_TRACELINK), me.name, looking_for,
                     ircd_version, debugmode, tname, "ac2ptr_is_NULL!!");
        return;
      }
    case HUNTED_ISME:
      break;
    default:
      return;
    }

  ltrace_spy(source_p);

  doall = (parv[1] && (parc > 1)) ? match(tname, me.name): TRUE;
  wilds = !parv[1] || strchr(tname, '*') || strchr(tname, '?');
  dow = wilds || doall;
  
  set_time();

  /* lusers cant issue ltrace.. */
  if(!dow)
  {
      target_p = find_client(tname);
      
      if(target_p && IsPerson(target_p)) 
        report_this_status(source_p, target_p, 0, 0, 0);

      sendto_one(source_p, form_str(RPL_ENDOFTRACE),me.name,
                 parv[0], tname);
      return;
  }

  memset((void *)link_s,0,sizeof(link_s));
  memset((void *)link_u,0,sizeof(link_u));

  /* count up the servers behind the server links only if were going
   * to be using them --fl
   */
  if (doall)
  {
    DLINK_FOREACH(ptr, global_serv_list.head)
    {
      target_p = ptr->data;

      link_u[target_p->from->localClient->fd] += target_p->serv->usercnt;
      link_s[target_p->from->localClient->fd]++;
    }
  }
  
  /* report all opers */
  DLINK_FOREACH(ptr, oper_list.head)
  {
    target_p = ptr->data;

    if(!doall && wilds && (match(tname, target_p->name) == 0))
      continue;

    report_this_status(source_p, target_p, 0, 0, 0);
  }

  /* report all servers */
  if(!ConfigServerHide.hide_servers)
  {
    DLINK_FOREACH(ptr, serv_list.head)
    {
      target_p = ptr->data;

      if (!doall && wilds && match(tname, target_p->name) == 0)
        continue;

      report_this_status(source_p, target_p, 0,
                         link_u[target_p->localClient->fd],
                         link_s[target_p->localClient->fd]);
    }
  }

  if(IsOper(source_p))
  {
    DLINK_FOREACH(ptr, unknown_list.head)
    {
      target_p = ptr->data;

      if(!doall && wilds && match(tname, target_p->name) == 0)
        continue;

      report_this_status(source_p, target_p, 0, 0, 0);
    }
  }

  sendto_one(source_p, form_str(RPL_ENDOFTRACE),me.name, parv[0],tname);
}

/*
 * report_this_status
 *
 * inputs	- pointer to client to report to
 * 		- pointer to client to report about
 * output	- counter of number of hits
 * side effects - NONE
 */
static int report_this_status(struct Client *source_p, struct Client *target_p,
                              int dow, int link_u_p, int link_s_p)
{
  const char* name;
  const char* class_name;
  char  ip[HOSTIPLEN];

  inetntop(target_p->localClient->aftype, &IN_ADDR(target_p->localClient->ip), ip, HOSTIPLEN);
  name = get_client_name(target_p, HIDE_IP);
  class_name = get_client_class(target_p);

  set_time();

  switch(target_p->status)
    {
    case STAT_CONNECTING:
      sendto_one(source_p, form_str(RPL_TRACECONNECTING), me.name,
                 source_p->name, class_name, 
#ifndef HIDE_SERVERS_IPS
                 IsOperAdmin(source_p) ? name : 
#endif
                 target_p->name);
		   
      break;

    case STAT_HANDSHAKE:
      sendto_one(source_p, form_str(RPL_TRACEHANDSHAKE), me.name,
                 source_p->name, class_name, 
#ifndef HIDE_SERVERS_IPS
                 IsOperAdmin(source_p) ? name :
#endif
                 target_p->name);
		   
      break;
      
    case STAT_ME:
    case STAT_UNKNOWN:
      break;

    case STAT_CLIENT:
#ifndef HIDE_SPOOF_IPS
      if (IsAdmin(target_p))
	sendto_one(source_p, form_str(RPL_TRACEOPERATOR),
                   me.name, source_p->name, class_name, name,
                   IsOperAdmin(source_p) ? ip :
                   (IsIPSpoof(target_p) ? "255.255.255.255" : ip),
                   CurrentTime - target_p->lasttime,
                   (target_p->user) ? (CurrentTime - target_p->user->last) : 0);
		       
      else
#endif
       if (IsOper(target_p))
	sendto_one(source_p, form_str(RPL_TRACEOPERATOR),
		   me.name, source_p->name, class_name, name, 
#ifndef HIDE_SPOOF_IPS
		   MyOper(source_p) ? ip : 
#endif
		   (IsIPSpoof(target_p) ? "255.255.255.255" : ip),
		   CurrentTime - target_p->lasttime,
		   (target_p->user)?(CurrentTime - target_p->user->last):0);

      break;

    case STAT_SERVER:
      sendto_one(source_p, form_str(RPL_TRACESERVER),
		 me.name, source_p->name, class_name, link_s_p,
		 link_u_p, 
#ifndef HIDE_SPOOF_IPS
                 IsOperAdmin(source_p) ? name :
#endif
                 target_p->name,
                 *(target_p->serv->by) ?
		 target_p->serv->by : "*", "*",
		 me.name, CurrentTime - target_p->lasttime);
      break;
      
    default: /* ...we actually shouldn't come here... --msa */
      sendto_one(source_p, form_str(RPL_TRACENEWTYPE), me.name,
		 source_p->name, name);
      break;
    }

  return 0;
}

/* ltrace_spy()
 *
 * input        - pointer to client
 * output       - none
 * side effects - hook event doing_trace is called
 */
static void ltrace_spy(struct Client *source_p)
{
  struct hook_spy_data data;

  data.source_p = source_p;

  hook_call_event("doing_ltrace", &data);
}

