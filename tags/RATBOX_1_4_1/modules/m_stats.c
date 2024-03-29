/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_stats.c: Sends the user statistics or config information.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
#include "tools.h"		/* dlink_node/dlink_list */
#include "handlers.h"		/* m_pass prototype */
#include "class.h"		/* report_classes */
#include "client.h"		/* Client */
#include "common.h"		/* TRUE/FALSE */
#include "irc_string.h"
#include "ircd.h"		/* me */
#include "listener.h"		/* show_ports */
#include "s_gline.h"
#include "ircd_handler.h"
#include "msg.h"		/* Message */
#include "hostmask.h"		/* report_mtrie_conf_links */
#include "numeric.h"		/* ERR_xxx */
#include "scache.h"		/* list_scache */
#include "send.h"		/* sendto_one */
#include "fdlist.h"		/* PF and friends */
#include "s_bsd.h"		/* highest_fd */
#include "s_conf.h"		/* ConfItem, report_configured_links */
#include "s_debug.h"		/* send_usage */
#include "s_misc.h"		/* serv_info */
#include "s_serv.h"		/* hunt_server */
#include "s_stats.h"		/* tstats */
#include "s_user.h"		/* show_opers */
#include "event.h"		/* events */
#include "linebuf.h"
#include "parse.h"
#include "modules.h"
#include "hook.h"
#include "resv.h"		/* report_resv */
#include "hash.h"
#include "cluster.h"

static void m_stats(struct Client *, struct Client *, int, char **);
static void mo_stats(struct Client *, struct Client *, int, char **);
static void ms_stats(struct Client *, struct Client *, int, char **);

struct Message stats_msgtab = {
	"STATS", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_stats, ms_stats, mo_stats}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&stats_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&stats_msgtab);
}

const char *_version = "$Revision$";
#endif

const char *Lformat = ":%s %d %s %s %u %u %u %u %u :%u %u %s";

static char *parse_stats_args(int, char **, int *, int *);

static void stats_l_list(struct Client *s, char *, int, int, dlink_list *, char);
static void stats_l_client(struct Client *source_p, struct Client *target_p,
				char statchar);
static void stats_spy(struct Client *, char);
static void stats_p_spy(struct Client *);
static void stats_L_spy(struct Client *, char, char *);

/* Heres our struct for the stats table */
struct StatsStruct
{
	char letter;
	void (*handler) ();
	int need_oper;
	int need_admin;
};

static void stats_adns_servers(struct Client *);
static void stats_connect(struct Client *);
static void stats_tdeny(struct Client *);
static void stats_deny(struct Client *);
static void stats_exempt(struct Client *);
static void stats_events(struct Client *);
static void stats_glines(struct Client *);
static void stats_pending_glines(struct Client *);
static void stats_hubleaf(struct Client *);
static void stats_auth(struct Client *);
static void stats_tklines(struct Client *);
static void stats_klines(struct Client *);
static void stats_messages(struct Client *);
static void stats_oper(struct Client *);
static void stats_operedup(struct Client *);
static void stats_ports(struct Client *);
static void stats_resv(struct Client *);
static void stats_usage(struct Client *);
static void stats_tstats(struct Client *);
static void stats_uptime(struct Client *);
static void stats_shared(struct Client *);
static void stats_servers(struct Client *);
static void stats_gecos(struct Client *);
static void stats_class(struct Client *);
static void stats_memory(struct Client *);
static void stats_servlinks(struct Client *);
static void stats_ltrace(struct Client *, int, char **);
static void stats_ziplinks(struct Client *);

static void report_tklines(struct Client *, dlink_list *);

/* This table contains the possible stats items, in order:
 * /stats name,  function to call, operonly? adminonly? /stats letter
 * case only matters in the stats letter column.. -- fl_ */
