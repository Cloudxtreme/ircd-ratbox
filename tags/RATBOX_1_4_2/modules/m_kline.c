/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_kline.c: Bans a user.
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
#include "fdlist.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_misc.h"
#include "send.h"
#include "hash.h"
#include "handlers.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "cluster.h"
#include "event.h"

static void mo_kline(struct Client *, struct Client *, int, char **);
static void ms_kline(struct Client *, struct Client *, int, char **);
static void mo_dline(struct Client *, struct Client *, int, char **);

struct Message kline_msgtab = {
	"KLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, ms_kline, mo_kline}
};

struct Message dline_msgtab = {
	"DLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, m_error, mo_dline}
};

#ifndef STATIC_MODULES

void
_modinit(void)
{
	mod_add_cmd(&kline_msgtab);
	mod_add_cmd(&dline_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&kline_msgtab);
	mod_del_cmd(&dline_msgtab);
}
const char *_version = "$Revision$";
#endif

/* Local function prototypes */

static time_t valid_tkline(struct Client *source_p, char *string);
static char *format_kline(char *);
static int find_user_host(struct Client *source_p, char *user_host_or_nick, char *user, char *host);

/* needed to remove unused definition warning */
static int valid_comment(struct Client *source_p, char *comment);
static int valid_user_host(struct Client *source_p, char *luser, char *lhost);
static int valid_wild_card(struct Client *source_p, char *luser, char *lhost);
static int already_placed_kline(struct Client *, char *, char *, 
				int tkline_time);

static void apply_kline(struct Client *source_p, struct ConfItem *aconf,
			const char *reason, const char *oper_reason, const char *current_date);
static void apply_tkline(struct Client *source_p, struct ConfItem *aconf,
			 const char *, const char *, const char *, int);

char buffer[IRCD_BUFSIZE];
char user[USERLEN + 2];
char host[HOSTLEN + 2];

#define MAX_EXT_REASON 100


/*
 * mo_kline
 *
 * inputs	- pointer to server
 *		- pointer to client
 *		- parameter count
 *		- parameter list
 * output	-
 * side effects - k line is added
 *
 */
static void
mo_kline(struct Client *client_p, struct Client *source_p, int parc, char **parv)
{
	char def[] = "No Reason";
	char *reason = def;
	char *oper_reason;
	const char *current_date;
	const char *target_server = NULL;
	struct ConfItem *aconf;
	time_t tkline_time = 0;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need kline = yes;",
			   me.name, source_p->name);
		return;
	}

	parv++;
	parc--;

	tkline_time = valid_tkline(source_p, *parv);

	if(tkline_time > 0)
	{
		parv++;
		parc--;
	}

	if(parc == 0)
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "KLINE");
		return;
	}

	if(find_user_host(source_p, *parv, user, host) == 0)
		return;

	parc--;
	parv++;

	if(parc != 0)
	{
		if(irccmp(*parv, "ON") == 0)
		{
			parc--;
			parv++;
			if(parc == 0)
			{
				sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
					   me.name, source_p->name, "KLINE");
				return;
			}
			target_server = *parv;
			parc--;
			parv++;
		}
	}

	if(parc != 0)
		reason = *parv;

	if(target_server != NULL)
	{
		sendto_match_servs(source_p, target_server, CAP_KLN, NOCAPS,
				   "KLINE %s %ld %s %s :%s",
				   target_server, (long) tkline_time,
				   user, host, reason);
		sendto_match_servs(source_p, target_server, CAP_ENCAP, CAP_KLN,
				"ENCAP %s KLINE %ld %s %s :%s",
				target_server, (long) tkline_time,
				user, host, reason);

		/* If we are sending it somewhere that doesnt include us, stop */
		if(!match(target_server, me.name))
			return;
	}
	else if(dlink_list_length(&cluster_list) > 0)
		cluster_kline(source_p, tkline_time, user, host, reason);

	if(!valid_user_host(source_p, user, host) || 
	   !valid_wild_card(source_p, user, host) ||
	   !valid_comment(source_p, reason))
		return;

	if(already_placed_kline(source_p, user, host, tkline_time))
		return;

	set_time();
	current_date = smalldate(CurrentTime);
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
	}

	if(tkline_time)
	{
		snprintf(buffer, sizeof(buffer),
			   "Temporary K-line %d min. - %s (%s)",
			   (int) (tkline_time / 60), reason, current_date);
		DupString(aconf->passwd, buffer);
		apply_tkline(source_p, aconf, reason, oper_reason, current_date, tkline_time);
	}
	else
	{
		snprintf(buffer, sizeof(buffer), "%s (%s)", reason, current_date);
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
}

