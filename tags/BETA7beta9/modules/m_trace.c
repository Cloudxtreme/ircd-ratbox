/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_trace.c
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
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <string.h>
#include <time.h>

static void m_trace(struct Client *, struct Client *, int, char **);
static void ms_trace(struct Client*, struct Client*, int, char**);
static void mo_trace(struct Client*, struct Client*, int, char**);

static void trace_spy(struct Client *);

struct Message trace_msgtab = {
  "TRACE", 0, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_trace, ms_trace, mo_trace}
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
char *_version = "20010109";
#endif
static int report_this_status(struct Client *source_p, struct Client *target_p,int dow,
                              int link_u_p, int link_u_s);


/*
 * m_trace()
 *
 *	parv[0] = sender prefix
 *	parv[1] = target client/server to trace
 */
static void m_trace(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  sendto_one(source_p, form_str(RPL_ENDOFTRACE), me.name,
             parv[0], parv[1]);
}


/*
** mo_trace
**      parv[0] = sender prefix
**      parv[1] = servername
*/
static void mo_trace(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  struct Client       *target_p = NULL;
  struct Class        *cltmp;
  char  *tname;
  int   doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
  int   cnt = 0, wilds, dow;
  static time_t now;
  dlink_node *ptr;
  char *looking_for = parv[0];
  if (parc > 2)
    if (hunt_server(client_p, source_p, ":%s TRACE %s :%s", 2, parc, parv))
      return;
  
  if (parc > 1)
    tname = parv[1];
  else
    {
      tname = me.name;
    }

  switch (hunt_server(client_p, source_p, ":%s TRACE :%s", 1, parc, parv))
    {
    case HUNTED_PASS: /* note: gets here only if parv[1] exists */
      {
        struct Client *ac2ptr;
        
        ac2ptr = next_client_double(GlobalClientList, tname);
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

  if(IsClient(source_p))
    trace_spy(source_p);

  doall = (parv[1] && (parc > 1)) ? match(tname, me.name): TRUE;
  wilds = !parv[1] || strchr(tname, '*') || strchr(tname, '?');
  dow = wilds || doall;
  
  now = time(NULL);

  if(!IsOper(source_p) || !dow) /* non-oper traces must be full nicks */
                              /* lets also do this for opers tracing nicks */
    {
      const char* name;
      const char* class_name;
      char ipaddr[HOSTIPLEN];

      target_p = find_client(tname);
      if(!target_p || !IsPerson(target_p)) 
        {
          /* this should only be reached if the matching
             target is this server */
          sendto_one(source_p, form_str(RPL_ENDOFTRACE),me.name,
                     parv[0], tname);
          return;
        }
      name = get_client_name(target_p, HIDE_IP);
      inetntop(target_p->localClient->aftype, &IN_ADDR(target_p->localClient->ip), ipaddr, HOSTIPLEN);

      class_name = get_client_class(target_p);

      if (IsOper(target_p))
        {
          sendto_one(source_p, form_str(RPL_TRACEOPERATOR),
                     me.name, parv[0], class_name,
                     name, 
                     IsOper(source_p)?ipaddr:(IsIPHidden(target_p)?"127.0.0.1":ipaddr),
                     now - target_p->lasttime,
                     (target_p->user)?(now - target_p->user->last):0);
        }
      else
        {
          sendto_one(source_p,form_str(RPL_TRACEUSER),
                     me.name, parv[0], class_name,
                     name, 
                     IsOper(source_p)?ipaddr:(IsIPHidden(target_p)?"127.0.0.1":ipaddr),
                     now - target_p->lasttime,
                     (target_p->user)?(now - target_p->user->last):0);
        }
      sendto_one(source_p, form_str(RPL_ENDOFTRACE),me.name,
                 parv[0], tname);
      return;
    }

  memset((void *)link_s,0,sizeof(link_s));
  memset((void *)link_u,0,sizeof(link_u));

  /*
   * Count up all the servers and clients in a downlink.
   */
  if (doall)
   {
    for (target_p = GlobalClientList; target_p; target_p = target_p->next)
     {
      if (IsPerson(target_p))
        {
          link_u[target_p->from->fd]++;
        }
      else if (IsServer(target_p))
	{
	  link_s[target_p->from->fd]++;
	}
     }
   }
  /* report all direct connections */
  for (ptr = lclient_list.head; ptr; ptr = ptr->next)
    {
      target_p = ptr->data;

      if (IsInvisible(target_p) && dow &&
          !(MyConnect(source_p) && IsOper(source_p)) &&
          !IsOper(target_p) && (target_p != source_p))
        continue;
      if (!doall && wilds && !match(tname, target_p->name))
        continue;
      if (!dow && irccmp(tname, target_p->name))
        continue;

      cnt = report_this_status(source_p,target_p,dow,0,0);
    }

  for (ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      target_p = ptr->data;

      if (!doall && wilds && !match(tname, target_p->name))
        continue;
      if (!dow && irccmp(tname, target_p->name))
        continue;

      cnt = report_this_status(source_p, target_p, dow,
                               link_u[target_p->fd],
                               link_s[target_p->fd]);
    }

  /* This section is to report the unknowns */
  for (ptr = unknown_list.head; ptr; ptr = ptr->next)
    {
      target_p = ptr->data;

      if (!doall && wilds && !match(tname, target_p->name))
        continue;
      if (!dow && irccmp(tname, target_p->name))
        continue;

      cnt = report_this_status(source_p,target_p,dow,0,0);
    }

  /*
   * Add these lines to summarize the above which can get rather long
   * and messy when done remotely - Avalon
   */
  if (!SendWallops(source_p) || !cnt)
    {
      if (cnt)
        {
          sendto_one(source_p, form_str(RPL_ENDOFTRACE),me.name,
                     parv[0],tname);
          return;
        }
      /* let the user have some idea that its at the end of the
       * trace
       */
      sendto_one(source_p, form_str(RPL_TRACESERVER),
                 me.name, parv[0], 0, link_s[me.fd],
                 link_u[me.fd], me.name, "*", "*", me.name);
      sendto_one(source_p, form_str(RPL_ENDOFTRACE),me.name,
                 parv[0],tname);
      return;
    }
  for (cltmp = ClassList; doall && cltmp; cltmp = cltmp->next)
    if (Links(cltmp) > 0)
      sendto_one(source_p, form_str(RPL_TRACECLASS), me.name,
                 parv[0], ClassName(cltmp), Links(cltmp));
  sendto_one(source_p, form_str(RPL_ENDOFTRACE),me.name, parv[0],tname);
}


/*
** ms_trace
**      parv[0] = sender prefix
**      parv[1] = servername
*/
static void ms_trace(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  if (hunt_server(client_p, source_p, ":%s TRACE %s :%s", 2, parc, parv))
    return;

  if( IsOper(source_p) )
    mo_trace(client_p,source_p,parc,parv);
  return;
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
  static time_t now;

  inetntop(target_p->localClient->aftype, &IN_ADDR(target_p->localClient->ip), ip, HOSTIPLEN);
  name = get_client_name(target_p, HIDE_IP);
  class_name = get_client_class(target_p);

  now = time(NULL);  

  switch(target_p->status)
    {
    case STAT_CONNECTING:
      sendto_one(source_p, form_str(RPL_TRACECONNECTING), me.name,
                 source_p->name, class_name, 
		 IsOperAdmin(source_p) ? name : target_p->name);
		   
      cnt++;
      break;
    case STAT_HANDSHAKE:
      sendto_one(source_p, form_str(RPL_TRACEHANDSHAKE), me.name,
                 source_p->name, class_name, 
                 IsOperAdmin(source_p) ? name : target_p->name);
		   
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
	  || !dow || IsOper(target_p))
	{
          if (IsAdmin(target_p))
	    sendto_one(source_p,
                       form_str(RPL_TRACEOPERATOR),
                       me.name,
                       source_p->name, class_name,
		       name,
                       IsOperAdmin(source_p) ? ip : "255.255.255.255",
                       now - target_p->lasttime,
                       (target_p->user)?(now - target_p->user->last):0);
	  else if (IsOper(target_p))
	    sendto_one(source_p,
		       form_str(RPL_TRACEOPERATOR),
		       me.name,
		       source_p->name, class_name,
		       name, IsOper(source_p)?ip:(IsIPHidden(target_p)?"127.0.0.1":ip),
		       now - target_p->lasttime,
		       (target_p->user)?(now - target_p->user->last):0);
	  else
	    sendto_one(source_p,form_str(RPL_TRACEUSER),
		       me.name, source_p->name, class_name,
		       name,
		       IsOper(source_p)?ip:(IsIPHidden(target_p)?"127.0.0.1":ip),
		       now - target_p->lasttime,
		       (target_p->user)?(now - target_p->user->last):0);
	  cnt++;
	}
      break;
    case STAT_SERVER:
    name = IsOperAdmin(source_p) ? get_client_name(target_p, HIDE_IP):get_client_name(target_p, MASK_IP);

      sendto_one(source_p, form_str(RPL_TRACESERVER),
		 me.name, source_p->name, class_name, link_s_p,
		 link_u_p, name, *(target_p->serv->by) ?
		 target_p->serv->by : "*", "*",
		 me.name, now - target_p->lasttime);
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

