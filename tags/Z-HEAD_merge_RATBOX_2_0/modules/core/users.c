/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_nick.c: Sets a users nick.
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
#include "client.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_user.h"
#include "hash.h"
#include "watch.h"
#include "whowas.h"
#include "s_serv.h"
#include "s_user.h"
#include "reject.h"
#include "send.h"
#include "channel.h"
#include "s_log.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "scache.h"
#include "s_newconf.h"
#include "sprintf_irc.h"
#include "listener.h"
#include "cache.h"
#include "hostmask.h"

#define UFLAGS  (FLAGS_INVISIBLE|FLAGS_WALLOP|FLAGS_SERVNOTICE)

static int mr_nick(struct Client *, struct Client *, int, const char **);
static int m_nick(struct Client *, struct Client *, int, const char **);
static int mc_nick(struct Client *, struct Client *, int, const char **);
static int ms_nick(struct Client *, struct Client *, int, const char **);
static int ms_uid(struct Client *, struct Client *, int, const char **);
static int mr_user(struct Client *, struct Client *, int, const char **);
static int mr_pong(struct Client *, struct Client *, int, const char **);
static int ms_pong(struct Client *, struct Client *, int, const char **);
static int m_ping(struct Client *, struct Client *, int, const char **);
static int ms_ping(struct Client *, struct Client *, int, const char **);
static int mr_pass(struct Client *, struct Client *, int, const char **);
static int m_version(struct Client *, struct Client *, int, const char **);

struct Message nick_msgtab = {
	"NICK", 0, 0, 0, MFLG_SLOW,
	{{mr_nick, 0}, {m_nick, 0}, {mc_nick, 3}, {ms_nick, 8}, mg_ignore, {m_nick, 0}}
};
struct Message uid_msgtab = {
	"UID", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, {ms_uid, 9}, mg_ignore, mg_ignore}
};
struct Message user_msgtab = {
	"USER", 0, 0, 0, MFLG_SLOW,
	{{mr_user, 5}, mg_reg, mg_ignore, mg_ignore, mg_ignore, mg_reg}
};
struct Message pong_msgtab = {
	"PONG", 0, 0, 0, MFLG_SLOW | MFLG_UNREG,
	{{mr_pong, 0}, mg_ignore, mg_ignore, {ms_pong, 2}, mg_ignore, mg_ignore}
};
struct Message ping_msgtab = {
	"PING", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_ping, 2}, {ms_ping, 2}, {ms_ping, 2}, mg_ignore, {m_ping, 2}}
};
struct Message pass_msgtab = {
	"PASS", 0, 0, 0, MFLG_SLOW | MFLG_UNREG,
	{{mr_pass, 2}, mg_reg, mg_ignore, mg_ignore, mg_ignore, mg_reg}
};
struct Message version_msgtab = {
	"VERSION", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_version, 0}, {m_version, 0}, {m_version, 0}, mg_ignore, {m_version, 0}}
};

mapi_clist_av1 users_clist[] = { &nick_msgtab, &uid_msgtab, &user_msgtab, &pong_msgtab, &ping_msgtab, 
				 &pass_msgtab, &version_msgtab,
				 NULL };

DECLARE_MODULE_AV1(users, NULL, NULL, users_clist, NULL, NULL, "$Revision$");

static int clean_nick(const char *, int loc_client);
static int clean_username(const char *);
static int clean_host(const char *);
static int clean_uid(const char *uid);

static void change_local_nick(struct Client *client_p, struct Client *source_p, char *nick);
static int change_remote_nick(struct Client *, struct Client *, int, const char **, time_t, const char *);

static int register_local_user(struct Client *client_p, struct Client *source_p);
static int register_client(struct Client *client_p, struct Client *server, 
			   const char *nick, time_t newts, int parc, const char *parv[]);

static int perform_nick_collides(struct Client *, struct Client *,
				 struct Client *, int, const char **, 
				 time_t, const char *, const char *);
static int perform_nickchange_collides(struct Client *, struct Client *,
				       struct Client *, int, const char **, 
				       time_t, const char *);

static void show_isupport(struct Client *source_p);
static char *confopts(struct Client *source_p);

/* mr_nick()
 *       parv[0] = sender prefix
 *       parv[1] = nickname
 */
static int
mr_nick(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	char nick[NICKLEN];
	char *s;

	if(parc < 2 || EmptyString(parv[1]) || (parv[1][0] == '~'))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN),
			   me.name, 
			   EmptyString(source_p->name) ? "*" : source_p->name);
		return 0;
	}

	/* due to the scandinavian origins, (~ being uppercase of ^) and ~
	 * being disallowed as a nick char, we need to chop the first ~
	 * instead of just erroring.
	 */
	if((s = strchr(parv[1], '~')))
		*s = '\0';

	/* copy the nick and terminate it */
	strlcpy(nick, parv[1], sizeof(nick));

	/* check the nickname is ok */
	if(!clean_nick(nick, 1))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, EmptyString(parv[0]) ? "*" : parv[0], parv[1]);
		return 0;
	}

	/* check if the nick is resv'd */
	if(find_nick_resv(nick))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, EmptyString(source_p->name) ? "*" : source_p->name, nick);
		return 0;
	}

	if(hash_find_nd(nick))
	{
		sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
			me.name, 
			EmptyString(source_p->name) ? "*" : source_p->name,
			nick);
		return 0;
	}

	if((target_p = find_client(nick)) == NULL)
	{
		/* This had to be copied here to avoid problems.. */
		source_p->tsinfo = CurrentTime;

		if(!EmptyString(source_p->name))
			del_from_client_hash(source_p->name, source_p);

		if(source_p->user == NULL)
		{
			source_p->user = make_user(source_p);
			source_p->user->server = me.name;
		}
		strcpy(source_p->name, nick);
		add_to_client_hash(nick, source_p);
		
		/* fd_desc is long enough */
		comm_note(client_p->localClient->fd, "Nick: %s", nick);

		if(HasSentUser(source_p))
			register_local_user(client_p, source_p);

	}
	else if(source_p == target_p)
	{
		/* This should be okay as this means we've already got a struct User */
		strcpy(source_p->name, nick);
	}
	else
		sendto_one(source_p, form_str(ERR_NICKNAMEINUSE), me.name, "*", nick);

	return 0;
}

/* m_nick()
 *     parv[0] = sender prefix
 *     parv[1] = nickname
 */