/*
 * ms_kline()
 *
 *
 */
static void
ms_kline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	const char *current_date;
	struct ConfItem *aconf = NULL;
	int tkline_time;

	char *kuser;
	char *khost;
	char *kreason;
	char *oper_reason;

	if(parc != 6)
		return;

	/* parv[0]  parv[1]        parv[2]      parv[3]  parv[4]  parv[5] */
	/* oper     target_server  tkline_time  user     host     reason */
	sendto_match_servs(source_p, parv[1], CAP_KLN, NOCAPS,
			   "KLINE %s %s %s %s :%s", 
			   parv[1], parv[2], parv[3], parv[4], parv[5]);
	sendto_match_servs(source_p, parv[1], CAP_ENCAP, CAP_KLN,
			"ENCAP %s KLINE %s %s %s :%s",
			parv[1], parv[2], parv[3], parv[4], parv[5]);

	if(!match(parv[1], me.name))
		return;

	if(!IsPerson(source_p))
		return;

	kuser = parv[3];
	khost = parv[4];
	kreason = parv[5];

	if(!find_cluster(source_p->user->server, CLUSTER_KLINE) &&
	   !find_u_conf((char *) source_p->user->server,
		       source_p->username, source_p->host, OPER_K))
		return;

	if(!valid_user_host(source_p, kuser, khost) || 
	   !valid_wild_card(source_p, kuser, khost) ||
	   !valid_comment(source_p, kreason))
		return;

	tkline_time = atoi(parv[2]);

	if(already_placed_kline(source_p, kuser, khost, tkline_time))
		return;

	aconf = make_conf();
	aconf->status = CONF_KILL;
	DupString(aconf->user, kuser);
	DupString(aconf->host, khost);

	/* Look for an oper reason */
	if((oper_reason = strchr(kreason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;
	}

	DupString(aconf->passwd, kreason);
	current_date = smalldate(CurrentTime);

	if(tkline_time)
		apply_tkline(source_p, aconf, kreason, oper_reason,
				current_date, tkline_time);
	else
		apply_kline(source_p, aconf, aconf->passwd, oper_reason, current_date);

	/* if were set to delay klines, schedule an event if needed */
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
}

/*
 * apply_kline
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

/*
 * apply_tkline
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
	if(BadPtr(oper_reason))
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s added temporary %d min. K-Line for [%s@%s] [%s]",
				     get_oper_name(source_p), tkline_time / 60,
				     aconf->user, aconf->host, reason);
		ilog(L_TRACE, "%s added temporary %d min. K-Line for [%s@%s] [%s]",
		     source_p->name, tkline_time / 60, aconf->user, aconf->host, reason);
	}
	else
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s added temporary %d min. K-Line for [%s@%s] [%s|%s]",
				     get_oper_name(source_p), tkline_time / 60,
				     aconf->user, aconf->host, reason, oper_reason);
		ilog(L_TRACE, "%s added temporary %d min. K-Line for [%s@%s] [%s|%s]",
		     source_p->name, tkline_time / 60,
		     aconf->user, aconf->host, reason, oper_reason);
	}

	sendto_one(source_p, ":%s NOTICE %s :Added temporary %d min. K-Line [%s@%s]",
		   me.name, source_p->name, tkline_time / 60, aconf->user, aconf->host);
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
valid_tkline(struct Client *source_p, char *p)
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
 * format_kline()
 *
 * inputs       - pointer to a hostname
 * output       - pointer to a static of the hostname masked
 *                for use in a kline.
 * side effects - NONE
 *
 */
static char *
format_kline(char *hostname)
{
	static char result[HOSTLEN + 1];	/* result to return */
	char temphost[HOSTLEN + 1];	/* work place */
	char *ipp;		/* used to find if host is ip # only */
	char *host_mask;	/* used to find host mask portion to '*' */
	char *zap_point = NULL;	/* used to zap last nnn portion of an ip # */
	char *tld;		/* Top Level Domain */
	int is_ip_number;	/* flag if its an ip # */
	int number_of_dots;	/* count number of dots for both ip# and
				   domain klines */
	if(!hostname)
		return (char *) NULL;	/* EEK! */

	/* If a '@' is found in the hostname, this is bogus
	 * and must have been introduced by server that doesn't
	 * check for bogus domains (dns spoof) very well. *sigh* just return it...
	 * I could also legitimately return (char *)NULL as above.
	 */

	if(strchr(hostname, '@'))
	{
		strlcpy(result, hostname, sizeof(result));
		return (result);
	}

	strlcpy(temphost, hostname, sizeof(temphost));

	is_ip_number = YES;	/* assume its an IP# */
	ipp = temphost;
	number_of_dots = 0;

	while (*ipp)
	{
		if(*ipp == '.')
		{
			number_of_dots++;

			if(number_of_dots == 3)
				zap_point = ipp;
			ipp++;
		}
		else if(!IsDigit(*ipp))
		{
			is_ip_number = NO;
			break;
		}
		ipp++;
	}

	if(is_ip_number && (number_of_dots == 3))
	{
		zap_point++;
		*zap_point++ = '*';	/* turn 111.222.333.444 into */
		*zap_point = '\0';	/*      111.222.333.*        */
		strlcpy(result, temphost, sizeof(result));
		return (result);
	}
	else
	{
		tld = strrchr(temphost, '.');
		if(tld)
		{
			number_of_dots = 2;
			if(tld[3])	/* its at least a 3 letter tld
					   i.e. ".com" tld[3] = 'm' not 
					   '\0' */
				/* 4 letter tld's are coming */
				number_of_dots = 1;

			if(tld != temphost)	/* in these days of dns spoofers ... */
				host_mask = tld - 1;	/* Look for host portion to '*' */
			else
				host_mask = tld;	/* degenerate case hostname is
							   '.com' etc. */

			while (host_mask != temphost)
			{
				if(*host_mask == '.')
					number_of_dots--;
				if(number_of_dots == 0)
				{
					strlcpy(result, "*", sizeof(result));
					strlcat(result, host_mask, sizeof(result));
					return (result);
				}
				host_mask--;
			}
			strlcpy(result, "*", sizeof(result));
			strlcat(result, temphost, sizeof(result));
		}
		else		/* no tld found oops. just return it as is */
		{
			strlcpy(result, temphost, sizeof(result));
			return (result);
		}
	}

	return (result);
}

/*
 * mo_dline
 *
 * inputs	- pointer to server
 *		- pointer to client
 *		- parameter count
 *		- parameter list
 * output	-
 * side effects - D line is added
 *
 */
static void
mo_dline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	char *dlhost, *oper_reason;
	const char *reason;
#ifndef IPV6
	char *p;
	struct Client *target_p;
#endif
	struct irc_inaddr daddr;
	char cidr_form_host[HOSTLEN + 1];
	struct ConfItem *aconf;
	int bits, t;
	char dlbuffer[IRCD_BUFSIZE];
	const char *current_date;
	time_t tdline_time = 0;
	int loc = 0;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need kline = yes;", me.name, parv[0]);
		return;
	}

	loc++;

	tdline_time = valid_tkline(source_p, parv[loc]);

	if(tdline_time == -1)
		return;
	else if(tdline_time)
		loc++;

	if(parc < loc + 1)
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "DLINE");
		return;
	}

	dlhost = parv[loc];
	strlcpy(cidr_form_host, dlhost, sizeof(cidr_form_host));

	if((t = parse_netmask(dlhost, NULL, &bits)) == HM_HOST)
	{
#ifdef IPV6
		sendto_one(source_p, ":%s NOTICE %s :Sorry, please supply an address.",
			   me.name, parv[0]);
		return;
#else
		if(!(target_p = find_chasing(source_p, parv[loc], NULL)))
			return;

		if(!target_p->user)
			return;
		t = HM_IPV4;
		if(IsServer(target_p))
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Can't DLINE a server silly", me.name, parv[0]);
			return;
		}

		if(!MyConnect(target_p))
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Can't DLINE nick on another server",
				   me.name, parv[0]);
			return;
		}

		if(IsExemptKline(target_p))
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :%s is E-lined", me.name, parv[0],
				   target_p->name);
			return;
		}

		/*
		 * XXX - this is always a fixed length output, we can get away
		 * with strcpy here
		 *
		 * strncpy_irc(cidr_form_host, inetntoa((char *)&target_p->ip), 32);
		 * cidr_form_host[32] = '\0';
		 */
		strcpy(cidr_form_host, inetntoa((char *) &target_p->localClient->ip));

		if((p = strchr(cidr_form_host, '.')) == NULL)
			return;
		/* 192. <- p */

		p++;
		if((p = strchr(p, '.')) == NULL)
			return;
		/* 192.168. <- p */

		p++;
		if((p = strchr(p, '.')) == NULL)
			return;
		/* 192.168.0. <- p */

		p++;
		*p++ = '0';
		*p++ = '/';
		*p++ = '2';
		*p++ = '4';
		*p++ = '\0';
		dlhost = cidr_form_host;

		bits = 0xFFFFFF00UL;
