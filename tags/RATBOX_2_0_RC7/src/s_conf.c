/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_conf.c: Configuration file functions.
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
#include "ircd_defs.h"
#include "tools.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "s_stats.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "event.h"
#include "hash.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "listener.h"
#include "hostmask.h"
#include "modules.h"
#include "numeric.h"
#include "commio.h"
#include "s_log.h"
#include "send.h"
#include "s_gline.h"
#include "memory.h"
#include "balloc.h"
#include "patricia.h"
#include "reject.h"
#include "cache.h"

struct config_server_hide ConfigServerHide;

extern int yyparse();		/* defined in y.tab.c */
extern char linebuf[];

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

static BlockHeap *confitem_heap = NULL;

dlink_list tkline_min;
dlink_list tkline_hour;
dlink_list tkline_day;
dlink_list tkline_week;

dlink_list tdline_min;
dlink_list tdline_hour;
dlink_list tdline_day;
dlink_list tdline_week;

/* internally defined functions */
static void set_default_conf(void);
static void validate_conf(void);
static void read_conf(FBFILE *);
static void clear_out_old_conf(void);

static void expire_tkline(dlink_list *, int);
static void expire_tdline(dlink_list *, int);

FBFILE *conf_fbfile_in;
extern char yytext[];

static int verify_access(struct Client *client_p, const char *username);
static int attach_iline(struct Client *, struct ConfItem *);

void
init_s_conf(void)
{
	confitem_heap = BlockHeapCreate(sizeof(struct ConfItem), CONFITEM_HEAP_SIZE);
}

/*
 * make_conf
 *
 * inputs	- none
 * output	- pointer to new conf entry
 * side effects	- none
 */
struct ConfItem *
make_conf()
{
	struct ConfItem *aconf;

	aconf = BlockHeapAlloc(confitem_heap);
	aconf->status = CONF_ILLEGAL;
	return (aconf);
}

/*
 * free_conf
 *
 * inputs	- pointer to conf to free
 * output	- none
 * side effects	- crucial password fields are zeroed, conf is freed
 */
void
free_conf(struct ConfItem *aconf)
{
	s_assert(aconf != NULL);
	if(aconf == NULL)
		return;

	/* security.. */
	if(aconf->passwd)
		memset(aconf->passwd, 0, strlen(aconf->passwd));
	if(aconf->spasswd)
		memset(aconf->spasswd, 0, strlen(aconf->spasswd));

	MyFree(aconf->passwd);
	MyFree(aconf->spasswd);
	MyFree(aconf->name);
	MyFree(aconf->className);
	MyFree(aconf->user);
	MyFree(aconf->host);

	BlockHeapFree(confitem_heap, aconf);
}

/*
 * check_client
 *
 * inputs	- pointer to client
 * output	- 0 = Success
 * 		  NOT_AUTHORISED (-1) = Access denied (no I line match)
 * 		  SOCKET_ERROR   (-2) = Bad socket.
 * 		  I_LINE_FULL    (-3) = I-line is full
 *		  TOO_MANY       (-4) = Too many connections from hostname
 * 		  BANNED_CLIENT  (-5) = K-lined
 * side effects - Ordinary client access check.
 *		  Look for conf lines which have the same
 * 		  status as the flags passed.
 */
int
check_client(struct Client *client_p, struct Client *source_p, const char *username)
{
	int i;

	ClearAccess(source_p);

	if((i = verify_access(source_p, username)))
	{
		ilog(L_FUSER, "Access denied: %s[%s]", 
		     source_p->name, source_p->sockhost);
	}
	
	switch (i)
	{
	case SOCKET_ERROR:
		exit_client(client_p, source_p, &me, "Socket Error");
		break;

	case TOO_MANY_LOCAL:
		sendto_realops_flags(UMODE_FULL, L_ALL,
				"Too many local connections for %s!%s%s@%s",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username, source_p->sockhost);

		ilog(L_FUSER, "Too many local connections from %s!%s%s@%s",
			source_p->name, IsGotId(source_p) ? "" : "~",
			source_p->username, source_p->sockhost);	

		ServerStats->is_ref++;
		exit_client(client_p, source_p, &me, "Too many host connections (local)");
		break;

	case TOO_MANY_GLOBAL:
		sendto_realops_flags(UMODE_FULL, L_ALL,
				"Too many global connections for %s!%s%s@%s",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username, source_p->sockhost);
		ilog(L_FUSER, "Too many global connections from %s!%s%s@%s",
			source_p->name, IsGotId(source_p) ? "" : "~",
			source_p->username, source_p->sockhost);

		ServerStats->is_ref++;
		exit_client(client_p, source_p, &me, "Too many host connections (global)");
		break;

	case TOO_MANY_IDENT:
		sendto_realops_flags(UMODE_FULL, L_ALL,
				"Too many user connections for %s!%s%s@%s",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username, source_p->sockhost);
		ilog(L_FUSER, "Too many user connections from %s!%s%s@%s",
			source_p->name, IsGotId(source_p) ? "" : "~",
			source_p->username, source_p->sockhost);

		ServerStats->is_ref++;
		exit_client(client_p, source_p, &me, "Too many user connections (global)");
		break;

	case I_LINE_FULL:
		sendto_realops_flags(UMODE_FULL, L_ALL,
				"I-line is full for %s!%s%s@%s (%s).",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username, source_p->host,
				source_p->sockhost);

		ilog(L_FUSER, "Too many connections from %s!%s%s@%s.", 
			source_p->name, IsGotId(source_p) ? "" : "~",
			source_p->username, source_p->sockhost);

		ServerStats->is_ref++;
		exit_client(client_p, source_p, &me,
			    "No more connections allowed in your connection class");
		break;

	case NOT_AUTHORISED:
		{
			int port = -1;
#ifdef IPV6
			if(source_p->localClient->ip.ss_family == AF_INET6)
				port = ((struct sockaddr_in6 *)&source_p->localClient->listener)->sin6_port;
			else
#endif
				port = ((struct sockaddr_in *)&source_p->localClient->listener)->sin_port;
			
			ServerStats->is_ref++;
			/* jdc - lists server name & port connections are on */
			/*       a purely cosmetical change */
			/* why ipaddr, and not just source_p->sockhost? --fl */
#if 0
			static char ipaddr[HOSTIPLEN];
			inetntop_sock(&source_p->localClient->ip, ipaddr, sizeof(ipaddr));
#endif
			sendto_realops_flags(UMODE_UNAUTH, L_ALL,
					"Unauthorised client connection from "
					"%s!%s%s@%s [%s] on [%s/%u].",
					source_p->name, IsGotId(source_p) ? "" : "~",
					source_p->username, source_p->host,
					source_p->sockhost,
					source_p->localClient->listener->name, port);

			ilog(L_FUSER,
				"Unauthorised client connection from %s!%s%s@%s on [%s/%u].",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username, source_p->sockhost,
				source_p->localClient->listener->name, port);
			add_reject(client_p);
			exit_client(client_p, source_p, &me,
				    "You are not authorised to use this server");
			break;
		}
	case BANNED_CLIENT:
		add_reject(client_p);
		exit_client(client_p, client_p, &me, "*** Banned ");
		ServerStats->is_ref++;
		break;

	case 0:
	default:
		break;
	}
	return (i);
}

