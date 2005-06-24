/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_gline.c: Votes towards globally banning a mask.
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
#include "handlers.h"
#include "s_gline.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "scache.h"
#include "send.h"
#include "msg.h"
#include "fileio.h"
#include "s_serv.h"
#include "hash.h"
#include "parse.h"
#include "modules.h"
#include "s_log.h"

/* internal functions */
static void set_local_gline(const char *oper_nick, const char *oper_user,
			    const char *oper_host, const char *oper_server,
			    const char *user, const char *host, const char *reason);

static void log_gline_request(const char *, const char *, const char *,
			      const char *oper_server, const char *, const char *, const char *);

static void log_gline(struct Client *, struct gline_pending *,
		      const char *, const char *, const char *,
		      const char *oper_server, const char *, 
		      const char *, const char *);

static int majority_gline(struct Client *source_p,
			  const char *oper_nick, const char *oper_username,
			  const char *oper_host, const char *oper_server,
			  const char *user, const char *host, const char *reason);

static int check_wild_gline(const char *, const char *);
static int invalid_gline(struct Client *, const char *, const char *, char *);

static char *small_file_date(void);

static int ms_gline(struct Client *, struct Client *, int, const char **);
static int mo_gline(struct Client *, struct Client *, int, const char **);

struct Message gline_msgtab = {
	"GLINE", 0, 0, 3, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, ms_gline, mo_gline}
};

mapi_clist_av1 gline_clist[] = { &gline_msgtab, NULL };
DECLARE_MODULE_AV1(gline, NULL, NULL, gline_clist, NULL, NULL, NULL, "$Revision$");

/* mo_gline()
 *
 * inputs       - The usual for a m_ function
 * output       -
 * side effects - place a gline if 3 opers agree
 */
static int
mo_gline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *user = NULL;
	char *host = NULL;	/* user and host of GLINE "victim" */
	char *reason = NULL;	/* reason for "victims" demise */
	char splat[] = "*";
	char tempuser[USERLEN + 2];
	char temphost[HOSTLEN + 1];

	if(EmptyString(parv[2]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "GLINE");
		return 0;
	}

	if(ConfigFileEntry.glines)
	{
		if(!IsOperGline(source_p))
		{
			sendto_one(source_p, ":%s NOTICE %s :You need gline = yes;", me.name,
				   parv[0]);
			return 0;
		}

		if((host = strchr(parv[1], '@')) || *parv[1] == '*')
		{
			/* Explicit user@host mask given */

			if(host)	/* Found user@host */
			{
				user = parv[1];	/* here is user part */
				*(host++) = '\0';	/* and now here is host */

				/* gline for "@host", use *@host */
				if(*user == '\0')
					user = splat;
			}
			else
			{
				user = splat;	/* no @ found, assume its *@somehost */
				host = LOCAL_COPY(parv[1]);
			}

			if(!*host)	/* duh. no host found, assume its '*' host */
				host = splat;

			strlcpy(tempuser, user, sizeof(tempuser));	/* allow for '*' */
			strlcpy(temphost, host, sizeof(temphost));
			user = tempuser;
			host = temphost;
		}
		else
		{
			sendto_one(source_p, ":%s NOTICE %s :Can't G-Line a nick use user@host",
				   me.name, parv[0]);
			return 0;
		}

		reason = LOCAL_COPY(parv[2]);

		if(invalid_gline(source_p, user, host, reason))
			return 0;

		/* Not enough non-wild characters were found, assume they are trying to gline *@*. */
		if(check_wild_gline(user, host))
		{
			if(MyClient(source_p))
				sendto_one(source_p,
					   ":%s NOTICE %s :Please include at least %d non-wildcard "
					   "characters with the user@host",
					   me.name, parv[0], ConfigFileEntry.min_nonwildcard);
			return 0;
		}

		/* inform users about the gline before we call majority_gline()
		 * so already voted comes below gline request --fl
		 */
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s!%s@%s on %s is requesting gline for [%s@%s] [%s]",
				     source_p->name, source_p->username,
				     source_p->host, me.name, user, host, reason);
		log_gline_request(source_p->name,
				  (const char *) source_p->username,
				  source_p->host, me.name, user, host, reason);

		/* If at least 3 opers agree this user should be G lined then do it */
		majority_gline(source_p, source_p->name, source_p->username,
			       source_p->host, me.name, user, host, reason);

		/* 4 param version for hyb-7 servers */
		sendto_server(NULL, NULL, CAP_GLN | CAP_UID, NOCAPS,
			      ":%s GLINE %s %s :%s", ID(source_p), user, host, reason);
		sendto_server(NULL, NULL, CAP_GLN, CAP_UID,
			      ":%s GLINE %s %s :%s", source_p->name, user, host, reason);

		/* 8 param for hyb-6 */
		sendto_server(NULL, NULL, CAP_UID, CAP_GLN,
			      ":%s GLINE %s %s %s %s %s %s :%s",
			      me.name, ID(source_p), source_p->username,
			      source_p->host, source_p->user->server, user, host, reason);
		sendto_server(NULL, NULL, NOCAPS, CAP_GLN | CAP_UID,
			      ":%s GLINE %s %s %s %s %s %s :%s",
			      me.name, source_p->name, source_p->username,
			      source_p->host, source_p->user->server, user, host, reason);
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :GLINE disabled", me.name, parv[0]);
	}

	return 0;
}

