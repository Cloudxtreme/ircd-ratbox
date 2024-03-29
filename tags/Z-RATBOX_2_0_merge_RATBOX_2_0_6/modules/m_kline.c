/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_kline.c: Bans/unbans a user.
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
#include "common.h"
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
#include "event.h"

static int mo_kline(struct Client *, struct Client *, int, const char **);
static int ms_kline(struct Client *, struct Client *, int, const char **);
static int me_kline(struct Client *, struct Client *, int, const char **);
static int mo_unkline(struct Client *, struct Client *, int, const char **);
static int ms_unkline(struct Client *, struct Client *, int, const char **);
static int me_unkline(struct Client *, struct Client *, int, const char **);

struct Message kline_msgtab = {
	"KLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_kline, 6}, {ms_kline, 6}, {me_kline, 5}, {mo_kline, 2}}
};

struct Message unkline_msgtab = {
	"UNKLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_unkline, 4}, {ms_unkline, 4}, {me_unkline, 3}, {mo_unkline, 2}}
};

mapi_clist_av1 kline_clist[] = { &kline_msgtab, &unkline_msgtab, NULL };
DECLARE_MODULE_AV1(kline, NULL, NULL, kline_clist, NULL, NULL, "$Revision$");

/* Local function prototypes */
static int find_user_host(struct Client *source_p, const char *userhost, char *user, char *host);
static int valid_comment(struct Client *source_p, char *comment);
static int valid_user_host(struct Client *source_p, const char *user, const char *host);
static int valid_wild_card(struct Client *source_p, const char *user, const char *host);

static void handle_remote_kline(struct Client *source_p, int tkline_time,
		const char *user, const char *host, const char *reason);
static void apply_kline(struct Client *source_p, struct ConfItem *aconf,
			const char *reason, const char *oper_reason, const char *current_date);
static void apply_tkline(struct Client *source_p, struct ConfItem *aconf,
			 const char *, const char *, const char *, int);
static int already_placed_kline(struct Client *, const char *, const char *, int);

static void handle_remote_unkline(struct Client *source_p, 
			const char *user, const char *host);
static void remove_permkline_match(struct Client *, const char *, const char *);
static int flush_write(struct Client *, FBFILE *, const char *, const char *);
static int remove_temp_kline(const char *, const char *);

/* mo_kline()
 *
 *   parv[1] - temp time or user@host
 *   parv[2] - user@host, "ON", or reason
 *   parv[3] - "ON", reason, or server to target
 *   parv[4] - server to target, or reason
 *   parv[5] - reason
 */
static int
mo_kline(struct Client *client_p, struct Client *source_p,
	 int parc, const char **parv)
{
	char def[] = "No Reason";
	char user[USERLEN + 2];
	char host[HOSTLEN + 2];
	char buffer[IRCD_BUFSIZE];
	char *reason = def;
	char *oper_reason;
	const char *current_date;
	const char *target_server = NULL;
	struct ConfItem *aconf;
	int tkline_time = 0;
	int loc = 1;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "kline");
		return 0;
	}

	if((tkline_time = valid_temp_time(parv[loc])) >= 0)
		loc++;
	/* we just set tkline_time to -1! */
	else
		tkline_time = 0;

	if(find_user_host(source_p, parv[loc], user, host) == 0)
		return 0;

	loc++;

	if(parc >= loc+2 && !irccmp(parv[loc], "ON"))
	{
		if(!IsOperRemoteBan(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				me.name, source_p->name, "remoteban");
			return 0;
		}

		target_server = parv[loc+1];
		loc += 2;
	}

	if(parc <= loc || EmptyString(parv[loc]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "KLINE");
		return 0;
	}

	reason = LOCAL_COPY(parv[loc]);

	if(target_server != NULL)
	{
		propagate_generic(source_p, "KLINE", target_server, CAP_KLN,
				"%d %s %s :%s",
				tkline_time, user, host, reason);

		/* If we are sending it somewhere that doesnt include us, stop */
		if(!match(target_server, me.name))
			return 0;
	}
	/* if we have cluster servers, send it to them.. */
	else if(dlink_list_length(&cluster_conf_list) > 0)
		cluster_generic(source_p, "KLINE", 
				(tkline_time > 0) ? SHARED_TKLINE : SHARED_PKLINE, CAP_KLN,
				"%lu %s %s :%s",
				tkline_time, user, host, reason);

	if(!valid_user_host(source_p, user, host) || 
	   !valid_wild_card(source_p, user, host) ||
	   !valid_comment(source_p, reason))
		return 0;

	if(already_placed_kline(source_p, user, host, tkline_time))
		return 0;

	set_time();
	current_date = smalldate();
	aconf = make_conf();
	aconf->status = CONF_KILL;
	DupString(aconf->host, host);
	DupString(aconf->user, user);
	aconf->port = 0;

	/* Look for an oper reason */
	if((oper_reason = strchr(reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			DupString(aconf->spasswd, oper_reason);
	}

	if(tkline_time > 0)
	{
		ircsnprintf(buffer, sizeof(buffer),
			   "Temporary K-line %d min. - %s (%s)",
			   (int) (tkline_time / 60), reason, current_date);
		DupString(aconf->passwd, buffer);
		apply_tkline(source_p, aconf, reason, oper_reason, current_date, tkline_time);
	}
	else
	{
		ircsnprintf(buffer, sizeof(buffer), "%s (%s)", reason, current_date);
		DupString(aconf->passwd, buffer);
		apply_kline(source_p, aconf, reason, oper_reason, current_date);
	}

	if(ConfigFileEntry.kline_delay)
	{
		if(kline_queued == 0)
		{
			eventAddOnce("check_klines", check_klines_event, NULL,
				     ConfigFileEntry.kline_delay);
			kline_queued = 1;
		}
	}
	else
		check_klines();

	return 0;
}

/* ms_kline()
 *
 *   parv[1] - server targeted at
 *   parv[2] - tkline time (0 if perm)
 *   parv[3] - user
 *   parv[4] - host
 *   parv[5] - reason
 */
static int
ms_kline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int tkline_time = atoi(parv[2]);

	propagate_generic(source_p, "KLINE", parv[1], CAP_KLN,
			"%d %s %s :%s",
			tkline_time, parv[3], parv[4], parv[5]);

	if(!match(parv[1], me.name))
		return 0;

	if(!IsPerson(source_p))
		return 0;

	handle_remote_kline(source_p, tkline_time, parv[3], parv[4], parv[5]);
	return 0;
}

