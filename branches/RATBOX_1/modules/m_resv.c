/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_resv.c: Reserves(jupes) a nickname or channel.
 *
 *  Copyright (C) 2001-2002 Hybrid Development Team
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
#include "handlers.h"
#include "client.h"
#include "channel.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "resv.h"
#include "hash.h"
#include "s_log.h"
#include "sprintf_irc.h"
#include "cluster.h"

static void mo_resv(struct Client *, struct Client *, int, char **);
static void ms_resv(struct Client *, struct Client *, int, char **);
static void me_resv(struct Client *, struct Client *, int, char **);
static void mo_unresv(struct Client *, struct Client *, int, char **);
static void ms_unresv(struct Client *, struct Client *, int, char **);
static void me_unresv(struct Client *, struct Client *, int, char **);

static void parse_resv(struct Client *source_p, char *name, 
			char *reason);
static void remove_resv(struct Client *source_p, const char *name);

static void propagate_resv(struct Client *source_p, const char *target,
		const char *name, const char *reason);
static void cluster_resv(struct Client *source_p, 
		const char *name, const char *reason);

struct Message resv_msgtab = {
	"RESV", 0, 0, 3, 0, MFLG_SLOW | MFLG_UNREG, 0,
	{m_ignore, m_not_oper, ms_resv, me_resv, mo_resv}
};

struct Message unresv_msgtab = {
	"UNRESV", 0, 0, 2, 0, MFLG_SLOW | MFLG_UNREG, 0,
	{m_ignore, m_not_oper, ms_unresv, me_unresv, mo_unresv}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&resv_msgtab);
	mod_add_cmd(&unresv_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&resv_msgtab);
	mod_del_cmd(&unresv_msgtab);
}

const char *_version = "$Revision$";
#endif

/*
 * mo_resv()
 *      parv[0] = sender prefix
 *      parv[1] = channel/nick to forbid
 *      parv[2] = reason
 */
static void
mo_resv(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	char *reason;

	if(parc == 5)
		reason = parv[4];
	else
		reason = parv[2];

	if(BadPtr(parv[1]) || BadPtr(reason))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "RESV");
		return;
	}

	/* remote resv.. */
	if((parc == 5) && (irccmp(parv[2], "ON") == 0))
	{
		propagate_resv(source_p, parv[3], parv[1], reason);

		if(match(parv[3], me.name) == 0)
			return;
	}
	else if(dlink_list_length(&cluster_list) > 0)
		cluster_resv(source_p, parv[1], reason);

	parse_resv(source_p, parv[1], reason);
}

/* ms_resv()
 *     parv[0] = sender prefix
 *     parv[1] = target server
 *     parv[2] = channel/nick to forbid
 *     parv[3] = reason
 */
static void
ms_resv(struct Client *client_p, struct Client *source_p,
	int parc, char *parv[])
{
	if((parc != 4) || BadPtr(parv[3]))
		return;

	/* parv[0]  parv[1]        parv[2]  parv[3]
	 * oper     target server  resv     reason
	 */
	propagate_resv(source_p, parv[1], parv[2], parv[3]);

	if(!match(parv[1], me.name))
		return;

	if(!IsPerson(source_p))
		return;

	if(find_cluster(source_p->user->server, CLUSTER_RESV) ||
	   find_u_conf((char *) source_p->user->server, source_p->username,
			source_p->host, OPER_RESV))
	{
		parse_resv(source_p, parv[2], parv[3]);
	}
}

static void
me_resv(struct Client *client_p, struct Client *source_p,
	int parc, char *parv[])
{
	if((parc != 5) || EmptyString(parv[4]))
		return;

	if(!IsPerson(source_p))
		return;

	if(find_cluster(source_p->user->server, CLUSTER_RESV) ||
	   find_u_conf((char *) source_p->user->server, source_p->username,
			source_p->host, OPER_RESV))
	{
		/* we cant handle temps. */
		if(atoi(parv[1]) > 0)
			return;

		parse_resv(source_p, parv[2], parv[4]);
	}
}

/* parse_resv()
 *
 * inputs	- source_p if error messages wanted
 * 		- thing to resv
 * 		- reason for resv
 * outputs	-
 * side effects - will parse the resv and create it if valid
 */
static void
parse_resv(struct Client *source_p, char *name, char *reason)
{
	if(IsChannelName(name))
	{
		struct ResvEntry *resv_p;

		resv_p = create_resv(name, reason, RESV_CHANNEL);

		if(resv_p == NULL)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :A RESV has already been placed on channel: %s",
				   me.name, source_p->name, name);
			return;
		}

		write_confitem(RESV_TYPE, source_p, NULL, resv_p->name, resv_p->reason,
			       NULL, NULL, 0);
	}
	else if(clean_resv_nick(name))
	{
		struct ResvEntry *resv_p;

		if(!valid_wild_card_simple(name))
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Please include at least %d non-wildcard characters with the resv",
				   me.name, source_p->name, 
				   ConfigFileEntry.min_nonwildcard_simple);
			return;
		}

		resv_p = create_resv(name, reason, RESV_NICK);

		if(resv_p == NULL)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :A RESV has already been placed on nick: %s",
				   me.name, source_p->name, name);
			return;
		}

		write_confitem(RESV_TYPE, source_p, NULL, resv_p->name, resv_p->reason,
			       NULL, NULL, 0);
	}
	else
		sendto_one(source_p,
			   ":%s NOTICE %s :You have specified an invalid resv: [%s]",
			   me.name, source_p->name, name);
}