static struct StatsStruct stats_cmd_table[] = {
	/* letter     function            need_oper need_admin */
	{'a', stats_adns_servers, 1, 1,},
	{'A', stats_adns_servers, 1, 1,},
	{'c', stats_connect, 0, 0,},
	{'C', stats_connect, 0, 0,},
	{'d', stats_tdeny, 1, 0,},
	{'D', stats_deny, 1, 0,},
	{'e', stats_exempt, 1, 0,},
	{'E', stats_events, 1, 1,},
	{'f', fd_dump, 1, 1,},
	{'F', fd_dump, 1, 1,},
	{'g', stats_pending_glines, 1, 0,},
	{'G', stats_glines, 1, 0,},
	{'h', stats_hubleaf, 0, 0,},
	{'H', stats_hubleaf, 0, 0,},
	{'i', stats_auth, 0, 0,},
	{'I', stats_auth, 0, 0,},
	{'k', stats_tklines, 0, 0,},
	{'K', stats_klines, 0, 0,},
	{'l', stats_ltrace, 0, 0,},
	{'L', stats_ltrace, 0, 0,},
	{'m', stats_messages, 0, 0,},
	{'M', stats_messages, 0, 0,},
	{'o', stats_oper, 0, 0,},
	{'O', stats_oper, 0, 0,},
	{'p', stats_operedup, 0, 0,},
	{'P', stats_ports, 0, 0,},
	{'q', stats_resv, 1, 0,},
	{'Q', stats_resv, 1, 0,},
	{'r', stats_usage, 1, 0,},
	{'R', stats_usage, 1, 0,},
	{'t', stats_tstats, 1, 0,},
	{'T', stats_tstats, 1, 0,},
	{'u', stats_uptime, 0, 0,},
	{'U', stats_shared, 1, 0,},
	{'v', stats_servers, 0, 0,},
	{'V', stats_servers, 0, 0,},
	{'x', stats_gecos, 1, 0,},
	{'X', stats_gecos, 1, 0,},
	{'y', stats_class, 0, 0,},
	{'Y', stats_class, 0, 0,},
	{'z', stats_memory, 1, 0,},
	{'Z', stats_ziplinks, 1, 0,},
	{'?', stats_servlinks, 0, 0,},
	{(char) 0, (void (*)()) 0, 0, 0,}
};

/*
 * m_stats by fl_
 *      parv[0] = sender prefix
 *      parv[1] = stat letter/command
 *      parv[2] = (if present) server/mask in stats L
 * 
 * This will search the tables for the appropriate stats letter/command,
 * if found execute it.  
 */

static void
m_stats(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	int i;
	char statchar;
	static time_t last_used = 0;

	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "STATS");
		return;
	}

	statchar = parv[1][0];

	/* Check the user is actually allowed to do /stats, and isnt flooding */
	if((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
	{
		/* safe enough to give this on a local connect only */
		sendto_one(source_p, form_str(RPL_LOAD2HI),
			   me.name, parv[0], "STATS");
		sendto_one(source_p, form_str(RPL_ENDOFSTATS), 
			   me.name, source_p->name, statchar);
		return;
	}
	else
	{
		last_used = CurrentTime;
	}

	/* Is the stats meant for us? */
	if(!ConfigServerHide.disable_remote)
	{
		if(hunt_server(client_p, source_p, ":%s STATS %s :%s", 2, parc, parv) !=
		   HUNTED_ISME)
			return;
	}

	for (i = 0; stats_cmd_table[i].handler; i++)
	{
		if(stats_cmd_table[i].letter == statchar)
		{
			/* The stats table says what privs are needed, so check --fl_ */
			if(stats_cmd_table[i].need_oper || stats_cmd_table[i].need_admin)
			{
				sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name,
					   source_p->name);
				break;
			}

			/* Blah, stats L needs the parameters, none of the others do.. */
			if(statchar == 'L' || statchar == 'l')
				stats_cmd_table[i].handler(source_p, parc, parv);
			else
				stats_cmd_table[i].handler(source_p);
		}
	}

	/* Send the end of stats notice, and the stats_spy */
	sendto_one(source_p, form_str(RPL_ENDOFSTATS), 
		   me.name, source_p->name, statchar);

	if((statchar != 'L') && (statchar != 'l'))
		stats_spy(source_p, statchar);
}

/*
 * mo_stats by fl_
 *      parv[0] = sender prefix
 *      parv[1] = stat letter/command
 *      parv[2] = (if present) server/mask in stats L, or target
 *
 * This will search the tables for the appropriate stats letter,
 * if found execute it.  
 */

static void
mo_stats(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	int i;
	char statchar;

	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "STATS");
		return;
	}

	if(hunt_server(client_p, source_p, ":%s STATS %s :%s", 2, parc, parv) != HUNTED_ISME)
		return;

	statchar = parv[1][0];

	for (i = 0; stats_cmd_table[i].handler; i++)
	{
		if(stats_cmd_table[i].letter == statchar)
		{
			/* The stats table says what privs are needed, so check --fl_ */
			/* Called for remote clients and for local opers, so check need_admin
			 * and need_oper
			 */
			if((stats_cmd_table[i].need_admin && !IsOperAdmin(source_p)) ||
			   (stats_cmd_table[i].need_oper && !IsOper(source_p)))
			{
				sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name,
					   source_p->name);
				break;
			}

			/* Blah, stats L needs the parameters, none of the others do.. */
			if(statchar == 'L' || statchar == 'l')
				stats_cmd_table[i].handler(source_p, parc, parv, statchar);
			else
				stats_cmd_table[i].handler(source_p);
		}
	}

	/* Send the end of stats notice, and the stats_spy */
	sendto_one(source_p, form_str(RPL_ENDOFSTATS),
		   me.name, source_p->name, statchar);

	if((statchar != 'L') && (statchar != 'l'))
		stats_spy(source_p, statchar);
}

