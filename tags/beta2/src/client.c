/************************************************************************
 *   IRC - Internet Relay Chat, src/client.c
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
#include "client.h"
#include "class.h"
#include "blalloc.h"
#include "channel.h"
#include "common.h"
#include "dline_conf.h"
#include "event.h"
#include "fdlist.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "s_gline.h"
#include "numeric.h"
#include "packet.h"
#include "s_auth.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_misc.h"
#include "s_serv.h"
#include "send.h"
#include "whowas.h"
#include "s_debug.h"
#include "s_user.h"
#include "linebuf.h"
#include "hash.h"
#include "memory.h"

#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>


/* 
 * Number of struct Client structures to preallocate at a time
 * for Efnet 1024 is reasonable 
 * for smaller nets who knows?
 *
 * This means you call MyMalloc 30 some odd times,
 * rather than 30k times 
 */
#define CLIENTS_PREALLOCATE 1024

/* 
 * for Wohali's block allocator 
 */
BlockHeap*        ClientFreeList;
BlockHeap*        localUserFreeList;
static const char* const BH_FREE_ERROR_MESSAGE = \
        "client.c BlockHeapFree failed for cptr = %p";

static void check_pings_list(dlink_list *list);
static void check_unknowns_list(dlink_list *list);

static EVH check_pings;

/*
 * init_client_heap 
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	- initialize client free memory
 */
void init_client_heap(void)
{
  ClientFreeList =
    BlockHeapCreate((size_t) sizeof(struct Client), CLIENTS_PREALLOCATE);

  localUserFreeList = 
    BlockHeapCreate((size_t) sizeof(struct LocalUser), MAXCONNECTIONS);

  /*
   * start off the check ping event ..  -- adrian
   *
   * Every 30 seconds is plenty -- db
   */
  eventAdd("check_pings", check_pings, NULL, 30, 0);
}

/*
 * clean_client_heap
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	- 
 */
void clean_client_heap(void)
{
  BlockHeapGarbageCollect(ClientFreeList);
  BlockHeapGarbageCollect(localUserFreeList);
}

/*
 * make_client - create a new Client struct and set it to initial state.
 *
 *      from == NULL,   create local client (a client connected
 *                      to a socket).
 *
 *      from,   create remote client (behind a socket
 *                      associated with the client defined by
 *                      'from'). ('from' is a local client!!).
 */
struct Client* make_client(struct Client* from)
{
  struct Client* cptr = NULL;
  struct LocalUser *localClient;
  dlink_node *m;

  if (!from)
    {
      cptr = BlockHeapALLOC(ClientFreeList, struct Client);
      if (cptr == NULL)
        outofmemory();
      assert(0 != cptr);

      memset(cptr, 0, sizeof(struct Client));

      cptr->from  = cptr; /* 'from' of local client is self! */
      cptr->since = cptr->lasttime = cptr->firsttime = CurrentTime;

      localClient = BlockHeapALLOC(localUserFreeList, struct LocalUser);
      memset(localClient, 0, sizeof(struct LocalUser));

      if (localClient == NULL)
        outofmemory();
      assert(0 != localClient);
      cptr->localClient = localClient;

      /* as good a place as any... */
      m = make_dlink_node();
      dlinkAdd(cptr, m, &unknown_list);
    }
  else
    { /* from is not NULL */
      cptr = BlockHeapALLOC(ClientFreeList, struct Client);
      if(cptr == NULL)
        outofmemory();
      assert(0 != cptr);

      memset(cptr, 0, sizeof(struct Client));

      cptr->from = from; /* 'from' of local client is self! */
    }

  cptr->status = STAT_UNKNOWN;
  cptr->fd = -1;
  strcpy(cptr->username, "unknown");

#if 0
  cptr->next    = NULL;
  cptr->prev    = NULL;
  cptr->hnext   = NULL;
  cptr->lnext   = NULL;
  cptr->lprev   = NULL;
  cptr->user    = NULL;
  cptr->serv    = NULL;
  cptr->servptr = NULL;
  cptr->whowas  = NULL;
#endif

  return cptr;
}

void _free_client(struct Client* cptr)
{
  int result = 0;
  assert(0 != cptr);
  assert(&me != cptr);
  assert(0 == cptr->prev);
  assert(0 == cptr->next);

  /* If localClient is non NULL, its a local client */
  if (cptr->localClient)
    {
      if (-1 < cptr->fd)
	fd_close(cptr->fd);


/*      if (cptr->localClient->dns_reply)
	--cptr->localClient->dns_reply->ref_count; */

#ifndef NDEBUG
      mem_frob(cptr->localClient, sizeof(struct LocalUser));
#endif
      if(BlockHeapFree(localUserFreeList, cptr->localClient))
        result = 1;

#ifndef NDEBUG
      mem_frob(cptr, sizeof(struct Client));
#endif
      if(BlockHeapFree(ClientFreeList, cptr))
        result = 1;
    }
  else {
#ifndef NDEBUG
    mem_frob(cptr, sizeof(struct Client));
#endif
    if(BlockHeapFree(ClientFreeList, cptr))
      result = 1;
  }

  assert(0 == result);
  if (result)
    {
      sendto_realops_flags(FLAGS_ALL,
			   BH_FREE_ERROR_MESSAGE, cptr);
      sendto_realops_flags(FLAGS_ALL,
			   "Please report to the hybrid team!" \
			   "ircd-hybrid@the-project.org");

      log(L_WARN, BH_FREE_ERROR_MESSAGE, cptr);
    }
}

/*
 * check_pings - go through the local client list and check activity
 * kill off stuff that should die
 *
 * inputs       - NOT USED (from event)
 * output       - next time_t when check_pings() should be called again
 * side effects - 
 *
 *
 * A PING can be sent to clients as necessary.
 *
 * Client/Server ping outs are handled.
 */

/*
 * Addon from adrian. We used to call this after nextping seconds,
 * however I've changed it to run once a second. This is only for
 * PING timeouts, not K/etc-line checks (thanks dianora!). Having it
 * run once a second makes life a lot easier - when a new client connects
 * and they need a ping in 4 seconds, if nextping was set to 20 seconds
 * we end up waiting 20 seconds. This is stupid. :-)
 * I will optimise (hah!) check_pings() once I've finished working on
 * tidying up other network IO evilnesses.
 *     -- adrian
 */

static void
check_pings(void *notused)
{               
  check_pings_list(&lclient_list);
  check_pings_list(&serv_list);
  check_unknowns_list(&unknown_list);

  /* Reschedule a new address */
  eventAdd("check_pings", check_pings, NULL, 30, 0);
}

/*
 * check_pings_list()
 *
 * inputs	- pointer to list to check
 * output	- NONE
 * side effects	- 
 */
