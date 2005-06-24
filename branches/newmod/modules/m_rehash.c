/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_rehash.c: Re-reads the configuration file.
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
#include "rpi/client.h"
#include "channel.h"
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
#include "s_gline.h"
#include "numeric.h"
#include "res.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_log.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "hostmask.h"
#include "reject.h"
#include "hash.h"
#include "cache.h"

static int mo_rehash(struct rpi_client *, struct rpi_client *, int, const char **);

struct Message rehash_msgtab = {
	"REHASH", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_rehash, 0}}
};

mapi_clist_av1 rehash_clist[] = { &rehash_msgtab, NULL };
DECLARE_MODULE_AV1(rehash, NULL, NULL, rehash_clist, NULL, NULL, "$Revision$");

struct hash_commands
{
	const char *cmd;
	void (*handler) (struct rpi_client * source_p);
};

static void
clear_temps(dlink_list * tlist)
{
	dlink_node *ptr;
	dlink_node *next_ptr;
	struct ConfItem *aconf;

	DLINK_FOREACH_SAFE(ptr, next_ptr, tlist->head)
	{
		aconf = ptr->data;

		delete_one_address_conf(aconf->host, aconf);
		dlinkDestroy(ptr, tlist);
	}
}

static void
rehash_dns(struct rpi_client *source_p)
{
	sendto_realops_flags(UMODE_ALL, L_ALL, "%s is rehashing DNS", 
			     get_oper_name(source_p));

	/* reread /etc/resolv.conf and reopen res socket */
	restart_resolver();
}

static void
rehash_motd(struct rpi_client *source_p)
{
	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s is forcing re-reading of MOTD file",
			     get_oper_name(source_p));

	free_cachefile(user_motd);
	user_motd = cache_file(MPATH, "ircd.motd", 0);
}

static void
rehash_omotd(struct rpi_client *source_p)
{
	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s is forcing re-reading of OPER MOTD file",
			     get_oper_name(source_p));

	free_cachefile(oper_motd);
	oper_motd = cache_file(OPATH, "opers.motd", 0);
}

static void
rehash_glines(struct rpi_client *source_p)
{
	sendto_realops_flags(UMODE_ALL, L_ALL, "%s is clearing G-lines",
			     source_p->name);
	clear_temps(&glines);
}

static void
rehash_pglines(struct rpi_client *source_p)
{
	struct gline_pending *glp_ptr;
	dlink_node *ptr;
	dlink_node *next_ptr;

	sendto_realops_flags(UMODE_ALL, L_ALL, "%s is clearing pending glines",
			     source_p->name);

	DLINK_FOREACH_SAFE(ptr, next_ptr, pending_glines.head)
	{
		glp_ptr = ptr->data;

		MyFree(glp_ptr->reason1);
		MyFree(glp_ptr->reason2);
		MyFree(glp_ptr);
		dlinkDestroy(ptr, &pending_glines);
	}
}

static void
rehash_tklines(struct rpi_client *source_p)
{
	sendto_realops_flags(UMODE_ALL, L_ALL, "%s is clearing temp klines", source_p->name);
	clear_temps(&tkline_min);
	clear_temps(&tkline_hour);
	clear_temps(&tkline_day);
	clear_temps(&tkline_week);
}

static void
rehash_tdlines(struct rpi_client *source_p)
{
	sendto_realops_flags(UMODE_ALL, L_ALL, "%s is clearing temp dlines", source_p->name);
	clear_temps(&tdline_min);
	clear_temps(&tdline_hour);
	clear_temps(&tdline_day);
	clear_temps(&tdline_week);
}

static void
rehash_rejectcache(struct rpi_client *source_p)
{
	sendto_realops_flags(UMODE_ALL, L_ALL, "%s is clearing reject cache", source_p->name);
	flush_reject();

}

static void
rehash_help(struct rpi_client *source_p)
{
	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s is forcing re-reading of HELP files", 
			     get_oper_name(source_p));
	clear_help_hash();
	load_help();
}

/* *INDENT-OFF* */
static struct hash_commands rehash_commands[] =
{
	{"DNS", 	rehash_dns		},
	{"MOTD", 	rehash_motd		},
	{"OMOTD", 	rehash_omotd		},
	{"GLINES", 	rehash_glines		},
	{"PGLINES", 	rehash_pglines		},
	{"TKLINES", 	rehash_tklines		},
	{"TDLINES", 	rehash_tdlines		},
	{"REJECTCACHE",	rehash_rejectcache	},
	{"HELP", 	rehash_help		},
	{NULL, 		NULL 			}
};
/* *INDENT-ON* */

/*
 * mo_rehash - REHASH message handler
 *
 */
static int
mo_rehash(struct rpi_client *client_p, struct rpi_client *source_p, int parc, const char *parv[])
{
	if(!IsOperRehash(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me->name, source_p->name, "rehash");
		return 0;
	}

	if(parc > 1)
	{
		int x;
		char cmdbuf[100];

		for (x = 0; rehash_commands[x].cmd != NULL && rehash_commands[x].handler != NULL;
		     x++)
		{
			if(irccmp(parv[1], rehash_commands[x].cmd) == 0)
			{
				sendto_one(source_p, form_str(RPL_REHASHING), me->name,
					   source_p->name, rehash_commands[x].cmd);
				rehash_commands[x].handler(source_p);
				ilog(L_MAIN, "REHASH %s From %s[%s]", parv[1],
				     get_oper_name(source_p), source_p->sockhost);
				return 0;
			}
		}

		/* We are still here..we didn't match */
		cmdbuf[0] = '\0';
		for (x = 0; rehash_commands[x].cmd != NULL && rehash_commands[x].handler != NULL;
		     x++)
		{
			strlcat(cmdbuf, " ", sizeof(cmdbuf));
			strlcat(cmdbuf, rehash_commands[x].cmd, sizeof(cmdbuf));
		}
		sendto_one(source_p, ":%s NOTICE %s :rehash one of:%s", me->name, source_p->name,
			   cmdbuf);
	}
	else
	{
		sendto_one(source_p, form_str(RPL_REHASHING), me->name, source_p->name,
			   ConfigFileEntry.configfile);
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s is rehashing server config file", get_oper_name(source_p));
		ilog(L_MAIN, "REHASH From %s[%s]", get_oper_name(source_p),
		     source_p->sockhost);
		rehash(0);
		return 0;
	}

	return 0;
}