/*
 * ms_stats - STATS message handler
 *      parv[0] = sender prefix
 *      parv[1] = statistics selector (defaults to Message frequency)
 *      parv[2] = server name (current server defaulted, if omitted)
 */

static void
ms_stats(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if(hunt_server(client_p, source_p, ":%s STATS %s :%s", 2, parc, parv) != HUNTED_ISME)
		return;

	if(IsClient(source_p))
		mo_stats(client_p, source_p, parc, parv);
}

static void
stats_adns_servers(struct Client *source_p)
{
	report_adns_servers(source_p);
}

static void
stats_connect(struct Client *source_p)
{
	if((ConfigFileEntry.stats_c_oper_only || ConfigServerHide.flatten_links) &&
	   !IsOper(source_p))
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);
	else
		report_configured_links(source_p, CONF_SERVER);
}

/* stats_tdeny()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given temp dline list.
 */
static void
stats_tdeny(struct Client *source_p)
{
	char *name, *host, *pass, *user, *classname;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i, port;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_DLINE)
			{
				aconf = arec->aconf;

				if(!(aconf->flags & CONF_FLAGS_TEMPORARY))
					continue;

				get_printable_conf(aconf, &name, &host, &pass, &user, &port,
						   &classname);

				sendto_one(source_p, form_str(RPL_STATSDLINE), me.name,
					   source_p->name, 'd', host, pass);
			}
		}
	}
}

/* stats_deny()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given dline list.
 */
static void
stats_deny(struct Client *source_p)
{
	char *name, *host, *pass, *user, *classname;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i, port;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_DLINE)
			{
				aconf = arec->aconf;

				if(aconf->flags & CONF_FLAGS_TEMPORARY)
					continue;

				get_printable_conf(aconf, &name, &host, &pass, &user, &port,
						   &classname);

				sendto_one(source_p, form_str(RPL_STATSDLINE), me.name,
					   source_p->name, 'D', host, pass);
			}
		}
	}
}


/* stats_exempt()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given list of exempt blocks
 */
static void
stats_exempt(struct Client *source_p)
{
	char *name, *host, *pass, *user, *classname;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i, port;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_EXEMPTDLINE)
			{
				aconf = arec->aconf;
				get_printable_conf(aconf, &name, &host, &pass,
						   &user, &port, &classname);

				sendto_one(source_p, form_str(RPL_STATSDLINE), me.name,
					   source_p->name, 'e', host, pass);
			}
		}
	}
}


static void
stats_events(struct Client *source_p)
{
	show_events(source_p);
}

/* stats_pending_glines()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown list of pending glines
 */
static void
stats_pending_glines(struct Client *source_p)
{
	if(ConfigFileEntry.glines)
	{
		dlink_node *pending_node;
		struct gline_pending *glp_ptr;
		char timebuffer[MAX_DATE_STRING];
		struct tm *tmptr;

		DLINK_FOREACH(pending_node, pending_glines.head)
		{
			glp_ptr = pending_node->data;

			tmptr = localtime(&glp_ptr->time_request1);
			strftime(timebuffer, MAX_DATE_STRING, "%Y/%m/%d %H:%M:%S", tmptr);

			sendto_one(source_p,
				   ":%s NOTICE %s :1) %s!%s@%s on %s requested gline at %s for %s@%s [%s]",
				   me.name, source_p->name, glp_ptr->oper_nick1,
				   glp_ptr->oper_user1, glp_ptr->oper_host1,
				   glp_ptr->oper_server1, timebuffer,
				   glp_ptr->user, glp_ptr->host, glp_ptr->reason1);

			if(glp_ptr->oper_nick2[0])
			{
				tmptr = localtime(&glp_ptr->time_request2);
				strftime(timebuffer, MAX_DATE_STRING, "%Y/%m/%d %H:%M:%S", tmptr);
				sendto_one(source_p,
					   ":%s NOTICE %s :2) %s!%s@%s on %s requested gline at %s for %s@%s [%s]",
					   me.name, source_p->name, glp_ptr->oper_nick2,
					   glp_ptr->oper_user2, glp_ptr->oper_host2,
					   glp_ptr->oper_server2, timebuffer,
					   glp_ptr->user, glp_ptr->host, glp_ptr->reason2);
			}
		}

		if(dlink_list_length(&pending_glines) > 0)
			sendto_one(source_p, ":%s NOTICE %s :End of Pending G-lines",
				   me.name, source_p->name);
	}
	else
		sendto_one(source_p, ":%s NOTICE %s :This server does not support G-Lines",
			   me.name, source_p->name);

}