static int
me_kline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* <tkline_time> <user> <host> :<reason> */
	if(!IsPerson(source_p))
		return 0;

	handle_remote_kline(source_p, atoi(parv[1]), parv[2], parv[3], parv[4]);
	return 0;
}

static void
handle_remote_kline(struct Client *source_p, int tkline_time,
		const char *user, const char *host, const char *kreason)
{
	const char *current_date;
	char *reason = LOCAL_COPY(kreason);
	struct ConfItem *aconf = NULL;
	char *oper_reason;

	if(!find_shared_conf(source_p->username, source_p->host,
				source_p->user->server, 
				(tkline_time > 0) ? SHARED_TKLINE : SHARED_PKLINE))
		return;

	if(!valid_user_host(source_p, user, host) ||
	   !valid_wild_card(source_p, user, host) ||
	   !valid_comment(source_p, reason))
		return;

	if(already_placed_kline(source_p, user, host, tkline_time))
		return;

	aconf = make_conf();

	aconf->status = CONF_KILL;
	DupString(aconf->user, user);
	DupString(aconf->host, host);

	/* Look for an oper reason */
	if((oper_reason = strchr(reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			DupString(aconf->spasswd, oper_reason);
	}

	DupString(aconf->passwd, reason);
	current_date = smalldate();

	if(tkline_time > 0)
		apply_tkline(source_p, aconf, reason, oper_reason,
				current_date, tkline_time);
	else
		apply_kline(source_p, aconf, aconf->passwd, oper_reason, current_date);

	if(ConfigFileEntry.kline_delay)
	{
		if(kline_queued == 0)
		{
			eventAddOnce("check_klines", check_klines_event, NULL,
				     ConfigFileEntry.kline_delay);
			kline_queued = 1;
		}
	}
	else
		check_klines();

	return;
}

/* mo_unkline()
 *
 *   parv[1] - kline to remove
 *   parv[2] - optional "ON"
 *   parv[3] - optional target server
 */
static int
mo_unkline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *user;
	char *host;
	char splat[] = "*";
	char *h = LOCAL_COPY(parv[1]);

	if(!IsOperUnkline(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "unkline");
		return 0;
	}

	if((host = strchr(h, '@')) || *h == '*')
	{
		/* Explicit user@host mask given */

		if(host)	/* Found user@host */
		{
			*host++ = '\0';

			/* check for @host */
			if(*h)
				user = h;
			else
				user = splat;

			/* check for user@ */
			if(!*host)
				host = splat;
		}
		else
		{
			user = splat;	/* no @ found, assume its *@somehost */
			host = h;
		}
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid parameters", me.name, source_p->name);
		return 0;
	}

	/* possible remote kline.. */
	if((parc > 3) && (irccmp(parv[2], "ON") == 0))
	{
		if(!IsOperRemoteBan(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				me.name, source_p->name, "remoteban");
			return 0;
		}

		propagate_generic(source_p, "UNKLINE", parv[3], CAP_UNKLN,
				"%s %s", user, host);

		if(match(parv[3], me.name) == 0)
			return 0;
	}
	else if(dlink_list_length(&cluster_conf_list) > 0)
		cluster_generic(source_p, "UNKLINE", SHARED_UNKLINE, CAP_UNKLN,
				"%s %s", user, host);

	if(remove_temp_kline(user, host))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Un-klined [%s@%s] from temporary k-lines",
			   me.name, parv[0], user, host);
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s has removed the temporary K-Line for: [%s@%s]",
				     get_oper_name(source_p), user, host);
		ilog(L_KLINE, "UK %s %s %s",
			get_oper_name(source_p), user, host);
		return 0;
	}

	remove_permkline_match(source_p, host, user);

	return 0;
}

