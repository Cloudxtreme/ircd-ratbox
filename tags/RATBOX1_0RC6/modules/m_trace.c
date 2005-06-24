/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  m_trace.c: Traces a path to a client/server.
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

static void m_trace(struct Client*, struct Client*, int, char**);

static void trace_spy(struct Client *);

struct Message trace_msgtab = {
  "TRACE", 0, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_trace, m_trace, m_trace}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  hook_add_event("doing_trace");
  mod_add_cmd(&trace_msgtab);
}

void
_moddeinit(void)
{
  hook_del_event("doing_trace");
  mod_del_cmd(&trace_msgtab);
}
const char *_version = "$Revision$";
#endif

static int report_this_status(struct Client *source_p, struct Client *target_p,int dow,
                              int link_u_p, int link_u_s);


/*
 * m_trace
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
static void m_trace(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  struct Client       *target_p = NULL;
  struct Class        *cltmp;
  char  *tname;
  int   doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
  int   cnt = 0, wilds, dow;
  dlink_node *ptr;
  char *looking_for = parv[0];

  if(!IsClient(source_p))
    return;

  if (parc > 1)
    tname = parv[1];
  else
    tname = me.name;

  /* during shide, allow a non-oper to trace themselves only */
  if(!IsOper(source_p) && ConfigServerHide.hide_servers)
  {
    if(MyClient(source_p) && irccmp(tname, source_p->name) == 0)
      report_this_status(source_p, source_p, 0, 0, 0);

    sendto_one(source_p, form_str(RPL_ENDOFTRACE), 
               me.name, parv[0], tname);
    return;
  }

  if (parc > 2)
  {
    if (hunt_server(client_p, source_p, ":%s TRACE %s :%s", 2, parc, parv) != HUNTED_ISME)
      return;
  }
  

  switch (hunt_server(client_p, source_p, ":%s TRACE :%s", 1, parc, parv))
    {
    case HUNTED_PASS: /* note: gets here only if parv[1] exists */
      {
        struct Client *ac2ptr;
        if( (ac2ptr = find_client(tname)) == NULL)
        {
          DLINK_FOREACH(ptr, global_client_list.head)
          {
            ac2ptr = (struct Client *)ptr->data;
            if(match(tname, ac2ptr->name) || match(ac2ptr->name, tname))
            {
              break;
            }
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

  trace_spy(source_p);

  doall = (parv[1] && (parc > 1)) ? match(tname, me.name): TRUE;
  wilds = !parv[1] || strchr(tname, '*') || strchr(tname, '?');
  dow = wilds || doall;
  
  set_time();

  /* specific trace */
  if(dow == 0)
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
  if (doall && (IsOper(source_p) || !ConfigServerHide.hide_servers))
  {
    DLINK_FOREACH(ptr, global_serv_list.head)
    {
      target_p = ptr->data;

      link_u[target_p->from->localClient->fd] += target_p->serv->usercnt;
      link_s[target_p->from->localClient->fd]++;
    }
  }

  /* give non-opers a limited trace output of themselves (if local), 
   * opers and servers (if no shide) --fl
   */
  if(!IsOper(source_p))
  {
    if(MyClient(source_p))
      report_this_status(source_p, source_p, 0, 0, 0);

    DLINK_FOREACH(ptr, oper_list.head)
    {
      target_p = ptr->data;

      if(!doall && wilds && (match(tname, target_p->name) == 0))
        continue;

      report_this_status(source_p, target_p, 0, 0, 0);
    }

    if(!ConfigServerHide.hide_servers)
    {
      DLINK_FOREACH(ptr, serv_list.head)
      {
        target_p = ptr->data;

        if(!doall && wilds && !match(tname, target_p->name))
          continue;

        report_this_status(source_p, target_p, 0, 
                           link_u[target_p->localClient->fd],
                           link_s[target_p->localClient->fd]);
      }
    }

    sendto_one(source_p, form_str(RPL_ENDOFTRACE), me.name, parv[0], tname);
    return;
  }

  /* source_p is opered */
  
  /* report all direct connections */
  DLINK_FOREACH(ptr, lclient_list.head)
  {
    target_p = ptr->data;

    /* dont show invisible users to remote opers */
    if(IsInvisible(target_p) && dow && !MyConnect(source_p) && 
       !IsOper(target_p))
      continue;

    if(!doall && wilds && !match(tname, target_p->name))
      continue;

    cnt = report_this_status(source_p, target_p, dow, 0, 0);
  }

  DLINK_FOREACH(ptr, serv_list.head)
  {
    target_p = ptr->data;

    if (!doall && wilds && !match(tname, target_p->name))
      continue;

    cnt = report_this_status(source_p, target_p, dow,
                             link_u[target_p->localClient->fd],
                             link_s[target_p->localClient->fd]);
  }

  /* This section is to report the unknowns */
  /* should this be done to remote opers? --fl */
  DLINK_FOREACH(ptr, unknown_list.head)
  {
    target_p = ptr->data;

    if (!doall && wilds && !match(tname, target_p->name))
      continue;

    cnt = report_this_status(source_p,target_p,dow,0,0);
  }

  /*
   * Add these lines to summarize the above which can get rather long
   * and messy when done remotely - Avalon
   */
  if (!SendWallops(source_p) || !cnt)
    {
      /* redundant given we dont allow trace from non-opers anyway.. but its
       * left here in case that should ever change --fl
       */
      if(!cnt)
	sendto_one(source_p, form_str(RPL_TRACESERVER),
	           me.name, parv[0], 0, link_s[me.localClient->fd],
		   link_u[me.localClient->fd], me.name, "*", "*", me.name);
		   
      /* let the user have some idea that its at the end of the
       * trace
       */
      sendto_one(source_p, form_str(RPL_ENDOFTRACE),me.name,
                 parv[0],tname);
      return;
    }
    
  for (cltmp = ClassList; doall && cltmp; cltmp = cltmp->next)
  {
    if (CurrUsers(cltmp) > 0)
      sendto_one(source_p, form_str(RPL_TRACECLASS), me.name,
                 parv[0], ClassName(cltmp), CurrUsers(cltmp));
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
  int cnt=0;

  /* sanity check - should never happen */
  if(!MyConnect(target_p))
    return 0;

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
		   
      cnt++;
      break;

    case STAT_HANDSHAKE:
      sendto_one(source_p, form_str(RPL_TRACEHANDSHAKE), me.name,
                 source_p->name, class_name, 
#ifndef HIDE_SERVERS_IPS
                 IsOperAdmin(source_p) ? name :
#endif
                 target_p->name);
		   
      cnt++;
      break;

    case STAT_ME:
      break;

    case STAT_UNKNOWN:
      /* added time -Taner */
      sendto_one(source_p, form_str(RPL_TRACEUNKNOWN),
		 me.name, source_p->name, class_name, name, ip,
		 target_p->firsttime ? CurrentTime - target_p->firsttime : -1);
      cnt++;
      break;
    case STAT_CLIENT:
      /* Only opers see users if there is a wildcard
       * but anyone can see all the opers.
       */
      if ((IsOper(source_p) &&
	   (MyClient(source_p) || !(dow && IsInvisible(target_p))))
	  || !dow || IsOper(target_p) || (source_p == target_p))
	{
#ifndef HIDE_SPOOF_IPS
          if (IsAdmin(target_p))
	    sendto_one(source_p, form_str(RPL_TRACEOPERATOR),
                       me.name, source_p->name, class_name, name,
                       IsOperAdmin(source_p) ? ip : "255.255.255.255",
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
		       
	  else
	    sendto_one(source_p, form_str(RPL_TRACEUSER),
		       me.name, source_p->name, class_name, name,
#ifndef HIDE_SPOOF_IPS
		       MyOper(source_p) ? ip : 
#endif
		       (IsIPSpoof(target_p) ? "255.255.255.255" : ip),
		       CurrentTime - target_p->lasttime,
		       (target_p->user)?(CurrentTime - target_p->user->last):0);
	  cnt++;
	}
      break;

    case STAT_SERVER:
      sendto_one(source_p, form_str(RPL_TRACESERVER),
		 me.name, source_p->name, class_name, link_s_p,
		 link_u_p, 
#ifndef HIDE_SERVERS_IPS
                 IsOperAdmin(source_p) ? name :
#endif
                 target_p->name,
                 *(target_p->serv->by) ? target_p->serv->by : "*", "*",
		 me.name, CurrentTime - target_p->lasttime);
      cnt++;
      break;
      
    default: /* ...we actually shouldn't come here... --msa */
      sendto_one(source_p, form_str(RPL_TRACENEWTYPE), me.name,
		 source_p->name, name);
      cnt++;
      break;
    }

  return(cnt);
}

/* trace_spy()
 *
 * input        - pointer to client
 * output       - none
 * side effects - hook event doing_trace is called
 */
static void trace_spy(struct Client *source_p)
{
  struct hook_spy_data data;

  data.source_p = source_p;

  hook_call_event("doing_trace", &data);
}