/* ms_gline()
 *
 * inputs       - The usual for a m_ function
 * output       -
 * side effects - attempts to place a gline, if 3 opers agree
 */
static int
ms_gline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* used for hyb6 compat */
	struct Client *acptr;
	const char *oper_nick = NULL;
	const char *oper_user = NULL;
	const char *oper_host = NULL;
	const char *oper_server = NULL;
	const char *user = NULL;
	const char *host = NULL;
	const char *reason = NULL;

	/* new style gline */
	if(parc == 4 && IsPerson(source_p))
	{
		oper_nick = parv[0];
		oper_user = source_p->username;
		oper_host = source_p->host;
		oper_server = source_p->user->server;
		user = parv[1];
		host = parv[2];
		reason = parv[3];
	}
	/* or it's a hyb-6 style */
	else if(parc == 8 && IsServer(source_p))
	{
		oper_nick = parv[1];
		oper_user = parv[2];
		oper_host = parv[3];
		oper_server = parv[4];
		user = parv[5];
		host = parv[6];
		reason = parv[7];
	}
	/* none of the above */
	else
		return 0;

	/* check the server and nick actually exist */
	if((acptr = find_server(oper_server)))
	{
		if((acptr = find_client(oper_nick)) == NULL)
			return 0;
	}
	else
		return 0;

	if(invalid_gline(acptr, user, host, (char *) reason))
		return 0;

	/* send in new form to compatable servers, hyb6 form to rest */
	sendto_server(client_p, NULL, CAP_GLN, NOCAPS,
		      ":%s GLINE %s %s :%s", oper_nick, user, host, reason);
	sendto_server(client_p, NULL, NOCAPS, CAP_GLN,
		      ":%s GLINE %s %s %s %s %s %s :%s",
		      oper_server, oper_nick, oper_user, oper_host,
		      oper_server, user, host, reason);

	if(!ConfigFileEntry.glines)
		return 0;

	/* check theres enough non-wildcard chars */
	if(check_wild_gline(user, host))
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				"%s!%s@%s on %s is requesting a gline without "
				"%d non-wildcard characters for [%s@%s] [%s]",
				oper_nick, oper_user, oper_host, oper_server,
				ConfigFileEntry.min_nonwildcard, user, host, reason);
		return 0;
	}

	log_gline_request(oper_nick, oper_user, oper_host, oper_server, user, host, reason);

	sendto_realops_flags(UMODE_ALL, L_ALL,
			"%s!%s@%s on %s is requesting gline for [%s@%s] [%s]",
			oper_nick,
			oper_user, oper_host, oper_server, user, host, reason);

	/* If at least 3 opers agree this user should be G lined then do it */
	majority_gline(source_p,
			oper_nick,
			oper_user, oper_host, oper_server, user, host, reason);
	return 0;
}