/* ms_unkline()
 *
 *   parv[1] - target server
 *   parv[2] - user to unkline
 *   parv[3] - host to unkline
 */
static int
ms_unkline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* parv[0]  parv[1]        parv[2]  parv[3]
	 * oper     target server  user     host    */
	propagate_generic(source_p, "UNKLINE", parv[1], CAP_UNKLN,
			"%s %s", parv[2], parv[3]);

	if(!match(parv[1], me.name))
		return 0;

	if(!IsPerson(source_p))
		return 0;

	handle_remote_unkline(source_p, parv[2], parv[3]);
	return 0;
}

static int
me_unkline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* user host */
	if(!IsPerson(source_p))
		return 0;

	handle_remote_unkline(source_p, parv[1], parv[2]);
	return 0;
}

static void
handle_remote_unkline(struct Client *source_p, const char *user, const char *host)
{
	if(!find_shared_conf(source_p->username, source_p->host,
				source_p->user->server, SHARED_UNKLINE))
		return;

	if(remove_temp_kline(user, host))
	{
		sendto_one_notice(source_p,
				":Un-klined [%s@%s] from temporary k-lines",
				user, host);

		sendto_realops_flags(UMODE_ALL, L_ALL,
				"%s has removed the temporary K-Line for: [%s@%s]",
				get_oper_name(source_p), user, host);

		ilog(L_KLINE, "UK %s %s %s",
			get_oper_name(source_p), user, host);
		return;
	}

	remove_permkline_match(source_p, host, user);
}

/* apply_kline()
 *
 * inputs	- 
 * output	- NONE
 * side effects	- kline as given, is added to the hashtable
 *		  and conf file
 */
static void
apply_kline(struct Client *source_p, struct ConfItem *aconf,
	    const char *reason, const char *oper_reason, const char *current_date)
{
	add_conf_by_address(aconf->host, CONF_KILL, aconf->user, aconf);
	write_confitem(KLINE_TYPE, source_p, aconf->user, aconf->host,
		       reason, oper_reason, current_date, 0);
}

/* apply_tkline()
 *
 * inputs	-
 * output	- NONE
 * side effects	- tkline as given is placed
 */
static void
apply_tkline(struct Client *source_p, struct ConfItem *aconf,
	     const char *reason, const char *oper_reason, const char *current_date, int tkline_time)
{
	aconf->hold = CurrentTime + tkline_time;
	add_temp_kline(aconf);

	/* no oper reason.. */
	if(EmptyString(oper_reason))
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s added temporary %d min. K-Line for [%s@%s] [%s]",
				     get_oper_name(source_p), tkline_time / 60,
				     aconf->user, aconf->host, reason);
		ilog(L_KLINE, "K %s %d %s %s %s",
			get_oper_name(source_p), tkline_time / 60,
			aconf->user, aconf->host, reason);
	}
	else
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s added temporary %d min. K-Line for [%s@%s] [%s|%s]",
				     get_oper_name(source_p), tkline_time / 60,
				     aconf->user, aconf->host, reason, oper_reason);
		ilog(L_KLINE, "K %s %d %s %s %s|%s",
			get_oper_name(source_p), tkline_time / 60,
			aconf->user, aconf->host, reason, oper_reason);
	}

	sendto_one_notice(source_p, ":Added temporary %d min. K-Line [%s@%s]",
			  tkline_time / 60, aconf->user, aconf->host);
}

/* find_user_host()
 * 
 * inputs	- client placing kline, user@host, user buffer, host buffer
 * output	- 0 if not ok to kline, 1 to kline i.e. if valid user host
 * side effects -
 */