/*
 * verify_access
 *
 * inputs	- pointer to client to verify
 *		- pointer to proposed username
 * output	- 0 if success -'ve if not
 * side effect	- find the first (best) I line to attach.
 */
static int
verify_access(struct Client *client_p, const char *username)
{
	struct ConfItem *aconf;
	char non_ident[USERLEN + 1];

	if(IsGotId(client_p))
	{
		aconf = find_address_conf(client_p->host, client_p->username,
					  &client_p->localClient->ip,client_p->localClient->ip.ss_family);
	}
	else
	{
		strlcpy(non_ident, "~", sizeof(non_ident));
		strlcat(non_ident, username, sizeof(non_ident));
		aconf = find_address_conf(client_p->host, non_ident, &client_p->localClient->ip,client_p->localClient->ip.ss_family);
	}

	if(aconf == NULL)
		return NOT_AUTHORISED;

	if(aconf->status & CONF_CLIENT)
	{
		if(aconf->flags & CONF_FLAGS_REDIR)
		{
			sendto_one(client_p, form_str(RPL_REDIR),
					me.name, client_p->name,
					aconf->name ? aconf->name : "", aconf->port);
			return (NOT_AUTHORISED);
		}


		if(IsConfDoIdentd(aconf))
			SetNeedId(client_p);

		/* Thanks for spoof idea amm */
		if(IsConfDoSpoofIp(aconf))
		{
			char *p;

			if(IsConfSpoofNotice(aconf))
			{
				sendto_realops_flags(UMODE_ALL, L_ALL,
						"%s spoofing: %s as %s",
						client_p->name,
#ifdef HIDE_SPOOF_IPS
						aconf->name,
#else
						client_p->host,
#endif
						aconf->name);
			}

			/* user@host spoof */
			if((p = strchr(aconf->name, '@')) != NULL)
			{
				char *host = p+1;
				*p = '\0';

				strlcpy(client_p->username, aconf->name,
					sizeof(client_p->username));
				strlcpy(client_p->host, host,
					sizeof(client_p->host));
				*p = '@';
			}
			else
				strlcpy(client_p->host, aconf->name, sizeof(client_p->host));

			SetIPSpoof(client_p);
		}
		return (attach_iline(client_p, aconf));
	}
	else if(aconf->status & CONF_KILL)
	{
		if(ConfigFileEntry.kline_with_reason)
		{
			sendto_one(client_p,
					":%s NOTICE %s :*** Banned %s",
					me.name, client_p->name, aconf->passwd);
		}
		return (BANNED_CLIENT);
	}
	else if(aconf->status & CONF_GLINE)
	{
		sendto_one(client_p, ":%s NOTICE %s :*** G-lined", me.name, client_p->name);

		if(ConfigFileEntry.kline_with_reason)
			sendto_one(client_p,
					":%s NOTICE %s :*** Banned %s",
					me.name, client_p->name, aconf->passwd);

		return (BANNED_CLIENT);
	}

	return NOT_AUTHORISED;
}


/*
 * add_ip_limit
 * 
 * Returns 1 if successful 0 if not
 *
 * This checks if the user has exceed the limits for their class
 * unless of course they are exempt..
 */

static int
add_ip_limit(struct Client *client_p, struct ConfItem *aconf)
{
	patricia_node_t *pnode;

	/* If the limits are 0 don't do anything.. */
	if(ConfCidrAmount(aconf) == 0 || ConfCidrBitlen(aconf) == 0)
		return -1;

	pnode = match_ip(ConfIpLimits(aconf), &client_p->localClient->ip);

	if(pnode == NULL)
		pnode = make_and_lookup_ip(ConfIpLimits(aconf), &client_p->localClient->ip, ConfCidrBitlen(aconf));

	s_assert(pnode != NULL);

	if(pnode != NULL)
	{
		if(((long) pnode->data) >= ConfCidrAmount(aconf)
		   && !IsConfExemptLimits(aconf))
		{
			/* This should only happen if the limits are set to 0 */
			if((unsigned long) pnode->data == 0)
			{
				patricia_remove(ConfIpLimits(aconf), pnode);
			}
			return (0);
		}

		pnode->data++;
	}
	return 1;
}