static void
check_pings_list(dlink_list *list)
{
  char         scratch[32];	/* way too generous but... */
  struct Client *cptr;          /* current local cptr being examined */
  int           ping = 0;       /* ping time value from client */
#if 0
  time_t        timeout;        /* found necessary ping time */
#endif
  char          *reason;
  dlink_node    *ptr, *next_ptr;

  for (ptr = list->head; ptr; ptr = next_ptr)
    {
      next_ptr = ptr->next;
      cptr = ptr->data;

      /*
      ** Note: No need to notify opers here. It's
      ** already done when "FLAGS_DEADSOCKET" is set.
      */
      if (cptr->flags & FLAGS_DEADSOCKET)
        {
          reason = ((cptr->flags & FLAGS_SENDQEX) ?
		    "SendQ exceeded" : "Dead socket");

	  (void)exit_client(cptr, cptr, &me, reason );
          continue; 
        }
      
      if (IsPerson(cptr))
        {
          if( !IsElined(cptr) &&
              GlobalSetOptions.idletime && 
              !IsOper(cptr) &&
              !IsIdlelined(cptr) && 
              ((CurrentTime - cptr->user->last) > GlobalSetOptions.idletime))
            {
              struct ConfItem *aconf;

              aconf = make_conf();
              aconf->status = CONF_KILL;

              DupString(aconf->host, cptr->host);
              DupString(aconf->passwd, "idle exceeder" );
              DupString(aconf->name, cptr->username);
              aconf->port = 0;
              aconf->hold = CurrentTime + 60;
              add_temp_kline(aconf);
              sendto_realops_flags(FLAGS_ALL,
			   "Idle time limit exceeded for %s - temp k-lining",
				   get_client_name(cptr, HIDE_IP));


	      (void)exit_client(cptr, cptr, &me, aconf->passwd);
              continue;
            }
        }

      if (!IsRegistered(cptr))
        ping = CONNECTTIMEOUT;
      else
        ping = get_client_ping(cptr);

      if (ping < (CurrentTime - cptr->lasttime))
        {
          /*
           * If the client/server hasnt talked to us in 2*ping seconds
           * and it has a ping time, then close its connection.
           */
          if (((CurrentTime - cptr->lasttime) >= (2 * ping) &&
               (cptr->flags & FLAGS_PINGSENT)))
            {
              if (IsServer(cptr) || IsConnecting(cptr) ||
                  IsHandshake(cptr))
                {
                  sendto_realops_flags(FLAGS_ADMIN,
				       "No response from %s, closing link",
				       get_client_name(cptr, HIDE_IP));
                  sendto_realops_flags(FLAGS_NOTADMIN,
                                       "No response from %s, closing link",
                                       get_client_name(cptr, MASK_IP));
                  log(L_NOTICE, "No response from %s, closing link",
                      get_client_name(cptr, HIDE_IP));
                }
	      (void)ircsprintf(scratch,
			       "Ping timeout: %d seconds",
			       (int)(CurrentTime - cptr->lasttime));
	      
	      (void)exit_client(cptr, cptr, &me, scratch);
              continue;
            }
          else if ((cptr->flags & FLAGS_PINGSENT) == 0)
            {
              /*
               * if we havent PINGed the connection and we havent
               * heard from it in a while, PING it to make sure
               * it is still alive.
               */
              cptr->flags |= FLAGS_PINGSENT;
              /* not nice but does the job */
              cptr->lasttime = CurrentTime - ping;
              sendto_one(cptr, "PING :%s", me.name);
            }
        }
      /* ping_timeout: */

      /* bloat for now */
#if 0
      timeout = cptr->lasttime + ping;
      while (timeout <= CurrentTime)
        timeout += ping;
#endif
    }
}

/*
 * check_unknowns_list
 *
 * inputs	- pointer to list of unknown clients
 * output	- NONE
 * side effects	- unknown clients get marked for termination after n seconds
 */
static void
check_unknowns_list(dlink_list *list)
{
  dlink_node *ptr, *next_ptr;
  struct Client *cptr;

  for(ptr = list->head; ptr; ptr = next_ptr)
    {
      next_ptr = ptr->next;
      cptr = ptr->data;

      /*
       * Check UNKNOWN connections - if they have been in this state
       * for > 30s, close them.
       */

      if (cptr->firsttime ? ((CurrentTime - cptr->firsttime) > 30) : 0)
	{
	  (void)exit_client(cptr, cptr, &me, "Connection timed out");
	}
    }
}

/*
 * check_klines
 * inputs	- NONE
 * output	- NONE
 * side effects - Check all connections for a pending kline against the
 * 		  client, exit the client if a kline matches.
 */
void 
check_klines(void)
{               
  struct Client *cptr;          /* current local cptr being examined */
  struct ConfItem     *aconf = (struct ConfItem *)NULL;
  char          *reason;                /* pointer to reason string */
  dlink_node    *ptr, *next_ptr;

  for (ptr = lclient_list.head; ptr; ptr = next_ptr)
    {
      next_ptr = ptr->next;
      cptr = ptr->data;

      if(IsMe(cptr))
	continue;

      if ((aconf = match_Dline(&cptr->localClient->ip)))
	/* if there is a returned struct ConfItem then kill it */
	{
	  if(IsConfElined(aconf))
	    {
	      sendto_realops_flags(FLAGS_ALL,
			   "DLINE over-ruled for %s, client is kline_exempt",
				   get_client_name(cptr, HIDE_IP));
	      continue;
	    }
	  sendto_realops_flags(FLAGS_ALL,"DLINE active for %s",
			 get_client_name(cptr, HIDE_IP));
	      
	  if(ConfigFileEntry.kline_with_connection_closed)
	    reason = "Connection closed";
	  else
	    {
	      if (ConfigFileEntry.kline_with_reason && aconf->passwd)
		reason = aconf->passwd;
	      else
		reason = "D-lined";
	    }

	  if (IsPerson(cptr)) 
            sendto_one(cptr, form_str(ERR_YOUREBANNEDCREEP),
                       me.name, cptr->name, reason);
          else
            sendto_one(cptr, "NOTICE DLINE :*** You have been D-lined");

	  (void)exit_client(cptr, cptr, &me, reason );

	  continue; /* and go examine next fd/cptr */
	}

      if(IsPerson(cptr))
	{
	  if( ConfigFileEntry.glines && (aconf = find_gkill(cptr, cptr->username)) )
	    {
	      if(IsElined(cptr))
		{
		  sendto_realops_flags(FLAGS_ALL,
		       "GLINE over-ruled for %s, client is kline_exempt",
				       get_client_name(cptr, HIDE_IP));
		  continue;
		}
	      
	      if(IsExemptGline(cptr))
		{
		  sendto_realops_flags(FLAGS_ALL,
		       "GLINE over-ruled for %s, client is gline_exempt",
				       get_client_name(cptr, HIDE_IP));
		  continue;
		}

	      sendto_realops_flags(FLAGS_ALL,
				   "GLINE active for %s",
				   get_client_name(cptr, HIDE_IP));
		  
	      if (ConfigFileEntry.kline_with_connection_closed)
		{
		  /*
		   * We use a generic non-descript message here on 
		   * purpose so as to prevent other users seeing the
		   * client disconnect from harassing the IRCops
		   */
		  reason = "Connection closed";
		}
	      else
		{
		  if (ConfigFileEntry.kline_with_reason && aconf->passwd)
		    reason = aconf->passwd;
		  else
		    reason = "G-lined";
		}

	      sendto_one(cptr, form_str(ERR_YOUREBANNEDCREEP),
			 me.name, cptr->name, reason);

	      (void)exit_client(cptr, cptr, &me, reason);

	      continue;         /* and go examine next fd/cptr */
	    }
	  else
	    if((aconf = find_kill(cptr))) /* if there is a returned
					     struct ConfItem.. then kill it */
	      {
		if(aconf->status & CONF_ELINE)
		  {
		    sendto_realops_flags(FLAGS_ALL,
			 "KLINE over-ruled for %s, client is kline_exmpt",
					 get_client_name(cptr, HIDE_IP));
		    continue;
		  }
		    
		sendto_realops_flags(FLAGS_ALL,
				     "KLINE active for %s",
				     get_client_name(cptr, HIDE_IP));

		if (ConfigFileEntry.kline_with_connection_closed)
		  reason = "Connection closed";
		else
		  {
		    if (ConfigFileEntry.kline_with_reason && aconf->passwd)
		      reason = aconf->passwd;
		    else
		      reason = "K-lined";
		  }

		sendto_one(cptr, form_str(ERR_YOUREBANNEDCREEP),
			   me.name, cptr->name, reason);
		(void)exit_client(cptr, cptr, &me, reason);
		continue; 
	      }
	}
    }
}

