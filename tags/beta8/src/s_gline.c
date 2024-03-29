/************************************************************************
 *   IRC - Internet Relay Chat, src/s_gline.c
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
 *  $Id$
 */
#include "tools.h"
#include "handlers.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "irc_string.h"
#include "ircd.h"
#include "m_kline.h"
#include "hostmask.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_misc.h"
#include "scache.h"
#include "send.h"
#include "msg.h"
#include "fileio.h"
#include "s_serv.h"
#include "s_gline.h"
#include "hash.h"
#include "event.h"
#include "list.h"
#include "memory.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

dlink_list glines;

static void expire_glines(void);
static void expire_pending_glines(void);

/* add_gline
 *
 * inputs       - pointer to struct ConfItem
 * output       - none
 * Side effects - links in given struct ConfItem into gline link list
 */
void
add_gline(struct ConfItem *aconf)
{
  dlink_node *gline_node;
  gline_node = make_dlink_node();
  dlinkAdd(aconf, gline_node, &glines);
}

/* find_gkill
 *
 * inputs       - struct Client pointer to a Client struct
 * output       - struct ConfItem pointer if a gline was found for this client
 * side effects - none
 */
struct ConfItem*
find_gkill(struct Client* client_p, char* username)
{
  assert(0 != client_p);
  return (IsExemptKline(client_p)) ? 0 : find_is_glined(client_p->host, username);
}

/*
 * find_is_glined
 * inputs       - hostname
 *              - username
 * output       - pointer to struct ConfItem if user@host glined
 * side effects -
 */
struct ConfItem*
find_is_glined(const char* host, const char* name)
{
  dlink_node *gline_node;
  struct ConfItem *kill_ptr; 

  for(gline_node = glines.head; gline_node; gline_node = gline_node->next)
    {
      kill_ptr = gline_node->data;
      if( (kill_ptr->name && (!name || match(kill_ptr->name,name)))
	  &&
	  (kill_ptr->host && (!host || match(kill_ptr->host,host))))
        {
          return(kill_ptr);
        }
    }

  return((struct ConfItem *)NULL);
}

/*
 * remove_gline_match
 *
 * inputs       - user@host
 * output       - 1 if successfully removed, otherwise 0
 * side effects -
 */
int
remove_gline_match(const char* user, const char* host)
{
  dlink_node *gline_node;
  struct ConfItem *kill_ptr;

  for(gline_node = glines.head; gline_node; gline_node = gline_node->next)
    {
      kill_ptr = gline_node->data;
      if(!irccmp(kill_ptr->host,host) && !irccmp(kill_ptr->name,user))
	{
          free_conf(kill_ptr);
          dlinkDelete(gline_node, &glines);
          free_dlink_node(gline_node);
          return 1;
	}
    }
  return 0;
}

/*
 * cleanup_glines
 *
 * inputs	- NONE
 * output	- NONE
 * side effects - expire gline lists
 *                This is an event started off in ircd.c
 */
void
cleanup_glines(void *notused)
{
  expire_glines();
  expire_pending_glines();
}

/*
 * expire_glines
 * 
 * inputs       - NONE
 * output       - NONE
 * side effects -
 *
 * Go through the gline list, expire any needed.
 */
static void
expire_glines()
{
  dlink_node *gline_node;
  dlink_node *next_node;
  struct ConfItem *kill_ptr;

  for(gline_node = glines.head; gline_node; gline_node = next_node)
    {
      kill_ptr = gline_node->data;
      next_node = gline_node->next;

      if(kill_ptr->hold <= CurrentTime)
	{
	  free_conf(kill_ptr);
          dlinkDelete(gline_node, &glines);
          free_dlink_node(gline_node);
	}
    }
}

/*
 * expire_pending_glines
 * 
 * inputs       - NONE
 * output       - NONE
 * side effects -
 *
 * Go through the pending gline list, expire any that haven't had
 * enough "votes" in the time period allowed
 */
static void
expire_pending_glines()
{
  dlink_node *pending_node;
  dlink_node *next_node;
  struct gline_pending *glp_ptr;

  for(pending_node = pending_glines.head; pending_node; pending_node = next_node)
    {
      glp_ptr = pending_node->data;
      next_node = pending_node->next;

      if( (glp_ptr->last_gline_time + GLINE_PENDING_EXPIRE) <= CurrentTime )
        {
          MyFree(glp_ptr->reason1);
          MyFree(glp_ptr->reason2);
          MyFree(glp_ptr);
          dlinkDelete(pending_node, &pending_glines);
          free_dlink_node(pending_node);
        }
    }
}
