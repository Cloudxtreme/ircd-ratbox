/*
 *  ircd-ratbox: A slightly useful ircd.
 *  client.c: Controls clients.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002 ircd-ratbox development team
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
#include "config.h"

#include "tools.h"
#include "client.h"
#include "class.h"
#include "channel_mode.h"
#include "common.h"
#include "event.h"
#include "fdlist.h"
#include "hash.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
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
#include "hostmask.h"
#include "balloc.h"
#include "listener.h"


static void check_pings_list(dlink_list * list);
static void check_unknowns_list(dlink_list * list);
static void free_exited_clients(void *unused);
static void exit_aborted_clients(void *unused);

static void qs_client(struct Client *, const char *);

static EVH check_pings;

static int remote_client_count = 0;
static int local_client_count = 0;

static BlockHeap *client_heap = NULL;
static BlockHeap *lclient_heap = NULL;

dlink_list dead_list;

/*
 * init_client
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	- initialize client free memory
 */
void
init_client(void)
{
	remote_client_count = 0;
	local_client_count = 0;
	/*
	 * start off the check ping event ..  -- adrian
	 * Every 30 seconds is plenty -- db
	 */
	client_heap = BlockHeapCreate(sizeof(struct Client), CLIENT_HEAP_SIZE);
	lclient_heap = BlockHeapCreate(sizeof(struct LocalUser), LCLIENT_HEAP_SIZE);
	eventAddIsh("check_pings", check_pings, NULL, 30);
	eventAddIsh("free_exited_clients", &free_exited_clients, NULL, 4);
	eventAddIsh("exit_aborted_clients", exit_aborted_clients, NULL, 1);
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
struct Client *
make_client(struct Client *from)
{
	struct Client *client_p = NULL;
	struct LocalUser *localClient;

	client_p = BlockHeapAlloc(client_heap);
	memset(client_p, 0, sizeof(struct Client));

	if(from == NULL)
	{
		client_p->from = client_p;	/* 'from' of local client is self! */
		client_p->since = client_p->lasttime = client_p->firsttime = CurrentTime;

		localClient = (struct LocalUser *) BlockHeapAlloc(lclient_heap);
		memset(localClient, 0, sizeof(struct LocalUser));

		client_p->localClient = localClient;

		client_p->localClient->fd = -1;
		client_p->localClient->ctrlfd = -1;
#ifndef HAVE_SOCKETPAIR
		client_p->localClient->fd_r = -1;
		client_p->localClient->ctrlfd_r = -1;
#endif
		/* as good a place as any... */
		dlinkAdd(client_p, &client_p->localClient->tnode, &unknown_list);
		++local_client_count;
	}
	else
	{			/* from is not NULL */
		client_p->localClient = NULL;
		client_p->from = from;	/* 'from' of local client is self! */
		++remote_client_count;
	}

	client_p->status = STAT_UNKNOWN;
	strcpy(client_p->username, "unknown");

	return client_p;
}

void
free_client(struct Client *client_p)
{
	s_assert(NULL != client_p);
	s_assert(&me != client_p);

	if(MyConnect(client_p))
	{
		s_assert(IsClosing(client_p) && IsDead(client_p));

		/*
		 * clean up extra sockets from P-lines which have been discarded.
		 */
		if(client_p->localClient->listener)
		{
			s_assert(0 < client_p->localClient->listener->ref_count);
			if(0 == --client_p->localClient->listener->ref_count &&
			   !client_p->localClient->listener->active)
				free_listener(client_p->localClient->listener);
			client_p->localClient->listener = 0;
		}

		if(client_p->localClient->fd >= 0)
			fd_close(client_p->localClient->fd);

		BlockHeapFree(lclient_heap, client_p->localClient);
		--local_client_count;
		s_assert(local_client_count >= 0);
	}
	else
	{
		--remote_client_count;
	}

	BlockHeapFree(client_heap, client_p);
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
}

/*
 * Check_pings_list()
 *
 * inputs	- pointer to list to check
 * output	- NONE
 * side effects	- 
 */
static void
check_pings_list(dlink_list * list)
{
	char scratch[32];	/* way too generous but... */
	struct Client *client_p;	/* current local client_p being examined */
	int ping = 0;		/* ping time value from client */
	dlink_node *ptr, *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, list->head)
	{
		client_p = ptr->data;

		/*
		 ** Note: No need to notify opers here. It's
		 ** already done when "FLAGS_DEADSOCKET" is set.
		 */
		if(client_p->flags & FLAGS_DEADSOCKET)
		{
			/* Ignore it, its been exited already */
			continue;
		}
		if(IsPerson(client_p))
		{
			if(!IsExemptKline(client_p) &&
			   GlobalSetOptions.idletime &&
			   !IsOper(client_p) &&
			   !IsIdlelined(client_p) &&
			   ((CurrentTime - client_p->user->last) > GlobalSetOptions.idletime))
			{
				struct ConfItem *aconf;

				aconf = make_conf();
				aconf->status = CONF_KILL;

				DupString(aconf->host, client_p->host);
				DupString(aconf->passwd, "idle exceeder");
				DupString(aconf->user, client_p->username);
				aconf->port = 0;
				aconf->hold = CurrentTime + 60;
				add_temp_kline(aconf);
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Idle time limit exceeded for %s - temp k-lining",
						     get_client_name(client_p, HIDE_IP));

				(void) exit_client(client_p, client_p, &me, aconf->passwd);
				continue;
			}
		}

		if(!IsRegistered(client_p))
			ping = ConfigFileEntry.connect_timeout;
		else
			ping = get_client_ping(client_p);

		if(ping < (CurrentTime - client_p->lasttime))
		{
			/*
			 * If the client/server hasnt talked to us in 2*ping seconds
			 * and it has a ping time, then close its connection.
			 */
			if(((CurrentTime - client_p->lasttime) >= (2 * ping) &&
			    (client_p->flags & FLAGS_PINGSENT)))
			{
				if(IsAnyServer(client_p))
				{
					sendto_realops_flags(UMODE_ALL, L_ADMIN,
							     "No response from %s, closing link",
							     get_client_name(client_p, HIDE_IP));
					sendto_realops_flags(UMODE_ALL, L_OPER,
							     "No response from %s, closing link",
							     get_client_name(client_p, MASK_IP));
					ilog(L_NOTICE, "No response from %s, closing link",
					     log_client_name(client_p, HIDE_IP));
				}
				(void) ircsprintf(scratch,
						  "Ping timeout: %d seconds",
						  (int) (CurrentTime - client_p->lasttime));

				(void) exit_client(client_p, client_p, &me, scratch);
				continue;
			}
			else if((client_p->flags & FLAGS_PINGSENT) == 0)
			{
				/*
				 * if we havent PINGed the connection and we havent
				 * heard from it in a while, PING it to make sure
				 * it is still alive.
				 */
				client_p->flags |= FLAGS_PINGSENT;
				/* not nice but does the job */
				client_p->lasttime = CurrentTime - ping;
				sendto_one(client_p, "PING :%s", me.name);
			}
		}
		/* ping_timeout: */

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
check_unknowns_list(dlink_list * list)
{
	dlink_node *ptr, *next_ptr;
	struct Client *client_p;

	DLINK_FOREACH_SAFE(ptr, next_ptr, list->head)
	{
		client_p = ptr->data;

		/*
		 * Check UNKNOWN connections - if they have been in this state
		 * for > 30s, close them.
		 */

		if(client_p->firsttime ? ((CurrentTime - client_p->firsttime) > 30) : 0)
		{
			(void) exit_client(client_p, client_p, &me, "Connection timed out");
		}
	}
}

/*
 * check_banned_lines
 * inputs	- NONE
 * output	- NONE
 * side effects - Check all connections for a pending k/d/gline against the
 * 		  client, exit the client if found.
 */
void
check_banned_lines(void)
{
	struct Client *client_p;	/* current local client_p being examined */
	struct ConfItem *aconf = (struct ConfItem *) NULL;
	const char *reason;		/* pointer to reason string */
	dlink_node *ptr, *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
	{
		client_p = ptr->data;

		if(IsMe(client_p))
			continue;

		/* if there is a returned struct ConfItem then kill it */
		if((aconf = find_dline(&client_p->localClient->ip, client_p->localClient->aftype)))
		{
			if(aconf->status & CONF_EXEMPTDLINE)
				continue;

			sendto_realops_flags(UMODE_ALL, L_ALL, "DLINE active for %s",
					     get_client_name(client_p, HIDE_IP));

			if(ConfigFileEntry.kline_with_connection_closed &&
			   ConfigFileEntry.kline_with_reason)
			{
				reason = "Connection closed";

				if(IsPerson(client_p))
					sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
						   me.name, client_p->name,
						   aconf->passwd ? aconf->passwd : "D-lined");
				else
					sendto_one(client_p,
						   "NOTICE DLINE :*** You have been D-lined");
			}
			else
			{
				if(ConfigFileEntry.kline_with_connection_closed)
					reason = "Connection closed";
				else if(ConfigFileEntry.kline_with_reason && aconf->passwd)
					reason = aconf->passwd;
				else
					reason = "D-lined";

				if(IsPerson(client_p))
					sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
						   me.name, client_p->name, reason);
				else
					sendto_one(client_p,
						   "NOTICE DLINE :*** You have been D-lined");
			}

			(void) exit_client(client_p, client_p, &me, reason);
			continue;	/* and go examine next fd/client_p */
		}

		if(IsPerson(client_p))
		{
			/* hunt for an xline */
			if(((aconf = find_x_conf(client_p->info)) != NULL) &&
			   (aconf->port != 0))
			{
				if(IsExemptKline(client_p))
				{
					sendto_realops_flags(UMODE_ALL, L_ALL,
							     "XLINE over-ruled for %s, client is kline_exempt",
							     get_client_name(client_p, HIDE_IP));
					continue;
				}

				sendto_realops_flags(UMODE_ALL, L_ALL, "XLINE active for %s",
						     get_client_name(client_p, HIDE_IP));

				(void) exit_client(client_p, client_p, &me, "Bad user info");
				continue;
			}

			/* hunt for a g/kline */
			if((aconf = find_kill(client_p)) == NULL)
				continue;

			if(aconf->status & CONF_GLINE)
			{
				if(IsExemptKline(client_p))
				{
					sendto_realops_flags(UMODE_ALL, L_ALL,
							     "GLINE over-ruled for %s, client is kline_exempt",
							     get_client_name(client_p, HIDE_IP));
					continue;
				}

				if(IsExemptGline(client_p))
				{
					sendto_realops_flags(UMODE_ALL, L_ALL,
							     "GLINE over-ruled for %s, client is gline_exempt",
							     get_client_name(client_p, HIDE_IP));
					continue;
				}

				sendto_realops_flags(UMODE_ALL, L_ALL, "GLINE active for %s",
						     get_client_name(client_p, HIDE_IP));

				if(ConfigFileEntry.kline_with_connection_closed &&
				   ConfigFileEntry.kline_with_reason)
				{
					reason = "Connection closed";

					sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
						   me.name, client_p->name,
						   aconf->passwd ? aconf->passwd : "G-lined");
				}
				else
				{
					if(ConfigFileEntry.kline_with_connection_closed)
						reason = "Connection closed";
					else if(ConfigFileEntry.kline_with_reason && aconf->passwd)
						reason = aconf->passwd;
					else
						reason = "G-lined";

					sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
						   me.name, client_p->name, reason);
				}

				(void) exit_client(client_p, client_p, &me, reason);
				/* and go examine next fd/client_p */
				continue;
			}
			else if(aconf->status & CONF_KILL)
			{
				/* if there is a returned struct ConfItem.. then kill it */
				if(IsExemptKline(client_p))
				{
					sendto_realops_flags(UMODE_ALL, L_ALL,
							     "KLINE over-ruled for %s, client is kline_exempt",
							     get_client_name(client_p, HIDE_IP));
					continue;
				}

				sendto_realops_flags(UMODE_ALL, L_ALL, "KLINE active for %s",
						     get_client_name(client_p, HIDE_IP));

				if(ConfigFileEntry.kline_with_connection_closed &&
				   ConfigFileEntry.kline_with_reason)
				{
					reason = "Connection closed";

					sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
						   me.name, client_p->name,
						   aconf->passwd ? aconf->passwd : "K-lined");
				}
				else
				{
					if(ConfigFileEntry.kline_with_connection_closed)
						reason = "Connection closed";
					else if(ConfigFileEntry.kline_with_reason && aconf->passwd)
						reason = aconf->passwd;
					else
						reason = "K-lined";

					sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
						   me.name, client_p->name, reason);
				}

				(void) exit_client(client_p, client_p, &me, reason);
				continue;
			}
		}
	}

	/* also check the unknowns list for new dlines */
	DLINK_FOREACH_SAFE(ptr, next_ptr, unknown_list.head)
	{
		client_p = ptr->data;

		if((aconf = find_dline(&client_p->localClient->ip, client_p->localClient->aftype)))
		{
			if(aconf->status & CONF_EXEMPTDLINE)
				continue;

			sendto_one(client_p, "NOTICE DLINE :*** You have been D-lined");
			exit_client(client_p, client_p, &me, "D-lined");
		}
	}

}