/*
 * check_wild_gline
 *
 * inputs       - user, host of gline
 * output       - 1 if not enough non-wildchar char's, 0 if ok
 * side effects - NONE
 */
static int
check_wild_gline(const char *user, const char *host)
{
	const char *p;
	char tmpch;
	int nonwild;

	nonwild = 0;
	p = user;

	while ((tmpch = *p++))
	{
		if(!IsKWildChar(tmpch))
		{
			/* enough of them, break */
			if(++nonwild >= ConfigFileEntry.min_nonwildcard)
				break;
		}
	}

	if(nonwild < ConfigFileEntry.min_nonwildcard)
	{
		/* user doesnt, try host */
		p = host;
		while ((tmpch = *p++))
		{
			if(!IsKWildChar(tmpch))
				if(++nonwild >= ConfigFileEntry.min_nonwildcard)
					break;
		}
	}

	if(nonwild < ConfigFileEntry.min_nonwildcard)
		return 1;
	else
		return 0;
}

/* invalid_gline
 *
 * inputs	- pointer to source client, ident, host and reason
 * outputs	- 1 if invalid, 0 if valid
 * side effects -
 */
static int
invalid_gline(struct Client *source_p, const char *luser,
	      const char *lhost, char *lreason)
{
	if(strchr(luser, '!'))
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid character '!' in gline",
			   me.name, source_p->name);
		return 1;
	}

	if(strlen(lreason) > REASONLEN)
		lreason[REASONLEN] = '\0';

	return 0;
}

/*
 * set_local_gline
 *
 * inputs	- pointer to oper nick/username/host/server,
 * 		  victim user/host and reason
 * output	- NONE
 * side effects	-
 */
static void
set_local_gline(const char *oper_nick, const char *oper_user,
		const char *oper_host, const char *oper_server, 
		const char *user, const char *host, const char *reason)
{
	char buffer[IRCD_BUFSIZE];
	struct ConfItem *aconf;
	const char *current_date;
	char *my_reason;
	char *oper_reason;

	current_date = smalldate();

	my_reason = LOCAL_COPY(reason);

	aconf = make_conf();
	aconf->status = CONF_GLINE;
	aconf->flags |= CONF_FLAGS_TEMPORARY;

	if(strlen(my_reason) > REASONLEN)
		my_reason[REASONLEN-1] = '\0';

	if((oper_reason = strchr(my_reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			DupString(aconf->spasswd, oper_reason);
	}

	snprintf(buffer, sizeof(buffer), "%s (%s)", reason, current_date);

	DupString(aconf->passwd, buffer);
	DupString(aconf->user, (char *) user);
	DupString(aconf->host, (char *) host);
	aconf->hold = CurrentTime + ConfigFileEntry.gline_time;
	add_gline(aconf);

	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s!%s@%s on %s has triggered gline for [%s@%s] [%s]",
			     oper_nick, oper_user, oper_host, oper_server, user, host, reason);
	check_glines();
}


/*
 * log_gline_request()
 *
 */
static void
log_gline_request(const char *oper_nick, const char *oper_user,
		  const char *oper_host, const char *oper_server, 
		  const char *user, const char *host, const char *reason)
{
	char buffer[2 * BUFSIZE];
	char filenamebuf[PATH_MAX + 1];
	static char timebuffer[MAX_DATE_STRING];
	struct tm *tmptr;
	FBFILE *out;

	if(ConfigFileEntry.glinefile == NULL)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem opening glinefile");
		return;
	}

	snprintf(filenamebuf, sizeof(filenamebuf), 
		 "%s.%s", ConfigFileEntry.glinefile, small_file_date());
	if((out = fbopen(filenamebuf, "+a")) == NULL)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem opening %s: %s",
				     filenamebuf, strerror(errno));
		return;
	}

	tmptr = localtime((const time_t *) &CurrentTime);
	strftime(timebuffer, MAX_DATE_STRING, "%Y/%m/%d %H:%M:%S", tmptr);

	snprintf(buffer, sizeof(buffer),
		   "#Gline for %s@%s [%s] requested by %s!%s@%s on %s at %s\n",
		   user, host, reason, oper_nick, oper_user, oper_host, oper_server, timebuffer);

	if(fbputs(buffer, out) == -1)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s (%s)",
				     filenamebuf, strerror(errno));
	}
	fbclose(out);
}