static void
remove_ip_limit(struct Client *client_p, struct ConfItem *aconf)
{
	patricia_node_t *pnode;

	/* If the limits are 0 don't do anything.. */
	if(ConfCidrAmount(aconf) == 0 || ConfCidrBitlen(aconf) == 0)
		return;

	pnode = match_ip(ConfIpLimits(aconf), &client_p->localClient->ip);
	if(pnode == NULL)
		return;

	pnode->data--;
	if(((unsigned long) pnode->data) == 0)
	{
		patricia_remove(ConfIpLimits(aconf), pnode);
	}

}

/*
 * attach_iline
 *
 * inputs	- client pointer
 *		- conf pointer
 * output	-
 * side effects	- do actual attach
 */
static int
attach_iline(struct Client *client_p, struct ConfItem *aconf)
{
	struct Client *target_p;
	dlink_node *ptr;
	int local_count = 0;
	int global_count = 0;
	int ident_count = 0;
	int unidented = 0;

	if(IsConfExemptLimits(aconf))
		return (attach_conf(client_p, aconf));

	if(*client_p->username == '~')
		unidented = 1;


	/* find_hostname() returns the head of the list to search */
	DLINK_FOREACH(ptr, find_hostname(client_p->host))
	{
		target_p = ptr->data;

		if(irccmp(client_p->host, target_p->host) != 0)
			continue;

		if(MyConnect(target_p))
			local_count++;

		global_count++;

		if(unidented)
		{
			if(*target_p->username == '~')
				ident_count++;
		}
		else if(irccmp(target_p->username, client_p->username) == 0)
			ident_count++;

		if(ConfMaxLocal(aconf) && local_count >= ConfMaxLocal(aconf))
			return (TOO_MANY_LOCAL);
		else if(ConfMaxGlobal(aconf) && global_count >= ConfMaxGlobal(aconf))
			return (TOO_MANY_GLOBAL);
		else if(ConfMaxIdent(aconf) && ident_count >= ConfMaxIdent(aconf))
			return (TOO_MANY_IDENT);
	}


	return (attach_conf(client_p, aconf));
}

/*
 * detach_conf
 *
 * inputs	- pointer to client to detach
 * output	- 0 for success, -1 for failure
 * side effects	- Disassociate configuration from the client.
 *		  Also removes a class from the list if marked for deleting.
 */
int
detach_conf(struct Client *client_p)
{
	struct ConfItem *aconf;

	aconf = client_p->localClient->att_conf;

	if(aconf != NULL)
	{
		if(ClassPtr(aconf))
		{
			remove_ip_limit(client_p, aconf);

			if(ConfCurrUsers(aconf) > 0)
				--ConfCurrUsers(aconf);

			if(ConfMaxUsers(aconf) == -1 && ConfCurrUsers(aconf) == 0)
			{
				free_class(ClassPtr(aconf));
				ClassPtr(aconf) = NULL;
			}

		}

		aconf->clients--;
		if(!aconf->clients && IsIllegal(aconf))
			free_conf(aconf);

		client_p->localClient->att_conf = NULL;
		return 0;
	}

	return -1;
}

/*
 * attach_conf
 * 
 * inputs	- client pointer
 * 		- conf pointer
 * output	-
 * side effects - Associate a specific configuration entry to a *local*
 *                client (this is the one which used in accepting the
 *                connection). Note, that this automatically changes the
 *                attachment if there was an old one...
 */
int
attach_conf(struct Client *client_p, struct ConfItem *aconf)
{
	if(IsIllegal(aconf))
		return (NOT_AUTHORISED);

	if(ClassPtr(aconf))
	{
		if(!add_ip_limit(client_p, aconf))
			return (TOO_MANY_LOCAL);
	}

	if((aconf->status & CONF_CLIENT) &&
	   ConfCurrUsers(aconf) >= ConfMaxUsers(aconf) && ConfMaxUsers(aconf) > 0)
	{
		if(!IsConfExemptLimits(aconf))
		{
			return (I_LINE_FULL);
		}
		else
		{
			sendto_one(client_p, ":%s NOTICE %s :*** I: line is full, but you have an >I: line!", 
			                      me.name, client_p->name);
			SetExemptLimits(client_p);
		}

	}

	if(client_p->localClient->att_conf != NULL)
		detach_conf(client_p);

	client_p->localClient->att_conf = aconf;

	aconf->clients++;
	ConfCurrUsers(aconf)++;
	return (0);
}

/*
 * rehash
 *
 * Actual REHASH service routine. Called with sig == 0 if it has been called
 * as a result of an operator issuing this command, else assume it has been
 * called as a result of the server receiving a HUP signal.
 */
int
rehash(int sig)
{
	if(sig != 0)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "Got signal SIGHUP, reloading ircd conf. file");
	}

	restart_resolver();
	/* don't close listeners until we know we can go ahead with the rehash */
	read_conf_files(NO);

	if(ServerInfo.description != NULL)
	{
		strlcpy(me.info, ServerInfo.description, sizeof(me.info));
	}

	check_banned_lines();
	open_logfiles();
	return (0);
}

/*
 * set_default_conf()
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	- Set default values here.
 *		  This is called **PRIOR** to parsing the
 *		  configuration file.  If you want to do some validation
 *		  of values later, put them in validate_conf().
 */

#define YES     1
#define NO      0
#define UNSET  -1