static int
m_nick(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	char nick[NICKLEN];
	char *s;

	if(parc < 2 || EmptyString(parv[1]) || (parv[1][0] == '~'))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN),
			   me.name, source_p->name);
		return 0;
	}

	/* due to the scandinavian origins, (~ being uppercase of ^) and ~
	 * being disallowed as a nick char, we need to chop the first ~
	 * instead of just erroring.
	 */
	if((s = strchr(parv[1], '~')))
		*s = '\0';

	/* mark end of grace period, to prevent nickflooding */
	if(!IsFloodDone(source_p))
		flood_endgrace(source_p);

	/* terminate nick to NICKLEN, we dont want clean_nick() to error! */
	strlcpy(nick, parv[1], sizeof(nick));

	/* check the nickname is ok */
	if(!clean_nick(nick, 1))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, parv[0], nick);
		return 0;
	}

	if(find_nick_resv(nick))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, source_p->name, nick);
		return 0;
	}

	if(hash_find_nd(nick))
	{
		sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
			me.name, 
			EmptyString(source_p->name) ? "*" : source_p->name,
			nick);
		return 0;
	}

	if((target_p = find_client(nick)))
	{
		/* If(target_p == source_p) the client is changing nicks between
		 * equivalent nicknames ie: [nick] -> {nick}
		 */
		if(target_p == source_p)
		{
			/* check the nick isnt exactly the same */
			if(strcmp(target_p->name, nick))
				change_local_nick(client_p, source_p, nick);

		}

		/* drop unregged client */
		else if(IsUnknown(target_p))
		{
			exit_client(NULL, target_p, &me, "Overridden");
			change_local_nick(client_p, source_p, nick);
		}
		else
			sendto_one(source_p, form_str(ERR_NICKNAMEINUSE), me.name, parv[0], nick);

		return 0;
	}
	else
		change_local_nick(client_p, source_p, nick);

	return 0;
}

/* ms_nick()
 *      
 * server -> server nick change
 *    parv[0] = sender prefix
 *    parv[1] = nickname
 *    parv[2] = TS when nick change
 *
 * server introducing new nick
 *    parv[0] = sender prefix
 *    parv[1] = nickname
 *    parv[2] = hop count
 *    parv[3] = TS
 *    parv[4] = umode
 *    parv[5] = username
 *    parv[6] = hostname
 *    parv[7] = server
 *    parv[8] = ircname
 */
static int
mc_nick(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	time_t newts = 0;

	/* if nicks erroneous, or too long, kill */
	if(!clean_nick(parv[1], 0))
	{
		ServerStats.is_kill++;
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Bad Nick: %s From: %s(via %s)",
				     parv[1], source_p->user->server,
				     client_p->name);
		sendto_one(client_p, ":%s KILL %s :%s (Bad Nickname)",
			   me.name, parv[1], me.name);

		/* bad nick change, issue kill for the old nick to the rest
		 * of the network.
		 */
		kill_client_serv_butone(client_p, source_p,
					"%s (Bad Nickname)", me.name);
		source_p->flags |= FLAGS_KILLED;
		exit_client(client_p, source_p, &me, "Bad Nickname");
		return 0;
	}
			      
	newts = atol(parv[2]);
	target_p = find_client(parv[1]);

	/* if the nick doesnt exist, allow it and process like normal */
	if(target_p == NULL)
	{
		change_remote_nick(client_p, source_p, parc, parv, newts, parv[1]);
	}
	else if(IsUnknown(target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		change_remote_nick(client_p, source_p, parc, parv, newts, parv[1]);
	}
	else if(target_p == source_p)
	{
		/* client changing case of nick */
		if(strcmp(target_p->name, parv[1]))
			change_remote_nick(client_p, source_p, parc, parv, newts, parv[1]);
	}
	/* we've got a collision! */
	else
		perform_nickchange_collides(source_p, client_p, target_p, 
					    parc, parv, newts, parv[1]);

	return 0;
}

static int
ms_nick(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	time_t newts = 0;

	if(parc < 9)
		return 0;

	/* if nicks empty, erroneous, or too long, kill */
	if(!clean_nick(parv[1], 0))
	{
		ServerStats.is_kill++;
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Bad Nick: %s From: %s(via %s)",
				     parv[1], parv[7], client_p->name);
		sendto_one(client_p, ":%s KILL %s :%s (Bad Nickname)",
			   me.name, parv[1], me.name);
		return 0;
	}
			      
	/* invalid username or host? */
	if(!clean_username(parv[5]) || !clean_host(parv[6]))
	{
		ServerStats.is_kill++;
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				"Bad user@host: %s@%s From: %s(via %s)",
				parv[5], parv[6], parv[7],
				client_p->name);
		sendto_one(client_p, ":%s KILL %s :%s (Bad user@host)",
				me.name, parv[1], me.name);
		return 0;
	}

	/* check the length of the clients gecos */
	if(strlen(parv[8]) > REALLEN)
	{
		char *s = LOCAL_COPY(parv[8]);
		/* why exactly do we care? --fl */
		sendto_realops_flags(UMODE_ALL, L_ALL,
				"Long realname from server %s for %s", parv[7],
				parv[1]);

		s[REALLEN] = '\0';
		parv[8] = s;
	}

	newts = atol(parv[3]);

	target_p = find_client(parv[1]);

	/* if the nick doesnt exist, allow it and process like normal */
	if(target_p == NULL)
	{
		register_client(client_p, NULL, parv[1], newts, parc, parv);
	}
	else if(IsUnknown(target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		register_client(client_p, NULL, parv[1], newts, parc, parv);
	}
	else if(target_p == source_p)
	{
		/* client changing case of nick */
		if(strcmp(target_p->name, parv[1]))
			register_client(client_p, NULL, parv[1], newts, parc, parv);
	}
	/* we've got a collision! */
	else
		perform_nick_collides(source_p, client_p, target_p, parc, parv, 
					newts, parv[1], NULL);

	return 0;
}

/* ms_uid()
 *     parv[1] - nickname
 *     parv[2] - hops
 *     parv[3] - TS
 *     parv[4] - umodes
 *     parv[5] - username
 *     parv[6] - hostname
 *     parv[7] - IP
 *     parv[8] - UID
 *     parv[9] - gecos
 */
static int
ms_uid(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	time_t newts = 0;

	newts = atol(parv[3]);

	if(parc < 10)
		return 0;

	/* if nicks erroneous, or too long, kill */
	if(!clean_nick(parv[1], 0))
	{
		ServerStats.is_kill++;
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Bad Nick: %s From: %s(via %s)",
				     parv[1], source_p->name,
				     client_p->name);
		sendto_one(client_p, ":%s KILL %s :%s (Bad Nickname)",
			   me.id, parv[8], me.name);
		return 0;
	}

	if(!clean_username(parv[5]) || !clean_host(parv[6]))
	{
		ServerStats.is_kill++;
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Bad user@host: %s@%s From: %s(via %s)",
				     parv[5], parv[6], source_p->name,
				     client_p->name);
		sendto_one(client_p, ":%s KILL %s :%s (Bad user@host)",
			   me.id, parv[8], me.name);
		return 0;
	}

	if(!clean_uid(parv[8]))
	{
		ServerStats.is_kill++;
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
					"Bad UID: %s From: %s(via %s)",
					parv[8], source_p->name,
					client_p->name);
		sendto_one(client_p, ":%s KILL %s :%s (Bad UID)",
				me.id, parv[8], me.name);
		return 0;
	}

	/* check length of clients gecos */
	if(strlen(parv[9]) > REALLEN)
	{
		char *s = LOCAL_COPY(parv[9]);
		sendto_realops_flags(UMODE_ALL, L_ALL, "Long realname from server %s for %s",
				     parv[0], parv[1]);
		s[REALLEN] = '\0';
		parv[9] = s;
	}

	target_p = find_client(parv[1]);
	
	if(target_p == NULL)
	{
		register_client(client_p, source_p, parv[1], newts, parc, parv);
	}
	else if(IsUnknown(target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		register_client(client_p, source_p, parv[1], newts, parc, parv);
	}
	/* we've got a collision! */
	else
		perform_nick_collides(source_p, client_p, target_p, parc, parv,
					newts, parv[1], parv[8]);

	return 0;
}