/*
 * update_client_exit_stats
 *
 * input	- pointer to client
 * output	- NONE
 * side effects	- 
 */
static void update_client_exit_stats(struct Client* cptr)
{
  if (IsServer(cptr))
    {
      --Count.server;
      sendto_realops_flags(FLAGS_EXTERNAL, "Server %s split from %s",
                           cptr->name, cptr->servptr->name);
    }
  else if (IsClient(cptr))
    {
      --Count.total;
      if (IsOper(cptr))
	--Count.oper;
      if (IsInvisible(cptr)) 
	--Count.invisi;
    }
}

/*
 * release_client_state
 *
 * input	- pointer to client to release
 * output	- NONE
 * side effects	- 
 */
static void
release_client_state(struct Client* cptr)
{
  if (cptr->user)
    {
      if (IsPerson(cptr))
	{
	  add_history(cptr,0);
	  off_history(cptr);
	}
      free_user(cptr->user, cptr); /* try this here */
    }
  if (cptr->serv)
    {
      if (cptr->serv->user)
        free_user(cptr->serv->user, cptr);
      MyFree((char*) cptr->serv);
    }
}

/*
 * remove_client_from_list
 * inputs	- point to client to remove
 * output	- NONE
 * side effects - taken the code from ExitOneClient() for this
 *		  and placed it here. - avalon
 */
void
remove_client_from_list(struct Client* cptr)
{
  assert(0 != cptr);
  
  /* A client made with make_client()
   * is on the unknown_list until removed.
   * If it =does= happen to exit before its removed from that list
   * and its =not= on the GlobalClientList, it will core here.
   * short circuit that case now -db
   */
  if (!cptr->prev && !cptr->next)
    {
      return;
    }

  if (cptr->prev)
    cptr->prev->next = cptr->next;
  else
    {
      GlobalClientList = cptr->next;
      GlobalClientList->prev = NULL;
    }

  if (cptr->next)
    cptr->next->prev = cptr->prev;
  cptr->next = cptr->prev = NULL;

  update_client_exit_stats(cptr);
}

/*
 * add_client_to_list
 * input	- pointer to client
 * output	- NONE
 * side effects	- although only a small routine,
 *		  it appears in a number of places
 * 		  as a collection of a few lines...functions like this
 *		  should be in this file, shouldnt they ?  after all,
 *		  this is list.c, isnt it ? (no
 *		  -avalon
 */
void
add_client_to_list(struct Client *cptr)
{
  /*
   * since we always insert new clients to the top of the list,
   * this should mean the "me" is the bottom most item in the list.
   */
  cptr->next = GlobalClientList;
  GlobalClientList = cptr;
  if (cptr->next)
    cptr->next->prev = cptr;
  return;
}

/* Functions taken from +CSr31, paranoified to check that the client
** isn't on a llist already when adding, and is there when removing -orabidoo
*/
void add_client_to_llist(struct Client **bucket, struct Client *client)
{
  if (!client->lprev && !client->lnext)
    {
      client->lprev = NULL;
      if ((client->lnext = *bucket) != NULL)
        client->lnext->lprev = client;
      *bucket = client;
    }
}

void
del_client_from_llist(struct Client **bucket, struct Client *client)
{
  if (client->lprev)
    {
      client->lprev->lnext = client->lnext;
    }
  else if (*bucket == client)
    {
      *bucket = client->lnext;
    }
  if (client->lnext)
    {
      client->lnext->lprev = client->lprev;
    }
  client->lnext = client->lprev = NULL;
}

/*
 *  find_client - find a client (server or user) by name.
 *
 *  *Note*
 *      Semantics of this function has been changed from
 *      the old. 'name' is now assumed to be a null terminated
 *      string and the search is the for server and user.
 */
struct Client* find_client(const char* name, struct Client *cptr)
{
  if (name)
    cptr = hash_find_client(name, cptr);

  return cptr;
}

/*
 *  find_userhost - find a user@host (server or user).
 *
 *  *Note*
 *      Semantics of this function has been changed from
 *      the old. 'name' is now assumed to be a null terminated
 *      string and the search is the for server and user.
 */
struct Client *find_userhost(char *user, char *host,
			     struct Client *cptr, int *count)
{
  struct Client       *c2ptr;
  struct Client       *res = cptr;

  *count = 0;
  if (collapse(user))
    for (c2ptr = GlobalClientList; c2ptr; c2ptr = c2ptr->next) 
      {
        if (!MyClient(c2ptr)) /* implies mine and a user */
          continue;
        if ((!host || match(host, c2ptr->host)) &&
            irccmp(user, c2ptr->username) == 0)
          {
            (*count)++;
            res = c2ptr;
          }
      }
  return res;
}

