/************************************************************************
 *   IRC - Internet Relay Chat, src/list.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Finland
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
 *  (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen
 *
 * $Id$
 */
#include "tools.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "list.h"
#include "hostmask.h"
#include "numeric.h"
#include "res.h"
#include "restart.h"
#include "s_log.h"
#include "send.h"
#include "memory.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* XXX assummed 32 bit ints */
int links_count=0;
int user_count=0;

/*
 * make_user
 *
 * inputs	- pointer to client struct
 * output	- pointer to struct User
 * side effects - add's an User information block to a client
 *                if it was not previously allocated.
 */
struct User* make_user(struct Client *client_p)
{
  struct User        *user;

  user = client_p->user;
  if (!user)
    {
      user = (struct User *)MyMalloc(sizeof (struct User));

      ++user_count;

      /* The commented out lines here are
       * for documentation purposes only
       * as they are zeroed by MyMalloc
       */

#if 0
      user->away = NULL;
      user->server = (char *)NULL;      /* scache server name */
      user->joined = 0;
      user->channel.head = NULL;
      user->channel.tail = NULL;
      user->invited.head = NULL;
      user->invited.tail = NULL;
      user->id[0] = '\0';
#endif
      user->refcnt = 1;
      client_p->user = user;
    }
  return user;
}

/*
 * make_server
 *
 * inputs	- pointer to client struct
 * output	- pointer to struct Server
 * side effects - add's an Server information block to a client
 *                if it was not previously allocated.
 */
struct Server *make_server(struct Client *client_p)
{
  struct Server* serv = client_p->serv;

  if (!serv)
    {
      serv = (struct Server *)MyMalloc(sizeof(struct Server));

      /* The commented out lines here are
       * for documentation purposes only
       * as they are zeroed by MyMalloc above
       */
#if 0
      serv->user = NULL;
      serv->users = NULL;
      serv->servers = NULL;
      *serv->by = '\0'; 
      serv->up = (char *)NULL;
#endif
      client_p->serv = serv;
    }
  return client_p->serv;
}

/*
 * free_user
 * 
 * inputs	- pointer to user struct
 *		- pointer to client struct
 * output	- none
 * side effects - Decrease user reference count by one and release block,
 *                if count reaches 0
 */
void _free_user(struct User* user, struct Client* client_p)
{
  if (--user->refcnt <= 0)
    {
      if (user->away)
        MyFree((char *)user->away);
      /*
       * sanity check
       */
      if (user->joined || user->refcnt < 0 ||
          user->invited.head || user->channel.head)
      {
        sendto_realops_flags(FLAGS_ALL,
			     "* %#lx user (%s!%s@%s) %#lx %#lx %#lx %d %d *",
			     (unsigned long)client_p, client_p ? client_p->name : "<noname>",
			     client_p->username, client_p->host, (unsigned long)user,
			     (unsigned long)user->invited.head,
			     (unsigned long)user->channel.head, user->joined,
			     user->refcnt);
        assert(0);
      }

      MyFree(user);
      --user_count;
      assert(user_count >= 0);
    }
}

/*
 * make_dlink_node
 *
 * inputs	- NONE
 * output	- pointer to new dlink_node
 * side effects	- NONE
 */
dlink_node *make_dlink_node()
{
  dlink_node *lp;

  lp = (dlink_node *)MyMalloc(sizeof(dlink_node));
  ++links_count;

  lp->next = NULL;
  lp->prev = NULL;

  return lp;
}

/*
 * _free_dlink_node
 *
 * inputs	- pointer to dlink_node
 * output	- NONE
 * side effects	- free given dlink_node 
 */
void _free_dlink_node(dlink_node *ptr)
{
  MyFree(ptr);
  --links_count;
  assert(links_count >= 0);
}


/*
 * count_user_memory
 *
 * inputs	- pointer to user memory actually used
 *		- pointer to user memory allocated total in block allocator
 * output	- NONE
 * side effects	- NONE
 */
void count_user_memory(int *count,int *user_memory_used)
{
  *count = user_count;
  *user_memory_used = user_count * sizeof(struct User);
}

/*
 * count_links_memory
 *
 * inputs	- pointer to dlinks memory actually used
 *		- pointer to dlinks memory allocated total in block allocator
 * output	- NONE
 * side effects	- NONE
 */
void count_links_memory(int *count,int *links_memory_used)
{
  *count = links_count;
  *links_memory_used = links_count * sizeof(dlink_node);
}