/* mr_user()
 *      parv[1] = username (login name, account)
 *      parv[2] = client host name (ignored)
 *      parv[3] = server host name (ignored)
 *      parv[4] = users gecos
 */
static int
mr_user(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char buf[BUFSIZE];
	struct User *user;
	char *p;

	if((p = strchr(parv[1], '@')))
		*p = '\0';

	if(source_p->user == NULL)
	{
		user = make_user(source_p);
		user->server = me.name;
	}

	ircsnprintf(buf, sizeof(buf), "%s %s", parv[2], parv[3]);
	MyFree(source_p->localClient->fullcaps);
	DupString(source_p->localClient->fullcaps, buf);

	strlcpy(source_p->info, parv[4], sizeof(source_p->info));

	/* This is in this location for a reason..If there is no identd
	 * and ping cookies are enabled..we need to have a copy of this
	 */
	if(!IsGotId(source_p))
		strlcpy(source_p->username, parv[1], sizeof(source_p->username));
	SetSentUser(source_p);
	/* NICK already received, now I have USER... */
	if(!EmptyString(source_p->name))
		register_local_user(client_p, source_p);

	return 0;
}

/*
** m_ping
**      parv[0] = sender prefix
**      parv[1] = origin
**      parv[2] = destination
*/
static int
m_ping(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	const char *destination;

	destination = parv[2];	/* Will get NULL or pointer (parc >= 2!!) */

	if(!EmptyString(destination) && !match(destination, me.name))
	{
		if((target_p = find_server(source_p, destination)))
		{
			sendto_one(target_p, ":%s PING %s :%s",
				   get_id(source_p, target_p),
				   source_p->name, get_id(target_p, target_p));
		}
		else
		{
			sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
					   form_str(ERR_NOSUCHSERVER),
					   destination);
			return 0;
		}
	}
	else
		sendto_one(source_p, ":%s PONG %s :%s", me.name,
			   (destination) ? destination : me.name, parv[1]);

	return 0;
}

static int
ms_ping(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	const char *destination;

	destination = parv[2];	/* Will get NULL or pointer (parc >= 2!!) */

	if(!EmptyString(destination) && irccmp(destination, me.name) &&
	   irccmp(destination, me.id))
	{
		if((target_p = find_client(destination)) && IsServer(target_p))
			sendto_one(target_p, ":%s PING %s :%s", 
				   get_id(source_p, target_p), source_p->name,
				   get_id(target_p, target_p));
		/* not directed at an id.. */
		else if(!IsDigit(*destination))
			sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
					   form_str(ERR_NOSUCHSERVER),
					   destination);
	}
	else
		sendto_one(source_p, ":%s PONG %s :%s", 
			   get_id(&me, source_p), me.name, 
			   get_id(source_p, source_p));

	return 0;
}

static int
ms_pong(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	const char *destination;

	destination = parv[2];
	source_p->flags &= ~FLAGS_PINGSENT;

	/* Now attempt to route the PONG, comstud pointed out routable PING
	 * is used for SPING.  routable PING should also probably be left in
	 *        -Dianora
	 * That being the case, we will route, but only for registered clients (a
	 * case can be made to allow them only from servers). -Shadowfax
	 */
	if(!EmptyString(destination) && !match(destination, me.name) &&
	   irccmp(destination, me.id))
	{
		if((target_p = find_client(destination)) || 
		   (target_p = find_server(NULL, destination)))
			sendto_one(target_p, ":%s PONG %s %s", 
				   get_id(source_p, target_p), parv[1], 
				   get_id(target_p, target_p));
		else
		{
			if(!IsDigit(*destination))
				sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
						   form_str(ERR_NOSUCHSERVER), destination);
			return 0;
		}
	}

	/* destination is us, emulate EOB */
	if(IsServer(source_p) && !HasSentEob(source_p))
	{
		if(MyConnect(source_p))
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "End of burst (emulated) from %s (%d seconds)",
					     source_p->name,
					     (signed int) (CurrentTime - source_p->localClient->firsttime));
		SetEob(source_p);
	}

	return 0;
}

static int
mr_pong(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc == 2 && !EmptyString(parv[1]))
	{
		if(ConfigFileEntry.ping_cookie && source_p->user && !EmptyString(source_p->name))
		{
			unsigned long incoming_ping = strtoul(parv[1], NULL, 16);
			if(incoming_ping)
			{
				if(source_p->localClient->random_ping == incoming_ping)
				{
					source_p->flags2 |= FLAGS2_PING_COOKIE;
					register_local_user(client_p, source_p);
				}
				else
				{
					sendto_one(source_p, form_str(ERR_WRONGPONG),
						   me.name, source_p->name,
						   source_p->localClient->random_ping);
					return 0;
				}
			}
		}

	}
	else
		sendto_one(source_p, form_str(ERR_NOORIGIN), me.name, parv[0]);

	source_p->flags &= ~FLAGS_PINGSENT;

	return 0;
}


/*
 * m_pass() - Added Sat, 4 March 1989
 *
 *
 * mr_pass - PASS message handler
 *      parv[0] = sender prefix
 *      parv[1] = password
 *      parv[2] = "TS" if this server supports TS.
 *      parv[3] = optional TS version field -- needed for TS6
 */
static int
mr_pass(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(client_p->localClient->passwd)
	{
		memset(client_p->localClient->passwd, 0,
			strlen(client_p->localClient->passwd));
		MyFree(client_p->localClient->passwd);
	}

	DupNString(client_p->localClient->passwd, parv[1], PASSWDLEN);

	if(parc > 2)
	{
		/* 
		 * It looks to me as if orabidoo wanted to have more
		 * than one set of option strings possible here...
		 * i.e. ":AABBTS" as long as TS was the last two chars
		 * however, as we are now using CAPAB, I think we can
		 * safely assume if there is a ":TS" then its a TS server
		 * -Dianora
		 */
		if(irccmp(parv[2], "TS") == 0 && client_p->tsinfo == 0)
			client_p->tsinfo = TS_DOESTS;

		/* kludge, if we're not using ts6, dont ever mark a server
		 * as TS6 capable, that way we'll never send them TS6 data.
		 */
		if(ServerInfo.use_ts6 && parc == 5 && atoi(parv[3]) >= 6)
		{
			/* only mark as TS6 if the SID is valid.. */
			if(IsDigit(parv[4][0]) && IsIdChar(parv[4][1]) &&
			   IsIdChar(parv[4][2]) && parv[4][3] == '\0')
			{
				client_p->localClient->caps |= CAP_TS6;
				strcpy(client_p->id, parv[4]);
			}
		}
	}

	return 0;
}