/*
 *  find_server - find server by name.
 *
 *      This implementation assumes that server and user names
 *      are unique, no user can have a server name and vice versa.
 *      One should maintain separate lists for users and servers,
 *      if this restriction is removed.
 *
 *  *Note*
 *      Semantics of this function has been changed from
 *      the old. 'name' is now assumed to be a null terminated
 *      string.
 */
struct Client* find_server(const char* name)
{
  if (name)
    return hash_find_server(name);
  return 0;
}

/*
 * next_client - find the next matching client. 
 * The search can be continued from the specified client entry. 
 * Normal usage loop is:
 *
 *      for (x = client; x = next_client(x,mask); x = x->next)
 *              HandleMatchingClient;
 *            
 */
struct Client*
next_client(struct Client *next,     /* First client to check */
            const char* ch)          /* search string (may include wilds) */
{
  struct Client *tmp = next;

  next = find_client(ch, tmp);
  if (tmp && tmp->prev == next)
    return ((struct Client *) NULL);

  if (next != tmp)
    return next;
  for ( ; next; next = next->next)
    {
      if (match(ch,next->name)) break;
    }
  return next;
}


/* 
 * this slow version needs to be used for hostmasks *sigh
 *
 * next_client_double - find the next matching client. 
 * The search can be continued from the specified client entry. 
 * Normal usage loop is:
 *
 *      for (x = client; x = next_client(x,mask); x = x->next)
 *              HandleMatchingClient;
 *            
 */
struct Client* 
next_client_double(struct Client *next, /* First client to check */
                   const char* ch)      /* search string (may include wilds) */
{
  struct Client *tmp = next;

  next = find_client(ch, tmp);
  if (tmp && tmp->prev == next)
    return NULL;
  if (next != tmp)
    return next;
  for ( ; next; next = next->next)
    {
      if (match(ch,next->name) || match(next->name,ch))
        break;
    }
  return next;
}

/*
 * find_person - find person by (nick)name.
 * inputs	- pointer to name
 *		- pointer to client
 * output	- return client pointer
 * side effects -
 */
struct Client *find_person(char *name, struct Client *cptr)
{
  struct Client       *c2ptr = cptr;

  c2ptr = find_client(name, c2ptr);

  if (c2ptr && IsClient(c2ptr) && c2ptr->user)
    return c2ptr;
  return cptr;
}

/*
 * find_chasing - find the client structure for a nick name (user) 
 *      using history mechanism if necessary. If the client is not found, 
 *      an error message (NO SUCH NICK) is generated. If the client was found
 *      through the history, chasing will be 1 and otherwise 0.
 */
struct Client *find_chasing(struct Client *sptr, char *user, int *chasing)
{
  struct Client *who = find_client(user, (struct Client *)NULL);
  
  if (chasing)
    *chasing = 0;
  if (who)
    return who;
  if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
    {
      sendto_one(sptr, form_str(ERR_NOSUCHNICK),
                 me.name, sptr->name, user);
      return ((struct Client *)NULL);
    }
  if (chasing)
    *chasing = 1;
  return who;
}



/*
 * check_registered_user - is used to cancel message, if the
 * originator is a server or not registered yet. In other
 * words, passing this test, *MUST* guarantee that the
 * sptr->user exists (not checked after this--let there
 * be coredumps to catch bugs... this is intentional --msa ;)
 *
 * There is this nagging feeling... should this NOT_REGISTERED
 * error really be sent to remote users? This happening means
 * that remote servers have this user registered, although this
 * one has it not... Not really users fault... Perhaps this
 * error message should be restricted to local clients and some
 * other thing generated for remotes...
 */
int check_registered_user(struct Client* client)
{
  if (!IsRegisteredUser(client))
    {
      sendto_one(client, form_str(ERR_NOTREGISTERED), me.name, "*");
      return -1;
    }
  return 0;
}

/*
 * check_registered user cancels message, if 'x' is not
 * registered (e.g. we don't know yet whether a server
 * or user)
 */
int check_registered(struct Client* client)
{
  if (!IsRegistered(client))
    {
      sendto_one(client, form_str(ERR_NOTREGISTERED), me.name, "*");
      return -1;
    }
  return 0;
}

/*
 * release_client_dns_reply - remove client dns_reply references
 *
 */
#if 0
void release_client_dns_reply(struct Client* client)
{
  assert(0 != client);
  if (client->localClient->dns_reply)
    {
      --client->localClient->dns_reply->ref_count;
      client->localClient->dns_reply = 0;
    }
}
#endif
/*
 * get_client_name -  Return the name of the client
 *    for various tracking and
 *      admin purposes. The main purpose of this function is to
 *      return the "socket host" name of the client, if that
 *        differs from the advertised name (other than case).
 *        But, this can be used to any client structure.
 *
 * NOTE 1:
 *        Watch out the allocation of "nbuf", if either sptr->name
 *        or sptr->sockhost gets changed into pointers instead of
 *        directly allocated within the structure...
 *
 * NOTE 2:
 *        Function return either a pointer to the structure (sptr) or
 *        to internal buffer (nbuf). *NEVER* use the returned pointer
 *        to modify what it points!!!
 */

const char* get_client_name(struct Client* client, int showip)
{
  static char nbuf[HOSTLEN * 2 + USERLEN + 5];

  assert(0 != client);

  if (MyConnect(client))
    {
      if (!irccmp(client->name, client->host))
        return client->name;

      /* And finally, let's get the host information, ip or name */
      switch (showip)
        {
          case SHOW_IP:
            ircsprintf(nbuf, "%s[%s@%s]", client->name, client->username,
              client->localClient->sockhost);
            break;
          case MASK_IP:
            ircsprintf(nbuf, "%s[%s@255.255.255.255]", client->name,
              client->username);
            break;
          default:
            ircsprintf(nbuf, "%s[%s@%s]", client->name, client->username,
              client->host);
        }
      return nbuf;
    }

  /* As pointed out by Adel Mezibra 
   * Neph|l|m@EFnet. Was missing a return here.
   */
  return client->name;
}

/*
 * get_client_host
 *
 * inputs	- pointer to client struct
 * output	- pointer to static char string with client hostname
 * side effects	-
 */
const char* get_client_host(struct Client* client)
{
  assert(0 != client);
  return get_client_name(client, HIDE_IP);
#if 0
  static char nbuf[HOSTLEN * 2 + USERLEN + 5];
  
  assert(0 != client);

  if (!MyConnect(client))
    return client->name;
  if (client->localClient->dns_query->answer.status != adns_s_ok)
    return get_client_name(client, HIDE_IP);
  else
    {
      ircsprintf(nbuf, "%s[%-.*s@%-.*s]",
                 client->name, USERLEN, client->username,
                 HOSTLEN, client->host);
    }
  return nbuf;
#endif
}