/* check_klines_event()
 *
 * inputs	-
 * outputs	-
 * side effects - check_klines() is called, kline_queued reset
 */
void
check_klines_event(void *unused)
{
	kline_queued = 0;
	check_klines();
}

/* check_klines
 *
 * inputs       -
 * outputs      -
 * side effects - all clients will be checked for klines
 */
void
check_klines(void)
{
	struct Client *client_p;
	struct ConfItem *aconf;
	const char *reason;
	dlink_node *ptr;
	dlink_node *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
	{
		client_p = ptr->data;

		if(IsMe(client_p) || !IsPerson(client_p))
			continue;

		if((aconf = find_kline(client_p)) != NULL)
		{
			if(IsExemptKline(client_p))
			{
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "KLINE over-ruled for %s, client is kline_exempt",
						     get_client_name(client_p, HIDE_IP));
				continue;
			}

			sendto_realops_flags(UMODE_ALL, L_ALL, "KLINE active for %s",
					     get_client_name(client_p, HIDE_IP));

			if(ConfigFileEntry.kline_with_connection_closed &&
			   ConfigFileEntry.kline_with_reason)
			{
				reason = "Connection closed";
				sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
					   me.name, client_p->name,
					   aconf->passwd ? aconf->passwd : "K-lined");
			}
			else
			{
				if(ConfigFileEntry.kline_with_connection_closed)
					reason = "Connection closed";
				else if(ConfigFileEntry.kline_with_reason && aconf->passwd)
					reason = aconf->passwd;
				else
					reason = "K-lined";

				sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
					   me.name, client_p->name, reason);
			}

			(void) exit_client(client_p, client_p, &me, reason);
			continue;
		}
	}
}