/* stats_glines()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown list of glines
 */
static void
stats_glines(struct Client *source_p)
{
	if(ConfigFileEntry.glines)
	{
		dlink_node *gline_node;
		struct ConfItem *kill_ptr;

		DLINK_FOREACH_PREV(gline_node, glines.tail)
		{
			kill_ptr = gline_node->data;

			sendto_one(source_p, form_str(RPL_STATSKLINE), me.name,
				   source_p->name, 'G',
				   kill_ptr->host ? kill_ptr->host : "*",
				   kill_ptr->user ? kill_ptr->user : "*",
				   kill_ptr->passwd ? kill_ptr->passwd : "No Reason");
		}
	}
	else
		sendto_one(source_p, ":%s NOTICE %s :This server does not support G-Lines",
			   me.name, source_p->name);
}


static void
stats_hubleaf(struct Client *source_p)
{
	if((ConfigFileEntry.stats_h_oper_only || ConfigServerHide.flatten_links) &&
	   !IsOper(source_p))
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);
	else
		report_configured_links(source_p, CONF_HUB | CONF_LEAF);
}


static void
stats_auth(struct Client *source_p)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_i_oper_only == 2) && !IsOper(source_p))
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);

	/* If unopered, Only return matching auth blocks */
	else if((ConfigFileEntry.stats_i_oper_only == 1) && !IsOper(source_p))
	{
		struct ConfItem *aconf;
		char *name, *host, *pass, *user, *classname;
		int port;

		if(MyConnect(source_p))
			aconf = find_conf_by_address(source_p->host,
						     &source_p->localClient->ip,
						     CONF_CLIENT,
						     source_p->localClient->aftype,
						     source_p->username);
		else
			aconf = find_conf_by_address(source_p->host, NULL, CONF_CLIENT,
						     0, source_p->username);

		if(aconf == NULL)
			return;

		get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);

		sendto_one(source_p, form_str(RPL_STATSILINE), me.name,
			   source_p->name, (IsConfRestricted(aconf)) ? 'i' : 'I',
			   name, show_iline_prefix(source_p, aconf, user), host, port, classname);
	}

	/* Theyre opered, or allowed to see all auth blocks */
	else
		report_auth(source_p);
}


static void
stats_tklines(struct Client *source_p)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_k_oper_only == 2) && !IsOper(source_p))
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);

	/* If unopered, Only return matching klines */
	else if((ConfigFileEntry.stats_k_oper_only == 1) && !IsOper(source_p))
	{
		struct ConfItem *aconf;
		char *name, *host, *pass, *user, *classname;
		int port;

		if(MyConnect(source_p))
			aconf = find_conf_by_address(source_p->host,
						     &source_p->localClient->ip,
						     CONF_KILL,
						     source_p->localClient->aftype,
						     source_p->username);
		else
			aconf = find_conf_by_address(source_p->host, NULL, CONF_KILL,
						     0, source_p->username);

		if(aconf == NULL)
			return;

		/* dont report a permanent kline as a tkline */
		if((aconf->flags & CONF_FLAGS_TEMPORARY) == 0)
			return;

		get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);

		sendto_one(source_p, form_str(RPL_STATSKLINE), me.name,
			   source_p->name, 'k', host, user, pass);
	}
	/* Theyre opered, or allowed to see all klines */
	else
	{
		report_tklines(source_p, &tkline_min);
		report_tklines(source_p, &tkline_hour);
		report_tklines(source_p, &tkline_day);
		report_tklines(source_p, &tkline_week);
	}
}

static void
report_tklines(struct Client *source_p, dlink_list * tkline_list)
{
	struct ConfItem *aconf;
	dlink_node *ptr;
	char *name;
	char *host;
	char *pass;
	char *user;
	char *classname;
	int port;

	DLINK_FOREACH(ptr, tkline_list->head)
	{
		aconf = ptr->data;

		get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);

		sendto_one(source_p, form_str(RPL_STATSKLINE), me.name, source_p->name,
			   'k', host, user, pass);
	}
}

static void
stats_klines(struct Client *source_p)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_k_oper_only == 2) && !IsOper(source_p))
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);

	/* If unopered, Only return matching klines */
	else if((ConfigFileEntry.stats_k_oper_only == 1) && !IsOper(source_p))
	{
		struct ConfItem *aconf;
		char *name, *host, *pass, *user, *classname;
		int port;

		/* search for a kline */
		if(MyConnect(source_p))
			aconf = find_conf_by_address(source_p->host,
						     &source_p->localClient->ip,
						     CONF_KILL,
						     source_p->localClient->aftype,
						     source_p->username);
		else
			aconf = find_conf_by_address(source_p->host, NULL, CONF_KILL,
						     0, source_p->username);

		if(aconf == NULL)
			return;

		/* dont report a tkline as a kline */
		if(aconf->flags & CONF_FLAGS_TEMPORARY)
			return;

		get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);

		sendto_one(source_p, form_str(RPL_STATSKLINE), me.name,
			   source_p->name, 'K', host, user, pass);
	}
	/* Theyre opered, or allowed to see all klines */
	else
		report_Klines(source_p);
}