static void
set_default_conf(void)
{
	/* ServerInfo.name is not rehashable */
	/* ServerInfo.name = ServerInfo.name; */
	ServerInfo.description = NULL;
	DupString(ServerInfo.network_name, NETWORK_NAME_DEFAULT);
	DupString(ServerInfo.network_desc, NETWORK_DESC_DEFAULT);

	memset(&ServerInfo.ip, 0, sizeof(ServerInfo.ip));
	ServerInfo.specific_ipv4_vhost = 0;
#ifdef IPV6
	memset(&ServerInfo.ip6, 0, sizeof(ServerInfo.ip6));
	ServerInfo.specific_ipv6_vhost = 0;
#endif
	ServerInfo.use_ts6 = YES;

	/* Don't reset hub, as that will break lazylinks */
	/* ServerInfo.hub = NO; */
	AdminInfo.name = NULL;
	AdminInfo.email = NULL;
	AdminInfo.description = NULL;

	strlcpy(ConfigFileEntry.default_operstring, "is an IRC operator",
		sizeof(ConfigFileEntry.default_operstring));
	strlcpy(ConfigFileEntry.default_adminstring, "is a Server Administrator",
		sizeof(ConfigFileEntry.default_adminstring));
	
	ConfigFileEntry.failed_oper_notice = YES;
	ConfigFileEntry.anti_nick_flood = NO;
	ConfigFileEntry.max_nick_time = 20;
	ConfigFileEntry.max_nick_changes = 5;
	ConfigFileEntry.max_accept = 20;
	ConfigFileEntry.nick_delay = 900;	/* 15 minutes */
	ConfigFileEntry.anti_spam_exit_message_time = 0;
	ConfigFileEntry.ts_warn_delta = TS_WARN_DELTA_DEFAULT;
	ConfigFileEntry.ts_max_delta = TS_MAX_DELTA_DEFAULT;
	ConfigFileEntry.client_exit = YES;
	ConfigFileEntry.kline_with_reason = YES;
	ConfigFileEntry.kline_delay = 0;
	ConfigFileEntry.warn_no_nline = YES;
	ConfigFileEntry.non_redundant_klines = YES;
	ConfigFileEntry.stats_e_disabled = NO;
	ConfigFileEntry.stats_o_oper_only = NO;
	ConfigFileEntry.stats_k_oper_only = 1;	/* masked */
	ConfigFileEntry.stats_i_oper_only = 1;	/* masked */
	ConfigFileEntry.stats_P_oper_only = NO;
	ConfigFileEntry.stats_c_oper_only = NO;
	ConfigFileEntry.stats_y_oper_only = NO;
	ConfigFileEntry.stats_h_oper_only = NO;
	ConfigFileEntry.map_oper_only = YES;
	ConfigFileEntry.operspy_admin_only = NO;
	ConfigFileEntry.pace_wait = 10;
	ConfigFileEntry.caller_id_wait = 60;
	ConfigFileEntry.pace_wait_simple = 1;
	ConfigFileEntry.short_motd = NO;
	ConfigFileEntry.no_oper_flood = NO;
	ConfigFileEntry.fname_userlog[0] = '\0';
	ConfigFileEntry.fname_fuserlog[0] = '\0';
	ConfigFileEntry.fname_operlog[0] = '\0';
	ConfigFileEntry.fname_foperlog[0] = '\0';
	ConfigFileEntry.fname_serverlog[0] = '\0';
	ConfigFileEntry.fname_glinelog[0] = '\0';
	ConfigFileEntry.fname_klinelog[0] = '\0';
	ConfigFileEntry.fname_operspylog[0] = '\0';
	ConfigFileEntry.fname_ioerrorlog[0] = '\0';
	ConfigFileEntry.glines = NO;
	ConfigFileEntry.use_egd = NO;
	ConfigFileEntry.gline_time = 12 * 3600;
	ConfigFileEntry.gline_min_cidr = 16;
	ConfigFileEntry.gline_min_cidr6 = 48;
	ConfigFileEntry.hide_error_messages = 1;
	ConfigFileEntry.idletime = 0;
	ConfigFileEntry.dots_in_ident = 0;
	ConfigFileEntry.max_targets = MAX_TARGETS_DEFAULT;
	DupString(ConfigFileEntry.servlink_path, SLPATH);
	ConfigFileEntry.egdpool_path = NULL;
	ConfigFileEntry.use_whois_actually = YES;
	ConfigFileEntry.burst_away = NO;

#ifdef HAVE_LIBZ
	ConfigFileEntry.compression_level = 4;
#endif

	ConfigFileEntry.oper_umodes = UMODE_LOCOPS | UMODE_SERVNOTICE |
		UMODE_OPERWALL | UMODE_WALLOP;
	ConfigFileEntry.oper_only_umodes = UMODE_DEBUG|UMODE_OPERSPY;

	ConfigChannel.use_except = YES;
	ConfigChannel.use_invex = YES;
	ConfigChannel.use_knock = YES;
	ConfigChannel.knock_delay = 300;
	ConfigChannel.knock_delay_channel = 60;
	ConfigChannel.max_chans_per_user = 15;
	ConfigChannel.max_bans = 25;
	ConfigChannel.no_oper_resvs = NO;
	ConfigChannel.burst_topicwho = NO;

	ConfigChannel.default_split_user_count = 0;
	ConfigChannel.default_split_server_count = 0;
	ConfigChannel.default_split_delay = 60;
	ConfigChannel.no_join_on_split = NO;
	ConfigChannel.no_create_on_split = NO;

	ConfigServerHide.flatten_links = 0;
	ConfigServerHide.links_delay = 300;
	ConfigServerHide.hidden = 0;
	ConfigServerHide.disable_hidden = 0;

	ConfigFileEntry.min_nonwildcard = 4;
	ConfigFileEntry.min_nonwildcard_simple = 3;
	ConfigFileEntry.default_floodcount = 8;
	ConfigFileEntry.client_flood = CLIENT_FLOOD_DEFAULT;
	ConfigFileEntry.tkline_expire_notices = 0;

#ifdef IPV6
	ConfigFileEntry.fallback_to_ip6_int = YES;
#endif
}