/*
 * log_gline()
 *
 */
static void
log_gline(struct Client *source_p, struct gline_pending *pending,
	  const char *oper_nick, const char *oper_user,
	  const char *oper_host, const char *oper_server, const char *user, 
	  const char *host, const char *reason)
{
	char buffer[2 * BUFSIZE];
	char filenamebuf[PATH_MAX + 1];
	static char timebuffer[MAX_DATE_STRING + 1];
	struct tm *tmptr;
	FBFILE *out;

	if(ConfigFileEntry.glinefile == NULL)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem opening glinefile.");
		return;
	}

	snprintf(filenamebuf, sizeof(filenamebuf),
		 "%s.%s", ConfigFileEntry.glinefile, small_file_date());

	if((out = fbopen(filenamebuf, "a")) == NULL)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem opening %s", filenamebuf);
		return;
	}

	tmptr = localtime((const time_t *) &CurrentTime);
	strftime(timebuffer, MAX_DATE_STRING, "%Y/%m/%d %H:%M:%S", tmptr);

	snprintf(buffer, sizeof(buffer),
		 "#Gline for %s@%s %s added by the following\n", user, host, timebuffer);

	if(fbputs(buffer, out) == -1)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s", filenamebuf);
		fbclose(out);
		return;
	}

	snprintf(buffer, sizeof(buffer), "#%s!%s@%s on %s [%s]\n",
		 pending->oper_nick1, pending->oper_user1,
		 pending->oper_host1, pending->oper_server1,
		 pending->reason1 ? pending->reason1 : "No reason");

	if(fbputs(buffer, out) == -1)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s", filenamebuf);
		return;
	}

	snprintf(buffer, sizeof(buffer), "#%s!%s@%s on %s [%s]\n",
		 pending->oper_nick2, pending->oper_user2,
		 pending->oper_host2, pending->oper_server2,
		 pending->reason2 ? pending->reason2 : "No reason");

	if(fbputs(buffer, out) == -1)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s", filenamebuf);
		fbclose(out);
		return;
	}

	snprintf(buffer, sizeof(buffer), "#%s!%s@%s on %s [%s]\n",
		   oper_nick, oper_user, oper_host, oper_server, (reason) ? reason : "No reason");

	if(fbputs(buffer, out) == -1)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s", filenamebuf);
		fbclose(out);
		return;
	}

	snprintf(buffer, sizeof(buffer), "\"%s\",\"%s\",\"%s %s\",\"%s\",%lu\n",
		   user, host, reason, timebuffer, oper_nick, (unsigned long) CurrentTime);

	if(fbputs(buffer, out) == -1)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s", filenamebuf);
		fbclose(out);
		return;
	}

	fbclose(out);
}

/*
 * majority_gline()
 *
 * inputs       - oper_nick, oper_user, oper_host, oper_server
 *                user,host reason
 *
 * output       - YES if there are 3 different opers on 3 different servers
 *                agreeing to this gline, NO if there are not.
 * Side effects -
 *      See if there is a majority agreement on a GLINE on the given user
 *      There must be at least 3 different opers agreeing on this GLINE
 *
 */