void free_exited_clients( void )
{
  dlink_node *ptr, *next;
  struct Client *acptr;
  
  for(ptr = dead_list.head; ptr; ptr = next)
  {
    acptr = ptr->data;
    next = ptr->next;
    if (ptr->data == NULL)
    {
      sendto_realops_flags(FLAGS_ALL,
                           "Warning: null client on dead_list!");
      dlinkDelete(ptr, &dead_list);
      free_dlink_node(ptr);
      continue;
    }

    release_client_state(acptr);
    free_client(acptr);
    dlinkDelete(ptr, &dead_list);
    free_dlink_node(ptr);
  }
}

/*
** Exit one client, local or remote. Assuming all dependents have
** been already removed, and socket closed for local client.
*/
static void exit_one_client(struct Client *cptr, struct 
			    Client *sptr, struct Client *from,
                            const char* comment)
{
  struct Client* acptr;
  dlink_node *lp;
  dlink_node *next_lp;

  if (IsServer(sptr))
    {
      if (sptr->servptr && sptr->servptr->serv)
        del_client_from_llist(&(sptr->servptr->serv->servers),
                                    sptr);
      else
        ts_warn("server %s without servptr!", sptr->name);
    }
  else if (sptr->servptr && sptr->servptr->serv)
      del_client_from_llist(&(sptr->servptr->serv->users), sptr);
  /* there are clients w/o a servptr: unregistered ones */

  /*
  **  For a server or user quitting, propogate the information to
  **  other servers (except to the one where is came from (cptr))
  */
  if (IsMe(sptr))
    {
      sendto_realops_flags(FLAGS_ALL,
			   "ERROR: tried to exit me! : %s", comment);
      return;        /* ...must *never* exit self!! */
    }
  else if (IsServer(sptr))
    {
      /*
      ** Old sendto_serv_but_one() call removed because we now
      ** need to send different names to different servers
      ** (domain name matching)
      */
      /*
      ** The bulk of this is done in remove_dependents now, all
      ** we have left to do is send the SQUIT upstream.  -orabidoo
      */
      acptr = sptr->from;
      if (acptr && IsServer(acptr) && acptr != cptr && !IsMe(acptr) &&
          (sptr->flags & FLAGS_KILLED) == 0)
        sendto_one(acptr, ":%s SQUIT %s :%s", from->name, sptr->name, comment);
    }
  else if (!(IsPerson(sptr)))
      /* ...this test is *dubious*, would need
      ** some thought.. but for now it plugs a
      ** nasty hole in the server... --msa
      */
      ; /* Nothing */
  else if (sptr->name[0]) /* ...just clean all others with QUIT... */
    {
      /*
      ** If this exit is generated from "m_kill", then there
      ** is no sense in sending the QUIT--KILL's have been
      ** sent instead.
      */
      if ((sptr->flags & FLAGS_KILLED) == 0)
        {
          sendto_ll_serv_butone(cptr,sptr,0,":%s QUIT :%s",
                             sptr->name, comment);
        }
      /*
      ** If a person is on a channel, send a QUIT notice
      ** to every client (person) on the same channel (so
      ** that the client can show the "**signoff" message).
      ** (Note: The notice is to the local clients *only*)
      */
      if (sptr->user)
        {
          sendto_common_channels_local(sptr, ":%s!%s@%s QUIT :%s",
				       sptr->name,
				       sptr->username,
				       sptr->host,
				       comment);

          for (lp = sptr->user->channel.head; lp; lp = next_lp)
	    {
	      next_lp = lp->next;
	      remove_user_from_channel(lp->data,sptr);
	    }
          
          /* Clean up invitefield */
          for (lp = sptr->user->invited.head; lp; lp = next_lp)
           {
              next_lp = lp->next;
              del_invite(lp->data, sptr);
           }

          /* Clean up allow lists */
          del_all_accepts(sptr);

	  if (HasID(sptr))
	    del_from_id_hash_table(sptr->user->id, sptr);
  
          /* again, this is all that is needed */
        }
    }
  
  /* 
   * Remove sptr from the client lists
   */
  del_from_client_hash_table(sptr->name, sptr);

  /* remove from global client list */
  remove_client_from_list(sptr);

  SetDead(sptr);
  /* add to dead client dlist */
  lp = make_dlink_node();
  dlinkAdd(sptr, lp, &dead_list);
}

/*
** Recursively send QUITs and SQUITs for sptr and all its dependent clients
** and servers to those servers that need them.  A server needs the client
** QUITs if it can't figure them out from the SQUIT (ie pre-TS4) or if it
** isn't getting the SQUIT because of @#(*&@)# hostmasking.  With TS4, once
** a link gets a SQUIT, it doesn't need any QUIT/SQUITs for clients depending
** on that one -orabidoo
*/
static void recurse_send_quits(struct Client *cptr, struct Client *sptr, struct Client *to,
                                const char* comment,  /* for servers */
                                const char* myname)
{
  struct Client *acptr;

  /* If this server can handle quit storm (QS) removal
   * of dependents, just send the SQUIT
   */

  if (IsCapable(to,CAP_QS))
    {
      if (match(myname, sptr->name))
        {
          for (acptr = sptr->serv->users; acptr; acptr = acptr->lnext)
            sendto_one(to, ":%s QUIT :%s", acptr->name, comment);
          for (acptr = sptr->serv->servers; acptr; acptr = acptr->lnext)
            recurse_send_quits(cptr, acptr, to, comment, myname);
        }
      else
        sendto_one(to, "SQUIT %s :%s", sptr->name, me.name);
    }
  else
    {
      for (acptr = sptr->serv->users; acptr; acptr = acptr->lnext)
        sendto_one(to, ":%s QUIT :%s", acptr->name, comment);
      for (acptr = sptr->serv->servers; acptr; acptr = acptr->lnext)
        recurse_send_quits(cptr, acptr, to, comment, myname);
      if (!match(myname, sptr->name))
        sendto_one(to, "SQUIT %s :%s", sptr->name, me.name);
    }
}

/* 
** Remove all clients that depend on sptr; assumes all (S)QUITs have
** already been sent.  we make sure to exit a server's dependent clients 
** and servers before the server itself; exit_one_client takes care of 
** actually removing things off llists.   tweaked from +CSr31  -orabidoo
*/
/*
 * added sanity test code.... sptr->serv might be NULL...
 */