/* check_glines()
 *
 * inputs       -
 * outputs      -
 * side effects - all clients will be checked for glines
 */
void
check_glines(void)
{
	struct Client *client_p;
	struct ConfItem *aconf;
	const char *reason;
	dlink_node *ptr;
	dlink_node *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
	{
		client_p = ptr->data;

		if(IsMe(client_p) || !IsPerson(client_p))
			continue;

		if((aconf = find_gline(client_p)) != NULL)
		{
			if(IsExemptKline(client_p))
			{
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "GLINE over-ruled for %s, client is kline_exempt",
						     get_client_name(client_p, HIDE_IP));
				continue;
			}

			if(IsExemptGline(client_p))
			{
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "GLINE over-ruled for %s, client is gline_exempt",
						     get_client_name(client_p, HIDE_IP));
				continue;
			}

			sendto_realops_flags(UMODE_ALL, L_ALL, "GLINE active for %s",
					     get_client_name(client_p, HIDE_IP));

			if(ConfigFileEntry.kline_with_connection_closed &&
			   ConfigFileEntry.kline_with_reason)
			{
				reason = "Connection closed";
				sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
					   me.name, client_p->name,
					   aconf->passwd ? aconf->passwd : "G-lined");
			}
			else
			{
				if(ConfigFileEntry.kline_with_connection_closed)
					reason = "Connection closed";
				else if(ConfigFileEntry.kline_with_reason && aconf->passwd)
					reason = aconf->passwd;
				else
					reason = "K-lined";

				sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
					   me.name, client_p->name, reason);
			}

			(void) exit_client(client_p, client_p, &me, reason);
			continue;
		}
	}
}

/* check_dlines()
 *
 * inputs       -
 * outputs      -
 * side effects - all clients will be checked for dlines
 */
void
check_dlines(void)
{
	struct Client *client_p;
	struct ConfItem *aconf;
	const char *reason;
	dlink_node *ptr;
	dlink_node *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
	{
		client_p = ptr->data;

		if(IsMe(client_p))
			continue;

		if((aconf = find_dline(&client_p->localClient->ip,
				       client_p->localClient->aftype)) != NULL)
		{
			if(aconf->status & CONF_EXEMPTDLINE)
				continue;

			sendto_realops_flags(UMODE_ALL, L_ALL, "DLINE active for %s",
					     get_client_name(client_p, HIDE_IP));

			if(ConfigFileEntry.kline_with_connection_closed &&
			   ConfigFileEntry.kline_with_reason)
			{
				reason = "Connection closed";

				if(IsPerson(client_p))
					sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
						   me.name, client_p->name,
						   aconf->passwd ? aconf->passwd : "D-lined");
				else
					sendto_one(client_p,
						   "NOTICE DLINE :*** You have been D-lined");
			}
			else
			{
				if(ConfigFileEntry.kline_with_connection_closed)
					reason = "Connection closed";
				else if(ConfigFileEntry.kline_with_reason && aconf->passwd)
					reason = aconf->passwd;
				else
					reason = "D-lined";

				if(IsPerson(client_p))
					sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
						   me.name, client_p->name, reason);
				else
					sendto_one(client_p,
						   "NOTICE DLINE :*** You have been D-lined");
			}

			(void) exit_client(client_p, client_p, &me, reason);
			continue;
		}
	}

	/* dlines need to be checked against unknowns too */
	DLINK_FOREACH_SAFE(ptr, next_ptr, unknown_list.head)
	{
		client_p = ptr->data;

		if((aconf = find_dline(&client_p->localClient->ip,
				       client_p->localClient->aftype)) != NULL)
		{
			if(aconf->status & CONF_EXEMPTDLINE)
				continue;

			sendto_one(client_p, "NOTICE DLINE :*** You have been D-lined");
			exit_client(client_p, client_p, &me, "D-lined");
		}
	}
}

/* check_xlines
 *
 * inputs       -
 * outputs      -
 * side effects - all clients will be checked for xlines
 */
