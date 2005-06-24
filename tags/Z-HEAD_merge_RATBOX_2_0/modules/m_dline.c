/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_dline.c: Bans/unbans a user.
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
#include "tools.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "commio.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_log.h"
#include "send.h"
#include "hash.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int mo_dline(struct Client *, struct Client *, int, const char **);
static int mo_undline(struct Client *, struct Client *, int, const char **);

struct Message dline_msgtab = {
	"DLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_dline, 2}}
};
struct Message undline_msgtab = {
	"UNDLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_undline, 2}}
};

mapi_clist_av1 dline_clist[] = { &dline_msgtab, &undline_msgtab, NULL };
DECLARE_MODULE_AV1(dline, NULL, NULL, dline_clist, NULL, NULL, "$Revision$");

static int valid_comment(char *comment);

/* mo_dline()
 * 
 *   parv[1] - dline to add
 *   parv[2] - reason
 */
static int
mo_dline(struct Client *client_p, struct Client *source_p,
	 int parc, const char *parv[])
{
	char def[] = "No Reason";
	const char *dlhost;
	char *oper_reason;
	char *reason = def;
	struct ConfItem *aconf;
	int bits;
	char dlbuffer[IRCD_BUFSIZE];
	const char *current_date;
	int tdline_time = 0;
	int loc = 1;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "kline");
		return 0;
	}

	if((tdline_time = valid_temp_time(parv[loc])) >= 0)
		loc++;

	if(parc < loc + 1)
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "DLINE");
		return 0;
	}

	dlhost = parv[loc];

	if(!parse_netmask(dlhost, NULL, &bits))
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid D-Line",
			   me.name, source_p->name);
		return 0;
	}

	loc++;

	if(parc >= loc + 1)	/* host :reason */
	{
		if(!EmptyString(parv[loc]))
			reason = LOCAL_COPY(parv[loc]);

		if(!valid_comment(reason))
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Invalid character '\"' in comment",
				   me.name, source_p->name);
			return 0;
		}
	}

	if(IsOperAdmin(source_p))
	{
		if(bits < 8)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :For safety, bitmasks less than 8 require conf access.",
				   me.name, parv[0]);
			return 0;
		}
	}
	else if(bits < 16)
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Dline bitmasks less than 16 are for admins only.",
			   me.name, parv[0]);
	}

	if(ConfigFileEntry.non_redundant_klines)
	{
		if((aconf = find_dline_string(dlhost)))
		{
			if(IsConfExemptKline(aconf))
				sendto_one(source_p,
					   ":%s NOTICE %s :[%s] is (E)d-lined by [%s]",
					   me.name, parv[0], dlhost, aconf->host);
			else
				sendto_one(source_p,
					   ":%s NOTICE %s :[%s] already D-lined by [%s] - %s",
					   me.name, parv[0], dlhost, aconf->host,
					   aconf->passwd);
			return 0;
		}
	}

	set_time();
	current_date = smalldate();

	aconf = make_conf();
	aconf->status = CONF_DLINE;
	DupString(aconf->host, dlhost);

	/* Look for an oper reason */
	if((oper_reason = strchr(reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			DupString(aconf->spasswd, oper_reason);
	}

	if(tdline_time > 0)
	{
		ircsnprintf(dlbuffer, sizeof(dlbuffer), 
			 "Temporary D-line %d min. - %s (%s)",
			 (int) (tdline_time / 60), reason, current_date);
		DupString(aconf->passwd, dlbuffer);
		aconf->hold = CurrentTime + tdline_time;

		if(EmptyString(oper_reason))
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s added temporary %d min. D-Line for [%s] [%s]",
					     get_oper_name(source_p), tdline_time / 60,
					     aconf->host, reason);
			ilog(L_KLINE, "D %s %d %s %s",
				get_oper_name(source_p), tdline_time / 60,
				aconf->host, reason);
		}
		else
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s added temporary %d min. D-Line for [%s] [%s|%s]",
					     get_oper_name(source_p), tdline_time / 60,
					     aconf->host, reason, oper_reason);
			ilog(L_KLINE, "D %s %d %s %s|%s",
				get_oper_name(source_p), tdline_time / 60,
				aconf->host, reason, oper_reason);
		}

		sendto_one(source_p, ":%s NOTICE %s :Added temporary %d min. D-Line for [%s]",
			   me.name, source_p->name, tdline_time / 60, aconf->host);
	}
	else
	{
		ircsnprintf(dlbuffer, sizeof(dlbuffer), "%s (%s)", reason, current_date);
		DupString(aconf->passwd, dlbuffer);

		if(EmptyString(oper_reason))
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"%s added D-Line for [%s] [%s]",
					get_oper_name(source_p), dlhost, reason);
			ilog(L_KLINE, "D %s 0 %s %s",
				get_oper_name(source_p), dlhost, reason);
		}
		else
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"%s added D-Line for [%s] [%s|%s]",
					get_oper_name(source_p), dlhost, reason,
					oper_reason);
			ilog(L_KLINE, "D %s 0 %s %s|%s",
				get_oper_name(source_p), dlhost, reason, oper_reason);
		}

		sendto_one(source_p, ":%s NOTICE %s :Added D-Line [%s]",
				me.name, source_p->name, dlhost);
	}

	add_dline(aconf);
	check_dlines();
	return 0;
}

/* mo_undline()
 *
 *      parv[1] = dline to remove
 */
static int
mo_undline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(!IsOperUnkline(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "unkline");
		return 0;
	}

	if(remove_dline(parv[1]))
	{
		sendto_one(source_p, ":%s NOTICE %s :D-Line for [%s] is removed",
				me.name, source_p->name, parv[1]);
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s has removed the D-Line for: [%s]", 
				     get_oper_name(source_p), parv[1]);
		ilog(L_KLINE, "UD %s %s", get_oper_name(source_p), parv[1]);
	}

	return 0;
}

/*
 * valid_comment
 * inputs	- pointer to client
 *              - pointer to comment
 * output       - 0 if no valid comment, 1 if valid
 * side effects - NONE
 */
static int
valid_comment(char *comment)
{
	if(strchr(comment, '"'))
		return 0;

	if(strlen(comment) > REASONLEN)
		comment[REASONLEN] = '\0';

	return 1;
}

