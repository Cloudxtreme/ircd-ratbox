/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  resv.c: Functions to reserve(jupe) a nick/channel.
 *
 *  Copyright (C) 2001-2002 Hybrid Development Team
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
#include "tools.h"
#include "restart.h"
#include "common.h"
#include "fdlist.h"
#include "ircd.h"
#include "send.h"
#include "s_debug.h"
#include "numeric.h"
#include "s_log.h"
#include "client.h"   
#include "memory.h"
#include "resv.h"
#include "hash.h"
#include "ircd_defs.h"

dlink_list resv_channel_list;
dlink_list resv_nick_list;

struct ResvChannel *
create_channel_resv(char *name, char *reason, int conf)
{
  struct ResvChannel *resv_p = NULL;
  int len;

  if(find_channel_resv(name))
    return NULL;

  if((len = strlen(reason)) > TOPICLEN)
  {
    reason[TOPICLEN] = '\0';
    len = TOPICLEN;
  }

  resv_p = (struct ResvChannel *)MyMalloc(sizeof(struct ResvChannel));

  strlcpy(resv_p->name, name, sizeof(resv_p->name));
  DupString(resv_p->reason, reason);
  resv_p->conf = conf;

  dlinkAddAlloc(resv_p, &resv_channel_list);
  add_to_resv_hash_table(resv_p->name, resv_p);

  return resv_p;
}

struct ResvNick *
create_nick_resv(char *name, char *reason, int conf)
{
  struct ResvNick *resv_p = NULL;
  int len;

  if((find_nick_resv(name)) != NULL)
    return NULL;

  if((len = strlen(reason)) > TOPICLEN)
  {
    reason[TOPICLEN] = '\0';
    len = TOPICLEN;
  }

  resv_p = (struct ResvNick *)MyMalloc(sizeof(struct ResvNick));

  strlcpy(resv_p->name, name, sizeof(resv_p->name));
  DupString(resv_p->reason, reason);
  resv_p->conf = conf;

  dlinkAddAlloc(resv_p, &resv_nick_list);

  return resv_p;
}

void
clear_conf_resv()
{
  struct ResvChannel *resv_cp;
  struct ResvNick *resv_np;
  dlink_node *ptr;
  dlink_node *next_ptr;

  DLINK_FOREACH_SAFE(ptr, next_ptr, resv_channel_list.head)
  {
    resv_cp = ptr->data;

    if(resv_cp->conf)
    {
      dlinkDestroy(ptr, &resv_channel_list);
      del_from_resv_hash_table(resv_cp->name, resv_cp);
      MyFree(resv_cp);
    }
  }

  DLINK_FOREACH_SAFE(ptr, next_ptr, resv_nick_list.head)
  {
    resv_np = ptr->data;

    if(resv_np->conf)
    {
      dlinkDestroy(ptr, &resv_nick_list);
      MyFree(resv_np);
    }
  }
}

int 
delete_channel_resv(struct ResvChannel *resv_p)
{
  if(resv_p == NULL)
    return 0;

  del_from_resv_hash_table(resv_p->name, resv_p);
  dlinkFindDestroy(&resv_channel_list, resv_p);
  MyFree((char *)resv_p);

  return 1;
}

int 
delete_nick_resv(struct ResvNick *resv_p)
{
  if(resv_p == NULL)
    return 0;

  dlinkFindDestroy(&resv_nick_list, resv_p);
  MyFree((char *)resv_p);

  return 1;
}

int
find_channel_resv(char *name)
{
  struct ResvChannel *resv_p;

  resv_p = (struct ResvChannel *)hash_find_resv(name);

  if (resv_p == NULL)
    return 0;

  return 1;
}

struct ResvNick *
find_nick_resv(char *name)
{
  struct ResvNick *resv_p;
  dlink_node *ptr;

  DLINK_FOREACH(ptr, resv_nick_list.head)
  {
    resv_p = ptr->data;
    
    if(irccmp(resv_p->name, name) == 0)
      return resv_p;
  }

  return NULL;
}

int
is_resvd(char *name)
{
  struct ResvNick *resv_p;
  dlink_node *ptr;

  DLINK_FOREACH(ptr, resv_nick_list.head)
  {
    resv_p = ptr->data;

    if(match(resv_p->name, name))
      return 1;
  }

  return 0;
}

void 
report_resv(struct Client *source_p)
{
  struct ResvChannel *resv_cp;
  struct ResvNick *resv_np;
  dlink_node *ptr;

  DLINK_FOREACH(ptr, resv_channel_list.head)
  {
    resv_cp = ptr->data;

    sendto_one(source_p, form_str(RPL_STATSQLINE),
               me.name, source_p->name,
	       resv_cp->conf ? 'Q' : 'q',
	       resv_cp->name, resv_cp->reason);
  }

  DLINK_FOREACH(ptr, resv_nick_list.head)
  {
    resv_np = ptr->data;

    sendto_one(source_p, form_str(RPL_STATSQLINE),
               me.name, source_p->name,
	       resv_np->conf ? 'Q' : 'q',
	       resv_np->name, resv_np->reason);
  }
}	       

int
clean_resv_nick(char *nick)
{
  char tmpch;
  int as=0;
  int q=0;
  int ch=0;

  if(*nick == '-' || IsDigit(*nick))
    return 0;
    
  while((tmpch = *nick++))
  {
    if(tmpch == '?')
      q++;
    else if(tmpch == '*')
      as++;
    else if(IsNickChar(tmpch))
      ch++;
    else
      return 0;
  }

  if(!ch && as)
    return 0;

  return 1;
}
    