void
check_xlines(void)
{
	struct Client *client_p;
	struct ConfItem *aconf;
	dlink_node *ptr;
	dlink_node *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
	{
		client_p = ptr->data;

		if(IsMe(client_p) || !IsPerson(client_p))
			continue;

		if((aconf = find_x_conf(client_p->info)) != NULL)
		{
			/* xline that just warns, ignore it */
			if(aconf->port == 0)
				continue;

			if(IsExemptKline(client_p))
			{
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "XLINE over-ruled for %s, client is kline_exempt",
						     get_client_name(client_p, HIDE_IP));
				continue;
			}

			sendto_realops_flags(UMODE_ALL, L_ALL, "XLINE active for %s",
					     get_client_name(client_p, HIDE_IP));

			(void) exit_client(client_p, client_p, &me, "Bad user info");
			continue;
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
static void
update_client_exit_stats(struct Client *client_p)
{
	if(IsServer(client_p))
	{
		--Count.server;
		sendto_realops_flags(UMODE_EXTERNAL, L_ALL,
				     "Server %s split from %s",
				     client_p->name, client_p->servptr->name);
	}
	else if(IsClient(client_p))
	{
		--Count.total;
		if(IsOper(client_p))
			--Count.oper;
		if(IsInvisible(client_p))
			--Count.invisi;
	}

	if(splitchecking && !splitmode)
		check_splitmode(NULL);
}

/*
 * release_client_state
 *
 * input	- pointer to client to release
 * output	- NONE
 * side effects	- 
 */
static void
release_client_state(struct Client *client_p)
{
	if(client_p->user != NULL)
	{
		free_user(client_p->user, client_p);	/* try this here */
		client_p->user = NULL;
	}
	if(client_p->serv)
	{
		if(client_p->serv->user != NULL)
			free_user(client_p->serv->user, client_p);
		MyFree((char *) client_p->serv);
		client_p->serv = NULL;
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
remove_client_from_list(struct Client *client_p)
{
	s_assert(NULL != client_p);

	if(client_p == NULL)
		return;

	/* A client made with make_client()
	 * is on the unknown_list until removed.
	 * If it =does= happen to exit before its removed from that list
	 * and its =not= on the global_client_list, it will core here.
	 * short circuit that case now -db
	 */
	if(client_p->node.prev == NULL && client_p->node.next == NULL)
		return;

	dlinkDelete(&client_p->node, &global_client_list);

	update_client_exit_stats(client_p);
}


/*
 * find_person	- find person by (nick)name.
 * inputs	- pointer to name
 * output	- return client pointer
 * side effects -
 */
struct Client *
find_person(char *name)
{
	struct Client *c2ptr;

	c2ptr = find_client(name);

	if(c2ptr && IsPerson(c2ptr))
		return (c2ptr);
	return (NULL);
}

/*
 * find_chasing - find the client structure for a nick name (user) 
 *      using history mechanism if necessary. If the client is not found, 
 *      an error message (NO SUCH NICK) is generated. If the client was found
 *      through the history, chasing will be 1 and otherwise 0.
 */
struct Client *
find_chasing(struct Client *source_p, char *user, int *chasing)
{
	struct Client *who = find_client(user);

	if(chasing)
		*chasing = 0;
	if(who)
		return who;
	if(!(who = get_history(user, (long) KILLCHASETIMELIMIT)))
	{
		sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, source_p->name, user);
		return (NULL);
	}
	if(chasing)
		*chasing = 1;
	return who;
}

/*
 * get_client_name -  Return the name of the client
 *    for various tracking and
 *      admin purposes. The main purpose of this function is to
 *      return the "socket host" name of the client, if that
 *        differs from the advertised name (other than case).
 *        But, this can be used to any client structure.
 *
 * NOTE 1:
 *        Watch out the allocation of "nbuf", if either source_p->name
 *        or source_p->sockhost gets changed into pointers instead of
 *        directly allocated within the structure...
 *
 * NOTE 2:
 *        Function return either a pointer to the structure (source_p) or
 *        to internal buffer (nbuf). *NEVER* use the returned pointer
 *        to modify what it points!!!
 */

const char *
get_client_name(struct Client *client, int showip)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	s_assert(NULL != client);
	if(client == NULL)
		return NULL;

	if(MyConnect(client))
	{
		if(!irccmp(client->name, client->host))
			return client->name;

#ifdef HIDE_SERVERS_IPS
		if(IsAnyServer(client))
			showip = MASK_IP;
#endif
#ifdef HIDE_SPOOF_IPS
		if(showip == SHOW_IP && IsIPSpoof(client))
			showip = MASK_IP;
#endif

		/* And finally, let's get the host information, ip or name */
		switch (showip)
		{
		case SHOW_IP:
			ircsprintf(nbuf, "%s[%s@%s]", client->name, client->username,
				   client->localClient->sockhost);
			break;
		case MASK_IP:
			ircsprintf(nbuf, "%s[%s@255.255.255.255]", client->name, client->username);
			break;
		default:
			ircsprintf(nbuf, "%s[%s@%s]", client->name, client->username, client->host);
		}
		return nbuf;
	}

	/* As pointed out by Adel Mezibra 
	 * Neph|l|m@EFnet. Was missing a return here.
	 */
	return client->name;
}

/* log_client_name()
 *
 * This version is the same as get_client_name, but doesnt contain the
 * code that will hide IPs always.  This should be used for logfiles.
 */
const char *
log_client_name(struct Client *target_p, int showip)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if(target_p == NULL)
		return NULL;

	if(MyConnect(target_p))
	{
		if(irccmp(target_p->name, target_p->host) == 0)
			return target_p->name;

		switch (showip)
		{
		case SHOW_IP:
			ircsprintf(nbuf, "%s[%s@%s]", target_p->name, target_p->username,
				   target_p->localClient->sockhost);
			break;

		case MASK_IP:
			ircsprintf(nbuf, "%s[%s@255.255.255.255]",
				   target_p->name, target_p->username);

		default:
			ircsprintf(nbuf, "%s[%s@%s]", target_p->name, target_p->username,
				   target_p->host);
		}

		return nbuf;
	}

	return target_p->name;
}

static void
free_exited_clients(void *unused)
{
	dlink_node *ptr, *next;
	struct Client *target_p;

	DLINK_FOREACH_SAFE(ptr, next, dead_list.head)
	{
		target_p = ptr->data;
		if(ptr->data == NULL)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Warning: null client on dead_list!");
			dlinkDestroy(ptr, &dead_list);
			continue;
		}
		release_client_state(target_p);
		free_client(target_p);
		dlinkDestroy(ptr, &dead_list);
	}
}