static int
find_user_host(struct Client *source_p, const char *userhost, char *luser, char *lhost)
{
	char *hostp;

	hostp = strchr(userhost, '@');
	
	if(hostp != NULL)	/* I'm a little user@host */
	{
		*(hostp++) = '\0';	/* short and squat */
		if(*userhost)
			strlcpy(luser, userhost, USERLEN + 1);	/* here is my user */
		else
			strcpy(luser, "*");
		if(*hostp)
			strlcpy(lhost, hostp, HOSTLEN + 1);	/* here is my host */
		else
			strcpy(lhost, "*");
		}
	else
	{
		/* no '@', no '.', so its not a user@host or host, therefore
		 * its a nick, which support was removed for.
		 */
		if(strchr(userhost, '.') == NULL)
			return 0;

		luser[0] = '*';	/* no @ found, assume its *@somehost */
		luser[1] = '\0';
		strlcpy(lhost, userhost, HOSTLEN + 1);
	}

	return 1;
}

/* valid_user_host()
 *
 * inputs       - user buffer, host buffer
 * output	- 0 if invalid, 1 if valid
 * side effects -
 */
static int
valid_user_host(struct Client *source_p, const char *luser, const char *lhost)
{
	/* # is invalid, as is '!' (n!u@h kline) */
	if(strchr(lhost, '#') || strchr(luser, '#') || strchr(luser, '!'))
	{
		sendto_one_notice(source_p, ":Invalid K-Line");
		return 0;
	}

	return 1;
}

/* valid_wild_card()
 * 
 * input        - user buffer, host buffer
 * output       - 0 if invalid, 1 if valid
 * side effects -
 */
