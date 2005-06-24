/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_dline.c: Bans/unbans a user.
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
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "commio.h"
#include "s_conf.h"
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
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {mo_dline, 2}}
};
struct Message undline_msgtab = {
	"UNDLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {mo_undline, 2}}
};

mapi_clist_av1 dline_clist[] = { &dline_msgtab, &undline_msgtab, NULL };
DECLARE_MODULE_AV1(dline, NULL, NULL, dline_clist, NULL, NULL, "$Revision$");

static time_t valid_tkline(struct Client *source_p, const char *string);
static int valid_comment(char *comment);
static int flush_write(struct Client *, FBFILE *, char *, char *);
static int remove_temp_dline(const char *);

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
	struct sockaddr_storage daddr;
	char cidr_form_host[HOSTLEN + 1];
	struct ConfItem *aconf;
	int bits;
	char dlbuffer[IRCD_BUFSIZE];
	const char *current_date;
	int tdline_time = 0;
	int loc = 0;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need kline = yes;", me.name, parv[0]);
		return 0;
	}

	loc++;

	tdline_time = valid_tkline(source_p, parv[loc]);

	if(tdline_time)
		loc++;

	if(parc < loc + 1)
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "DLINE");
		return 0;
	}

	dlhost = parv[loc];
	strlcpy(cidr_form_host, dlhost, sizeof(cidr_form_host));

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
	else
	{
		if(bits < 16)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Dline bitmasks less than 16 are for admins only.",
				   me.name, parv[0]);
			return 0;
		}
	}

	if(ConfigFileEntry.non_redundant_klines)
	{
		const char *creason;
		int t = AF_INET;
		(void) parse_netmask(dlhost, &daddr, NULL);
#ifdef IPV6
        	if(t == HM_IPV6)
                	t = AF_INET6;
                else
#endif
			t = AF_INET;
                                  		
		if((aconf = find_dline(&daddr, t)) != NULL)
		{
			creason = aconf->passwd ? aconf->passwd : "<No Reason>";
			if(IsConfExemptKline(aconf))
				sendto_one(source_p,
					   ":%s NOTICE %s :[%s] is (E)d-lined by [%s] - %s",
					   me.name, parv[0], dlhost, aconf->host, creason);
			else
				sendto_one(source_p,
					   ":%s NOTICE %s :[%s] already D-lined by [%s] - %s",
					   me.name, parv[0], dlhost, aconf->host, creason);
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

	if(tdline_time)
	{
		ircsnprintf(dlbuffer, sizeof(dlbuffer), 
			 "Temporary D-line %d min. - %s (%s)",
			 (int) (tdline_time / 60), reason, current_date);
		DupString(aconf->passwd, dlbuffer);
		aconf->hold = CurrentTime + tdline_time;
		add_temp_dline(aconf);

		if(EmptyString(oper_reason))
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s added temporary %d min. D-Line for [%s] [%s]",
					     get_oper_name(source_p), tdline_time / 60,
					     aconf->host, reason);
			ilog(L_KLINE, "%s added temporary %d min. D-Line for [%s] [%s]",
			     source_p->name, tdline_time / 60, aconf->host, reason);
		}
		else
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s added temporary %d min. D-Line for [%s] [%s|%s]",
					     get_oper_name(source_p), tdline_time / 60,
					     aconf->host, reason, oper_reason);
			ilog(L_KLINE, "%s added temporary %d min. D-Line for [%s] [%s|%s]",
			     source_p->name, tdline_time / 60, aconf->host, reason, oper_reason);
		}

		sendto_one(source_p, ":%s NOTICE %s :Added temporary %d min. D-Line for [%s]",
			   me.name, source_p->name, tdline_time / 60, aconf->host);
	}
	else
	{
		ircsnprintf(dlbuffer, sizeof(dlbuffer), "%s (%s)", reason, current_date);
		DupString(aconf->passwd, dlbuffer);
		add_conf_by_address(aconf->host, CONF_DLINE, NULL, aconf);
		write_confitem(DLINE_TYPE, source_p, NULL, aconf->host, reason,
			       oper_reason, current_date, 0);
	}

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
	FBFILE *in;
	FBFILE *out;
	char buf[BUFSIZE], buff[BUFSIZE], temppath[BUFSIZE], *p;
	const char *filename, *found_cidr;
	const char *cidr;
	int pairme = NO, error_on_write = NO;
	mode_t oldumask;

	ircsnprintf(temppath, sizeof(temppath), "%s.tmp", ConfigFileEntry.dlinefile);

	if(!IsOperUnkline(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need unkline = yes;", me.name, parv[0]);
		return 0;
	}

	cidr = parv[1];

	if(remove_temp_dline(cidr))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Un-dlined [%s] from temporary D-lines",
			   me.name, parv[0], cidr);
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s has removed the temporary D-Line for: [%s]",
				     get_oper_name(source_p), cidr);
		ilog(L_KLINE, "%s removed temporary D-Line for [%s]", parv[0], cidr);
		return 0;
	}

	filename = get_conf_name(DLINE_TYPE);

	if((in = fbopen(filename, "r")) == 0)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s", me.name, parv[0], filename);
		return 0;
	}

	oldumask = umask(0);
	if((out = fbopen(temppath, "w")) == 0)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s", me.name, parv[0], temppath);
		fbclose(in);
		umask(oldumask);
		return 0;
	}

	umask(oldumask);

	while (fbgets(buf, sizeof(buf), in))
	{
		strlcpy(buff, buf, sizeof(buff));

		if((p = strchr(buff, '\n')) != NULL)
			*p = '\0';

		if((*buff == '\0') || (*buff == '#'))
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
			continue;
		}

		if((found_cidr = getfield(buff)) == NULL)
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
			continue;
		}

		if(irccmp(found_cidr, cidr) == 0)
		{
			pairme++;
		}
		else
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
			continue;
		}
	}

	fbclose(in);
	fbclose(out);

	if(error_on_write)
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Couldn't write D-line file, aborted", 
			   me.name, parv[0]);
		return 0;
	}
	else if(!pairme)
	{
		sendto_one(source_p, ":%s NOTICE %s :No D-Line for %s",
			   me.name, parv[0], cidr);

		if(temppath != NULL)
			(void) unlink(temppath);

		return 0;
	}

	(void) rename(temppath, filename);
	rehash(0);


	sendto_one(source_p, ":%s NOTICE %s :D-Line for [%s] is removed", me.name, parv[0], cidr);
	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s has removed the D-Line for: [%s]", get_oper_name(source_p), cidr);
	ilog(L_KLINE, "%s removed D-Line for [%s]", get_oper_name(source_p), cidr);

	return 0;
}