/*
 * m_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static int
m_version(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if(parc > 1)
	{
		if(MyClient(source_p) && !IsOper(source_p))
		{
			if((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
			{
				/* safe enough to give this on a local connect only */
				sendto_one(source_p, form_str(RPL_LOAD2HI),
					   me.name, source_p->name, "VERSION");
				return 0;
			}
			else
				last_used = CurrentTime;
		}

		if(hunt_server(client_p, source_p, ":%s VERSION :%s", 1, parc, parv) != HUNTED_ISME)
			return 0;
	}

	sendto_one_numeric(source_p, RPL_VERSION, form_str(RPL_VERSION),
			   ircd_version, serno,
			   me.name, confopts(source_p), TS_CURRENT,
			   ServerInfo.sid);

	show_isupport(source_p);

	return 0;
}

#define FEATURES "STD=i-d"		\
		" STATUSMSG=@+"		\
                "%s%s%s"		\
                " MODES=%i"		\
                " MAXCHANNELS=%i"	\
                " MAXBANS=%i"		\
                " MAXTARGETS=%i"	\
                " NICKLEN=%i"		\
                " TOPICLEN=%i"		\
                " KICKLEN=%i"		\
		" AWAYLEN=%i"

#define FEATURESVALUES ConfigChannel.use_knock ? " KNOCK" : "", \
        ConfigChannel.use_except ? " EXCEPTS" : "", \
        ConfigChannel.use_invex ? " INVEX" : "", \
        MAXMODEPARAMS,ConfigChannel.max_chans_per_user, \
        ConfigChannel.max_bans, \
        ConfigFileEntry.max_targets,NICKLEN-1,TOPICLEN,REASONLEN,AWAYLEN

#define FEATURES2 "CHANNELLEN=%i"	\
		" CHANTYPES=#&"		\
		" PREFIX=(ov)@+" 	\
		" CHANMODES=%s%sb,k,l,imnpst"	\
		" NETWORK=%s"		\
		" CASEMAPPING=rfc1459"	\
		" CHARSET=ascii"	\
		" CALLERID"		\
		" WALLCHOPS"		\
		" ETRACE"		\
		" SAFELIST"		\
		" ELIST=U"		\
		" WATCH=%i"		

#define FEATURES2VALUES LOC_CHANNELLEN, \
			ConfigChannel.use_except ? "e" : "", \
                        ConfigChannel.use_invex ? "I" : "", \
                        ServerInfo.network_name, \
			ConfigFileEntry.max_watch

/*
 * show_isupport
 *
 * inputs	- pointer to client
 * output	- 
 * side effects	- display to client what we support (for them)
 */
static void
show_isupport(struct Client *source_p)
{
	char isupportbuffer[512];

	ircsprintf(isupportbuffer, FEATURES, FEATURESVALUES);
	sendto_one_numeric(source_p, RPL_ISUPPORT, form_str(RPL_ISUPPORT), isupportbuffer);

	ircsprintf(isupportbuffer, FEATURES2, FEATURES2VALUES);
	sendto_one_numeric(source_p, RPL_ISUPPORT, form_str(RPL_ISUPPORT), isupportbuffer);

	return;
}

/* confopts()
 * input  - client pointer
 * output - ircd.conf option string
 * side effects - none
 */
static char *
confopts(struct Client *source_p)
{
	static char result[15];
	char *p;

	result[0] = '\0';
	p = result;

	if(ConfigChannel.use_except)
		*p++ = 'e';

	if(ConfigFileEntry.glines)
		*p++ = 'g';
	*p++ = 'G';

	/* might wanna hide this :P */
	if(ServerInfo.hub)
		*p++ = 'H';

	if(ConfigChannel.use_invex)
		*p++ = 'I';

	if(ConfigChannel.use_knock)
		*p++ = 'K';

	*p++ = 'M';
	*p++ = 'p';
#ifdef HAVE_LIBZ
	*p++ = 'Z';
#endif
#ifdef IPV6
	*p++ = '6';
#endif

	*p = '\0';

	return result;
}


/* report_and_set_user_flags
 *
 * Inputs       - pointer to source_p
 *              - pointer to aconf for this user
 * Output       - NONE
 * Side effects -
 * Report to user any special flags they are getting, and set them.
 */
static void
report_and_set_user_flags(struct Client *source_p, struct ConfItem *aconf)
{
	/* If this user is being spoofed, tell them so */
	if(IsConfDoSpoofIp(aconf))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :*** Spoofing your IP. congrats.",
			   me.name, source_p->name);
	}

	/* If this user is in the exception class, Set it "E lined" */
	if(IsConfExemptKline(aconf))
	{
		SetExemptKline(source_p);
		sendto_one(source_p,
			   ":%s NOTICE %s :*** You are exempt from K/D/G/X lines. congrats.",
			   me.name, source_p->name);
	}

	if(IsConfExemptGline(aconf))
	{
		SetExemptGline(source_p);

		/* dont send both a kline and gline exempt notice */
		if(!IsConfExemptKline(aconf))
			sendto_one(source_p,
				   ":%s NOTICE %s :*** You are exempt from G lines.",
				   me.name, source_p->name);
	}

	/* If this user is exempt from user limits set it F lined" */
	if(IsConfExemptLimits(aconf))
	{
		SetExemptLimits(source_p);
		sendto_one(source_p,
			   ":%s NOTICE %s :*** You are exempt from user limits. congrats.",
			   me.name, source_p->name);
	}

	/* If this user is exempt from idle time outs */
	if(IsConfIdlelined(aconf))
	{
		SetIdlelined(source_p);
		sendto_one(source_p,
			   ":%s NOTICE %s :*** You are exempt from idle limits. congrats.",
			   me.name, source_p->name);
	}

	if(IsConfExemptFlood(aconf))
	{
		SetExemptFlood(source_p);
		sendto_one(source_p,
			   ":%s NOTICE %s :*** You are exempt from flood limits.",
			   me.name, source_p->name);
	}

	if(IsConfExemptSpambot(aconf))
	{
		SetExemptSpambot(source_p);
		sendto_one(source_p,
			   ":%s NOTICE %s :*** You are exempt from spambot checks.",
			   me.name, source_p->name);
	}

	if(IsConfExemptShide(aconf))
	{
		SetExemptShide(source_p);
		sendto_one(source_p,
			   ":%s NOTICE %s :*** You are exempt from serverhiding.",
			   me.name, source_p->name);
	}
}

/* 
 * user_welcome
 *
 * inputs	- client pointer to client to welcome
 * output	- NONE
 * side effects	-
 */
