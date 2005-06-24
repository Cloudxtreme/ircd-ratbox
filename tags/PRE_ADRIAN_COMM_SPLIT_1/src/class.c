/*
 *   IRC - Internet Relay Chat, src/class.c
 *   Copyright (C) 1990 Darren Reed
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
#include "class.h"
#include "client.h"
#include "common.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "s_conf.h"
#include "send.h"
#include "irc_string.h"
#include "s_debug.h"

#include <string.h>
#include <stdlib.h>

#define BAD_CONF_CLASS          -1
#define BAD_PING                -2
#define BAD_CLIENT_CLASS        -3

struct Class* ClassList;

struct Class *make_class()
{
  struct Class        *tmp;

  tmp = (struct Class *)MyMalloc(sizeof(struct Class));
  memset((void*)tmp, 0, sizeof(struct Class));
  return tmp;
}

void free_class(struct Class *tmp)
{
  MyFree(tmp->className);
  MyFree((char *)tmp);
}

static  int     get_conf_ping(struct ConfItem *aconf)
{
  if ((aconf) && ClassPtr(aconf))
    return (ConfPingFreq(aconf));

  Debug((DEBUG_DEBUG,"No Ping For %s",
         (aconf) ? aconf->name : "*No Conf*"));

  return (BAD_PING);
}

const char*     get_client_class(struct Client *acptr)
{
  struct SLink  *tmp;
  struct Class        *cl;
  const char*   retc = (const char *)NULL;

  if (acptr && !IsMe(acptr)  && (acptr->confs))
    for (tmp = acptr->confs; tmp; tmp = tmp->next)
      {
        if (!tmp->value.aconf ||
            !(cl = ClassPtr(tmp->value.aconf)))
          continue;
        if (ClassName(cl))
          retc = ClassName(cl);
      }

  Debug((DEBUG_DEBUG,"Returning Class %s For %s",retc,acptr->name));

  return (retc);
}

int     get_client_ping(struct Client *acptr)
{
  int   ping = 0, ping2;
  struct ConfItem     *aconf;
  struct SLink  *link;

  link = acptr->confs;

  if (link)
    while (link)
      {
        aconf = link->value.aconf;
        if (aconf->status & (CONF_CLIENT|CONF_CONNECT_SERVER|
                             CONF_NOCONNECT_SERVER))
          {
            ping2 = get_conf_ping(aconf);
            if ((ping2 != BAD_PING) && ((ping > ping2) ||
                                        !ping))
              ping = ping2;
          }
        link = link->next;
      }
  else
    {
      ping = PINGFREQUENCY;
      Debug((DEBUG_DEBUG,"No Attached Confs"));
    }
  if (ping <= 0)
    ping = PINGFREQUENCY;
  Debug((DEBUG_DEBUG,"Client %s Ping %d", acptr->name, ping));
  return (ping);
}

int     get_con_freq(struct Class *clptr)
{
  if (clptr)
    return (ConFreq(clptr));
  else
    return (CONNECTFREQUENCY);
}

/*
 * When adding a class, check to see if it is already present first.
 * if so, then update the information for that class, rather than create
 * a new entry for it and later delete the old entry.
 * if no present entry is found, then create a new one and add it in
 * immediately after the first one (class 0).
 */
void    add_class(char *classname,
                  int ping,
                  int confreq,
                  int maxli,
                  long sendq)
{
  struct Class *t, *p;

  if(!classname)
    return;

  t = find_class(classname);
  if (t == ClassList)
    {
      p = (struct Class *)make_class();
      p->next = t->next;
      t->next = p;
    }
  else
    p = t;
  Debug((DEBUG_DEBUG,
         "Add Class %s: p %x t %x - cf: %d pf: %d ml: %d sq: %l",
         classname, p, t, confreq, ping, maxli, sendq));

  /* classname already known to be non NULL */
  DupString(ClassName(p),classname);
  ConFreq(p) = confreq;
  PingFreq(p) = ping;
  MaxLinks(p) = maxli;
  MaxSendq(p) = (sendq > 0) ? sendq : MAXSENDQLENGTH;
  if (p != t)
    Links(p) = 0;
}

struct Class  *find_class(char* classname)
{
  struct Class *cltmp;

  if(!classname)
    return NULL;

  for (cltmp = ClassList; cltmp; cltmp = cltmp->next)
    if (!strcmp(ClassName(cltmp),classname))
      return cltmp;
  return ClassList;
}

void    check_class()
{
  struct Class *cltmp, *cltmp2;

  Debug((DEBUG_DEBUG, "Class check:"));

  for (cltmp2 = cltmp = ClassList; cltmp; cltmp = cltmp2->next)
    {
      Debug((DEBUG_DEBUG,
             "ClassName %s Class %d : CF: %d PF: %d ML: %d LI: %d SQ: %ld",
             ClassName(cltmp),ClassType(cltmp), ConFreq(cltmp), PingFreq(cltmp),
             MaxLinks(cltmp), Links(cltmp), MaxSendq(cltmp)));
      if (MaxLinks(cltmp) < 0)
        {
          cltmp2->next = cltmp->next;
          if (Links(cltmp) <= 0)
            free_class(cltmp);
        }
      else
        cltmp2 = cltmp;
    }
}

void    initclass()
{
  ClassList = (struct Class *)make_class();

  ClassType(ClassList) = 0;
  DupString(ClassName(ClassList),"default");
  ConFreq(ClassList) = CONNECTFREQUENCY;
  PingFreq(ClassList) = PINGFREQUENCY;
  MaxLinks(ClassList) = MAXIMUM_LINKS;
  MaxSendq(ClassList) = MAXSENDQLENGTH;
  Links(ClassList) = 0;
  ClassList->next = NULL;
}

void    report_classes(struct Client *sptr)
{
  struct Class *cltmp;

  for (cltmp = ClassList; cltmp; cltmp = cltmp->next)
    sendto_one(sptr, form_str(RPL_STATSYLINE), me.name, sptr->name,
               'Y', ClassName(cltmp), PingFreq(cltmp), ConFreq(cltmp),
               MaxLinks(cltmp), MaxSendq(cltmp));
}

long    get_sendq(struct Client *cptr)
{
  int   sendq = MAXSENDQLENGTH, retc = BAD_CLIENT_CLASS;
  struct SLink  *tmp;
  struct Class        *cl;

  if (cptr && !IsMe(cptr)  && (cptr->confs))
    for (tmp = cptr->confs; tmp; tmp = tmp->next)
      {
        if (!tmp->value.aconf ||
            !(cl = ClassPtr(tmp->value.aconf)))
          continue;
        if (ClassType(cl) > retc)
          sendq = MaxSendq(cl);
      }
  return sendq;
}