static void
stats_messages(struct Client *source_p)
{
	report_messages(source_p);
}

static void
stats_oper(struct Client *source_p)
{
	if(!IsOper(source_p) && ConfigFileEntry.stats_o_oper_only)
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);
	else
		report_configured_links(source_p, CONF_OPERATOR);
}


/* stats_operedup()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown a list of active opers
 */
static void
stats_operedup(struct Client *source_p)
{
	struct Client *target_p;
	struct ConfItem *aconf;
	dlink_node *oper_ptr;
	dlink_node *ptr;
	int j = 0;

	DLINK_FOREACH(oper_ptr, oper_list.head)
	{
		target_p = oper_ptr->data;

		if(MyClient(source_p) && IsOper(source_p))
		{
			ptr = target_p->localClient->confs.head;
			aconf = ptr->data;

			sendto_one(source_p, ":%s %d %s p :[%c][%s] %s (%s@%s) Idle: %d",
				   me.name, RPL_STATSDEBUG, source_p->name,
				   IsAdmin(target_p) ? 'A' : 'O',
				   oper_privs_as_string(target_p, aconf->port),
				   target_p->name, target_p->username, target_p->host,
				   (int) (CurrentTime - target_p->user->last));
			j++;
		}
		else
		{
			if (!IsHiddenOper(target_p) || IsOper(source_p))
			{
				sendto_one(source_p, ":%s %d %s p :[%c] %s (%s@%s) Idle: %d",
					   me.name, RPL_STATSDEBUG, source_p->name,
					   IsAdmin(target_p) ? 'A' : 'O',
					   target_p->name, target_p->username, target_p->host,
					   (int) (CurrentTime - target_p->user->last));
				j++;
			}
		}
	}

	sendto_one(source_p, ":%s %d %s p :%d OPER(s)", me.name, RPL_STATSDEBUG, source_p->name, j);

	stats_p_spy(source_p);
}

static void
stats_ports(struct Client *source_p)
{
	if(!IsOper(source_p) && ConfigFileEntry.stats_P_oper_only)
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);
	else
		show_ports(source_p);
}

static void
stats_resv(struct Client *source_p)
{
	report_resv(source_p);
}

static void
stats_usage(struct Client *source_p)
{
	send_usage(source_p);
}

static void
stats_tstats(struct Client *source_p)
{
	tstats(source_p);
}

static void
stats_uptime(struct Client *source_p)
{
	time_t now;

	now = CurrentTime - me.since;
	sendto_one(source_p, form_str(RPL_STATSUPTIME), me.name, source_p->name,
		   now / 86400, (now / 3600) % 24, (now / 60) % 60, now % 60);
	if(!ConfigServerHide.disable_remote || IsOper(source_p))
		sendto_one(source_p, form_str(RPL_STATSCONN), me.name, source_p->name,
			   MaxConnectionCount, MaxClientCount, Count.totalrestartcount);
}

struct shared_flags
{
	int flag;
	char has;
	char hasnt;
};

static struct shared_flags shared_flagtable[] = {
	{ OPER_K,		'K', 'k' },
	{ OPER_UNKLINE,		'U', 'u' },
	{ OPER_XLINE,		'X', 'x' },
	{ OPER_XLINE,		'Y', 'y' },
	{ OPER_RESV,		'Q', 'q' },
	{ OPER_RESV,		'R', 'r' },
	{ 0,			'\0', '\0' }
};
static struct shared_flags cluster_flagtable[] = {
	{ CLUSTER_KLINE,	'K', 'k' },
	{ CLUSTER_UNKLINE,	'U', 'u' },
	{ CLUSTER_XLINE,	'X', 'x' },
	{ CLUSTER_UNXLINE,	'Y', 'y' },
	{ CLUSTER_RESV,		'Q', 'q' },
	{ CLUSTER_UNRESV,	'R', 'r' },
	{ CLUSTER_LOCOPS,	'L', 'l' },
	{ 0,			'\0', '\0' }
};