/*
** Exit one client, local or remote. Assuming all dependents have
** been already removed, and socket closed for local client.
*/
static void
exit_one_client(struct Client *client_p,
		struct Client *source_p, struct Client *from, const char *comment)
{
	struct Client *target_p;
	dlink_node *lp;
	dlink_node *next_lp;
	
	if(IsServer(source_p))
	{
		if(source_p->servptr && source_p->servptr->serv)
			dlinkDelete(&source_p->lnode, &source_p->servptr->serv->servers);
		else
			ts_warn("server %s without servptr!", source_p->name);

		if(!IsMe(source_p))
			remove_server_from_list(source_p);
	}
	else if(source_p->servptr && source_p->servptr->serv)
	{
		dlinkDelete(&source_p->lnode, &source_p->servptr->serv->users);
	}
	/* there are clients w/o a servptr: unregistered ones */

	/*
	 **  For a server or user quitting, propogate the information to
	 **  other servers (except to the one where is came from (client_p))
	 */
	if(IsMe(source_p))
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "ERROR: tried to exit me! : %s", comment);
		return;		/* ...must *never* exit self!! */
	}
	else if(IsServer(source_p))
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
		if(MyConnect(source_p))
		{
			if(source_p->localClient->ctrlfd > -1)
			{
				fd_close(source_p->localClient->ctrlfd);
				source_p->localClient->ctrlfd = -1;

#ifndef HAVE_SOCKETPAIR
				fd_close(source_p->localClient->ctrlfd_r);
				fd_close(source_p->localClient->fd_r);

				source_p->localClient->ctrlfd_r = -1;
				source_p->localClient->fd_r = -1;
#endif
			}
		}

		target_p = source_p->from;
		if(target_p && IsServer(target_p) && target_p != client_p && !IsMe(target_p) &&
		   (source_p->flags & FLAGS_KILLED) == 0)
			sendto_one(target_p, ":%s SQUIT %s :%s", from->name, source_p->name,
				   comment);
	}
	else if(IsPerson(source_p))	/* ...just clean all others with QUIT... */
	{
		/*
		 ** If this exit is generated from "m_kill", then there
		 ** is no sense in sending the QUIT--KILL's have been
		 ** sent instead.
		 */
		if((source_p->flags & FLAGS_KILLED) == 0)
		{
			sendto_server(client_p, NULL, NOCAPS, NOCAPS,
				      ":%s QUIT :%s", source_p->name, comment);
		}
		/*
		 ** If a person is on a channel, send a QUIT notice
		 ** to every client (person) on the same channel (so
		 ** that the client can show the "**signoff" message).
		 ** (Note: The notice is to the local clients *only*)
		 */
		sendto_common_channels_local(source_p, ":%s!%s@%s QUIT :%s",
					     source_p->name,
					     source_p->username, source_p->host, comment);

		remove_user_from_channels(source_p);

		/* Should not be in any channels now */
		s_assert(source_p->user->channel.head == NULL);

		/* Clean up invitefield */
		DLINK_FOREACH_SAFE(lp, next_lp, source_p->user->invited.head)
		{
			del_invite(lp->data, source_p);
		}

		/* Clean up allow lists */
		del_all_accepts(source_p);

		add_history(source_p, 0);
		off_history(source_p);

		if(HasID(source_p))
			del_from_id_hash_table(source_p->user->id, source_p);

		del_from_hostname_hash_table(source_p->host, source_p);

		/* again, this is all that is needed */
	}

	/* 
	 * Remove source_p from the client lists
	 */
	del_from_client_hash_table(source_p->name, source_p);

	/* remove from global client list */
	remove_client_from_list(source_p);

	if(MyConnect(source_p))
	{
		linebuf_donebuf(&source_p->localClient->buf_recvq);
		linebuf_donebuf(&source_p->localClient->buf_sendq);
	}

	/* add to dead client dlist */
	SetDead(source_p);
	dlinkAddAlloc(source_p, &dead_list);
}

/*
** Recursively send QUITs and SQUITs for source_p and all its dependent clients
** and servers to those servers that need them.  A server needs the client
** QUITs if it can't figure them out from the SQUIT (ie pre-TS4) or if it
** isn't getting the SQUIT because of @#(*&@)# hostmasking.  With TS4, once
** a link gets a SQUIT, it doesn't need any QUIT/SQUITs for clients depending
** on that one -orabidoo
*/
static void
recurse_send_quits(struct Client *client_p, struct Client *source_p, struct Client *to, const char *comment,	/* for servers */
		   const char *myname)
{
	struct Client *target_p;
	dlink_node *ptr, *ptr_next;
	/* If this server can handle quit storm (QS) removal
	 * of dependents, just send the SQUIT
	 */

	if(IsCapable(to, CAP_QS))
	{
		if(match(myname, source_p->name))
		{
			DLINK_FOREACH_SAFE(ptr, ptr_next, source_p->serv->users.head)
			{
				target_p = (struct Client *) ptr->data;
				sendto_one(to, ":%s QUIT :%s", target_p->name, comment);
			}
			DLINK_FOREACH_SAFE(ptr, ptr_next, source_p->serv->servers.head)
			{
				target_p = (struct Client *) ptr->data;
				recurse_send_quits(client_p, target_p, to, comment, myname);
			}
		}
		else
			sendto_one(to, "SQUIT %s :%s", source_p->name, me.name);
	}
	else
	{
		DLINK_FOREACH_SAFE(ptr, ptr_next, source_p->serv->users.head)
		{
			target_p = (struct Client *) ptr->data;
			sendto_one(to, ":%s QUIT :%s", target_p->name, comment);
		}
		DLINK_FOREACH_SAFE(ptr, ptr_next, source_p->serv->servers.head)
		{
			target_p = (struct Client *) ptr->data;
			recurse_send_quits(client_p, target_p, to, comment, myname);
		}
		if(!match(myname, source_p->name))
			sendto_one(to, "SQUIT %s :%s", source_p->name, me.name);
	}
}

/* 
** Remove all clients that depend on source_p; assumes all (S)QUITs have
** already been sent.  we make sure to exit a server's dependent clients 
** and servers before the server itself; exit_one_client takes care of 
** actually removing things off llists.   tweaked from +CSr31  -orabidoo
*/
/*
 * added sanity test code.... source_p->serv might be NULL...
 */
