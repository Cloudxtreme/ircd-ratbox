/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_info.c: Sends information about the server.
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
#include "tools.h"
#include "m_info.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
#include "hook.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void send_conf_options(struct Client *source_p);
static void send_birthdate_online_time(struct Client *source_p);
static void send_info_text(struct Client *source_p);
static void info_spy(struct Client *);

static int m_info(struct Client *, struct Client *, int, const char **);
static int mo_info(struct Client *, struct Client *, int, const char **);

struct Message info_msgtab = {
	"INFO", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_info, 0}, {mo_info, 0}, mg_ignore, mg_ignore, {mo_info, 0}}
};

int doing_info_hook;

mapi_clist_av1 info_clist[] = { &info_msgtab, NULL };
mapi_hlist_av1 info_hlist[] = {
	{ "doing_hook",		&doing_info_hook },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(info, NULL, NULL, info_clist, NULL, NULL, "$Revision$");

/*
 * jdc -- Structure for our configuration value table
 */
struct InfoStruct
{
	const char *name;	/* Displayed variable name */
	unsigned int output_type;	/* See below #defines */
	void *option;		/* Pointer reference to the value */
	const char *desc;	/* ASCII description of the variable */
};
/* Types for output_type in InfoStruct */
#define OUTPUT_STRING      0x0001	/* Output option as %s w/ dereference */
#define OUTPUT_STRING_PTR  0x0002	/* Output option as %s w/out deference */
#define OUTPUT_DECIMAL     0x0004	/* Output option as decimal (%d) */
#define OUTPUT_BOOLEAN     0x0008	/* Output option as "ON" or "OFF" */
#define OUTPUT_BOOLEAN_YN  0x0010	/* Output option as "YES" or "NO" */
#define OUTPUT_BOOLEAN2	   0x0020	/* Output option as "YES/NO/MASKED" */

/* *INDENT-OFF* */
static struct InfoStruct info_table[] = {
	/* --[  START OF TABLE  ]-------------------------------------------- */
	{
		"anti_nick_flood",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.anti_nick_flood,
		"NICK flood protection"
	},
	{
		"anti_spam_exit_message_time",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.anti_spam_exit_message_time,
		"Duration a client must be connected for to have an exit message"
	},
	{
		"caller_id_wait",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.caller_id_wait,
		"Minimum delay between notifying UMODE +g users of messages"
	},
	{
		"client_exit",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.client_exit,
		"Prepend 'Client Exit:' to user QUIT messages"
	},
	{
		"client_flood",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.client_flood,
		"Number of lines before a client Excess Flood's",
	},
	{
		"connect_timeout",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.connect_timeout,
		"Connect timeout for connections to servers"
	},
	{
		"default_floodcount",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.default_floodcount,
		"Startup value of FLOODCOUNT",
	},
	{
		"default_adminstring",
		OUTPUT_STRING_PTR,
		&ConfigFileEntry.default_adminstring,
		"Default adminstring at startup.",
	},
	{
		"default_operstring",
		OUTPUT_STRING_PTR,
		&ConfigFileEntry.default_operstring,
		"Default operstring at startup.",
	},
	{
		"disable_auth",
		OUTPUT_BOOLEAN_YN,
		&ConfigFileEntry.disable_auth,
		"Controls whether auth checking is disabled or not"
	},
	{
		"dot_in_ip6_addr",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.dot_in_ip6_addr,
		"Suffix a . to ip6 addresses",
	},
	{
		"dots_in_ident",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.dots_in_ident,
		"Number of permissable dots in an ident"
	},
	{
		"failed_oper_notice",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.failed_oper_notice,
		"Inform opers if someone /oper's with the wrong password"
	},
	{
		/* fname_operlog is a char [] */
		"fname_operlog",
		OUTPUT_STRING_PTR,
		&ConfigFileEntry.fname_operlog,
		"Operator log file"
	},
	{
		/* fname_foperlog is a char [] */
		"fname_foperlog",
		OUTPUT_STRING_PTR,
		&ConfigFileEntry.fname_foperlog,
		"Failed operator log file"
	},
	{
		/* fname_userlog is a char [] */
		"fname_userlog",
		OUTPUT_STRING_PTR,
		&ConfigFileEntry.fname_userlog,
		"User log file"
	},
	{
		"glines",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.glines,
		"G-line (network-wide K-line) support"
	},
	{
		"gline_time",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.gline_time,
		"Expiry time for G-lines"
	},
	{
		"gline_min_cidr",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.gline_min_cidr,
		"Minimum CIDR bitlen for ipv4 glines"
	},
	{
		"gline_min_cidr6",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.gline_min_cidr6,
		"Minimum CIDR bitlen for ipv6 glines"
	},
	{
		"hide_error_messages",
		OUTPUT_BOOLEAN2,
		&ConfigFileEntry.hide_error_messages,
		"Hide ERROR messages coming from servers"
	},
	{
		"hub",
		OUTPUT_BOOLEAN_YN,
		&ServerInfo.hub,
		"Server is a hub"
	},
	{
		"idletime",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.idletime,
		"Number of minutes before a client is considered idle"
	},
	{
		"kline_delay",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.kline_delay,
		"Duration of time to delay kline checking"
	},
	{
		"kline_reason",
		OUTPUT_STRING,
		&ConfigFileEntry.kline_reason,
		"K-lined clients sign off with this reason"
	},
	{
		"kline_with_reason",
		OUTPUT_BOOLEAN_YN,
		&ConfigFileEntry.kline_with_reason,
		"Display K-line reason to client on disconnect"
	},
	{
		"max_accept",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.max_accept,
		"Maximum nicknames on accept list",
	},
	{
		"max_nick_changes",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.max_nick_changes,
		"NICK change threshhold setting"
	},
	{
		"max_nick_time",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.max_nick_time,
		"NICK flood protection time interval"
	},
	{
		"max_targets",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.max_targets,
		"The maximum number of PRIVMSG/NOTICE targets"
	},
	{
		"min_nonwildcard",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.min_nonwildcard,
		"Minimum non-wildcard chars in K/G lines",
	},
	{
		"min_nonwildcard_simple",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.min_nonwildcard_simple,
		"Minimum non-wildcard chars in xlines/resvs",
	},
	{
		"network_name",
		OUTPUT_STRING,
		&ServerInfo.network_name,
		"Network name"
	},
	{
		"network_desc",
		OUTPUT_STRING,
		&ServerInfo.network_desc,
		"Network description"
	},
	{
		"nick_delay",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.nick_delay,
		"Delay nicks are locked for on split",
	},
	{
		"no_oper_flood",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.no_oper_flood,
		"Disable flood control for operators",
	},
	{
		"non_redundant_klines",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.non_redundant_klines,
		"Check for and disallow redundant K-lines"
	},
	{
		"operspy_admin_only",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.operspy_admin_only,
		"Send +Z operspy notices to admins only"
	},
	{
		"pace_wait",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.pace_wait,
		"Minimum delay between uses of certain commands"
	},
	{
		"pace_wait_simple",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.pace_wait_simple,
		"Minimum delay between less intensive commands"
	},
	{
		"ping_cookie",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.ping_cookie,
		"Require ping cookies to connect",
	},
	{
		"reject_after_count",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.reject_after_count,   
		"Client rejection threshold setting",
	},
	{
		"reject_ban_time",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.reject_ban_time,
		"Client rejection time interval",
	},
	{
		"reject_duration",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.reject_duration,
		"Client rejection cache duration",
	},
	{
		"short_motd",
		OUTPUT_BOOLEAN_YN,
		&ConfigFileEntry.short_motd,
		"Do not show MOTD; only tell clients they should read it"
	},
	{
		"stats_e_disabled",
		OUTPUT_BOOLEAN_YN,
		&ConfigFileEntry.stats_e_disabled,
		"STATS e output is disabled",
	},
	{
		"stats_c_oper_only",
		OUTPUT_BOOLEAN_YN,
		&ConfigFileEntry.stats_c_oper_only,
		"STATS C output is only shown to operators",
	},
	{
		"stats_h_oper_only",
		OUTPUT_BOOLEAN_YN,
		&ConfigFileEntry.stats_h_oper_only,
		"STATS H output is only shown to operators",
	},
	{
		"stats_i_oper_only",
		OUTPUT_BOOLEAN2,
		&ConfigFileEntry.stats_i_oper_only,
		"STATS I output is only shown to operators",
	},
	{
		"stats_k_oper_only",
		OUTPUT_BOOLEAN2,
		&ConfigFileEntry.stats_k_oper_only,
		"STATS K output is only shown to operators",
	},
	{
		"stats_o_oper_only",
		OUTPUT_BOOLEAN_YN,
		&ConfigFileEntry.stats_o_oper_only,
		"STATS O output is only shown to operators",
	},
	{
		"stats_P_oper_only",
		OUTPUT_BOOLEAN_YN,
		&ConfigFileEntry.stats_P_oper_only,
		"STATS P is only shown to operators",
	},
	{
		"stats_y_oper_only",
		OUTPUT_BOOLEAN_YN,
		&ConfigFileEntry.stats_y_oper_only,
		"STATS Y is only shown to operators",
	},
	{
		"tkline_expire_notices",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.tkline_expire_notices,
		"Notices given to opers when tklines expire"
	},
	{
		"ts_max_delta",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.ts_max_delta,
		"Maximum permitted TS delta from another server"
	},
	{
		"ts_warn_delta",
		OUTPUT_DECIMAL,
		&ConfigFileEntry.ts_warn_delta,
		"Maximum permitted TS delta before displaying a warning"
	},
	{
		"warn_no_nline",
		OUTPUT_BOOLEAN,
		&ConfigFileEntry.warn_no_nline,
		"Display warning if connecting server lacks N-line"
	},
	{
		"default_split_server_count",
		OUTPUT_DECIMAL,
		&ConfigChannel.default_split_server_count,
		"Startup value of SPLITNUM",
	},
	{
		"default_split_user_count",
		OUTPUT_DECIMAL,
		&ConfigChannel.default_split_user_count,
		"Startup value of SPLITUSERS",
	},
	{
		"knock_delay",
		OUTPUT_DECIMAL,
		&ConfigChannel.knock_delay,
		"Delay between a users KNOCK attempts"
	},
	{
		"knock_delay_channel",
		OUTPUT_DECIMAL,
		&ConfigChannel.knock_delay_channel,
		"Delay between KNOCK attempts to a channel",
	},
	{
		"max_bans",
		OUTPUT_DECIMAL,
		&ConfigChannel.max_bans,
		"Total +b/e/I modes allowed in a channel",
	},
	{
		"max_chans_per_user",
		OUTPUT_DECIMAL,
		&ConfigChannel.max_chans_per_user,
		"Maximum number of channels a user can join",
	},
	{
		"no_create_on_split",
		OUTPUT_BOOLEAN_YN,
		&ConfigChannel.no_create_on_split,
		"Disallow creation of channels when split",
	},
	{
		"no_join_on_split",
		OUTPUT_BOOLEAN_YN,
		&ConfigChannel.no_join_on_split,
		"Disallow joining channels when split",
	},
	{
		"no_oper_resvs",
		OUTPUT_BOOLEAN_YN,
		&ConfigChannel.no_oper_resvs,
		"Disable channel resvs for operators",
	},
	{
		"quiet_on_ban",
		OUTPUT_BOOLEAN_YN,
		&ConfigChannel.quiet_on_ban,
		"Banned users may not send text to a channel"
	},
	{
		"use_except",
		OUTPUT_BOOLEAN_YN,
		&ConfigChannel.use_except,
		"Enable chanmode +e (ban exceptions)",
	},
	{
		"use_invex",
		OUTPUT_BOOLEAN_YN,
		&ConfigChannel.use_invex,
		"Enable chanmode +I (invite exceptions)",
	},
	{
		"use_knock",
		OUTPUT_BOOLEAN_YN,
		&ConfigChannel.use_knock,
		"Enable /KNOCK",
	},
	{
		"disable_hidden",
		OUTPUT_BOOLEAN_YN,
		&ConfigServerHide.disable_hidden,
		"Prevent servers from hiding themselves from a flattened /links",
	},
	{
		"flatten_links",
		OUTPUT_BOOLEAN_YN,
		&ConfigServerHide.flatten_links,
		"Flatten /links list",
	},
	{
		"hidden",
		OUTPUT_BOOLEAN_YN,
		&ConfigServerHide.hidden,
		"Hide this server from a flattened /links on remote servers",
	},
	{
		"links_delay",
		OUTPUT_DECIMAL,
		&ConfigServerHide.links_delay,
		"Links rehash delay"
	},
	/* --[  END OF TABLE  ]---------------------------------------------- */
	{ (char *) 0, (unsigned int) 0, (void *) 0, (char *) 0}
};
/* *INDENT-ON* */

/*
** m_info
**  parv[0] = sender prefix
**  parv[1] = servername
*/
static int
m_info(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
	{
		/* safe enough to give this on a local connect only */
		sendto_one(source_p, form_str(RPL_LOAD2HI),
			   me.name, source_p->name, "INFO");
		sendto_one_numeric(source_p, RPL_ENDOFINFO, form_str(RPL_ENDOFINFO));
		return 0;
	}
	else
		last_used = CurrentTime;

	if(hunt_server(client_p, source_p, ":%s INFO :%s", 1, parc, parv) != HUNTED_ISME)
		return 0;

	info_spy(source_p);

	send_info_text(source_p);
	send_birthdate_online_time(source_p);

	sendto_one_numeric(source_p, RPL_ENDOFINFO, form_str(RPL_ENDOFINFO));
	return 0;
}

/*
** mo_info
**  parv[0] = sender prefix
**  parv[1] = servername
*/
static int
mo_info(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(hunt_server(client_p, source_p, ":%s INFO :%s", 1, parc, parv) == HUNTED_ISME)
	{
		info_spy(source_p);

		send_info_text(source_p);

		if(IsOper(source_p))
			send_conf_options(source_p);

		send_birthdate_online_time(source_p);

		sendto_one_numeric(source_p, RPL_ENDOFINFO, form_str(RPL_ENDOFINFO));
	}

	return 0;
}

/*
 * send_info_text
 *
 * inputs	- client pointer to send info text to
 * output	- none
 * side effects	- info text is sent to client
 */
static void
send_info_text(struct Client *source_p)
{
	const char **text = infotext;

	while (*text)
	{
		sendto_one_numeric(source_p, RPL_INFO, form_str(RPL_INFO), *text++);
	}

	sendto_one_numeric(source_p, RPL_INFO, form_str(RPL_INFO), "");
}

/*
 * send_birthdate_online_time
 *
 * inputs	- client pointer to send to
 * output	- none
 * side effects	- birthdate and online time are sent
 */
static void
send_birthdate_online_time(struct Client *source_p)
{
	sendto_one(source_p, ":%s %d %s :Birth Date: %s, compile # %s",
		   get_id(&me, source_p), RPL_INFO, 
		   get_id(source_p, source_p), creation, generation);

	sendto_one(source_p, ":%s %d %s :On-line since %s",
		   get_id(&me, source_p), RPL_INFO, 
		   get_id(source_p, source_p), myctime(me.firsttime));
}

/*
 * send_conf_options
 *
 * inputs	- client pointer to send to
 * output	- none
 * side effects	- send config options to client
 */
static void
send_conf_options(struct Client *source_p)
{
	Info *infoptr;
	int i = 0;

	/*
	 * Now send them a list of all our configuration options
	 * (mostly from config.h)
	 */
	for (infoptr = MyInformation; infoptr->name; infoptr++)
	{
		if(infoptr->intvalue)
		{
			sendto_one(source_p, ":%s %d %s :%-30s %-5d [%-30s]",
				   get_id(&me, source_p), RPL_INFO,
				   get_id(source_p, source_p),
				   infoptr->name, infoptr->intvalue, 
				   infoptr->desc);
		}
		else
		{
			sendto_one(source_p, ":%s %d %s :%-30s %-5s [%-30s]",
				   get_id(&me, source_p), RPL_INFO,
				   get_id(source_p, source_p),
				   infoptr->name, infoptr->strvalue, 
				   infoptr->desc);
		}
	}

	/*
	 * Parse the info_table[] and do the magic.
	 */
	for (i = 0; info_table[i].name; i++)
	{
		switch (info_table[i].output_type)
		{
			/*
			 * For "char *" references
			 */
		case OUTPUT_STRING:
			{
				char *option = *((char **) info_table[i].option);

				sendto_one(source_p, ":%s %d %s :%-30s %-5s [%-30s]",
					   get_id(&me, source_p), RPL_INFO,
					   get_id(source_p, source_p),
					   info_table[i].name,
					   option ? option : "NONE",
					   info_table[i].desc ? info_table[i].desc : "<none>");

				break;
			}
			/*
			 * For "char foo[]" references
			 */
		case OUTPUT_STRING_PTR:
			{
				char *option = (char *) info_table[i].option;

				sendto_one(source_p, ":%s %d %s :%-30s %-5s [%-30s]",
					   get_id(&me, source_p), RPL_INFO,
					   get_id(source_p, source_p),
					   info_table[i].name,
					   option ? option : "NONE",
					   info_table[i].desc ? info_table[i].desc : "<none>");

				break;
			}
			/*
			 * Output info_table[i].option as a decimal value.
			 */
		case OUTPUT_DECIMAL:
			{
				int option = *((int *) info_table[i].option);

				sendto_one(source_p, ":%s %d %s :%-30s %-5d [%-30s]",
					   get_id(&me, source_p), RPL_INFO,
					   get_id(source_p, source_p),
					   info_table[i].name,
					   option,
					   info_table[i].desc ? info_table[i].desc : "<none>");

				break;
			}

			/*
			 * Output info_table[i].option as "ON" or "OFF"
			 */
		case OUTPUT_BOOLEAN:
			{
				int option = *((int *) info_table[i].option);

				sendto_one(source_p, ":%s %d %s :%-30s %-5s [%-30s]",
					   get_id(&me, source_p), RPL_INFO,
					   get_id(source_p, source_p),
					   info_table[i].name,
					   option ? "ON" : "OFF",
					   info_table[i].desc ? info_table[i].desc : "<none>");

				break;
			}
			/*
			 * Output info_table[i].option as "YES" or "NO"
			 */
		case OUTPUT_BOOLEAN_YN:
			{
				int option = *((int *) info_table[i].option);

				sendto_one(source_p, ":%s %d %s :%-30s %-5s [%-30s]",
					   get_id(&me, source_p), RPL_INFO,
					   get_id(source_p, source_p),
					   info_table[i].name,
					   option ? "YES" : "NO",
					   info_table[i].desc ? info_table[i].desc : "<none>");

				break;
			}

		case OUTPUT_BOOLEAN2:
		{
			int option = *((int *) info_table[i].option);

			sendto_one(source_p, ":%s %d %s :%-30s %-5s [%-30s]",
				   me.name, RPL_INFO, source_p->name,
				   info_table[i].name,
				   option ? ((option == 1) ? "MASK" : "YES") : "NO",
				   info_table[i].desc ? info_table[i].desc : "<none>");
		}		/* switch (info_table[i].output_type) */
		}
	}			/* forloop */


	/* Don't send oper_only_umodes...it's a bit mask, we will have to decode it
	 ** in order for it to show up properly to opers who issue INFO
	 */

	sendto_one_numeric(source_p, RPL_INFO, form_str(RPL_INFO), "");
}

/* info_spy()
 * 
 * input        - pointer to client
 * output       - none
 * side effects - hook doing_info is called
 */
static void
info_spy(struct Client *source_p)
{
	struct hook_spy_data data;

	data.source_p = source_p;

	hook_call_event(doing_info_hook, &data);
}