static void
propagate_resv(struct Client *source_p, const char *target,
		const char *name, const char *reason)
{
	sendto_match_servs(source_p, target, CAP_CLUSTER, NOCAPS,
			"RESV %s %s :%s",
			target, name, reason);
	sendto_match_servs(source_p, target, CAP_ENCAP, CAP_CLUSTER,
			"ENCAP %s RESV 0 %s 0 :%s",
			target, name, reason);
}

static void
cluster_resv(struct Client *source_p, const char *name, const char *reason)
{
	struct cluster *clptr;
	dlink_node *ptr;

	DLINK_FOREACH(ptr, cluster_list.head)
	{
		clptr = ptr->data;

		if(!(clptr->type & CLUSTER_RESV))
			continue;

		sendto_match_servs(source_p, clptr->name, CAP_CLUSTER, NOCAPS,
				"RESV %s %s :%s",
				clptr->name, name, reason);
		sendto_match_servs(source_p, clptr->name, CAP_ENCAP, CAP_CLUSTER,
				"ENCAP %s RESV 0 %s 0 :%s",
				clptr->name, name, reason);
	}
}

/*
 * mo_unresv()
 *     parv[0] = sender prefix
 *     parv[1] = channel/nick to unforbid
 */
static void
mo_unresv(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if(BadPtr(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "RESV");
		return;
	}

	if((parc == 4) && (irccmp(parv[2], "ON") == 0))
	{
		propagate_generic(source_p, "UNRESV", parv[3], CAP_CLUSTER,
				"%s", parv[1]);

		if(match(parv[3], me.name) == 0)
			return;
	}
	else if(dlink_list_length(&cluster_list) > 0)
	{
		cluster_generic(source_p, "UNRESV", CLUSTER_UNRESV, CAP_CLUSTER,
				"%s", parv[1]);
	}

	remove_resv(source_p, parv[1]);
}

/* ms_unresv()
 *     parv[0] = sender prefix
 *     parv[1] = target server
 *     parv[2] = resv to remove
 */
static void
ms_unresv(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if((parc != 3) || BadPtr(parv[2]))
		return;

	/* parv[0]  parv[1]        parv[2]
	 * oper     target server  resv to remove
	 */
	propagate_generic(source_p, "UNRESV", parv[1], CAP_CLUSTER,
			"%s", parv[2]);

	if(!match(me.name, parv[1]))
		return;

	if(!IsPerson(source_p))
		return;

	if(find_cluster(source_p->user->server, CLUSTER_UNRESV) ||
	   find_u_conf((char *) source_p->user->server, source_p->username,
			    source_p->host, OPER_RESV))
	{
		remove_resv(source_p, parv[2]);
	}
}

static void
me_unresv(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if(parc < 2 || EmptyString(parv[1]))
		return;

	if(!IsPerson(source_p))
		return;

	if(find_cluster(source_p->user->server, CLUSTER_UNRESV) ||
	   find_u_conf((char *) source_p->user->server, source_p->username,
			source_p->host, OPER_RESV))
	{
		remove_resv(source_p, parv[1]);
	}
}

/* remove_resv()
 *
 * inputs	- client removing the resv
 * 		- resv to remove
 * outputs	-
 * side effects - resv if found, is removed
 */
static void
remove_resv(struct Client *source_p, const char *name)
{
	FBFILE *in, *out;
	char buf[BUFSIZE];
	char buff[BUFSIZE];
	char temppath[BUFSIZE];
	const char *filename;
	mode_t oldumask;
	char *p;
	int error_on_write = 0;
	int found_resv = 0;

	ircsprintf(temppath, "%s.tmp", ConfigFileEntry.resvfile);
	filename = get_conf_name(RESV_TYPE);

	if((in = fbopen(filename, "r")) == NULL)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
			   me.name, source_p->name, filename);
		return;
	}

	oldumask = umask(0);

	if((out = fbopen(temppath, "w")) == NULL)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
			   me.name, source_p->name, temppath);
		fbclose(in);
		umask(oldumask);
		return;
	}

	umask(oldumask);

	while (fbgets(buf, sizeof(buf), in))
	{
		char *resv_name;

		if(error_on_write)
		{
			if(temppath != NULL)
				(void) unlink(temppath);

			break;
		}

		strlcpy(buff, buf, sizeof(buff));

		if((p = strchr(buff, '\n')) != NULL)
			*p = '\0';

		if((*buff == '\0') || (*buff == '#'))
		{
			error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
			continue;
		}

		if((resv_name = getfield(buff)) == NULL)
		{
			error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
			continue;
		}

		if(irccmp(resv_name, name) == 0)
		{
			found_resv++;
		}
		else
		{
			error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
		}
	}

	fbclose(in);
	fbclose(out);

	if(!error_on_write)
	{
		(void) rename(temppath, filename);
		rehash(0);
	}
	else
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Couldn't write temp resv file, aborted",
			   me.name, source_p->name);
		return;
	}

	
	if(!found_resv)
	{
		sendto_one(source_p, ":%s NOTICE %s :No RESV for %s",
			   me.name, source_p->name, name);
		return;
	}

	sendto_one(source_p, ":%s NOTICE %s :RESV for [%s] is removed",
		   me.name, source_p->name, name);

	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s has removed the RESV for: [%s]",
			     get_oper_name(source_p), name);
	ilog(L_NOTICE, "%s has removed the RESV for [%s]",
	     get_oper_name(source_p), name);
}