static void
recurse_remove_clients(struct Client *source_p, const char *comment)
{
	struct Client *target_p;
	dlink_node *ptr, *ptr_next;
	if(IsMe(source_p))
		return;

	if(source_p->serv == NULL)	/* oooops. uh this is actually a major bug */
		return;

	DLINK_FOREACH_SAFE(ptr, ptr_next, source_p->serv->users.head)
	{
		target_p = (struct Client *) ptr->data;

		/* XXX - this next line can probably go now --fl */
		target_p->flags |= FLAGS_KILLED;
		qs_client(target_p, comment);
	}

	DLINK_FOREACH_SAFE(ptr, ptr_next, source_p->serv->servers.head)
	{
		target_p = (struct Client *) ptr->data;
		recurse_remove_clients(target_p, comment);
		target_p->flags |= FLAGS_KILLED;
		exit_one_client(NULL, target_p, &me, me.name);
	}
}

/* qs_client()
 *
 * inputs       - client to remove, reason
 * outputs      -
 * side effects - client is 'quitstormed' off the network
 */
static void
qs_client(struct Client *source_p, const char *comment)
{
	dlink_node *ptr;
	dlink_node *next_ptr;

	if(IsDead(source_p))
		return;
	
	if(source_p->servptr && source_p->servptr->serv)
	{
		dlinkDelete(&source_p->lnode, &source_p->servptr->serv->users);
	}

	sendto_common_channels_local(source_p, ":%s!%s@%s QUIT :%s",
				     source_p->name, source_p->username, source_p->host, comment);

	qs_user_from_channels(source_p);

	/* shouldnt be in any channels */
	s_assert(source_p->user->channel.head == NULL);

	/* clean up various lists */
	DLINK_FOREACH_SAFE(ptr, next_ptr, source_p->user->invited.head)
	{
		del_invite(ptr->data, source_p);
	}

	del_all_accepts(source_p);

	/* add to whowas */
	add_history(source_p, 0);
	off_history(source_p);

	if(HasID(source_p))
		del_from_id_hash_table(source_p->user->id, source_p);

	del_from_hostname_hash_table(source_p->host, source_p);

	del_from_client_hash_table(source_p->name, source_p);

	/* remove from global client list */
	remove_client_from_list(source_p);

	/* this stops them being sent things like channel messages */
	SetDead(source_p);
	dlinkAddAlloc(source_p, &dead_list);
}

/*
** Remove *everything* that depends on source_p, from all lists, and sending
** all necessary QUITs and SQUITs.  source_p itself is still on the lists,
** and its SQUITs have been sent except for the upstream one  -orabidoo
*/
static void
remove_dependents(struct Client *client_p,
		  struct Client *source_p,
		  struct Client *from, const char *comment, const char *comment1)
{
	struct Client *to;
	struct ConfItem *aconf;
	static char myname[HOSTLEN + 1];
	dlink_node *ptr, *next;

	DLINK_FOREACH_SAFE(ptr, next, serv_list.head)
	{
		to = ptr->data;

		if(IsMe(to) || to == source_p->from || (to == client_p && IsCapable(to, CAP_QS)))
			continue;

		/* MyConnect(source_p) is rotten at this point: if source_p
		 * was mine, ->from is NULL. 
		 */
		/* The WALLOPS isn't needed here as pointed out by
		 * comstud, since m_squit already does the notification.
		 */

		if((aconf = to->serv->sconf))
			strlcpy(myname, my_name_for_link(me.name, aconf), sizeof(myname));
		else
			strlcpy(myname, me.name, sizeof(myname));
		recurse_send_quits(client_p, source_p, to, comment1, myname);
	}

	recurse_remove_clients(source_p, comment1);
}



struct abort_client
{
	dlink_node node;
	struct Client *client;
	char notice[TOPICLEN];
};

static dlink_list abort_list;

void
exit_aborted_clients(void *unused)
{
	dlink_node *ptr, *next;
	DLINK_FOREACH_SAFE(ptr, next, abort_list.head)
	{
		struct abort_client *abt = ptr->data;
		dlinkDelete(ptr, &abort_list);
		if(!IsPerson(abt->client) && !IsUnknown(abt->client) && !IsClosing(abt->client))
		{
			sendto_realops_flags(UMODE_ALL, L_ADMIN,
					     "Closing link to %s: %s",
					     get_client_name(abt->client, HIDE_IP), abt->notice);
			sendto_realops_flags(UMODE_ALL, L_OPER,
					     "Closing link to %s: %s",
					     get_client_name(abt->client, MASK_IP), abt->notice);
		}
		exit_client(abt->client, abt->client, &me, abt->notice);
		MyFree(abt);
	}
}

/*
 * dead_link - Adds client to a list of clients that need an exit_client()
 *
 */
