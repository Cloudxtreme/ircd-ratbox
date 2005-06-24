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
#include "blalloc.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "list.h"
#include "mtrie_conf.h"
#include "numeric.h"
#include "res.h"
#include "restart.h"
#include "s_log.h"
#include "send.h"
#include "memory.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/*
 * re-written to use Wohali (joant@cadence.com)
 * block allocator routines. very nicely done Wohali
 */

#define LINK_PREALLOCATE 1024
#define USERS_PREALLOCATE 1024

void    outofmemory();

/* for Wohali's block allocator */
BlockHeap *free_dlink_nodes;
BlockHeap *free_anUsers;

void initlists()
{
  init_client_heap();
  free_dlink_nodes =
    BlockHeapCreate((size_t)sizeof(dlink_node),LINK_PREALLOCATE);

  /* struct User structs are used by both local struct Clients, and remote struct Clients */

  free_anUsers = BlockHeapCreate(sizeof(struct User),
                                 USERS_PREALLOCATE + MAXCONNECTIONS);
}

/*
 * outofmemory()
 *
 * input        - NONE
 * output       - NONE
 * side effects - simply try to report there is a problem
 *                I free all the memory in the kline lists
 *                hoping to free enough memory so that a proper
 *                report can be made. If I was already here (was_here)
 *                then I got called twice, and more drastic measures
 *                are in order. I'll try to just abort() at least.
 */
void outofmemory()
{
  static int was_here = 0;

  if (was_here)
    abort();

  was_here = YES;
  clear_mtrie_conf_links();

  log(L_CRIT, "Out of memory: restarting server...");
  restart("Out of Memory");
}

        
/*
 * make_user
 *
 * inputs	- pointer to client struct
 * output	- pointer to struct User
 * side effects - add's an User information block to a client
 *                if it was not previously allocated.
 */
struct User* make_user(struct Client *cptr)
{
  struct User        *user;

  user = cptr->user;
  if (!user)
    {
      user = BlockHeapALLOC(free_anUsers,struct User);
      if( user == (struct User *)NULL)
        outofmemory();
      user->away = NULL;
      user->server = (char *)NULL;      /* scache server name */
      user->refcnt = 1;
      user->joined = 0;
      user->channel.head = NULL;
      user->channel.tail = NULL;
      user->invited.head = NULL;
      user->invited.tail = NULL;
	  user->id[0] = '\0';
      cptr->user = user;
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
struct Server *make_server(struct Client *cptr)
{
  struct Server* serv = cptr->serv;

  if (!serv)
    {
      serv = (struct Server *)MyMalloc(sizeof(struct Server));

      /* The commented out lines before are
       * for documentation purposes only
       * as they are zeroed by memset above
       */
      /*      serv->user = NULL; */
      /*      serv->users = NULL; */
      /*      serv->servers = NULL; */
      /*      *serv->by = '\0'; */
      /*      serv->up = (char *)NULL; */

      cptr->serv = serv;
    }
  return cptr->serv;
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
void _free_user(struct User* user, struct Client* cptr)
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
			   (unsigned long)cptr, cptr ? cptr->name : "<noname>",
			   cptr->username, cptr->host, (unsigned long)user,
			   (unsigned long)user->invited.head,
			   (unsigned long)user->channel.head, user->joined,
			   user->refcnt);
        assert(0);
      }

      if(BlockHeapFree(free_anUsers,user))
        {
          sendto_realops_flags(FLAGS_ALL,
	       "list.c couldn't BlockHeapFree(free_anUsers,user) user = %lX",
			       (unsigned long)user );
          sendto_realops_flags(FLAGS_ALL,
       "Please report to the hybrid team! ircd-hybrid@the-project.org");

#ifdef SYSLOG_BLOCK_ALLOCATOR 
          log(L_DEBUG,"list.c couldn't BlockHeapFree(free_anUsers,user) user = %lX", (long unsigned int) user);
#endif
        }


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

  lp = BlockHeapALLOC(free_dlink_nodes,dlink_node);
  if (lp == NULL)
    outofmemory();

  lp->next = NULL;
  lp->prev = NULL;

  return lp;
}

/*
 * _free_dlink_node
 *
 * inputs	- pointer to dlink_node
 * output	- NONE
 * side effects	- free given pointer, put back on block allocator
 *		  for dlink_node's
 */
void _free_dlink_node(dlink_node *ptr)
{
  if(BlockHeapFree(free_dlink_nodes,ptr))
    {
      sendto_realops_flags(FLAGS_ALL,
	   "list.c couldn't BlockHeapFree(free_dlink_nodes,ptr) ptr = %lX",
			   (unsigned long)ptr );
    }
}


/*
 * Attempt to free up some block memory
 *
 * block_garbage_collect
 *
 * inputs          - NONE
 * output          - NONE
 * side effects    - memory is possibly freed up
 */
void block_garbage_collect()
{
  BlockHeapGarbageCollect(free_dlink_nodes);
  BlockHeapGarbageCollect(free_anUsers);
  clean_client_heap();
}

/*
 * count_user_memory
 *
 * inputs	- pointer to user memory actually used
 *		- pointer to user memory allocated total in block allocator
 * output	- NONE
 * side effects	- NONE
 */
void count_user_memory(int *user_memory_used,
                       int *user_memory_allocated )
{
  BlockHeapCountMemory( free_anUsers,
                        user_memory_used,
                        user_memory_allocated);
}

/*
 * count_links_memory
 *
 * inputs	- pointer to dlinks memory actually used
 *		- pointer to dlinks memory allocated total in block allocator
 * output	- NONE
 * side effects	- NONE
 */
void count_links_memory(int *links_memory_used,
                       int *links_memory_allocated )
{
  BlockHeapCountMemory( free_dlink_nodes,
                        links_memory_used,
                        links_memory_allocated);
}