#undef YES
#undef NO

/*
 * read_conf() 
 *
 *
 * inputs       - file descriptor pointing to config file to use
 * output       - None
 * side effects	- Read configuration file.
 */
static void
read_conf(FBFILE * file)
{
	lineno = 0;

	set_default_conf();	/* Set default values prior to conf parsing */
	yyparse();		/* Load the values from the conf */
	validate_conf();	/* Check to make sure some values are still okay. */
	/* Some global values are also loaded here. */
	check_class();		/* Make sure classes are valid */
}

static void
validate_conf(void)
{
	if(ConfigFileEntry.ts_warn_delta < TS_WARN_DELTA_MIN)
		ConfigFileEntry.ts_warn_delta = TS_WARN_DELTA_DEFAULT;

	if(ConfigFileEntry.ts_max_delta < TS_MAX_DELTA_MIN)
		ConfigFileEntry.ts_max_delta = TS_MAX_DELTA_DEFAULT;

	if(ConfigFileEntry.servlink_path == NULL)
		DupString(ConfigFileEntry.servlink_path, SLPATH);

	if(ServerInfo.network_name == NULL)
		DupString(ServerInfo.network_name, NETWORK_NAME_DEFAULT);

	if(ServerInfo.network_desc == NULL)
		DupString(ServerInfo.network_desc, NETWORK_DESC_DEFAULT);

	if((ConfigFileEntry.client_flood < CLIENT_FLOOD_MIN) ||
	   (ConfigFileEntry.client_flood > CLIENT_FLOOD_MAX))
		ConfigFileEntry.client_flood = CLIENT_FLOOD_MAX;

	GlobalSetOptions.idletime = (ConfigFileEntry.idletime * 60);
}

/*
 * lookup_confhost - start DNS lookups of all hostnames in the conf
 * line and convert an IP addresses in a.b.c.d number for to IP#s.
 *
 */

/*
 * conf_connect_allowed
 *
 * inputs	- pointer to inaddr
 *		- int type ipv4 or ipv6
 * output	- BANNED or accepted
 * side effects	- none
 */
int
conf_connect_allowed(struct sockaddr_storage *addr, int aftype)
{
	struct ConfItem *aconf = find_dline(addr, aftype);

	/* DLINE exempt also gets you out of static limits/pacing... */
	if(aconf && (aconf->status & CONF_EXEMPTDLINE))
		return (0);

	if(aconf != NULL)
		return (BANNED_CLIENT);

	return 0;
}

/* add_temp_kline()
 *
 * inputs        - pointer to struct ConfItem
 * output        - none
 * Side effects  - links in given struct ConfItem into 
 *                 temporary kline link list
 */
void
add_temp_kline(struct ConfItem *aconf)
{
	if(aconf->hold >= CurrentTime + (10080 * 60))
		dlinkAddAlloc(aconf, &tkline_week);
	else if(aconf->hold >= CurrentTime + (1440 * 60))
		dlinkAddAlloc(aconf, &tkline_day);
	else if(aconf->hold >= CurrentTime + (60 * 60))
		dlinkAddAlloc(aconf, &tkline_hour);
	else
		dlinkAddAlloc(aconf, &tkline_min);

	aconf->flags |= CONF_FLAGS_TEMPORARY;
	add_conf_by_address(aconf->host, CONF_KILL, aconf->user, aconf);
}

/* add_temp_dline()
 *
 * input	- pointer to struct ConfItem
 * output	- none
 * side effects - added to tdline link list and address hash
 */
void
add_temp_dline(struct ConfItem *aconf)
{
	if(aconf->hold >= CurrentTime + (10080 * 60))
		dlinkAddAlloc(aconf, &tdline_week);
	else if(aconf->hold >= CurrentTime + (1440 * 60))
		dlinkAddAlloc(aconf, &tdline_day);
	else if(aconf->hold >= CurrentTime + (60 * 60))
		dlinkAddAlloc(aconf, &tdline_hour);
	else
		dlinkAddAlloc(aconf, &tdline_min);

	aconf->flags |= CONF_FLAGS_TEMPORARY;
	add_conf_by_address(aconf->host, CONF_DLINE, aconf->user, aconf);
}

void
cleanup_temps_min(void *notused)
{
	expire_tkline(&tkline_min, TEMP_MIN);
	expire_tdline(&tdline_min, TEMP_MIN);
}

void
cleanup_temps_hour(void *notused)
{
	expire_tkline(&tkline_hour, TEMP_HOUR);
	expire_tdline(&tdline_hour, TEMP_HOUR);
}

void
cleanup_temps_day(void *notused)
{
	expire_tkline(&tkline_day, TEMP_DAY);
	expire_tdline(&tdline_day, TEMP_DAY);
}

void
cleanup_temps_week(void *notused)
{
	expire_tkline(&tkline_week, TEMP_WEEK);
	expire_tdline(&tdline_week, TEMP_WEEK);
}

/* expire_tkline()
 *
 * inputs       - list pointer
 * 		- type
 * output       - NONE
 * side effects - expire tklines and moves them between lists
 */