static void
user_welcome(struct Client *source_p)
{
	sendto_one(source_p, form_str(RPL_WELCOME), me.name, source_p->name,
		   ServerInfo.network_name, source_p->name);
	sendto_one(source_p, form_str(RPL_YOURHOST), me.name,
		   source_p->name,
		   get_listener_name(source_p->localClient->listener), ircd_version);

	sendto_one(source_p, form_str(RPL_CREATED), me.name, source_p->name, creation);
	sendto_one(source_p, form_str(RPL_MYINFO), me.name, source_p->name, me.name, ircd_version);

	show_isupport(source_p);

	show_lusers(source_p);

	if(ConfigFileEntry.short_motd)
	{
		sendto_one(source_p,
			   "NOTICE %s :*** Notice -- motd was last changed at %s",
			   source_p->name, user_motd_changed);

		sendto_one(source_p,
			   "NOTICE %s :*** Notice -- Please read the motd if you haven't read it",
			   source_p->name);

		sendto_one(source_p, form_str(RPL_MOTDSTART), 
			   me.name, source_p->name, me.name);

		sendto_one(source_p, form_str(RPL_MOTD),
			   me.name, source_p->name, "*** This is the short motd ***");

		sendto_one(source_p, form_str(RPL_ENDOFMOTD), me.name, source_p->name);
	}
	else
		send_user_motd(source_p);
}

/*
 * introduce_client
 *
 * inputs	-
 * output	-
 * side effects - This common function introduces a client to the rest
 *		  of the net, either from a local client connect or
 *		  from a remote connect.
 */
static int
introduce_client(struct Client *client_p, struct Client *source_p, struct User *user, const char *nick)
{
	static char ubuf[12];

	if(MyClient(source_p))
		send_umode(source_p, source_p, 0, SEND_UMODES, ubuf);
	else
		send_umode(NULL, source_p, 0, SEND_UMODES, ubuf);

	if(!*ubuf)
	{
		ubuf[0] = '+';
		ubuf[1] = '\0';
	}

	/* if it has an ID, introduce it with its id to TS6 servers,
	 * otherwise introduce it normally to all.
	 */
	if(has_id(source_p))
	{
		sendto_server(client_p, NULL, CAP_TS6, NOCAPS,
			      ":%s UID %s %d %ld %s %s %s %s %s :%s",
			      source_p->servptr->id, nick,
			      source_p->hopcount + 1,
			      (long) source_p->tsinfo, ubuf,
			      source_p->username, source_p->host,
			      IsIPSpoof(source_p) ? "0" : source_p->sockhost,
			      source_p->id, source_p->info);

		sendto_server(client_p, NULL, NOCAPS, CAP_TS6,
			      "NICK %s %d %ld %s %s %s %s :%s",
			      nick, source_p->hopcount + 1,
			      (long) source_p->tsinfo,
			      ubuf, source_p->username, source_p->host,
			      user->server, source_p->info);
	}
	else
		sendto_server(client_p, NULL, NOCAPS, NOCAPS,
			      "NICK %s %d %ld %s %s %s %s :%s",
			      nick, source_p->hopcount + 1,
			      (long) source_p->tsinfo,
			      ubuf, source_p->username, source_p->host,
			      user->server, source_p->info);


	return 0;
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

	pnode = match_ip(ConfIpLimits(aconf), (struct sockaddr *)&client_p->localClient->ip);

	if(pnode == NULL)
		pnode = make_and_lookup_ip(ConfIpLimits(aconf), (struct sockaddr *)&client_p->localClient->ip, ConfCidrBitlen(aconf));

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
static int
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
		aconf = find_address_conf(client_p->host, client_p->sockhost,
					  client_p->username,
					  (struct sockaddr *)&client_p->localClient->ip,
					  client_p->localClient->ip.ss_family);
	}
	else
	{
		strlcpy(non_ident, "~", sizeof(non_ident));
		strlcat(non_ident, username, sizeof(non_ident));
		aconf = find_address_conf(client_p->host, client_p->sockhost,
					  non_ident, 
					  (struct sockaddr *)&client_p->localClient->ip,
					  client_p->localClient->ip.ss_family);
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
static int
check_client(struct Client *client_p, struct Client *source_p, const char *username)
{
	int i;

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

		ServerStats.is_ref++;
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

		ServerStats.is_ref++;
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

		ServerStats.is_ref++;
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

		ServerStats.is_ref++;
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
			
			ServerStats.is_ref++;
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
		ServerStats.is_ref++;
		break;

	case 0:
	default:
		break;
	}
	return (i);
}

/*
** register_local_user
**      This function is called when both NICK and USER messages
**      have been accepted for the client, in whatever order. Only
**      after this, is the USER message propagated.
**
**      NICK's must be propagated at once when received, although
**      it would be better to delay them too until full info is
**      available. Doing it is not so simple though, would have
**      to implement the following:
**
**      (actually it has been implemented already for a while) -orabidoo
**
**      1) user telnets in and gives only "NICK foobar" and waits
**      2) another user far away logs in normally with the nick
**         "foobar" (quite legal, as this server didn't propagate
**         it).
**      3) now this server gets nick "foobar" from outside, but
**         has alread the same defined locally. Current server
**         would just issue "KILL foobar" to clean out dups. But,
**         this is not fair. It should actually request another
**         nick from local user or kill him/her...
*/

static int
register_local_user(struct Client *client_p, struct Client *source_p)
{
	struct ConfItem *aconf;
	struct User *user = source_p->user;
	tgchange *target;
	char tmpstr2[IRCD_BUFSIZE];
	char ipaddr[HOSTIPLEN];
	char myusername[USERLEN+1];
	char tempuser[USERLEN+1];
	const char *nick;
	const char *username;
	int status;

	s_assert(NULL != source_p);
	s_assert(MyConnect(source_p));

	
	if(source_p == NULL)
		return -1;

	if(IsAnyDead(source_p))
		return -1;

	if(ConfigFileEntry.ping_cookie)
	{
		if(!(source_p->flags & FLAGS_PINGSENT) && source_p->localClient->random_ping == 0)
		{
			source_p->localClient->random_ping = (unsigned long) (rand() * rand()) << 1;
			sendto_one(source_p, "PING :%08lX",
				   (unsigned long) source_p->localClient->random_ping);
			source_p->flags |= FLAGS_PINGSENT;
			return -1;
		}
		if(!(source_p->flags2 & FLAGS2_PING_COOKIE))
		{
			return -1;
		}
	}


	client_p->localClient->last = CurrentTime;
	/* Straight up the maximum rate of flooding... */
	source_p->localClient->allow_read = MAX_FLOOD_BURST;

	strlcpy(tempuser, source_p->username, sizeof(tempuser));	
	username = tempuser;
	nick = source_p->name;

	/* XXX - fixme. we shouldnt have to build a users buffer twice.. */
	if(!IsGotId(source_p) && (strchr(username, '[') != NULL))
	{
		const char *p;
		int i = 0;

		p = username;

		while(*p && i < USERLEN)
		{
			if(*p != '[')
				myusername[i++] = *p;
			p++;
		}

		myusername[i] = '\0';
		username = myusername;
	}

	if((status = check_client(client_p, source_p, username)) < 0)
		return (CLIENT_EXITED);

	if(!valid_hostname(source_p->host))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :*** Notice -- You have an illegal character in your hostname",
			   me.name, source_p->name);

		strlcpy(source_p->host, source_p->sockhost, sizeof(source_p->host));

#ifdef IPV6
		if(ConfigFileEntry.dot_in_ip6_addr == 1)
			strlcat(source_p->host, ".", sizeof(source_p->host));