/* XXX: Fix me for IPV6 */
#endif
	}

	loc++;

	if(parc >= loc + 1)	/* host :reason */
	{
		if(!valid_comment(source_p, parv[loc]))
			return;

		if(*parv[loc])
			reason = parv[loc];
		else
			reason = "No reason";
	}
	else
		reason = "No reason";


	if(IsOperAdmin(source_p))
	{
		if(bits < 8)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :For safety, bitmasks less than 8 require conf access.",
				   me.name, parv[0]);
			return;
		}
	}
	else
	{
		if(bits < 16)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Dline bitmasks less than 16 are for admins only.",
				   me.name, parv[0]);
			return;
		}
	}

#ifdef IPV6
	if(t == HM_IPV6)
		t = AF_INET6;
	else
#endif
		t = AF_INET;
	if(ConfigFileEntry.non_redundant_klines)
	{
		const char *creason;
		(void) parse_netmask(dlhost, &daddr, NULL);

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
			return;
		}
	}

	set_time();
	current_date = smalldate(CurrentTime);

	aconf = make_conf();

	/* Look for an oper reason */
	if((oper_reason = strchr(reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;
	}

	aconf->status = CONF_DLINE;
	DupString(aconf->host, dlhost);

	if(tdline_time)
	{
		snprintf(dlbuffer, sizeof(dlbuffer), 
			 "Temporary D-line %d min. - %s (%s)",
			 (int) (tdline_time / 60), reason, current_date);
		DupString(aconf->passwd, dlbuffer);
		aconf->hold = CurrentTime + tdline_time;
		add_temp_dline(aconf);

		if(BadPtr(oper_reason))
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s added temporary %ld min. D-Line for [%s] [%s]",
					     get_oper_name(source_p), tdline_time / 60,
					     aconf->host, reason);
			ilog(L_TRACE, "%s added temporary %ld min. D-Line for [%s] [%s]",
			     source_p->name, tdline_time / 60, aconf->host, reason);
		}
		else
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s added temporary %ld min. D-Line for [%s] [%s|%s]",
					     get_oper_name(source_p), tdline_time / 60,
					     aconf->host, reason, oper_reason);
			ilog(L_TRACE, "%s added temporary %ld min. D-Line for [%s] [%s|%s]",
			     source_p->name, tdline_time / 60, aconf->host, reason, oper_reason);
		}

		sendto_one(source_p, ":%s NOTICE %s :Added temporary %ld min. D-Line for [%s]",
			   me.name, source_p->name, tdline_time / 60, aconf->host);
	}
	else
	{
		snprintf(dlbuffer, sizeof(dlbuffer),
			 "%s (%s)", reason, current_date);
		DupString(aconf->passwd, dlbuffer);
		add_conf_by_address(aconf->host, CONF_DLINE, NULL, aconf);
		write_confitem(DLINE_TYPE, source_p, NULL, aconf->host, reason,
			       oper_reason, current_date, 0);
	}

	check_dlines();
}

