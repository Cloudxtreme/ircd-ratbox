/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  list.c: Various assorted functions for various structures.
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
#include "balloc.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* XXX assummed 32 bit ints */
int links_count=0;
int user_count=0;

/*
 * initUser
 *
 * inputs	- none
 * outputs	- none
 *
 * side effects - Creates a block heap for struct Users
 *
 */
static BlockHeap *user_heap;
void initUser(void)
{
  user_heap = BlockHeapCreate(sizeof(struct User), USER_HEAP_SIZE);
  if(!user_heap)
     outofmemory();	
}
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
      user = (struct User *)BlockHeapAlloc(user_heap);

      ++user_count;

      memset(user, 0, sizeof(*user));
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
      memset(serv, 0, sizeof(*serv));

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
        sendto_realops_flags(FLAGS_ALL, L_ALL,
			     "* %#lx user (%s!%s@%s) %#lx %#lx %#lx %d %d *",
			     (unsigned long)client_p, client_p ? client_p->name : "<noname>",
			     client_p->username, client_p->host, (unsigned long)user,
			     (unsigned long)user->invited.head,
			     (unsigned long)user->channel.head, user->joined,
			     user->refcnt);
        assert(!user->joined);
        assert(!user->refcnt);
        assert(!user->invited.head);
        assert(!user->channel.head);
      }

      BlockHeapFree(user_heap, user);
      --user_count;
      assert(user_count >= 0);
    }
}


/*
 * init_dlink_nodes
 *
 */
static BlockHeap *dnode_heap;
void init_dlink_nodes(void)
{
  dnode_heap = BlockHeapCreate(sizeof(dlink_node), DNODE_HEAP_SIZE);
  if(dnode_heap == NULL)
     outofmemory();
}
 
/*
 * make_dlink_node
 *
 * inputs	- NONE
 * output	- pointer to new dlink_node
 * side effects	- NONE
 */
#ifndef MEMDEBUG
dlink_node*
_make_dlink_node(void)
{
  dlink_node *lp;

  lp = (dlink_node *)BlockHeapAlloc(dnode_heap);;
  ++links_count;

  lp->next = NULL;
  lp->prev = NULL;
  return lp;
}
#else
dlink_node*
_make_dlink_node(const char *file, int line)
{
 char nambuf[50];
 dlink_node *lp;
 #define DLINK_DBG_PREFIX "DLINK:"
 strcpy(nambuf, DLINK_DBG_PREFIX);
 strlcpy(nambuf+sizeof(DLINK_DBG_PREFIX)-1, file,
             sizeof(nambuf)-sizeof(DLINK_DBG_PREFIX));
#ifdef NOBALLOC
 lp = _MyMalloc(sizeof(*lp), nambuf, line);
#else
 lp = memlog(_BlockHeapAlloc(dnode_heap), sizeof(*lp), nambuf, line);
#endif
 lp->next = NULL;
 lp->prev = NULL;
 ++links_count;
 return lp;
}
#endif

/*
 * _free_dlink_node
 *
 * inputs	- pointer to dlink_node
 * output	- NONE
 * side effects	- free given dlink_node 
 */
void _free_dlink_node(dlink_node *ptr)
{
#ifdef NOBALLOC
  MyFree(ptr);
#else
  BlockHeapFree(dnode_heap, ptr);
#endif
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