#endif
 	}
 
	aconf = source_p->localClient->att_conf;

	if(aconf == NULL)
	{
		exit_client(client_p, source_p, &me, "*** Not Authorised");
		return (CLIENT_EXITED);
	}

	if(!IsGotId(source_p))
	{
		const char *p;
		int i = 0;

		if(IsNeedIdentd(aconf))
		{
			ServerStats.is_ref++;
			sendto_one(source_p,
				   ":%s NOTICE %s :*** Notice -- You need to install identd to use this server",
				   me.name, client_p->name);
			exit_client(client_p, source_p, &me, "Install identd");
			return (CLIENT_EXITED);
		}

		/* dont replace username if its supposed to be spoofed --fl */
		if(!IsConfDoSpoofIp(aconf) || !strchr(aconf->name, '@'))
		{
			p = username;

			if(!IsNoTilde(aconf))
				source_p->username[i++] = '~';

			while (*p && i < USERLEN)
			{
				if(*p != '[')
					source_p->username[i++] = *p;
				p++;
			}

			source_p->username[i] = '\0';
		}
	}

	/* password check */
	if(!EmptyString(aconf->passwd))
	{
		const char *encr;

		if(EmptyString(source_p->localClient->passwd))
			encr = "";
		else if(IsConfEncrypted(aconf))
			encr = crypt(source_p->localClient->passwd, aconf->passwd);
		else
			encr = source_p->localClient->passwd;

		if(strcmp(encr, aconf->passwd))
		{
			ServerStats.is_ref++;
			sendto_one(source_p, form_str(ERR_PASSWDMISMATCH), me.name, source_p->name);
			exit_client(client_p, source_p, &me, "Bad Password");
			return (CLIENT_EXITED);
		}
	}

	if(source_p->localClient->passwd)
	{
		memset(source_p->localClient->passwd, 0, strlen(source_p->localClient->passwd));
		MyFree(source_p->localClient->passwd);
		source_p->localClient->passwd = NULL;
	}

	/* report if user has &^>= etc. and set flags as needed in source_p */
	report_and_set_user_flags(source_p, aconf);

	/* Limit clients */
	/*
	 * We want to be able to have servers and F-line clients
	 * connect, so save room for "buffer" connections.
	 * Smaller servers may want to decrease this, and it should
	 * probably be just a percentage of the MAXCLIENTS...
	 *   -Taner
	 */
	/* Except "F:" clients */
	if(((dlink_list_length(&lclient_list) + 1) >= 
	   ((unsigned long)GlobalSetOptions.maxclients + MAX_BUFFER) ||
           (dlink_list_length(&lclient_list) + 1) >= 
	    ((unsigned long)GlobalSetOptions.maxclients - 5)) && !(IsExemptLimits(source_p)))
	{
		sendto_realops_flags(UMODE_FULL, L_ALL,
				     "Too many clients, rejecting %s[%s].", nick, source_p->host);

		ServerStats.is_ref++;
		exit_client(client_p, source_p, &me, "Sorry, server is full - try later");
		return (CLIENT_EXITED);
	}

	/* valid user name check */

	if(!valid_username(source_p->username))
	{
		sendto_realops_flags(UMODE_REJ, L_ALL,
				     "Invalid username: %s (%s@%s)",
				     nick, source_p->username, source_p->host);
		ServerStats.is_ref++;
		ircsprintf(tmpstr2, "Invalid username [%s]", source_p->username);
		exit_client(client_p, source_p, &me, tmpstr2);
		return (CLIENT_EXITED);
	}

	/* end of valid user name check */

	/* kline exemption extends to xline too */
	if(!IsExemptKline(source_p) &&
	   find_xline(source_p->info, 1) != NULL)
	{
		ServerStats.is_ref++;
		add_reject(source_p);
		exit_client(client_p, source_p, &me, "Bad user info");
		return CLIENT_EXITED;
	}

	/* Any hooks using this event *must* check for the client being dead
	 * when its called.  If the hook wants to terminate the client, it
	 * can call exit_client(). --fl
	 */
	call_hook(h_client_auth_id, client_p);

	if(IsAnyDead(client_p))
		return CLIENT_EXITED;

	inetntop_sock((struct sockaddr *)&source_p->localClient->ip, ipaddr, sizeof(ipaddr));

	sendto_realops_flags(UMODE_CCONN, L_ALL,
			     "Client connecting: %s (%s@%s) [%s] {%s} [%s]",
			     nick, source_p->username, source_p->host,
#ifdef HIDE_SPOOF_IPS
			     IsIPSpoof(source_p) ? "255.255.255.255" :
#endif
			     ipaddr, get_client_class(source_p), source_p->info);

	sendto_realops_flags(UMODE_CCONNEXT, L_ALL,
			"CLICONN %s %s %s %s %s %s 0 %s",
			nick, source_p->username, source_p->host,
#ifdef HIDE_SPOOF_IPS
			IsIPSpoof(source_p) ? "255.255.255.255" :
#endif
			ipaddr, get_client_class(source_p),
			source_p->localClient->fullcaps,
			source_p->info);

	/* If they have died in send_* don't do anything. */
	if(IsAnyDead(source_p))
		return CLIENT_EXITED;

	add_to_hostname_hash(source_p->host, source_p);

	strcpy(source_p->id, generate_uid());
	add_to_id_hash(source_p->id, source_p);

	if(ConfigFileEntry.default_invisible)
	{
		source_p->umodes |= UMODE_INVISIBLE;
		Count.invisi++;
	}

	s_assert(!IsClient(source_p));
	dlinkMoveNode(&source_p->localClient->tnode, &unknown_list, &lclient_list);
	SetClient(source_p);

	/* XXX source_p->servptr is &me, since local client */
	source_p->servptr = find_server(NULL, user->server);
	dlinkAdd(source_p, &source_p->lnode, &source_p->servptr->serv->users);
	/* Increment our total user count here */
	if(++Count.total > Count.max_tot)
		Count.max_tot = Count.total;
	source_p->localClient->allow_read = MAX_FLOOD_BURST;
	source_p->localClient->target_last = CurrentTime;

	Count.totalrestartcount++;

	if((target = find_tgchange((struct sockaddr *)&source_p->localClient->ip)))
	{
		if(target->expiry > CurrentTime)
			source_p->localClient->targinfo[1] = ConfigFileEntry.tgchange_reconnect;
	}

	s_assert(source_p->localClient != NULL);

	if(dlink_list_length(&lclient_list) > (unsigned long)Count.max_loc)
	{
		Count.max_loc = dlink_list_length(&lclient_list);
		if(!(Count.max_loc % 10))
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "New Max Local Clients: %d", Count.max_loc);
	}

	hash_check_watch(source_p, RPL_LOGON);
	user_welcome(source_p);
	return (introduce_client(client_p, source_p, user, nick));
}

/* clean_nick()
 *
 * input	- nickname to check
 * output	- 0 if erroneous, else 1
 * side effects - 
 */