/*
 * find_user_host
 * inputs	- pointer to client placing kline
 *              - pointer to user_host_or_nick
 *              - pointer to user buffer
 *              - pointer to host buffer
 * output	- 0 if not ok to kline, 1 to kline i.e. if valid user host
 * side effects -
 */
static int
find_user_host(struct Client *source_p, char *user_host_or_nick, char *luser, char *lhost)
{
	struct Client *target_p;
	char *hostp;

	if((hostp = strchr(user_host_or_nick, '@')) || *user_host_or_nick == '*')
	{
		/* Explicit user@host mask given */

		if(hostp != NULL)	/* I'm a little user@host */
		{
			*(hostp++) = '\0';	/* short and squat */
			if(*user_host_or_nick)
				strlcpy(luser, user_host_or_nick, USERLEN + 1);	/* here is my user */
			else
				strcpy(luser, "*");
			if(*hostp)
				strlcpy(lhost, hostp, HOSTLEN + 1);	/* here is my host */
			else
				strcpy(lhost, "*");
		}
		else
		{
			luser[0] = '*';	/* no @ found, assume its *@somehost */
			luser[1] = '\0';
			strlcpy(lhost, user_host_or_nick, HOSTLEN + 1);
		}

		return 1;
	}
	else
	{
		/* Try to find user@host mask from nick */

		if(!(target_p = find_chasing(source_p, user_host_or_nick, NULL)))
			return 0;

		if(!target_p->user)
			return 0;

		if(IsServer(target_p))
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Can't KLINE a server, use @'s where appropriate",
				   me.name, source_p->name);
			return 0;
		}

		if(IsExemptKline(target_p))
		{
			if(!IsServer(source_p))
				sendto_one(source_p,
					   ":%s NOTICE %s :%s is E-lined", me.name, source_p->name,
					   target_p->name);
			return 0;
		}

		/* turn the "user" bit into "*user", blow away '~'
		 * if found in original user name (non-idented)
		 */

		strlcpy(luser, target_p->username, USERLEN + 1);
		if(*target_p->username == '~')
			luser[0] = '*';

		strlcpy(lhost, format_kline(target_p->host), HOSTLEN + 1);
	}

	return 1;
}