static void
stats_shared(struct Client *source_p)
{
	struct ConfItem *aconf;
	struct cluster *clptr;
	dlink_node *ptr;
	char buf[9];
	char *p;
	char *name, *host, *pass, *user, *classname;
	int port;
	int i;

	for (aconf = u_conf; aconf != NULL; aconf = aconf->next)
	{
		if(aconf->status & CONF_ULINE)
		{
			p = buf;

			get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);

			*p++ = 'c';

			for(i = 0; shared_flagtable[i].flag != 0; i++)
			{
				if(port & shared_flagtable[i].flag)
					*p++ = shared_flagtable[i].has;
				else
					*p++ = shared_flagtable[i].hasnt;
			}

			*p++ = 'l';
			*p = '\0';

			sendto_one(source_p, form_str(RPL_STATSULINE), me.name,
				   source_p->name, name, user, host, buf);
		}
	}

	DLINK_FOREACH(ptr, cluster_list.head)
	{
		clptr = ptr->data;
		p = buf;

		*p++ = 'C';

		for(i = 0; cluster_flagtable[i].flag != 0; i++)
		{
			if(clptr->type & cluster_flagtable[i].flag)
				*p++ = cluster_flagtable[i].has;
			else
				*p++ = cluster_flagtable[i].hasnt;
		}

		*p++ = '\0';

		sendto_one(source_p, form_str(RPL_STATSULINE),
			   me.name, source_p->name, clptr->name, 
			   "*", "*", buf);
	}
}

/* stats_servers()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown lists of who connected servers
 */
static void
stats_servers(struct Client *source_p)
{
	struct Client *target_p;
	dlink_node *ptr;
	time_t seconds;
	int days, hours, minutes;
	int j = 0;

	if(ConfigServerHide.flatten_links && !IsOper(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);
		return;
	}

	DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		j++;
		seconds = CurrentTime - target_p->firsttime;

		days = (int) (seconds / 86400);
		seconds %= 86400;
		hours = (int) (seconds / 3600);
		seconds %= 3600;
		minutes = (int) (seconds / 60);
		seconds %= 60;

		sendto_one(source_p, ":%s %d %s V :%s (%s!%s@%s) Idle: %d SendQ: %d "
			   "Connected: %d day%s, %d:%02d:%02ld",
			   me.name, RPL_STATSDEBUG, source_p->name,
			   target_p->name,
			   (target_p->serv->by[0] ? target_p->serv->by : "Remote."),
			   "*", "*", (int) (CurrentTime - target_p->lasttime),
			   (int) linebuf_len(&target_p->localClient->buf_sendq),
			   days, (days == 1) ? "" : "s", hours, minutes, seconds);
	}

	sendto_one(source_p, ":%s %d %s V :%d Server(s)", me.name, RPL_STATSDEBUG, source_p->name, j);
}

static void
stats_gecos(struct Client *source_p)
{
	struct ConfItem *aconf;
	char *name, *host, *pass, *user, *classname;
	int port;

	for (aconf = x_conf; aconf != NULL; aconf = aconf->next)
	{
		if(aconf->status & CONF_XLINE)
		{
			get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);

			sendto_one(source_p, form_str(RPL_STATSXLINE), 
				   me.name, source_p->name, 'X',
				   port, name, pass);
		}
	}

	for(aconf = x_temp_conf; aconf != NULL; aconf = aconf->next)
	{
		if(aconf->status & CONF_XLINE)
		{
			get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);

			sendto_one(source_p, form_str(RPL_STATSXLINE),
				   me.name, source_p->name, 'x',
				   port, name, pass);
		}
	}
}

static void
stats_class(struct Client *source_p)
{
	if(ConfigFileEntry.stats_y_oper_only && !IsOper(source_p))
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);
	else
		report_classes(source_p);
}

static void
stats_memory(struct Client *source_p)
{
	count_memory(source_p);
}

static void
stats_ziplinks(struct Client *source_p)
{
	dlink_node *ptr;
	struct Client *target_p;
	int sent_data = 0;

	DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;
		if(IsCapable(target_p, CAP_ZIP))
		{
			/* we use memcpy(3) and a local copy of the structure to
			 * work around a register use bug on GCC on the SPARC.
			 * -jmallett, 04/27/2002
			 */
			struct ZipStats zipstats;
			memcpy(&zipstats, &target_p->localClient->zipstats,
			       sizeof(struct ZipStats));
			sendto_one(source_p,
				   ":%s %d %s Z :ZipLinks stats for %s send[%.2f%% compression (%lu bytes data/%lu bytes wire)] recv[%.2f%% compression (%lu bytes data/%lu bytes wire)]",
				   me.name, RPL_STATSDEBUG, source_p->name, target_p->name,
				   zipstats.out_ratio, zipstats.out, zipstats.out_wire,
				   zipstats.in_ratio, zipstats.in, zipstats.in_wire);
			sent_data++;
		}
	}
	sendto_one(source_p, ":%s %d %s Z :%u ziplink(s)",
		   me.name, RPL_STATSDEBUG, source_p->name, sent_data);
}