static void recurse_remove_clients(struct Client* sptr, const char* comment)
{
  struct Client *acptr;

  if (IsMe(sptr))
    return;

  if (!sptr->serv)        /* oooops. uh this is actually a major bug */
    return;

  while ( (acptr = sptr->serv->servers) )
    {
      recurse_remove_clients(acptr, comment);
      /*
      ** a server marked as "KILLED" won't send a SQUIT 
      ** in exit_one_client()   -orabidoo
      */
      acptr->flags |= FLAGS_KILLED;
      exit_one_client(NULL, acptr, &me, me.name);
    }

  while ( (acptr = sptr->serv->users) )
    {
      acptr->flags |= FLAGS_KILLED;
      exit_one_client(NULL, acptr, &me, comment);
    }
}

/*
** Remove *everything* that depends on sptr, from all lists, and sending
** all necessary QUITs and SQUITs.  sptr itself is still on the lists,
** and its SQUITs have been sent except for the upstream one  -orabidoo
*/
static void remove_dependents(struct Client* cptr, 
                               struct Client* sptr,
                               struct Client* from,
                               const char* comment,
                               const char* comment1)
{
  struct Client *to;
  struct ConfItem *aconf;
  static char myname[HOSTLEN+1];
  dlink_node *ptr;

  for(ptr = serv_list.head; ptr; ptr=ptr->next)
    {
      to = ptr->data;

      if (IsMe(to) ||to == sptr->from || (to == cptr && IsCapable(to,CAP_QS)))
        continue;

      /* MyConnect(sptr) is rotten at this point: if sptr
       * was mine, ->from is NULL. 
       */
      /* The WALLOPS isn't needed here as pointed out by
       * comstud, since m_squit already does the notification.
       */

      if ((aconf = to->serv->sconf))
        strncpy_irc(myname, my_name_for_link(me.name, aconf), HOSTLEN);
      else
        strncpy_irc(myname, me.name, HOSTLEN);
      recurse_send_quits(cptr, sptr, to, comment1, myname);
    }

  recurse_remove_clients(sptr, comment1);
}


/*
** exit_client - This is old "m_bye". Name  changed, because this is not a
**        protocol function, but a general server utility function.
**
**        This function exits a client of *any* type (user, server, etc)
**        from this server. Also, this generates all necessary prototol
**        messages that this exit may cause.
**
**   1) If the client is a local client, then this implicitly
**        exits all other clients depending on this connection (e.g.
**        remote clients having 'from'-field that points to this.
**
**   2) If the client is a remote client, then only this is exited.
**
** For convenience, this function returns a suitable value for
** m_function return value:
**
**        CLIENT_EXITED        if (cptr == sptr)
**        0                if (cptr != sptr)
*/
int exit_client(
struct Client* cptr, /*
                     ** The local client originating the exit or NULL, if this
                     ** exit is generated by this server for internal reasons.
                     ** This will not get any of the generated messages.
                     */
struct Client* sptr,        /* Client exiting */
struct Client* from,        /* Client firing off this Exit, never NULL! */
const char* comment         /* Reason for the exit */
                   )
{
  struct Client        *acptr;
  struct Client        *next;
  char comment1[HOSTLEN + HOSTLEN + 2];
  dlink_node *m;

  /* sptr->flags |= FLAGS_DEADSOCKET; */

  if (MyConnect(sptr))
    {
      if (sptr->flags & FLAGS_IPHASH)
        remove_one_ip(&sptr->localClient->ip);
      
      delete_adns_queries(sptr->localClient->dns_query);
      delete_identd_queries(sptr);

      client_flush_input(sptr);

      /* This sptr could have status of one of STAT_UNKNOWN, STAT_CONNECTING
       * STAT_HANDSHAKE or STAT_UNKNOWN
       * all of which are lumped together into unknown_list
       *
       * In all above cases IsRegistered() will not be true.
       */
      if (!IsRegistered(sptr))
	{
	  m = dlinkFind(&unknown_list,sptr);
	  if( m != NULL )
	    {
	      dlinkDelete(m, &unknown_list);
	      free_dlink_node(m);
	    }
	}
      if (IsOper(sptr))
        {
	  m = dlinkFind(&oper_list,sptr);
	  if( m != NULL )
	    {
	      dlinkDelete(m, &oper_list);
	      free_dlink_node(m);
	    }
        }
      if (IsClient(sptr))
        {
          Count.local--;

          if(IsPerson(sptr))        /* a little extra paranoia */
            {
	      m = dlinkFind(&lclient_list,sptr);
	      if( m != NULL )
		{
		  dlinkDelete(m,&lclient_list);
		  free_dlink_node(m);
		}
            }
        }

      /* As soon as a client is known to be a server of some sort
       * it has to be put on the serv_list, or SJOIN's to this new server
       * from the connect burst will not be seen.
       */
      if (IsServer(sptr) || IsConnecting(sptr) || IsHandshake(sptr))
	{
	  m = dlinkFind(&serv_list,sptr);
	  if( m != NULL )
	    {
	      dlinkDelete(m,&serv_list);
	      free_dlink_node(m);
	    }
	}

      if (IsServer(sptr))
        {
          Count.myserver--;

	  if(ServerInfo.hub)
	    remove_lazylink_flags(sptr->localClient->serverMask);
	  else
	    uplink = NULL;
        }

      sptr->flags |= FLAGS_CLOSING;

      if (IsPerson(sptr))
        sendto_realops_flags(FLAGS_CCONN,
                             "Client exiting: %s (%s@%s) [%s] [%s]",
                             sptr->name, sptr->username, sptr->host,
                             comment, sptr->localClient->sockhost);

      log_user_exit(sptr);

      if (sptr->fd >= 0)
	{
	  if (cptr != NULL && sptr != cptr)
	    sendto_one(sptr, "ERROR :Closing Link: %s %s (%s)",
		       sptr->host, sptr->name, comment);
	  else
	    sendto_one(sptr, "ERROR :Closing Link: %s (%s)",
		       sptr->host, comment);
	}
      /*
      ** Currently only server connections can have
      ** depending remote clients here, but it does no
      ** harm to check for all local clients. In
      ** future some other clients than servers might
      ** have remotes too...
      **
      ** Close the Client connection first and mark it
      ** so that no messages are attempted to send to it.
      ** (The following *must* make MyConnect(sptr) == FALSE!).
      ** It also makes sptr->from == NULL, thus it's unnecessary
      ** to test whether "sptr != acptr" in the following loops.
      */
      
      close_connection(sptr);
    }

  if(IsServer(sptr))
    {        
      if( GlobalSetOptions.hide_server )
	{
          /* 
          ** Replaces the name of the splitting server with net.split
          ** when a client exits from a split, in an attempt to 
          ** hide topology without breaking too many clients..
          */
          strcpy(comment1, me.name);         
          strcat(comment1, " net.split");
	}
      else
	{
	  if((sptr->serv) && (sptr->serv->up))
	    strcpy(comment1, sptr->serv->up);
	  else
	    strcpy(comment1, "<Unknown>" );

	  strcat(comment1," ");
	  strcat(comment1, sptr->name);
	}

      remove_dependents(cptr, sptr, from, comment, comment1);

      if (sptr->servptr == &me)
        {
          sendto_realops_flags(FLAGS_ALL,
		       "%s was connected for %d seconds.  %d/%d sendK/recvK.",
			       sptr->name, (int)(CurrentTime - sptr->firsttime),
			       sptr->localClient->sendK,
			       sptr->localClient->receiveK);
          log(L_NOTICE, "%s was connected for %d seconds.  %d/%d sendK/recvK.",
              sptr->name, CurrentTime - sptr->firsttime, 
              sptr->localClient->sendK, sptr->localClient->receiveK);

              /* Just for paranoia... this shouldn't be necessary if the
              ** remove_dependents() stuff works, but it's still good
              ** to do it.    MyConnect(sptr) has been set to false,
              ** so we look at servptr, which should be ok  -orabidoo
              */
              for (acptr = GlobalClientList; acptr; acptr = next)
                {
                  next = acptr->next;
                  if (!IsServer(acptr) && acptr->from == sptr)
                    {
                      ts_warn("Dependent client %s not on llist!?",
                              acptr->name);
                      exit_one_client(NULL, acptr, &me, comment1);
                    }
                }
              /*
              ** Second SQUIT all servers behind this link
              */
              for (acptr = GlobalClientList; acptr; acptr = next)
                {
                  next = acptr->next;
                  if (IsServer(acptr) && acptr->from == sptr)
                    {
                      ts_warn("Dependent server %s not on llist!?", 
                                     acptr->name);
                      exit_one_client(NULL, acptr, &me, me.name);
                    }
                }
            }
        }

  exit_one_client(cptr, sptr, from, comment);
  return cptr == sptr ? CLIENT_EXITED : 0;
}