static int
majority_gline(struct Client *source_p,
	       const char *oper_nick,
	       const char *oper_user,
	       const char *oper_host,
	       const char *oper_server, const char *user, const char *host, const char *reason)
{
	dlink_node *pending_node;
	struct gline_pending *pending;

	/* to avoid desync.. --fl */
	cleanup_glines();

	/* if its already glined, why bother? :) -- fl_ */
	if(find_is_glined(host, user))
		return NO;

	DLINK_FOREACH(pending_node, pending_glines.head)
	{
		pending = pending_node->data;

		if((irccmp(pending->user, user) == 0) &&
		   (irccmp(pending->host, host) == 0))
		{
			/* check oper or server hasnt already voted */
			if(((irccmp(pending->oper_user1, oper_user) == 0) ||
			    (irccmp(pending->oper_host1, oper_host) == 0)))
			{
				sendto_realops_flags(UMODE_ALL, L_ALL, "oper has already voted");
				return NO;
			}
			else if(irccmp(pending->oper_server1, oper_server) == 0)
			{
				sendto_realops_flags(UMODE_ALL, L_ALL, "server has already voted");
				return NO;
			}

			if(pending->oper_user2[0] != '\0')
			{
				/* if two other opers on two different servers have voted yes */
				if(((irccmp(pending->oper_user2, oper_user) == 0) ||
				    (irccmp(pending->oper_host2, oper_host) == 0)))
				{
					sendto_realops_flags(UMODE_ALL, L_ALL,
							     "oper has already voted");
					return NO;
				}
				else if(irccmp(pending->oper_server2, oper_server) == 0)
				{
					sendto_realops_flags(UMODE_ALL, L_ALL,
							     "server has already voted");
					return NO;
				}

				log_gline(source_p, pending,
					  oper_nick, oper_user, oper_host, oper_server,
					  user, host, reason);

				/* trigger the gline using the original reason --fl */
				set_local_gline(oper_nick, oper_user, oper_host, oper_server,
						user, host, pending->reason1);

				cleanup_glines();
				return YES;
			}
			else
			{
				strlcpy(pending->oper_nick2, oper_nick,
					sizeof(pending->oper_nick2));
				strlcpy(pending->oper_user2, oper_user,
					sizeof(pending->oper_user2));
				strlcpy(pending->oper_host2, oper_host,
					sizeof(pending->oper_host2));
				DupString(pending->reason2, reason);
				pending->oper_server2 = find_or_add(oper_server);
				pending->last_gline_time = CurrentTime;
				pending->time_request2 = CurrentTime;
				return NO;
			}
		}
	}

	/* no pending gline, create a new one */
	pending = (struct gline_pending *) 
			    MyMalloc(sizeof(struct gline_pending));

	strlcpy(pending->oper_nick1, oper_nick, sizeof(pending->oper_nick1));
	strlcpy(pending->oper_user1, oper_user, sizeof(pending->oper_user1));
	strlcpy(pending->oper_host1, oper_host, sizeof(pending->oper_host1));

	pending->oper_server1 = find_or_add(oper_server);

	strlcpy(pending->user, user, sizeof(pending->user));
	strlcpy(pending->host, host, sizeof(pending->host));
	DupString(pending->reason1, reason);
	pending->reason2 = NULL;

	pending->last_gline_time = CurrentTime;
	pending->time_request1 = CurrentTime;

	dlinkAddAlloc(pending, &pending_glines);

	return NO;
}

/* small_file_date()
 *
 * Make a small YYYYMMDD formatted string suitable for a
 * dated file stamp.
 */
static char *
small_file_date(void)
{
	static char tbuffer[MAX_DATE_STRING];
	struct tm *tmptr;
	time_t lclock;

	lclock = CurrentTime;
	tmptr = localtime(&lclock);
	strftime(tbuffer, MAX_DATE_STRING, "%Y%m%d", tmptr);
	return tbuffer;
}