static void
stats_servlinks(struct Client *source_p)
{
	static char Sformat[] = ":%s %d %s %s %u %u %u %u %u :%u %u %s";
	long uptime, sendK, receiveK;
	struct Client *target_p;
	dlink_node *ptr;
	int j = 0;

	if(ConfigServerHide.flatten_links && !IsOper(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, source_p->name);
		return;
	}

	sendK = receiveK = 0;

	DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		j++;
		sendK += target_p->localClient->sendK;
		receiveK += target_p->localClient->receiveK;

		sendto_one(source_p, Sformat, me.name, RPL_STATSLINKINFO, source_p->name,
#ifndef HIDE_SERVERS_IPS
			   IsOperAdmin(source_p) ? get_client_name(target_p, SHOW_IP) :
#endif
			   get_client_name(target_p, MASK_IP),
			   (int) linebuf_len(&target_p->localClient->buf_sendq),
			   (int) target_p->localClient->sendM,
			   (int) target_p->localClient->sendK,
			   (int) target_p->localClient->receiveM,
			   (int) target_p->localClient->receiveK,
			   CurrentTime - target_p->firsttime,
			   (CurrentTime > target_p->since) ? (CurrentTime - target_p->since) : 0,
			   IsOper(source_p) ? show_capabilities(target_p) : "TS");
	}

	sendto_one(source_p, ":%s %d %s ? :%u total server(s)",
		   me.name, RPL_STATSDEBUG, source_p->name, j);

	sendto_one(source_p, ":%s %d %s ? :Sent total : %7.2f %s",
		   me.name, RPL_STATSDEBUG, source_p->name, _GMKv(sendK), _GMKs(sendK));
	sendto_one(source_p, ":%s %d %s ? :Recv total : %7.2f %s",
		   me.name, RPL_STATSDEBUG, source_p->name, _GMKv(receiveK), _GMKs(receiveK));

	uptime = (CurrentTime - me.since);

	sendto_one(source_p, ":%s %d %s ? :Server send: %7.2f %s (%4.1f K/s)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   _GMKv(me.localClient->sendK), _GMKs(me.localClient->sendK),
		   (float) ((float) me.localClient->sendK / (float) uptime));
	sendto_one(source_p, ":%s %d %s ? :Server recv: %7.2f %s (%4.1f K/s)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   _GMKv(me.localClient->receiveK),
		   _GMKs(me.localClient->receiveK),
		   (float) ((float) me.localClient->receiveK / (float) uptime));
}

static void
stats_ltrace(struct Client *source_p, int parc, char *parv[])
{
	int doall = 0;
	int wilds = 0;
	char *name = NULL;
	char statchar;

	name = parse_stats_args(parc, parv, &doall, &wilds);

	if(name)
	{
		statchar = parv[1][0];

		stats_L_spy(source_p, statchar, name);

		/* on a single client, this is simple */
		if(!doall && !wilds)
		{
			struct Client *target_p;

			target_p = find_client(name);

			if(target_p != NULL && IsPerson(target_p))
				stats_l_client(source_p, target_p, statchar);
			
			return;
		}

		/* give non-opers, or remote opers a limited output of 
		 * servers, opers and themselves (if local)
		 */
		if(!MyOper(source_p))
		{
			/* if we're in shide, give the users nothing. */
			if(ConfigServerHide.hide_servers)
				return;

			stats_l_list(source_p, name, doall, wilds, &serv_list, statchar);
			stats_l_list(source_p, name, doall, wilds, &oper_list, statchar);

			if(MyClient(source_p))
			{
				if(doall || (wilds && match(name, source_p->name)))
					stats_l_client(source_p, source_p, statchar);
			}

			return;
		}
					
			
		stats_l_list(source_p, name, doall, wilds, &unknown_list, statchar);
		stats_l_list(source_p, name, doall, wilds, &lclient_list, statchar);
		stats_l_list(source_p, name, doall, wilds, &serv_list, statchar);
	}
	else
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "STATS");

	return;
}


static void
stats_l_list(struct Client *source_p, char *name, int doall, int wilds,
	     dlink_list * list, char statchar)
{
	dlink_node *ptr;
	struct Client *target_p;

	/* send information about connections which match.  note, we
	 * dont need tests for IsInvisible(), because non-opers will
	 * never get here for normal clients --fl
	 */
	DLINK_FOREACH(ptr, list->head)
	{
		target_p = ptr->data;

		if(!doall && wilds && !match(name, target_p->name))
			continue;

		stats_l_client(source_p, target_p, statchar);
	}
}