/*
 * Count up local client memory
 */

/* XXX one common Client list now */
void count_local_client_memory(int *local_client_memory_used,
                               int *local_client_memory_allocated )
{
  BlockHeapCountMemory( localUserFreeList,
                        local_client_memory_used,
                        local_client_memory_allocated);
}

/*
 * Count up remote client memory
 */
void count_remote_client_memory(int *remote_client_memory_used,
                               int *remote_client_memory_allocated )
{
  BlockHeapCountMemory( ClientFreeList,
                        remote_client_memory_used,
                        remote_client_memory_allocated);
}


/*
 * accept processing, this adds a form of "caller ID" to ircd
 * 
 * If a client puts themselves into "caller ID only" mode,
 * only clients that match a client pointer they have put on 
 * the accept list will be allowed to message them.
 *
 * [ source.on_allow_list ] -> [ target1 ] -> [ target2 ]
 *
 * [target.allow_list] -> [ source1 ] -> [source2 ]
 *
 * i.e. a target will have a link list of source pointers it will allow
 * each source client then has a back pointer pointing back
 * to the client that has it on its accept list.
 * This allows for exit_one_client to remove these now bogus entries
 * from any client having an accept on them. 
 */

/*
 * accept_message
 *
 * inputs	- pointer to source client
 * 		- pointer to target client
 * output	- 1 if accept this message 0 if not
 * side effects - See if source is on target's allow list
 */
int accept_message(struct Client *source, struct Client *target)
{
  dlink_node *ptr;
  struct Client *acptr;

  for(ptr = target->allow_list.head; ptr; ptr = ptr->next )
    {
      acptr = ptr->data;
      if(source == acptr)
	return 1;
    }
  return 0;
}

/*
 * add_to_accept
 *
 * inputs	- pointer to source client
 * 		- pointer to target client
 * output	- 
 * side effects - Add's source pointer to targets allow list
 */
void add_to_accept(struct Client *source, struct Client *target)
{
  dlink_node *m;
  int len;

  /* Safety checks, neither of these tests should happen */
  if (!IsPerson(source))
    return;

  if (!IsPerson(target))
    return;

  /* XXX MAX_ALLOW should be in config file not hard coded */
  if ( (len = dlink_list_length(&target->allow_list)) >= 
       MAX_ALLOW)
    {
      sendto_one(target,":%s NOTICE %s :Max accept targets reached %d",
		 me.name, target->name, len);
      return;
    }

  m = make_dlink_node();
  dlinkAdd(source, m, &target->allow_list);

  m = make_dlink_node();
  dlinkAdd(target, m, &source->on_allow_list);
}

/*
 * del_from_accept
 *
 * inputs	- pointer to source client
 * 		- pointer to target client
 * output	- NONE
 * side effects - Delete's source pointer to targets allow list
 *
 * Walk through the target's accept list, remove if source is found,
 * Then walk through the source's on_accept_list remove target if found.
 */
void del_from_accept(struct Client *source, struct Client *target)
{
  dlink_node *ptr;
  dlink_node *ptr2;
  dlink_node *next_ptr;
  dlink_node *next_ptr2;
  struct Client *acptr;

  for (ptr = target->allow_list.head; ptr; ptr = next_ptr)
    {
      next_ptr = ptr->next;

      acptr = ptr->data;
      if(source == acptr)
	{
	  dlinkDelete(ptr, &target->allow_list);
	  free_dlink_node(ptr);

	  for (ptr2 = source->on_allow_list.head; ptr2;
	       ptr2 = next_ptr2)
	    {
	      next_ptr2 = ptr2->next;

	      acptr = ptr2->data;
	      if (target == acptr)
		{
		  dlinkDelete(ptr2, &source->on_allow_list);
		  free_dlink_node(ptr2);
		}
	    }
	}
    }
}

/*
 * del_all_accepts
 *
 * inputs	- pointer to exiting client
 * output	- NONE
 * side effects - Walk through given clients allow_list and on_allow_list
 *                remove all references to this client
 */
void del_all_accepts(struct Client *cptr)
{
  dlink_node *ptr;
  dlink_node *next_ptr;
  struct Client *acptr;

  for (ptr = cptr->allow_list.head; ptr; ptr = next_ptr)
    {
      next_ptr = ptr->next;
      acptr = ptr->data;
      if(acptr != NULL)
        del_from_accept(acptr,cptr);
    }

  for (ptr = cptr->on_allow_list.head; ptr; ptr = next_ptr)
    {
      next_ptr = ptr->next;
      acptr = ptr->data;
      if(acptr != NULL)
	del_from_accept(cptr, acptr);
    }
}

/*
 * list_all_accepts
 *
 * inputs	- pointer to exiting client
 * output	- NONE
 * side effects - list allow list
 */