static int
clean_nick(const char *nick, int loc_client)
{
	int len = 0;

	/* nicks cant start with a digit or -, and must have a length */
	if(*nick == '-' || *nick == '\0')
		return 0;

	if(loc_client && IsDigit(*nick))
		return 0;

	for (; *nick; nick++)
	{
		len++;
		if(!IsNickChar(*nick))
			return 0;
	}

	/* nicklen is +1 */
	if(len >= NICKLEN)
		return 0;

	return 1;
}

/* clean_username()
 *
 * input	- username to check
 * output	- 0 if erroneous, else 0
 * side effects -
 */
static int
clean_username(const char *username)
{
	int len = 0;

	for (; *username; username++)
	{
		len++;

		if(!IsUserChar(*username))
			return 0;
	}

	if(len > USERLEN)
		return 0;

	return 1;
}

/* clean_host()
 *
 * input	- host to check
 * output	- 0 if erroneous, else 0
 * side effects -
 */
static int
clean_host(const char *host)
{
	int len = 0;

	for (; *host; host++)
	{
		len++;

		if(!IsHostChar(*host))
			return 0;
	}

	if(len > HOSTLEN)
		return 0;

	return 1;
}

static int
clean_uid(const char *uid)
{
	int len = 1;

	if(!IsDigit(*uid++))
		return 0;

	for(; *uid; uid++)
	{
		len++;

		if(!IsIdChar(*uid))
			return 0;
	}

	if(len != IDLEN-1)
		return 0;

	return 1;
}

static void
change_local_nick(struct Client *client_p, struct Client *source_p, char *nick)
{
	int samenick;

	if((source_p->localClient->last_nick_change + ConfigFileEntry.max_nick_time) < CurrentTime)
		source_p->localClient->number_of_nick_changes = 0;

	source_p->localClient->last_nick_change = CurrentTime;
	source_p->localClient->number_of_nick_changes++;

	/* check theyre not nick flooding */
	if(ConfigFileEntry.anti_nick_flood && !IsOper(source_p) &&
	   (source_p->localClient->number_of_nick_changes > ConfigFileEntry.max_nick_changes))
	   
	{
		sendto_one(source_p, form_str(ERR_NICKTOOFAST), 
			   me.name, source_p->name, source_p->name, 
			   nick, ConfigFileEntry.max_nick_time);
		return;
	}

	samenick = irccmp(source_p->name, nick) ? 0 : 1;

	/* dont reset TS if theyre just changing case of nick */
	if(!samenick)
		source_p->tsinfo = CurrentTime;

	sendto_realops_flags(UMODE_NCHANGE, L_ALL,
			     "Nick change: From %s to %s [%s@%s]",
			     source_p->name, nick, source_p->username, source_p->host);

	/* send the nick change to the users channels */
	sendto_common_channels_local(source_p, ":%s!%s@%s NICK :%s",
				     source_p->name,
				     source_p->username, source_p->host, nick);

	/* send the nick change to servers.. */
	if(source_p->user)
	{
		add_history(source_p, 1);

		sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s NICK %s :%ld",
			      use_id(source_p), nick, (long) source_p->tsinfo);
		sendto_server(client_p, NULL, NOCAPS, CAP_TS6, ":%s NICK %s :%ld",
			      source_p->name, nick, (long) source_p->tsinfo);
	}

	if(!samenick)
		hash_check_watch(source_p, RPL_LOGOFF);

	/* Finally, add to hash */
	del_from_client_hash(source_p->name, source_p);
	strcpy(source_p->name, nick);
	add_to_client_hash(nick, source_p);
	if(!samenick)
		hash_check_watch(source_p, RPL_LOGON);

	/* Make sure everyone that has this client on its accept list
	 * loses that reference. 
	 */
	del_all_accepts(source_p);

	/* fd_desc is long enough */
	comm_note(client_p->localClient->fd, "Nick: %s", nick);

	return;
}

/*
 * change_remote_nick()
 */
static int
change_remote_nick(struct Client *client_p, struct Client *source_p, int parc,
		 const char *parv[], time_t newts, const char *nick)
{
	struct nd_entry *nd;
	int samenick = irccmp(source_p->name, nick) ? 0 : 1;
	/* client changing their nick */
	if(samenick)
		source_p->tsinfo = newts ? newts : CurrentTime;

	sendto_common_channels_local(source_p, ":%s!%s@%s NICK :%s",
				     source_p->name, source_p->username,
				     source_p->host, nick);

	if(source_p->user)
	{
		add_history(source_p, 1);
		sendto_server(client_p, NULL, NOCAPS, NOCAPS, ":%s NICK %s :%ld",
			      parv[0], nick, (long) source_p->tsinfo);
	}
	if(!samenick)
		hash_check_watch(source_p, RPL_LOGOFF);

	del_from_client_hash(source_p->name, source_p);
	
	/* invalidate nick delay when a remote client uses the nick.. */
	if((nd = hash_find_nd(nick)))
		free_nd_entry(nd);

	strcpy(source_p->name, nick);
	add_to_client_hash(nick, source_p);

	if(!samenick)
		hash_check_watch(source_p, RPL_LOGOFF);

	/* remove all accepts pointing to the client */
	del_all_accepts(source_p);

	return 0;
}

static int
perform_nick_collides(struct Client *source_p, struct Client *client_p,
		      struct Client *target_p, int parc, const char *parv[],
		      time_t newts, const char *nick, const char *uid)
{
	int sameuser;

	/* if we dont have a ts, or their TS's are the same, kill both */
	if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo))
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "Nick collision on %s(%s <- %s)(both killed)",
				     target_p->name, target_p->from->name, client_p->name);

		sendto_one_numeric(target_p, ERR_NICKCOLLISION,
				   form_str(ERR_NICKCOLLISION), target_p->name);

		/* if the new client being introduced has a UID, we need to
		 * issue a KILL for it..
		 */
		if(uid)
			sendto_one(client_p, ":%s KILL %s :%s (Nick collision (new))",
					me.id, uid, me.name);

		/* we then need to KILL the old client everywhere */
		kill_client_serv_butone(NULL, target_p,
					"%s (Nick collision (new))", me.name);
		ServerStats.is_kill++;

		target_p->flags |= FLAGS_KILLED;
		exit_client(client_p, target_p, &me, "Nick collision (new)");
		return 0;
	}
	/* the timestamps are different */
	else
	{
		sameuser = (target_p->user) && !irccmp(target_p->username, parv[5])
				&& !irccmp(target_p->host, parv[6]);

		if((sameuser && newts < target_p->tsinfo) ||
		   (!sameuser && newts > target_p->tsinfo))
		{
			/* if we have a UID, then we need to issue a KILL,
			 * otherwise we do nothing and hope that the other
			 * client will collide it..
			 */
			if(uid)
				sendto_one(client_p,
					":%s KILL %s :%s (Nick collision (new))",
					me.id, uid, me.name);
			return 0;
		}
		else
		{
			if(sameuser)
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Nick collision on %s(%s <- %s)(older killed)",
						     target_p->name, target_p->from->name,
						     client_p->name);
			else
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Nick collision on %s(%s <- %s)(newer killed)",
						     target_p->name, target_p->from->name,
						     client_p->name);

			ServerStats.is_kill++;
			sendto_one_numeric(target_p, ERR_NICKCOLLISION,
					   form_str(ERR_NICKCOLLISION), target_p->name);

			/* now we just need to kill the existing client */
			kill_client_serv_butone(client_p, target_p,
						"%s (Nick collision (new))", me.name);

			target_p->flags |= FLAGS_KILLED;
			(void) exit_client(client_p, target_p, &me, "Nick collision");

			register_client(client_p, parc == 10 ? source_p : NULL,
					nick, newts, parc, parv);

			return 0;
		}
	}
}


