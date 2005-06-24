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
#include "channel.h"
#include "class.h"
#include "client.h"
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
#include "banconf.h"

struct config_server_hide ConfigServerHide;

extern int yyparse();		/* defined in y.tab.c */
extern char linebuf[];

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

static BlockHeap *confitem_heap = NULL;

/* internally defined functions */
static void set_default_conf(void);
static void validate_conf(void);
static void read_conf(FILE *);

FILE *conf_fbfile_in;
extern char yytext[];

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

static void
remove_ip_limit(struct Client *client_p, struct ConfItem *aconf)
{
	patricia_node_t *pnode;

	/* If the limits are 0 don't do anything.. */
	if(ConfCidrAmount(aconf) == 0 || ConfCidrBitlen(aconf) == 0)
		return;

	pnode = match_ip(ConfIpLimits(aconf), (struct sockaddr *)&client_p->localClient->ip);
	if(pnode == NULL)
		return;

	pnode->data--;
	if(((unsigned long) pnode->data) == 0)
	{
		patricia_remove(ConfIpLimits(aconf), pnode);
	}

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
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "Got signal SIGHUP, reloading ircd conf. file");

	restart_resolver();
	/* don't close listeners until we know we can go ahead with the rehash */
	read_ircd_conf(NO);

	if(ServerInfo.description != NULL)
		strlcpy(me.info, ServerInfo.description, sizeof(me.info));

	open_logfiles();
	return (0);
}