void list_all_accepts(struct Client *sptr)
{
  dlink_node *ptr;
  struct Client *acptr;
  char *nicks[8];
  int j=0;

  nicks[0] = nicks[1] = nicks[2] = nicks[3] = nicks[4] = nicks[5]
    = nicks[6] = nicks[7] = "";

  sendto_one(sptr,":%s NOTICE %s :*** Current accept list",
	     me.name, sptr->name);

  for (ptr = sptr->allow_list.head; ptr; ptr = ptr->next)
    {
      acptr = ptr->data;

      if(acptr != NULL)
	{
	  nicks[j++] = acptr->name;
	}

      if(j > 7)
	{
	  sendto_one(sptr,":%s NOTICE %s :%s %s %s %s %s %s %s %s",
		     me.name, sptr->name,
		     nicks[0], nicks[1], nicks[2], nicks[3],
		     nicks[4], nicks[5], nicks[6], nicks[7] );
	  j = 0;
	  nicks[0] = nicks[1] = nicks[2] = nicks[3] = nicks[4] = nicks[5]
	    = nicks[6] = nicks[7] = "";
	}
	
    }

  if(j)
    sendto_one(sptr,":%s NOTICE %s :%s %s %s %s %s %s %s %s",
	       me.name, sptr->name,
	       nicks[0], nicks[1], nicks[2], nicks[3],
	       nicks[4], nicks[5], nicks[6], nicks[7] );
}

/*
 * set_initial_nick
 * inputs
 * output
 * side effects	-
 *
 * This function is only called to set up an initially registering
 * client. 
 */
int
set_initial_nick(struct Client *cptr, struct Client *sptr,
                 char *nick)
{
  char buf[USERLEN + 1];
  char nickbuf[NICKLEN + 10];
  /* Client setting NICK the first time */

  /* This had to be copied here to avoid problems.. */
  strcpy(sptr->name, nick);
  sptr->tsinfo = CurrentTime;
  if (sptr->user)
    {
      strncpy_irc(buf, sptr->username, USERLEN);
      buf[USERLEN] = '\0';
      /*
      ** USER already received, now we have NICK.
      ** *NOTE* For servers "NICK" *must* precede the
      ** user message (giving USER before NICK is possible
      ** only for local client connection!). register_user
      ** may reject the client and call exit_client for it
      ** --must test this and exit m_nick too!!!
      */
#ifdef USE_IAUTH
      /*
       * Send the client to the iauth module for verification
       */
      BeginAuthorization(sptr);
#else
      if (register_local_user(cptr, sptr, nick, buf) == CLIENT_EXITED)
	return CLIENT_EXITED;
#endif
    }

  /*
  **  Finally set new nick name.
  */
  if (sptr->name[0])
    del_from_client_hash_table(sptr->name, sptr);
  strcpy(sptr->name, nick);
  add_to_client_hash_table(nick, sptr);

  /*
   * .. and update the new nick in the fd note.
   */

  strcpy(nickbuf, "Nick: ");
  /* nick better be the right length! -- adrian */
  strncat(nickbuf, nick, NICKLEN);
  fd_note(cptr->fd, nickbuf);

  /* They have the nick they want now.. */
  *cptr->llname = '\0';

  return 0;
}

/*
 * change_local_nick
 * inputs	- pointer to server
 *		- pointer to client
 * output	- 
 * side effects	- changes nick of a LOCAL user
 *
 */
int change_local_nick(struct Client *cptr, struct Client *sptr,
                      char *nick)
{
  char nickbuf[NICKLEN + 10];

  /*
  ** Client just changing his/her nick. If he/she is
  ** on a channel, send note of change to all clients
  ** on that channel. Propagate notice to other servers.
  */

  if( (sptr->localClient->last_nick_change +
       ConfigFileEntry.max_nick_time) < CurrentTime)
    sptr->localClient->number_of_nick_changes = 0;
  sptr->localClient->last_nick_change = CurrentTime;
  sptr->localClient->number_of_nick_changes++;

  if((ConfigFileEntry.anti_nick_flood && 
      (sptr->localClient->number_of_nick_changes
       <= ConfigFileEntry.max_nick_changes)) ||
     !ConfigFileEntry.anti_nick_flood)
    {
      sendto_realops_flags(FLAGS_NCHANGE,
			   "Nick change: From %s to %s [%s@%s]",
			   sptr->name, nick, sptr->username,
			   sptr->host);

      sendto_common_channels_local(sptr, ":%s!%s@%s NICK :%s",
				   sptr->name, sptr->username, sptr->host,
				   nick);
      if (sptr->user)
	{
	  add_history(sptr,1);
	  
	  /* Only hubs care about lazy link nicks not being sent on yet
	   * lazylink leafs/leafs always send their nicks up to hub,
	   * hence must always propogate nick changes.
	   * hubs might not propogate a nick change, if the leaf
	   * does not know about that client yet.
	   */
          sendto_ll_serv_butone(cptr, sptr, 0, ":%s NICK %s :%lu",
                                sptr->name, nick, sptr->tsinfo);
	}
    }
  else
    {
      sendto_one(sptr,
		 ":%s NOTICE %s :*** Notice -- Too many nick changes wait %d seconds before trying to change it again.",
		 me.name,
		 sptr->name,
		 ConfigFileEntry.max_nick_time);
      return 0;
    }

  /* Finally, add to hash */
  del_from_client_hash_table(sptr->name, sptr);
  strcpy(sptr->name, nick);
  add_to_client_hash_table(nick, sptr);

  /* Make sure everyone that has this client on its accept list
   * loses that reference. 
   */

  del_all_accepts(sptr);

  /*
   * .. and update the new nick in the fd note.
   */
  strcpy(nickbuf, "Nick: ");
  /* nick better be the right length! -- adrian */
  strncat(nickbuf, nick, NICKLEN);
  fd_note(cptr->fd, nickbuf);

  return 1;
}

/*
 * clean_nick_name - ensures that the given parameter (nick) is
 * really a proper string for a nickname (note, the 'nick'
 * may be modified in the process...)
 *
 *      RETURNS the length of the final NICKNAME (0, if
 *      nickname is illegal)
 *
 *  Nickname characters are in range
 *      'A'..'}', '_', '-', '0'..'9'
 *  anything outside the above set will terminate nickname.
 *  In addition, the first character cannot be '-'
 *  or a Digit.
 *
 *  Note:
 *      '~'-character should be NOT be allowed.
 */
int clean_nick_name(char* nick)
{
  char* ch   = nick;
  char* endp = ch + NICKLEN;
  assert(0 != nick);

  if (*nick == '-' || IsDigit(*nick)) /* first character in [0..9-] */
    return 0;
  
  for ( ; ch < endp && *ch; ++ch)
    {
      if (!IsNickChar(*ch))
	break;
    }
  *ch = '\0';

  return (ch - nick);
}