static void
expire_tkline(dlink_list * tklist, int type)
{
	dlink_node *ptr;
	dlink_node *next_ptr;
	struct ConfItem *aconf;

	DLINK_FOREACH_SAFE(ptr, next_ptr, tklist->head)
	{
		aconf = ptr->data;

		if(aconf->hold <= CurrentTime)
		{
			/* Alert opers that a TKline expired - Hwy */
			if(ConfigFileEntry.tkline_expire_notices)
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Temporary K-line for [%s@%s] expired",
						     (aconf->user) ? aconf->
						     user : "*", (aconf->host) ? aconf->host : "*");

			delete_one_address_conf(aconf->host, aconf);
			dlinkDestroy(ptr, tklist);
		}
		else if((type == TEMP_WEEK
			 && aconf->hold < (CurrentTime + (10080 * 60)))
			|| (type == TEMP_DAY
			    && aconf->hold < (CurrentTime + (1440 * 60)))
			|| (type == TEMP_HOUR && aconf->hold < (CurrentTime + (60 * 60))))
		{
			/* expires within a hour, check every minute.. */
			if(aconf->hold < CurrentTime + (60 * 60))
				dlinkMoveNode(ptr, tklist, &tkline_min);

			/* .. a day, check hourly */
			else if(aconf->hold < CurrentTime + (1440 * 60))
				dlinkMoveNode(ptr, tklist, &tkline_hour);

			/* .. a week, check daily */
			else if(aconf->hold < CurrentTime + (10080 * 60))
				dlinkMoveNode(ptr, tklist, &tkline_day);
		}
	}
}

/* expire_tdline()
 *
 * inputs       - list pointer
 * 		- type
 * output       - NONE
 * side effects - expire tdlines and moves them between lists
 */
static void
expire_tdline(dlink_list * tdlist, int type)
{
	dlink_node *ptr;
	dlink_node *next_ptr;
	struct ConfItem *aconf;

	DLINK_FOREACH_SAFE(ptr, next_ptr, tdlist->head)
	{
		aconf = ptr->data;

		if(aconf->hold <= CurrentTime)
		{
			if(ConfigFileEntry.tkline_expire_notices)
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Temporary D-line for [%s] expired",
						     aconf->host);

			delete_one_address_conf(aconf->host, aconf);
			dlinkDestroy(ptr, tdlist);
		}
		else if((type == TEMP_WEEK
			 && aconf->hold < (CurrentTime + (10080 * 60)))
			|| (type == TEMP_DAY
			    && aconf->hold < (CurrentTime + (1440 * 60)))
			|| (type == TEMP_HOUR && aconf->hold < (CurrentTime + (60 * 60))))
		{
			/* expires within an hour, check every minute.. */
			if(aconf->hold < CurrentTime + (60 * 60))
				dlinkMoveNode(ptr, tdlist, &tdline_min);

			/* .. a day, check hourly */
			else if(aconf->hold < CurrentTime + (1440 * 60))
				dlinkMoveNode(ptr, tdlist, &tdline_hour);

			/* .. a week, check daily */
			else if(aconf->hold < CurrentTime + (10080 * 60))
				dlinkMoveNode(ptr, tdlist, &tdline_day);
		}
	}
}

/* const char* get_oper_name(struct Client *client_p)
 * Input: A client to find the active oper{} name for.
 * Output: The nick!user@host{oper} of the oper.
 *         "oper" is server name for remote opers
 * Side effects: None.
 */
char *
get_oper_name(struct Client *client_p)
{
	/* +5 for !,@,{,} and null */
	static char buffer[NICKLEN + USERLEN + HOSTLEN + HOSTLEN + 5];

	if(MyOper(client_p))
	{
		ircsnprintf(buffer, sizeof(buffer), "%s!%s@%s{%s}",
				client_p->name, client_p->username,
				client_p->host, client_p->localClient->opername);
		return buffer;
	}

	ircsnprintf(buffer, sizeof(buffer), "%s!%s@%s{%s}",
		   client_p->name, client_p->username, 
		   client_p->host, client_p->servptr->name);
	return buffer;
}

/*
 * get_printable_conf
 *
 * inputs        - struct ConfItem
 *
 * output         - name 
 *                - host
 *                - pass
 *                - user
 *                - port
 *
 * side effects        -
 * Examine the struct struct ConfItem, setting the values
 * of name, host, pass, user to values either
 * in aconf, or "<NULL>" port is set to aconf->port in all cases.
 */
void
get_printable_conf(struct ConfItem *aconf, char **name, char **host,
		   char **pass, char **user, int *port, char **classname)
{
	static char null[] = "<NULL>";
	static char zero[] = "default";

	*name = EmptyString(aconf->name) ? null : aconf->name;
	*host = EmptyString(aconf->host) ? null : aconf->host;
	*pass = EmptyString(aconf->passwd) ? null : aconf->passwd;
	*user = EmptyString(aconf->user) ? null : aconf->user;
	*classname = EmptyString(aconf->className) ? zero : aconf->className;
	*port = (int) aconf->port;
}

void
get_printable_kline(struct Client *source_p, struct ConfItem *aconf, 
		    char **host, char **reason,
		    char **user, char **oper_reason)
{
	static char null[] = "<NULL>";

	*host = EmptyString(aconf->host) ? null : aconf->host;
	*reason = EmptyString(aconf->passwd) ? null : aconf->passwd;
	*user = EmptyString(aconf->user) ? null : aconf->user;

	if(EmptyString(aconf->spasswd) || !IsOper(source_p))
		*oper_reason = NULL;
	else
		*oper_reason = aconf->spasswd;
}