int
rehash_ban(int sig)
{
	if(sig != 0)
		sendto_realops_flags(UMODE_ALL, L_ALL,
				"Got signal SIGUSR2, reloading ban configs");

	read_ban_confs(NO);

	check_banned_lines();
	return 0;
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

	DupString(ConfigFileEntry.default_operstring, "is an IRC operator");
	DupString(ConfigFileEntry.default_adminstring, "is a Server Administrator");
	ConfigFileEntry.kline_reason = NULL;

	ConfigFileEntry.fname_userlog = NULL;
	ConfigFileEntry.fname_fuserlog = NULL;
	ConfigFileEntry.fname_operlog = NULL;
	ConfigFileEntry.fname_foperlog = NULL;
	ConfigFileEntry.fname_serverlog = NULL;
	ConfigFileEntry.fname_glinelog = NULL;
	ConfigFileEntry.fname_klinelog = NULL;
	ConfigFileEntry.fname_killlog = NULL;
	ConfigFileEntry.fname_operspylog = NULL;
	ConfigFileEntry.fname_ioerrorlog = NULL;

	ConfigFileEntry.failed_oper_notice = YES;
	ConfigFileEntry.anti_nick_flood = NO;
	ConfigFileEntry.disable_fake_channels = NO;
	ConfigFileEntry.max_nick_time = 20;
	ConfigFileEntry.max_nick_changes = 5;
	ConfigFileEntry.max_accept = 20;
	ConfigFileEntry.max_watch = 128;
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
	ConfigFileEntry.glines = NO;
	ConfigFileEntry.use_egd = NO;
	ConfigFileEntry.gline_time = 12 * 3600;
	ConfigFileEntry.gline_min_cidr = 16;
	ConfigFileEntry.gline_min_cidr6 = 48;
	ConfigFileEntry.hide_error_messages = 1;
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
	ConfigChannel.invite_ops_only = YES;

	ConfigChannel.default_split_user_count = 0;
	ConfigChannel.default_split_server_count = 0;
	ConfigChannel.default_split_delay = 60;
	ConfigChannel.no_join_on_split = NO;
	ConfigChannel.no_create_on_split = NO;

	ConfigServerHide.flatten_links = 0;
	ConfigServerHide.hidden = 0;
	ConfigServerHide.disable_hidden = 0;

	ConfigFileEntry.min_nonwildcard = 4;
	ConfigFileEntry.min_nonwildcard_simple = 3;
	ConfigFileEntry.default_invisible = 0;
	ConfigFileEntry.default_floodcount = 8;
	ConfigFileEntry.client_flood = CLIENT_FLOOD_DEFAULT;
	ConfigFileEntry.tkline_expire_notices = 0;
	ConfigFileEntry.target_change = YES;
	ConfigFileEntry.tgchange_remote = 1800;
	ConfigFileEntry.tgchange_reconnect = 5;
	ConfigFileEntry.tgchange_expiry = 900;
        ConfigFileEntry.reject_after_count = 5;
	ConfigFileEntry.reject_ban_time = 300;  
	ConfigFileEntry.reject_duration = 120;
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
read_conf(FILE * file)
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

	if(ConfigFileEntry.tgchange_reconnect > 10)
		ConfigFileEntry.tgchange_reconnect = 10;
	else if(ConfigFileEntry.tgchange_reconnect < 0)
		ConfigFileEntry.tgchange_reconnect = 0;
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
conf_connect_allowed(struct sockaddr *addr, int aftype)
{
	struct ConfItem *aconf = find_dline(addr);

	/* DLINE exempt also gets you out of static limits/pacing... */
	if(aconf && (aconf->status & CONF_EXEMPTDLINE))
		return (0);

	if(aconf != NULL)
		return (BANNED_CLIENT);

	return 0;
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


static void
clear_ircd_conf(void)
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

	clear_out_address_conf(CONF_CLIENT);
	clear_s_newconf_ircd();

	/* clean out module paths */
#ifndef STATIC_MODULES
	mod_clear_paths();
	mod_add_path(MODULE_DIR);
	mod_add_path(MODULE_DIR  "/autoload");
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

	/* clear out log {}; */
	MyFree(ConfigFileEntry.fname_userlog);
	MyFree(ConfigFileEntry.fname_fuserlog);
	MyFree(ConfigFileEntry.fname_operlog);
	MyFree(ConfigFileEntry.fname_foperlog);
	MyFree(ConfigFileEntry.fname_serverlog);
	MyFree(ConfigFileEntry.fname_glinelog);
	MyFree(ConfigFileEntry.fname_klinelog);
	MyFree(ConfigFileEntry.fname_killlog);
	MyFree(ConfigFileEntry.fname_operspylog);
	MyFree(ConfigFileEntry.fname_ioerrorlog);
	ConfigFileEntry.fname_userlog = NULL;
	ConfigFileEntry.fname_fuserlog = NULL;
	ConfigFileEntry.fname_operlog = NULL;
	ConfigFileEntry.fname_foperlog = NULL;
	ConfigFileEntry.fname_serverlog = NULL;
	ConfigFileEntry.fname_glinelog = NULL;
	ConfigFileEntry.fname_klinelog = NULL;
	ConfigFileEntry.fname_killlog = NULL;
	ConfigFileEntry.fname_operspylog = NULL;
	ConfigFileEntry.fname_ioerrorlog = NULL;

	/* operator{} and class{} blocks are freed above */
	/* clean out listeners */
	close_listeners();

	/* auth{}, quarantine{}, shared{}, connect{}, kill{}, deny{}, exempt{}
	 * and gecos{} blocks are freed above too
	 */

	/* clean out general */
	MyFree(ConfigFileEntry.servlink_path);
	ConfigFileEntry.servlink_path = NULL;
	MyFree(ConfigFileEntry.egdpool_path);
	ConfigFileEntry.egdpool_path = NULL;

	MyFree(ConfigFileEntry.default_operstring);
	ConfigFileEntry.default_operstring = NULL;
	MyFree(ConfigFileEntry.default_adminstring);
	ConfigFileEntry.default_adminstring = NULL;
	MyFree(ConfigFileEntry.kline_reason);
	ConfigFileEntry.kline_reason = NULL;

	/* OK, that should be everything... */
}

static void
clear_ban_confs(void)
{
	clear_out_address_conf(CONF_KILL);
	clear_s_newconf_bans();
}


/*
 * read_conf_files
 *
 * inputs       - cold start YES or NO
 * output       - none
 * side effects - read all conf files needed, ircd.conf kline.conf etc.
 */
void
read_ircd_conf(int cold)
{
	const char *filename;

	conf_fbfile_in = NULL;

	filename = get_conf_name(CONF_TYPE);

	/* We need to know the initial filename for the yyerror() to report
	   FIXME: The full path is in conffilenamebuf first time since we
	   dont know anything else

	   - Gozem 2002-07-21 
	 */
	strlcpy(conffilebuf, filename, sizeof(conffilebuf));

	if((conf_fbfile_in = fopen(filename, "r")) == NULL)
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
		clear_ircd_conf();

	read_conf(conf_fbfile_in);
	fclose(conf_fbfile_in);
}

void
read_ban_confs(int cold)
{
	if(!cold)
		clear_ban_confs();

	read_kline_conf(KLINEPATH, 0);
	read_kline_conf(KLINEPATHPERM, 1);
	read_dline_conf(DLINEPATH, 0);
	read_dline_conf(DLINEPATHPERM, 1);
	read_xline_conf(XLINEPATH, 0);
	read_xline_conf(XLINEPATHPERM, 1);
	read_resv_conf(RESVPATH, 0);
	read_resv_conf(RESVPATHPERM, 1);
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
	FILE *out;
	const char *filename;	/* filename to use for kline */

	filename = get_conf_name(type);


	if((out = fopen(filename, "a")) == NULL)
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
	else if(type == RESV_TYPE)
	{
		ircsnprintf(buffer, sizeof(buffer), "\"%s\",\"%s\",\"%s\",%ld\n",
			   host, reason, get_oper_name(source_p), CurrentTime);
	}

	if(fputs(buffer, out) == -1)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s", filename);
		fclose(out);
		return;
	}
	else
		fflush(out);

	fclose(out);

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
		return (DLINEPATH);
	}
	else if(type == RESV_TYPE)
	{
		return (RESVPATH);
	}

	return KLINEPATH;
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
conf_fgets(char *lbuf, int max_size, FILE * fb)
{
	char *buff;

	if((buff = fgets(lbuf, max_size, fb)) == NULL)
		return (0);

	return (strlen(lbuf));
}

int
conf_yy_fatal_error(const char *msg)
{
	return (0);
}