/*
 * valid_tkline()
 * 
 * inputs       - pointer to client requesting kline
 *              - argument count
 *              - pointer to ascii string in
 * output       - -1 not enough parameters
 *              - 0 if not an integer number, else the number
 * side effects - none
 */
static time_t
valid_tkline(struct Client *source_p, const char *p)
{
	time_t result = 0;

	while (*p)
	{
		if(IsDigit(*p))
		{
			result *= 10;
			result += ((*p) & 0xF);
			p++;
		}
		else
			return (0);
	}
	/* in the degenerate case where oper does a /quote kline 0 user@host :reason 
	 * i.e. they specifically use 0, I am going to return 1 instead
	 * as a return value of non-zero is used to flag it as a temporary kline
	 */

	if(result == 0)
		result = 1;

	if(result > (24 * 60 * 7 * 4))
		result = (24 * 60 * 7 * 4);	/* Max it at 4 weeks */

	result = (time_t) result *(time_t) 60;	/* turn it into seconds */

	return (result);
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

/*
 * flush_write()
 *
 * inputs       - pointer to client structure of oper requesting unkline
 *              - out is the file descriptor
 *              - buf is the buffer to write
 *              - ntowrite is the expected number of character to be written
 *              - temppath is the temporary file name to be written
 * output       - YES for error on write
 *              - NO for success
 * side effects - if successful, the buf is written to output file
 *                if a write failure happesn, and the file pointed to
 *                by temppath, if its non NULL, is removed.
 *
 * The idea here is, to be as robust as possible when writing to the 
 * kline file.
 *
 * -Dianora
 */
static int
flush_write(struct Client *source_p, FBFILE * out, char *buf, char *temppath)
{
	int error_on_write = (fbputs(buf, out) < 0) ? YES : NO;

	if(error_on_write)
	{
		sendto_one(source_p, ":%s NOTICE %s :Unable to write to %s",
			   me.name, source_p->name, temppath);
		fbclose(out);
		if(temppath != NULL)
			(void) unlink(temppath);
	}
	return (error_on_write);
}

static dlink_list *tdline_list[] = {
	&tdline_hour,
	&tdline_day,
	&tdline_min,
	&tdline_week,
	NULL
};

/* remove_temp_dline()
 *
 * inputs       - hostname to undline
 * outputs      -
 * side effects - tries to undline anything that matches
 */
static int
remove_temp_dline(const char *host)
{
	dlink_list *tdlist;
	struct ConfItem *aconf;
	dlink_node *ptr;
	struct sockaddr_storage addr, caddr;
	int bits, cbits;
	int i;

	parse_netmask(host, &addr, &bits);

	for (i = 0; tdline_list[i] != NULL; i++)
	{
		tdlist = tdline_list[i];

		DLINK_FOREACH(ptr, tdlist->head)
		{
			aconf = ptr->data;

			parse_netmask(aconf->host, &caddr, &cbits);

			if(comp_with_mask_sock(&addr, &caddr, bits) && bits == cbits)
			{
				dlinkDestroy(ptr, tdlist);
				delete_one_address_conf(aconf->host, aconf);
				return YES;
			}
		}
	}

	return NO;
}