void
dead_link(struct Client *client_p)
{
	struct abort_client *abt;
	if(IsClosing(client_p) || IsDead(client_p) || IsMe(client_p))
		return;

	abt = MyMalloc(sizeof(struct abort_client));
	abt->client = client_p;

	if(client_p->flags & FLAGS_SENDQEX)
		strcpy(abt->notice, "Max SendQ exceeded");
	else
	{
		ircsprintf(abt->notice, "Write error: %s", strerror(errno));
	}

	Debug((DEBUG_ERROR, "Closing link to %s: %s", get_client_name(client_p, HIDE_IP), notice));
	SetDead(client_p);	/* You are dead my friend */
	dlinkAdd(abt, &abt->node, &abort_list);
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
**        CLIENT_EXITED        if (client_p == source_p)
**        0                if (client_p != source_p)
*/
int
exit_client(struct Client *client_p,	/* The local client originating the
					 * exit or NULL, if this exit is
					 * generated by this server for
					 * internal reasons.
					 * This will not get any of the
					 * generated messages. */
	    struct Client *source_p,	/* Client exiting */
	    struct Client *from,	/* Client firing off this Exit,
					 * never NULL! */
	    const char *comment	/* Reason for the exit */
	)
{
	char comment1[HOSTLEN + HOSTLEN + 2];
	if(MyConnect(source_p))
	{
		/* DO NOT REMOVE. exit_client can be called twice after a failed
		 * read/write.
		 */
		if(IsClosing(source_p))
			return 0;

		SetClosing(source_p);

		delete_auth_queries(source_p);
		client_flush_input(source_p);

		/* This source_p could have status of one of STAT_UNKNOWN, STAT_CONNECTING
		 * STAT_HANDSHAKE or STAT_UNKNOWN
		 * all of which are lumped together into unknown_list
		 *
		 * In all above cases IsRegistered() will not be true.
		 */
		if(!IsRegistered(source_p))
		{
			dlinkDelete(&source_p->localClient->tnode, &unknown_list);
		}
		if(IsOper(source_p))
		{
			dlinkFindDestroy(&oper_list, source_p);
		}
		if(IsClient(source_p))
		{
			Count.local--;

			if(IsPerson(source_p))	/* a little extra paranoia */
			{
				dlinkDelete(&source_p->localClient->tnode, &lclient_list);
			}
		}

		/* As soon as a client is known to be a server of some sort
		 * it has to be put on the serv_list, or SJOIN's to this new server
		 * from the connect burst will not be seen.
		 */
		if(IsServer(source_p))
		{
			dlinkDelete(&source_p->localClient->tnode, &serv_list);
			Count.myserver--;
			unset_chcap_usage_counts(source_p);
		}

		if(IsPerson(source_p))
			sendto_realops_flags(UMODE_CCONN, L_ALL,
					     "Client exiting: %s (%s@%s) [%s] [%s]",
					     source_p->name, source_p->username, source_p->host,
					     comment,
#ifdef HIDE_SPOOF_IPS
					     IsIPSpoof(source_p) ? "255.255.255.255" :
#endif
					     source_p->localClient->sockhost);

		log_user_exit(source_p);

		if(source_p->localClient->fd >= 0)
		{
			if(client_p != NULL && source_p != client_p)
				sendto_one(source_p, "ERROR :Closing Link: %s %s (%s)",
					   source_p->host, source_p->name, comment);
			else
				sendto_one(source_p, "ERROR :Closing Link: %s (%s)",
					   source_p->host, comment);
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
		 ** (The following *must* make MyConnect(source_p) == FALSE!).
		 ** It also makes source_p->from == NULL, thus it's unnecessary
		 ** to test whether "source_p != target_p" in the following loops.
		 */
		close_connection(source_p);
	}

	if(IsServer(source_p))
	{
		if(ConfigServerHide.hide_servers)
		{
			/* set netsplit message to "me.name *.split" to still show 
			 * that its a split, but hide the servers splitting
			 */
			ircsprintf(comment1, "%s *.split", me.name);
		}
		else
		{
			if((source_p->serv) && (source_p->serv->up))
				strcpy(comment1, source_p->serv->up);
			else
				strcpy(comment1, "<Unknown>");

			strcat(comment1, " ");
			strcat(comment1, source_p->name);
		}

		if(source_p->serv != NULL)	/* XXX Why does this happen */
			remove_dependents(client_p, source_p, from, comment, comment1);

		if(source_p->servptr == &me)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s was connected for %d seconds.  %d/%d sendK/recvK.",
					     source_p->name,
					     (int) (CurrentTime - source_p->firsttime),
					     source_p->localClient->sendK,
					     source_p->localClient->receiveK);
			ilog(L_NOTICE, "%s was connected for %ld seconds.  %d/%d sendK/recvK.",
			     source_p->name, CurrentTime - source_p->firsttime,
			     source_p->localClient->sendK, source_p->localClient->receiveK);
		}
	}

	exit_one_client(client_p, source_p, from, comment);
	return client_p == source_p ? CLIENT_EXITED : 0;
}

/*
 * Count up local client memory
 */

/* XXX one common Client list now */
void
count_local_client_memory(int *count, size_t *local_client_memory_used)
{
	*count = local_client_count;
	*local_client_memory_used = local_client_count *
		(sizeof(struct Client) + sizeof(struct LocalUser));
}

/*
 * Count up remote client memory
 */