void
stats_l_client(struct Client *source_p, struct Client *target_p,
		char statchar)
{
	if(IsAnyServer(target_p))
	{
		sendto_one(source_p, Lformat, me.name, RPL_STATSLINKINFO,
			   source_p->name,
#ifndef HIDE_SERVERS_IPS
			   IsOperAdmin(source_p) ? get_client_name(target_p, SHOW_IP) :
#endif
			   get_client_name(target_p, MASK_IP),
			   (int) linebuf_len(&target_p->localClient->buf_sendq),
			   (int) target_p->localClient->sendM,
			   (int) target_p->localClient->sendK,
			   (int) target_p->localClient->receiveM,
			   (int) target_p->localClient->receiveK,
			   CurrentTime - target_p->firsttime,
			   (CurrentTime >
			    target_p->since) ? (CurrentTime - target_p->since) : 0,
			   IsOper(source_p) ? show_capabilities(target_p) : "-");
	}

	else if(IsIPSpoof(target_p))
	{
		sendto_one(source_p, Lformat, me.name, RPL_STATSLINKINFO,
			   source_p->name,
#ifndef HIDE_SPOOF_IPS
			   IsOper(source_p) ?
			   (IsUpper(statchar) ?
			    get_client_name(target_p, SHOW_IP) :
			    get_client_name(target_p, HIDE_IP)) :
#endif
			   get_client_name(target_p, MASK_IP),
			   (int) linebuf_len(&target_p->localClient->buf_sendq),
			   (int) target_p->localClient->sendM,
			   (int) target_p->localClient->sendK,
			   (int) target_p->localClient->receiveM,
			   (int) target_p->localClient->receiveK,
			   CurrentTime - target_p->firsttime,
			   (CurrentTime >
			    target_p->since) ? (CurrentTime - target_p->since) : 0,
			   "-");
	}

	else
	{
		sendto_one(source_p, Lformat, me.name, RPL_STATSLINKINFO,
			   source_p->name,
			   IsUpper(statchar) ?
			   get_client_name(target_p, SHOW_IP) :
			   get_client_name(target_p, HIDE_IP),
			   (int) linebuf_len(&target_p->localClient->buf_sendq),
			   (int) target_p->localClient->sendM,
			   (int) target_p->localClient->sendK,
			   (int) target_p->localClient->receiveM,
			   (int) target_p->localClient->receiveK,
			   CurrentTime - target_p->firsttime,
			   (CurrentTime >
			    target_p->since) ? (CurrentTime - target_p->since) : 0,
			   "-");
	}
}

/*
 * stats_spy
 *
 * inputs	- pointer to client doing the /stats
 *		- char letter they are doing /stats on
 * output	- none
 * side effects -
 * This little helper function reports to opers if configured.
 * personally, I don't see why opers need to see stats requests
 * at all. They are just "noise" to an oper, and users can't do
 * any damage with stats requests now anyway. So, why show them?
 * -Dianora
 */
static void
stats_spy(struct Client *source_p, char statchar)
{
	struct hook_stats_data data;

	data.source_p = source_p;
	data.statchar = statchar;
	data.name = NULL;

	hook_call_event(h_doing_stats_id, &data);
}

/* stats_p_spy()
 *
 * input	- pointer to client doing stats
 * ouput	-
 * side effects - call hook doing_stats_p
 */
static void
stats_p_spy(struct Client *source_p)
{
	struct hook_stats_data data;

	data.source_p = source_p;
	data.name = NULL;
	data.statchar = 'p';

	hook_call_event(h_doing_stats_p_id, &data);
}

/* 
 * stats_L_spy
 * 
 * inputs	- pointer to source_p, client doing stats L
 *		- stat that they are doing 'L' 'l'
 * 		- any name argument they have given
 * output	- NONE
 * side effects	- a notice is sent to opers, IF spy mode is configured
 * 		  in the conf file.
 */
static void
stats_L_spy(struct Client *source_p, char statchar, char *name)
{
	struct hook_stats_data data;

	data.source_p = source_p;
	data.statchar = statchar;
	data.name = name;

	hook_call_event(h_doing_stats_id, &data);
}

/*
 * parse_stats_args
 *
 * inputs	- arg count
 *		- args
 *		- doall flag
 *		- wild card or not
 * output	- pointer to name to use
 * side effects	-
 * common parse routine for m_stats args
 * 
 */
static char *
parse_stats_args(int parc, char *parv[], int *doall, int *wilds)
{
	char *name;

	if(parc > 2)
	{
		name = parv[2];
		if(!irccmp(name, me.name))
			*doall = 2;
		else if(match(name, me.name))
			*doall = 1;
		if(strchr(name, '*') || strchr(name, '?'))
			*wilds = 1;

		return (name);
	}
	else
		return (NULL);
}