/*
 * valid_user_host
 * inputs       - pointer to source
 *              - pointer to user buffer
 *              - pointer to host buffer
 * output	- 1 if not valid user or host, 0 if valid
 * side effects -
 */
static int
valid_user_host(struct Client *source_p, char *luser, char *lhost)
{
	/* # is invalid, as is '!' (n!u@h kline) */
	if(strchr(lhost, '#') || strchr(luser, '#') || strchr(luser, '!'))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Invalid character in user@host",
			   me.name, source_p->name);
		return 0;
	}

	return 1;
}

/*
 * valid_wild_card
 * input        - pointer to client
 *              - pointer to user to check
 *              - pointer to host to check
 * output       - 0 if not valid, 1 if valid
 * side effects -
 */
static int
valid_wild_card(struct Client *source_p, char *luser, char *lhost)
{
	char *p;
	char tmpch;
	int nonwild;

	/*
	 * Now we must check the user and host to make sure there
	 * are at least NONWILDCHARS non-wildcard characters in
	 * them, otherwise assume they are attempting to kline
	 * *@* or some variant of that. This code will also catch
	 * people attempting to kline *@*.tld, as long as NONWILDCHARS
	 * is greater than 3. In that case, there are only 3 non-wild
	 * characters (tld), so if NONWILDCHARS is 4, the kline will
	 * be disallowed.
	 * -wnder
	 */

	nonwild = 0;
	p = luser;
	while ((tmpch = *p++))
	{
		if(!IsKWildChar(tmpch))
		{
			/*
			 * If we find enough non-wild characters, we can
			 * break - no point in searching further.
			 */
			if(++nonwild >= ConfigFileEntry.min_nonwildcard)
				return 1;
		}
	}

	if(nonwild < ConfigFileEntry.min_nonwildcard)
	{
		/*
		 * The user portion did not contain enough non-wild
		 * characters, try the host.
		 */
		p = lhost;
		while ((tmpch = *p++))
		{
			if(!IsKWildChar(tmpch))
				if(++nonwild >= ConfigFileEntry.min_nonwildcard)
					return 1;
		}
	}

	sendto_one(source_p,
		   ":%s NOTICE %s :Please include at least %d non-wildcard "
		   "characters with the user@host",
		   me.name, source_p->name, ConfigFileEntry.min_nonwildcard);
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
		sendto_one(source_p,
			   ":%s NOTICE %s :Invalid character '\"' in comment",
			   me.name, source_p->name);
		return 0;
	}

	if(strlen(comment) > REASONLEN)
		comment[REASONLEN] = '\0';

	return 1;
}

/* static int already_placed_kline(source_p, luser, lhost)
 * Input: user to complain to, username & host to check for.
 * Output: returns 1 on existing K-line, 0 if doesn't exist.
 * Side-effects: Notifies source_p if the K-line already exists.
 * Note: This currently works if the new K-line is a special case of an
 *       existing K-line, but not the other way round. To do that we would
 *       have to walk the hash and check every existing K-line. -A1kmm.
 */
static int
already_placed_kline(struct Client *source_p, char *luser, char *lhost,
			int tkline_time)
{
	const char *reason;
	struct irc_inaddr iphost, *piphost;
	struct ConfItem *aconf;
	int t;

	if(ConfigFileEntry.non_redundant_klines)
	{
		if((t = parse_netmask(lhost, &iphost, &t)) != HM_HOST)
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
		{
			t = 0;
			piphost = NULL;
		}

		if((aconf = find_conf_by_address(lhost, piphost, CONF_KILL, t, luser)))
		{
			/* setting a tkline, or existing one is perm */
			if(tkline_time || ((aconf->flags & CONF_FLAGS_TEMPORARY) == 0))
			{
				reason = aconf->passwd ? aconf->passwd : "<No Reason>";

				sendto_one(source_p,
					   ":%s NOTICE %s :[%s@%s] already K-Lined by [%s@%s] - %s",
					   me.name, source_p->name, luser, lhost, aconf->user,
					   aconf->host, reason);
				return 1;
			}
		}
	}

	return 0;
}