/*
 * read_conf_files
 *
 * inputs       - cold start YES or NO
 * output       - none
 * side effects - read all conf files needed, ircd.conf kline.conf etc.
 */
void
read_conf_files(int cold)
{
	FBFILE *file;
	const char *filename, *kfilename, *dfilename;	/* kline or conf filename */
	const char *xfilename;
	const char *resvfilename;

	conf_fbfile_in = NULL;

	filename = get_conf_name(CONF_TYPE);

	/* We need to know the initial filename for the yyerror() to report
	   FIXME: The full path is in conffilenamebuf first time since we
	   dont know anything else

	   - Gozem 2002-07-21 
	 */
	strlcpy(conffilebuf, filename, sizeof(conffilebuf));

	if((conf_fbfile_in = fbopen(filename, "r")) == NULL)
	{
		if(cold)
		{
			ilog(L_MAIN, "Failed in reading configuration file %s", filename);
			exit(-1);
		}
		else
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Can't open file '%s' - aborting rehash!", filename);
			return;
		}
	}

	if(!cold)
	{
		clear_out_old_conf();
	}

	read_conf(conf_fbfile_in);
	fbclose(conf_fbfile_in);

	kfilename = get_conf_name(KLINE_TYPE);
	if(irccmp(filename, kfilename))
	{
		if((file = fbopen(kfilename, "r")) == NULL)
		{
			if(cold)
				ilog(L_MAIN, "Failed reading kline file %s", filename);
			else
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Can't open %s file klines could be missing!",
						     kfilename);
		}
		else
		{
			parse_k_file(file);
			fbclose(file);
		}
	}

	dfilename = get_conf_name(DLINE_TYPE);
	if(irccmp(filename, dfilename) && irccmp(kfilename, dfilename))
	{
		if((file = fbopen(dfilename, "r")) == NULL)
		{
			if(cold)
				ilog(L_MAIN, "Failed reading dline file %s", dfilename);
			else
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Can't open %s file dlines could be missing!",
						     dfilename);
		}
		else
		{
			parse_d_file(file);
			fbclose(file);
		}
	}

	xfilename = ConfigFileEntry.xlinefile;

	if(irccmp(filename, xfilename) && irccmp(kfilename, xfilename) &&
	   irccmp(dfilename, xfilename))
	{
		if((file = fbopen(xfilename, "r")) == NULL)
		{
			if(cold)
				ilog(L_MAIN, "Failed reading xline file %s", xfilename);
			else
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Can't open %s file xlines could be missing!",
						     xfilename);
		}
		else
		{
			parse_x_file(file);
			fbclose(file);
		}
	}

	resvfilename = get_conf_name(RESV_TYPE);
	if(irccmp(filename, resvfilename))
	{
		if((file = fbopen(resvfilename, "r")) == NULL)
		{
			if(cold)
				ilog(L_MAIN, "Failed reading resv file %s", resvfilename);
			else
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Can't open %s file resvs could be missing!",
						     resvfilename);
		}
		else
		{
			parse_resv_file(file);
			fbclose(file);
		}
	}
}

/*
 * clear_out_old_conf
 *
 * inputs       - none
 * output       - none
 * side effects - Clear out the old configuration
 */
static void
clear_out_old_conf(void)
{
	struct Class *cltmp;
	dlink_node *ptr;

	/*
	 * don't delete the class table, rather mark all entries
	 * for deletion. The table is cleaned up by check_class. - avalon
	 */
	DLINK_FOREACH(ptr, class_list.head)
	{
		cltmp = ptr->data;
		MaxUsers(cltmp) = -1;
	}

	clear_out_address_conf();
	clear_s_newconf();

	/* clean out module paths */
#ifndef STATIC_MODULES
	mod_clear_paths();
#endif

	/* clean out ServerInfo */
	MyFree(ServerInfo.description);
	ServerInfo.description = NULL;
	MyFree(ServerInfo.network_name);
	ServerInfo.network_name = NULL;
	MyFree(ServerInfo.network_desc);
	ServerInfo.network_desc = NULL;

	/* clean out AdminInfo */
	MyFree(AdminInfo.name);
	AdminInfo.name = NULL;
	MyFree(AdminInfo.email);
	AdminInfo.email = NULL;
	MyFree(AdminInfo.description);
	AdminInfo.description = NULL;

	/* operator{} and class{} blocks are freed above */
	/* clean out listeners */
	close_listeners();

	/* auth{}, quarantine{}, shared{}, connect{}, kill{}, deny{}, exempt{}
	 * and gecos{} blocks are freed above too
	 */

	/* clean out general */
	MyFree(ConfigFileEntry.servlink_path);
	ConfigFileEntry.servlink_path = NULL;

	/* OK, that should be everything... */
}


/* write_confitem()
 *
 * inputs       - kline, dline or resv type flag
 *              - client pointer to report to
 *              - user name of target
 *              - host name of target
 *              - reason for target
 *              - time string
 *              - type of xline
 * output       - NONE
 * side effects - This function takes care of finding the right conf
 *                file and adding the line to it, as well as notifying
 *                opers and the user.
 */