static int
perform_nickchange_collides(struct Client *source_p, struct Client *client_p,
			    struct Client *target_p, int parc, 
			    const char *parv[], time_t newts, const char *nick)
{
	int sameuser;

	/* its a client changing nick and causing a collide */
	if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo) || !source_p->user)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "Nick change collision from %s to %s(%s <- %s)(both killed)",
				     source_p->name, target_p->name, target_p->from->name,
				     client_p->name);

		ServerStats.is_kill++;
		sendto_one_numeric(target_p, ERR_NICKCOLLISION,
				   form_str(ERR_NICKCOLLISION), target_p->name);

		kill_client_serv_butone(NULL, source_p, "%s (Nick change collision)", me.name);

		ServerStats.is_kill++;

		kill_client_serv_butone(NULL, target_p, "%s (Nick change collision)", me.name);

		target_p->flags |= FLAGS_KILLED;
		exit_client(NULL, target_p, &me, "Nick collision(new)");
		source_p->flags |= FLAGS_KILLED;
		exit_client(client_p, source_p, &me, "Nick collision(old)");
		return 0;
	}
	else
	{
		sameuser = !irccmp(target_p->username, source_p->username) &&
			!irccmp(target_p->host, source_p->host);

		if((sameuser && newts < target_p->tsinfo) ||
		   (!sameuser && newts > target_p->tsinfo))
		{
			if(sameuser)
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Nick change collision from %s to %s(%s <- %s)(older killed)",
						     source_p->name, target_p->name,
						     target_p->from->name, client_p->name);
			else
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Nick change collision from %s to %s(%s <- %s)(newer killed)",
						     source_p->name, target_p->name,
						     target_p->from->name, client_p->name);

			ServerStats.is_kill++;

			sendto_one_numeric(target_p, ERR_NICKCOLLISION,
					   form_str(ERR_NICKCOLLISION), target_p->name);

			/* kill the client issuing the nickchange */
			kill_client_serv_butone(client_p, source_p,
						"%s (Nick change collision)", me.name);

			source_p->flags |= FLAGS_KILLED;

			if(sameuser)
				exit_client(client_p, source_p, &me, "Nick collision(old)");
			else
				exit_client(client_p, source_p, &me, "Nick collision(new)");
			return 0;
		}
		else
		{
			if(sameuser)
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Nick collision on %s(%s <- %s)(older killed)",
						     target_p->name, target_p->from->name,
						     client_p->name);
			else
				sendto_realops_flags(UMODE_ALL, L_ALL,
						     "Nick collision on %s(%s <- %s)(newer killed)",
						     target_p->name, target_p->from->name,
						     client_p->name);

			sendto_one_numeric(target_p, ERR_NICKCOLLISION,
					   form_str(ERR_NICKCOLLISION), target_p->name);

			/* kill the client who existed before hand */
			kill_client_serv_butone(client_p, target_p, 
					"%s (Nick collision)", me.name);

			ServerStats.is_kill++;

			target_p->flags |= FLAGS_KILLED;
			(void) exit_client(client_p, target_p, &me, "Nick collision");
		}
	}

	change_remote_nick(client_p, source_p, parc, parv, newts, nick);

	return 0;
}

static int
register_client(struct Client *client_p, struct Client *server, 
		const char *nick, time_t newts, int parc, const char *parv[])
{
	struct Client *source_p;
	struct User *user;
	struct nd_entry *nd;
	const char *m;
	int flag;

	source_p = make_client(client_p);
	user = make_user(source_p);
	dlinkAddTail(source_p, &source_p->node, &global_client_list);

	source_p->hopcount = atoi(parv[2]);
	source_p->tsinfo = newts;

	strcpy(source_p->name, nick);
	strlcpy(source_p->username, parv[5], sizeof(source_p->username));
	strlcpy(source_p->host, parv[6], sizeof(source_p->host));
	
	if(parc == 10)
	{
		user->server = find_or_add(server->name);
		strlcpy(source_p->info, parv[9], sizeof(source_p->info));
		strlcpy(source_p->sockhost, parv[7], sizeof(source_p->sockhost));
		strlcpy(source_p->id, parv[8], sizeof(source_p->id));
		add_to_id_hash(source_p->id, source_p);
	}
	else
	{
		user->server = find_or_add(parv[7]);
		strlcpy(source_p->info, parv[8], sizeof(source_p->info));
	}

	/* remove any nd entries for this nick */
	if((nd = hash_find_nd(nick)))
		free_nd_entry(nd);

	add_to_client_hash(nick, source_p);
	add_to_hostname_hash(source_p->host, source_p);
	hash_check_watch(source_p, RPL_LOGON);
	
	m = &parv[4][1];
	while(*m)
	{
		flag = user_modes_from_c_to_bitmask[(unsigned char) *m];

		/* increment +i count if theyre invis */
		if(!(source_p->umodes & UMODE_INVISIBLE) &&
		   (flag & UMODE_INVISIBLE))
			Count.invisi++;

		/* increment opered count if theyre opered */
		if(!(source_p->umodes & UMODE_OPER) && (flag & UMODE_OPER))
			Count.oper++;

		source_p->umodes |= (flag & SEND_UMODES);
		m++;
	}

	SetRemoteClient(source_p);

	if(++Count.total > Count.max_tot)
		Count.max_tot = Count.total;
	
	if(server == NULL)
	{
		if((source_p->servptr = find_server(NULL, user->server)) == NULL)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Ghost killed: %s on invalid server %s",
					     source_p->name, user->server);
			kill_client(client_p, source_p, "%s (Server doesn't exist)", me.name);
			source_p->flags |= FLAGS_KILLED;
			return exit_client(NULL, source_p, &me, "Ghosted Client");
		}
	}
	else
		source_p->servptr = server;

	dlinkAdd(source_p, &source_p->lnode, &source_p->servptr->serv->users);

	/* fake direction */
	if(source_p->servptr->from != source_p->from)
	{
		struct Client *target_p = source_p->servptr->from;

		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Bad User [%s] :%s USER %s@%s %s, != %s[%s]",
				     client_p->name, source_p->name,
				     source_p->username, source_p->host,
				     user->server, target_p->name,
				     target_p->from->name);
		kill_client(client_p, source_p,
			    "%s (NICK from wrong direction (%s != %s))",
			    me.name, user->server, target_p->from->name);
		source_p->flags |= FLAGS_KILLED;
		return exit_client(source_p, source_p, &me, "USER server wrong direction");
	}

	return (introduce_client(client_p, source_p, user, nick));
}