void
count_remote_client_memory(int *count, size_t *remote_client_memory_used)
{
	*count = remote_client_count;
	*remote_client_memory_used = remote_client_count * sizeof(struct Client);
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
int
accept_message(struct Client *source, struct Client *target)
{
	if(dlinkFind(&target->allow_list, source) != NULL)
		return 1;

	return 0;
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
void
del_from_accept(struct Client *source, struct Client *target)
{
	dlink_node *ptr;
	dlink_node *ptr2;
	dlink_node *next_ptr;
	dlink_node *next_ptr2;
	struct Client *target_p;

	DLINK_FOREACH_SAFE(ptr, next_ptr, target->allow_list.head)
	{
		target_p = ptr->data;
		if(source == target_p)
		{
			dlinkDestroy(ptr, &target->allow_list);

			DLINK_FOREACH_SAFE(ptr2, next_ptr2, source->on_allow_list.head)
			{
				target_p = ptr2->data;
				if(target == target_p)
				{
					dlinkDestroy(ptr2, &source->on_allow_list);
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
void
del_all_accepts(struct Client *client_p)
{
	dlink_node *ptr;
	dlink_node *next_ptr;
	struct Client *target_p;

	DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->allow_list.head)
	{
		target_p = ptr->data;
		if(target_p != NULL)
			del_from_accept(target_p, client_p);
	}

	DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->on_allow_list.head)
	{
		target_p = ptr->data;
		if(target_p != NULL)
			del_from_accept(client_p, target_p);
	}
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
set_initial_nick(struct Client *client_p, struct Client *source_p, char *nick)
{
	char buf[USERLEN + 1];

	/* This had to be copied here to avoid problems.. */
	source_p->tsinfo = CurrentTime;
	if(source_p->name[0])
		del_from_client_hash_table(source_p->name, source_p);

	strcpy(source_p->name, nick);
	add_to_client_hash_table(nick, source_p);

	/* fd_desc is long enough */
	fd_note(client_p->localClient->fd, "Nick: %s", nick);

	if(source_p->user)
	{
		strlcpy(buf, source_p->username, sizeof(buf));

		/* USER already received, now we have NICK. */
		if(register_local_user(client_p, source_p, nick, buf) == CLIENT_EXITED)
			return CLIENT_EXITED;

	}

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
int
change_local_nick(struct Client *client_p, struct Client *source_p, char *nick)
{
	/*
	 ** Client just changing his/her nick. If he/she is
	 ** on a channel, send note of change to all clients
	 ** on that channel. Propagate notice to other servers.
	 */

	source_p->tsinfo = CurrentTime;

	if((source_p->localClient->last_nick_change + ConfigFileEntry.max_nick_time) < CurrentTime)
		source_p->localClient->number_of_nick_changes = 0;
	source_p->localClient->last_nick_change = CurrentTime;
	source_p->localClient->number_of_nick_changes++;

	if((ConfigFileEntry.anti_nick_flood &&
	    (source_p->localClient->number_of_nick_changes
	     <= ConfigFileEntry.max_nick_changes)) ||
	   !ConfigFileEntry.anti_nick_flood || (IsOper(source_p) && ConfigFileEntry.no_oper_flood))
	{
		sendto_realops_flags(UMODE_NCHANGE, L_ALL,
				     "Nick change: From %s to %s [%s@%s]",
				     source_p->name, nick, source_p->username, source_p->host);

		sendto_common_channels_local(source_p, ":%s!%s@%s NICK :%s",
					     source_p->name, source_p->username, source_p->host,
					     nick);
		if(source_p->user)
		{
			add_history(source_p, 1);

			sendto_server(client_p, NULL, NOCAPS, NOCAPS,
				      ":%s NICK %s :%lu", source_p->name,
				      nick, (unsigned long) source_p->tsinfo);
		}
	}
	else
	{
		sendto_one(source_p,
			   form_str(ERR_NICKTOOFAST), me.name, source_p->name,
			   source_p->name, nick, ConfigFileEntry.max_nick_time);
		return 0;
	}

	/* Finally, add to hash */
	del_from_client_hash_table(source_p->name, source_p);
	strcpy(source_p->name, nick);
	add_to_client_hash_table(nick, source_p);

	/* Make sure everyone that has this client on its accept list
	 * loses that reference. 
	 */

	del_all_accepts(source_p);

	/* fd_desc is long enough */
	fd_note(client_p->localClient->fd, "Nick: %s", nick);

	return 1;
}

/*
 * show_ip() - asks if the true IP shoudl be shown when source is
 *             askin for info about target 
 *
 * Inputs	- source_p who is asking
 *		- target_p who do we want the info on
 * Output	- returns 1 if clear IP can be showed, otherwise 0
 * Side Effects	- none
 *
 *
 * Truth tables, one if source_p is local to target_p and one if not. 
 * Key:
 *  x  Show IP
 *  -  Don't show IP
 *  ?  Ask IsIPSpoof()
 */

static char show_ip_local[7][7] = {
	/*Target:        A   O   L   H   S   C   E */
	/*Source: */
	/*Admin */ {'x', 'x', 'x', 'x', 'x', 'x', '-'},
	/*Oper */ {'-', '?', 'x', '?', '?', '?', '-'},
	/*Luser */ {'-', '-', '?', '-', '-', '-', '-'},
	/*Handshaker */ {'-', '-', '-', '-', '-', '-', '-'},
	/*Server */ {'-', '-', '-', '-', '-', '-', '-'},
	/*Connecting */ {'-', '-', '-', '-', '-', '-', '-'},
	/*Else */ {'-', '-', '-', '-', '-', '-', '-'}
};

static char show_ip_remote[7][7] = {
	/*Target:        A   O   L   H   S   C   E */
	/*Source: */
	/*Admin */ {'?', '?', '?', '?', '?', '-', '-'},
	/*Oper */ {'-', '?', '?', '?', '?', '-', '-'},
	/*Luser */ {'-', '-', '?', '-', '-', '-', '-'},
	/*Handshake */ {'-', '-', '-', '-', '-', '-', '-'},
	/*Server */ {'-', '-', '-', '-', '-', '-', '-'},
	/*Connecting */ {'-', '-', '-', '-', '-', '-', '-'},
	/*Else */ {'-', '-', '-', '-', '-', '-', '-'}
};

int
show_ip(struct Client *source_p, struct Client *target_p)
{
	int s, t, res;

	if(IsAdmin(source_p))
		s = 0;
	else if(IsOper(source_p))
		s = 1;
	else if(IsClient(source_p))
		s = 2;
	else if(IsHandshake(source_p))
		s = 3;
	else if(IsServer(source_p))
		s = 4;
	else if(IsConnecting(source_p))
		s = 5;
	else
		s = 6;

	if(IsAdmin(target_p))
		t = 0;
	else if(IsOper(target_p))
		t = 1;
	else if(IsClient(target_p))
		t = 2;
	else if(IsHandshake(target_p))
		t = 3;
	else if(IsServer(target_p))
		t = 4;
	else if(IsConnecting(target_p))
		t = 5;
	else
		t = 6;

	if(MyClient(source_p) && MyClient(target_p))
		res = show_ip_local[s][t];
	else
		res = show_ip_remote[s][t];

	if(res == '-')
		return 0;

#ifdef HIDE_SPOOF_IPS
	if(IsIPSpoof(target_p))
		return 0;
#endif

#ifdef HIDE_SERVERS_IPS
	if(IsAnyServer(target_p))
		return 0;
#endif

	if(res == '?')
		return !IsIPSpoof(target_p);

	if(res == 'x')
		return 1;

	/* This should never happen */
	return 0;
}

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
void
initUser(void)
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
struct User *
make_user(struct Client *client_p)
{
	struct User *user;

	user = client_p->user;
	if(!user)
	{
		user = (struct User *) BlockHeapAlloc(user_heap);
		memset(user, 0, sizeof(struct User));
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
struct Server *
make_server(struct Client *client_p)
{
	struct Server *serv = client_p->serv;

	if(!serv)
	{
		serv = (struct Server *) MyMalloc(sizeof(struct Server));

		/* The commented out lines here are
		 * for documentation purposes only
		 * as they are zeroed by MyMalloc above
		 */
#if 0
		serv->user = NULL;
		serv->users = NULL;
		serv->servers = NULL;
		*serv->by = '\0';
		serv->up = (char *) NULL;
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
void
free_user(struct User *user, struct Client *client_p)
{
	if(--user->refcnt <= 0)
	{
		if(user->away)
			MyFree((char *) user->away);
		/*
		 * sanity check
		 */
		if(user->refcnt < 0 || user->invited.head || user->channel.head)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "* %#lx user (%s!%s@%s) %#lx %#lx %#lx %lu %d *",
					     (unsigned long) client_p,
					     client_p ? client_p->name : "<noname>",
					     client_p->username, client_p->host,
					     (unsigned long) user,
					     (unsigned long) user->invited.head,
					     (unsigned long) user->channel.head,
					     dlink_list_length(&user->channel),
					     user->refcnt);
			s_assert(!user->refcnt);
			s_assert(!user->invited.head);
			s_assert(!user->channel.head);
		}

		BlockHeapFree(user_heap, user);
	}
}