void
write_confitem(KlineType type, struct Client *source_p, char *user,
	       char *host, const char *reason, const char *oper_reason,
	       const char *current_date, int xtype)
{
	char buffer[1024];
	FBFILE *out;
	const char *filename;	/* filename to use for kline */

	filename = get_conf_name(type);


	if((out = fbopen(filename, "a")) == NULL)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem opening %s ", filename);
		return;
	}

	if(oper_reason == NULL)
		oper_reason = "";

	if(type == KLINE_TYPE)
	{
		ircsnprintf(buffer, sizeof(buffer),
			   "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%ld\n",
			   user, host, reason, oper_reason, current_date,
			   get_oper_name(source_p), CurrentTime);
	}
	else if(type == DLINE_TYPE)
	{
		ircsnprintf(buffer, sizeof(buffer),
			   "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%ld\n", host,
			   reason, oper_reason, current_date, get_oper_name(source_p), CurrentTime);
	}
	else if(type == RESV_TYPE)
	{
		ircsnprintf(buffer, sizeof(buffer), "\"%s\",\"%s\",\"%s\",%ld\n",
			   host, reason, get_oper_name(source_p), CurrentTime);
	}

	if(fbputs(buffer, out) == -1)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s", filename);
		fbclose(out);
		return;
	}

	fbclose(out);

	if(type == KLINE_TYPE)
	{
		if(EmptyString(oper_reason))
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"%s added K-Line for [%s@%s] [%s]",
					get_oper_name(source_p), user, 
					host, reason);
			ilog(L_KLINE, "K %s 0 %s %s %s",
				get_oper_name(source_p), user, host, reason);
		}
		else
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"%s added K-Line for [%s@%s] [%s|%s]",
					get_oper_name(source_p), user,
					host, reason, oper_reason);
			ilog(L_KLINE, "K %s 0 %s %s %s|%s",
				get_oper_name(source_p), user, host,
				reason, oper_reason);
		}

		sendto_one_notice(source_p, ":Added K-Line [%s@%s]",
				  user, host);
	}
	else if(type == DLINE_TYPE)
	{
		if(EmptyString(oper_reason))
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"%s added D-Line for [%s] [%s]",
					get_oper_name(source_p), host, reason);
			ilog(L_KLINE, "D %s 0 %s %s",
				get_oper_name(source_p), host, reason);
		}
		else
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"%s added D-Line for [%s] [%s|%s]",
					get_oper_name(source_p), host, 
					reason, oper_reason);
			ilog(L_KLINE, "D %s 0 %s %s|%s",
				get_oper_name(source_p), host, 
				reason, oper_reason);
		}

		sendto_one(source_p,
			   ":%s NOTICE %s :Added D-Line [%s] to %s", me.name,
			   source_p->name, host, filename);

	}
	else if(type == RESV_TYPE)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				"%s added RESV for [%s] [%s]",
				get_oper_name(source_p), host, reason);
		ilog(L_KLINE, "R %s 0 %s %s",
			get_oper_name(source_p), host, reason);

		sendto_one_notice(source_p, ":Added RESV for [%s] [%s]",
				  host, reason);
	}
}

/* get_conf_name
 *
 * inputs       - type of conf file to return name of file for
 * output       - pointer to filename for type of conf
 * side effects - none
 */
const char *
get_conf_name(KlineType type)
{
	if(type == CONF_TYPE)
	{
		return (ConfigFileEntry.configfile);
	}
	else if(type == DLINE_TYPE)
	{
		return (ConfigFileEntry.dlinefile);
	}
	else if(type == RESV_TYPE)
	{
		return (ConfigFileEntry.resvfile);
	}

	return ConfigFileEntry.klinefile;
}

/*
 * conf_add_class_to_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a class pointer to a conf 
 */

void
conf_add_class_to_conf(struct ConfItem *aconf)
{
	if(aconf->className == NULL)
	{
		DupString(aconf->className, "default");
		ClassPtr(aconf) = default_class;
		return;
	}

	ClassPtr(aconf) = find_class(aconf->className);

	if(ClassPtr(aconf) == default_class)
	{
		if(aconf->status == CONF_CLIENT)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Warning -- Using default class for missing class \"%s\" in auth{} for %s@%s",
					     aconf->className, aconf->user, aconf->host);
		}

		MyFree(aconf->className);
		DupString(aconf->className, "default");
		return;
	}

	if(ConfMaxUsers(aconf) < 0)
	{
		ClassPtr(aconf) = default_class;
		MyFree(aconf->className);
		DupString(aconf->className, "default");
		return;
	}
}

/*
 * conf_add_d_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a d/D line
 */
void
conf_add_d_conf(struct ConfItem *aconf)
{
	if(aconf->host == NULL)
		return;

	aconf->user = NULL;

	/* XXX - Should 'd' ever be in the old conf? For new conf we don't
	 *       need this anyway, so I will disable it for now... -A1kmm
	 */

	if(parse_netmask(aconf->host, NULL, NULL) == HM_HOST)
	{
		ilog(L_MAIN, "Invalid Dline %s ignored", aconf->host);
		free_conf(aconf);
	}
	else
	{
		add_conf_by_address(aconf->host, CONF_DLINE, NULL, aconf);
	}
}


/*
 * yyerror
 *
 * inputs	- message from parser
 * output	- none
 * side effects	- message to opers and log file entry is made
 */
void
yyerror(const char *msg)
{
	char newlinebuf[BUFSIZE];

	strip_tabs(newlinebuf, (const unsigned char *) linebuf, strlen(linebuf));

	sendto_realops_flags(UMODE_ALL, L_ALL, "\"%s\", line %d: %s at '%s'",
			     conffilebuf, lineno + 1, msg, newlinebuf);

	ilog(L_MAIN, "\"%s\", line %d: %s at '%s'", conffilebuf, lineno + 1, msg, newlinebuf);
}

int
conf_fbgets(char *lbuf, int max_size, FBFILE * fb)
{
	char *buff;

	if((buff = fbgets(lbuf, max_size, fb)) == NULL)
		return (0);

	return (strlen(lbuf));
}

int
conf_yy_fatal_error(const char *msg)
{
	return (0);
}