static int
valid_wild_card(struct Client *source_p, const char *luser, const char *lhost)
{
	const char *p;
	char tmpch;
	int nonwild = 0;

	/* check there are enough non wildcard chars */
	p = luser;
	while ((tmpch = *p++))
	{
		if(!IsKWildChar(tmpch))
		{
			/* found enough chars, return */
			if(++nonwild >= ConfigFileEntry.min_nonwildcard)
				return 1;
		}
	}

	/* try host, as user didnt contain enough */
	p = lhost;
	while ((tmpch = *p++))
	{
		if(!IsKWildChar(tmpch))
			if(++nonwild >= ConfigFileEntry.min_nonwildcard)
				return 1;
	}

	sendto_one_notice(source_p,
		   	  ":Please include at least %d non-wildcard "
			  "characters with the user@host",
			  ConfigFileEntry.min_nonwildcard);
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
valid_comment(struct Client *source_p, char *comment)
{
	if(strchr(comment, '"'))
	{
		sendto_one_notice(source_p, ":Invalid character '\"' in comment");
		return 0;
	}

	if(strlen(comment) > REASONLEN)
		comment[REASONLEN] = '\0';

	return 1;
}

/* already_placed_kline()
 *
 * inputs       - source to notify, user@host to check, tkline time
 * outputs      - 1 if a perm kline or a tkline when a tkline is being
 *                set exists, else 0
 * side effects - notifies source_p kline exists
 */
/* Note: This currently works if the new K-line is a special case of an
 *       existing K-line, but not the other way round. To do that we would
 *       have to walk the hash and check every existing K-line. -A1kmm.
 */
static int
already_placed_kline(struct Client *source_p, const char *luser, const char *lhost, int tkline)
{
	const char *reason;
	struct irc_sockaddr_storage iphost, *piphost;
	struct ConfItem *aconf;
        int t;
	if(ConfigFileEntry.non_redundant_klines)
	{
		if((t = parse_netmask(lhost, (struct sockaddr *)&iphost, NULL)) != HM_HOST)
		{
#ifdef IPV6
			if(t == HM_IPV6)
				t = AF_INET6;
			else
#endif
				t = AF_INET;
				
			piphost = &iphost;
		}
		else
			piphost = NULL;

		if((aconf = find_conf_by_address(lhost, NULL, (struct sockaddr *)piphost, CONF_KILL, t, luser)))
		{
			/* setting a tkline, or existing one is perm */
			if(tkline || ((aconf->flags & CONF_FLAGS_TEMPORARY) == 0))
			{
				reason = aconf->passwd ? aconf->passwd : "<No Reason>";

				sendto_one_notice(source_p,
						  ":[%s@%s] already K-Lined by [%s@%s] - %s",
						  luser, lhost, aconf->user,
						  aconf->host, reason);
				return 1;
			}
		}
	}

	return 0;
}

/* remove_permkline_match()
 *
 * hunts for a permanent kline, and removes it.
 */
static void
remove_permkline_match(struct Client *source_p, const char *host, const char *user)
{
	FBFILE *in, *out;
	int pairme = 0;
	int error_on_write = NO;
	char buf[BUFSIZE];
	char matchbuf[BUFSIZE];
	char temppath[BUFSIZE];
	const char *filename;
	mode_t oldumask;
	int matchlen;

	ircsnprintf(temppath, sizeof(temppath),
		 "%s.tmp", ConfigFileEntry.klinefile);

	filename = get_conf_name(KLINE_TYPE);

	if((in = fbopen(filename, "r")) == 0)
	{
		sendto_one_notice(source_p, ":Cannot open %s", filename);
		return;
	}

	oldumask = umask(0);
	if((out = fbopen(temppath, "w")) == 0)
	{
		sendto_one_notice(source_p, ":Cannot open %s", temppath);
		fbclose(in);
		umask(oldumask);
		return;
	}

	umask(oldumask);

	snprintf(matchbuf, sizeof(matchbuf), "\"%s\",\"%s\"", user, host);
	matchlen = strlen(matchbuf);

	while (fbgets(buf, sizeof(buf), in))
	{
		if(error_on_write)
			break;

		if(!strncasecmp(buf, matchbuf, matchlen))
		{
			pairme++;
			break;
		}
		else
			error_on_write = flush_write(source_p, out, buf, temppath);
	}

	/* we dropped out of the loop early because we found a match,
	 * to drop into this somewhat faster loop as we presume we'll never
	 * have two matching klines --anfl
	 */
	if(pairme && !error_on_write)
	{
		while(fbgets(buf, sizeof(buf), in))
		{
			if(error_on_write)
				break;

			error_on_write = flush_write(source_p, out, buf, temppath);
		}
	}

	fbclose(in);
	fbclose(out);

	/* The result of the rename should be checked too... oh well */
	/* If there was an error on a write above, then its been reported
	 * and I am not going to trash the original kline /conf file
	 */
	if(error_on_write)
	{
		sendto_one_notice(source_p, ":Couldn't write temp kline file, aborted");
		return;
	}
	else if(!pairme)
	{
		sendto_one_notice(source_p, ":No K-Line for %s@%s",
				  user, host);

		if(temppath != NULL)
			(void) unlink(temppath);

		return;
	}
		
	(void) rename(temppath, filename);
	rehash(0);

	sendto_one_notice(source_p, ":K-Line for [%s@%s] is removed",
			  user, host);

	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s has removed the K-Line for: [%s@%s]",
			     get_oper_name(source_p), user, host);

	ilog(L_KLINE, "UK %s %s %s",
		get_oper_name(source_p), user, host);
	return;
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
flush_write(struct Client *source_p, FBFILE * out, const char *buf, const char *temppath)
{
	int error_on_write = (fbputs(buf, out) < 0) ? YES : NO;

	if(error_on_write)
	{
		sendto_one_notice(source_p, ":Unable to write to %s",
				  temppath);
		if(temppath != NULL)
			(void) unlink(temppath);
	}
	return (error_on_write);
}

static dlink_list *tkline_list[] = {
	&tkline_hour,
	&tkline_day,
	&tkline_min,
	&tkline_week,
	NULL
};

/* remove_temp_kline()
 *
 * inputs       - username, hostname to unkline
 * outputs      -
 * side effects - tries to unkline anything that matches
 */
static int
remove_temp_kline(const char *user, const char *host)
{
	dlink_list *tklist;
	struct ConfItem *aconf;
	dlink_node *ptr;
	struct irc_sockaddr_storage addr, caddr;
	int bits, cbits;
	int mtype, ktype;
	int i;

	mtype = parse_netmask(host, (struct sockaddr *)&addr, &bits);

	for (i = 0; tkline_list[i] != NULL; i++)
	{
		tklist = tkline_list[i];

		DLINK_FOREACH(ptr, tklist->head)
		{
			aconf = ptr->data;

			ktype = parse_netmask(aconf->host, (struct sockaddr *)&caddr, &cbits);

			if(ktype != mtype || (user && irccmp(user, aconf->user)))
				continue;

			if(ktype == HM_HOST)
			{
				if(irccmp(aconf->host, host))
					continue;
			}
			else if(bits != cbits || 
				!comp_with_mask_sock((struct sockaddr *)&addr,
						(struct sockaddr *)&caddr, bits))
				continue;

			dlinkDestroy(ptr, tklist);
			delete_one_address_conf(aconf->host, aconf);
			return YES;
		}
	}

	return NO;
}
