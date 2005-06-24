/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_nick.c: Sets a users nick.
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
#include "handlers.h"
#include "client.h"
#include "hash.h"
#include "fdlist.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_stats.h"
#include "s_user.h"
#include "hash.h"
#include "whowas.h"
#include "s_serv.h"
#include "send.h"
#include "channel.h"
#include "s_log.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "common.h"
#include "packet.h"


static int mr_nick(struct Client *, struct Client *, int, const char **);
static int m_nick(struct Client *, struct Client *, int, const char **);
static int ms_nick(struct Client *, struct Client *, int, const char **);

static int ms_client(struct Client *, struct Client *, int, const char **);

static int nick_from_server(struct Client *, struct Client *, int, const char **, time_t, const char *);
static int client_from_server(struct Client *, struct Client *, int, const char **, time_t, const char *);

static int check_clean_nick(struct Client *, struct Client *, const char *, const char *, const char *);
static int check_clean_user(struct Client *, const char *, const char *, const char *);
static int check_clean_host(struct Client *, const char *, const char *, const char *);

static int clean_nick_name(const char *);
static int clean_user_name(const char *);
static int clean_host_name(const char *);

static int perform_nick_collides(struct Client *, struct Client *,
				 struct Client *, int, const char **, time_t, const char *);


struct Message nick_msgtab = {
	"NICK", 0, 0, 1, 0, MFLG_SLOW, 0,
	{mr_nick, m_nick, ms_nick, m_nick}
};

struct Message client_msgtab = {
	"CLIENT", 0, 0, 10, 0, MFLG_SLOW, 0,
	{m_ignore, m_ignore, ms_client, m_ignore}
};

mapi_clist_av1 nick_clist[] = {
	&nick_msgtab, &client_msgtab, NULL
};
DECLARE_MODULE_AV1(nick, NULL, NULL, nick_clist, NULL, NULL, NULL, "$Revision$");

/*
 * mr_nick()
 *
 *       parv[0] = sender prefix
 *       parv[1] = nickname
 */
static int
mr_nick(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	char nick[NICKLEN];
	char *s;

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN),
			   me.name, EmptyString(parv[0]) ? "*" : parv[0]);
		return 0;
	}

	/* Terminate the nick at the first ~ */
	if((s = strchr(parv[1], '~')))
		*s = '\0';

	/* and if the first ~ was the first letter.. */
	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, EmptyString(parv[0]) ? "*" : parv[0], parv[1]);
		return 0;
	}

	/* copy the nick and terminate it */
	strlcpy(nick, parv[1], sizeof(nick));

	/* check the nickname is ok */
	if(!clean_nick_name(nick))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, EmptyString(parv[0]) ? "*" : parv[0], parv[1]);
		return 0;
	}

	/* check if the nick is resv'd */
	if(find_nick_resv(nick))
	{
		sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
			   me.name, EmptyString(parv[0]) ? "*" : parv[0], nick);
		return 0;
	}

	if((target_p = find_client(nick)) == NULL)
	{
		set_initial_nick(client_p, source_p, nick);
		return 0;
	}
	else if(source_p == target_p)
	{
		strcpy(source_p->name, nick);
		return 0;
	}
	else
	{
		sendto_one(source_p, form_str(ERR_NICKNAMEINUSE), me.name, "*", nick);
	}

	return 0;
}

/*
 * m_nick()
 *
 *     parv[0] = sender prefix
 *     parv[1] = nickname
 */
static int
m_nick(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	char nick[NICKLEN];
	struct Client *target_p;

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN), me.name, parv[0]);
		return 0;
	}

	/* mark end of grace period, to prevent nickflooding */
	if(!IsFloodDone(source_p))
		flood_endgrace(source_p);

	/* terminate nick to NICKLEN */
	strlcpy(nick, parv[1], sizeof(nick));

	/* check the nickname is ok */
	if(!clean_nick_name(nick))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME), me.name, parv[0], nick);
		return 0;
	}

	if(find_nick_resv(nick))
	{
		sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE), me.name, parv[0], nick);
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
			{
				change_local_nick(client_p, source_p, nick);
				return 0;
			}
			else
			{
				/* client is doing :old NICK old
				 * ignore it..
				 */
				return 0;
			}
		}

		/* if the client that has the nick isnt registered yet (nick but no
		 * user) then drop the unregged client
		 */
		if(IsUnknown(target_p))
		{
			/* the old code had an if(MyConnect(target_p)) here.. but I cant see
			 * how that can happen, m_nick() is local only --fl_
			 */

			exit_client(NULL, target_p, &me, "Overridden");
			change_local_nick(client_p, source_p, nick);
			return 0;
		}
		else
		{
			sendto_one(source_p, form_str(ERR_NICKNAMEINUSE), me.name, parv[0], nick);
			return 0;
		}

	}
	else
	{
		change_local_nick(client_p, source_p, nick);
		return 0;
	}

	return 0;
}

/*
 * ms_nick()
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
ms_nick(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	char nick[NICKLEN];
	time_t newts = 0;

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN), me.name, parv[0]);
		return 0;
	}

	/* parc == 3 on nickchange, parc == 9 on new nick */
	if((IsClient(source_p) && (parc != 3)) || (IsServer(source_p) && (parc != 9)))
	{
		char tbuf[BUFSIZE] = { 0 };
		int j;

		for (j = 0; j < parc; j++)
		{
			strcat(tbuf, parv[j]);
			strcat(tbuf, " ");
		}

		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "Dropping server %s due to (invalid) command 'NICK' "
				     "with only %d arguments.  (Buf: '%s')",
				     client_p->name, parc, tbuf);
		ilog(L_CRIT, "Insufficient parameters (%d) for command 'NICK' from %s.  Buf: %s",
		     parc, client_p->name, tbuf);
		exit_client(client_p, client_p, client_p,
			    "Not enough arguments to server command.");
		return 0;
	}

	/* fix the length of the nick */
	strlcpy(nick, parv[1], sizeof(nick));

	if(check_clean_nick(client_p, source_p, nick, parv[1],
			    (parc == 9 ? parv[7] : (char *)source_p->user->server)))
		return 0;

	if(parc == 9)
	{
		if(check_clean_user(client_p, nick, parv[5], parv[7]) ||
		   check_clean_host(client_p, nick, parv[6], parv[7]))
			return 0;

		/* check the length of the clients gecos */
		if(strlen(parv[8]) > REALLEN)
		{
			char *s = LOCAL_COPY(parv[8]);
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Long realname from server %s for %s", parv[7],
					     parv[1]);
			
			s[REALLEN] = '\0';
			parv[8] = s;
		}

		if(IsServer(source_p))
			newts = atol(parv[3]);
	}
	else
	{
		if(!IsServer(source_p))
			newts = atol(parv[2]);
	}

	/* if the nick doesnt exist, allow it and process like normal */
	if(!(target_p = find_client(nick)))
	{
		nick_from_server(client_p, source_p, parc, parv, newts, nick);
		return 0;
	}

	/* we're not living in the past anymore, an unknown client is local only. */
	if(IsUnknown(target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		nick_from_server(client_p, source_p, parc, parv, newts, nick);
		return 0;
	}

	if(target_p == source_p)
	{
		if(strcmp(target_p->name, nick))
		{
			/* client changing case of nick */
			nick_from_server(client_p, source_p, parc, parv, newts, nick);
			return 0;
		}
		else
			/* client not changing nicks at all */
			return 0;
	}

	perform_nick_collides(source_p, client_p, target_p, parc, parv, newts, nick);

	return 0;
}

/*
 * ms_client()
 */
static int
ms_client(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	char nick[NICKLEN];
	time_t newts = 0;
	const char *id;
	const char *name;

	id = parv[8];
	name = parv[9];

	/* parse the nickname */
	strlcpy(nick, parv[1], sizeof(nick));

	/* check the nicknames, usernames and hostnames are ok */
	if(check_clean_nick(client_p, source_p, nick, parv[1], parv[7]) ||
	   check_clean_user(client_p, nick, parv[5], parv[7]) ||
	   check_clean_host(client_p, nick, parv[6], parv[7]))
		return 0;

	/* check length of clients gecos */
	if(strlen(name) > REALLEN)
	{
		char *s = LOCAL_COPY(name);
		sendto_realops_flags(UMODE_ALL, L_ALL, "Long realname from server %s for %s",
				     parv[0], parv[1]);
		s[REALLEN] = '\0';
		name = parv[9] = s;
	}

	newts = atol(parv[3]);

	/* if there is an ID collision, kill our client, and kill theirs.
	 * this may generate 401's, but it ensures that both clients always
	 * go, even if the other server refuses to do the right thing.
	 */
	if((target_p = find_id(id)))
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "ID collision on %s(%s <- %s)(both killed)",
				     target_p->name, target_p->from->name, client_p->name);

		kill_client_serv_butone(NULL, target_p, "%s (ID collision)", me.name);

		ServerStats->is_kill++;

		target_p->flags |= FLAGS_KILLED;
		exit_client(client_p, target_p, &me, "ID Collision");
		return 0;
	}

	if(!(target_p = find_client(nick)))
	{
		client_from_server(client_p, source_p, parc, parv, newts, nick);
		return 0;
	}


	if(IsUnknown(target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		client_from_server(client_p, source_p, parc, parv, newts, nick);
		return 0;
	}

	perform_nick_collides(source_p, client_p, target_p, parc, parv, newts, nick);

	return 0;
}


/* 
 * check_clean_nick()
 * 
 * input	- pointer to source
 *		- nickname
 *		- truncated nickname
 *		- origin of client
 * output	- none
 * side effects - if nickname is erroneous, or a different length to
 *                truncated nickname, return 1
 */
static int
check_clean_nick(struct Client *client_p, struct Client *source_p,
		 const char *nick, const char *newnick, const char *server)
{
	/* the old code did some wacky stuff here, if the nick is invalid, kill it
	 * and dont bother messing at all
	 */

	/*
	 * Zero length nicks are bad too..this shouldn't happen but..
	 */

	if(EmptyString(nick) || !clean_nick_name(nick) || strcmp(nick, newnick))
	{
		ServerStats->is_kill++;
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Bad Nick: %s From: %s(via %s)", nick, server, client_p->name);

		sendto_one(client_p, ":%s KILL %s :%s (Bad Nickname)", me.name, newnick, me.name);

		/* bad nick change */
		if(source_p != client_p)
		{
			kill_client_serv_butone(client_p, source_p, "%s (Bad Nickname)", me.name);
			source_p->flags |= FLAGS_KILLED;
			exit_client(client_p, source_p, &me, "Bad Nickname");
		}

		return 1;
	}

	return 0;
}

/* check_clean_user()
 * 
 * input	- pointer to client sending data
 *              - nickname
 *              - username to check
 *		- origin of NICK
 * output	- none
 * side effects - if username is erroneous, return 1
 */
static int
check_clean_user(struct Client *client_p, const char *nick, const char *user, const char *server)
{
	if(strlen(user) > USERLEN)
	{
		ServerStats->is_kill++;
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Long Username: %s Nickname: %s From: %s(via %s)",
				     user, nick, server, client_p->name);

		sendto_one(client_p, ":%s KILL %s :%s (Bad Username)", me.name, nick, me.name);

		return 1;
	}

	if(!clean_user_name(user))
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Bad Username: %s Nickname: %s From: %s(via %s)",
				     user, nick, server, client_p->name);

	return 0;
}

/* check_clean_host()
 * 
 * input	- pointer to client sending us data
 *              - nickname
 *              - hostname to check
 *		- source name
 * output	- none
 * side effects - if hostname is erroneous, return 1
 */
static int
check_clean_host(struct Client *client_p, const char *nick, const char *host, const char *server)
{
	if(strlen(host) > HOSTLEN)
	{
		ServerStats->is_kill++;
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Long Hostname: %s Nickname: %s From: %s(via %s)",
				     host, nick, server, client_p->name);

		sendto_one(client_p, ":%s KILL %s :%s (Bad Hostname)", me.name, nick, me.name);

		return 1;
	}

	if(!clean_host_name(host))
		sendto_realops_flags(UMODE_DEBUG, L_ALL,
				     "Bad Hostname: %s Nickname: %s From: %s(via %s)",
				     host, nick, server, client_p->name);

	return 0;
}

/* clean_nick_name()
 *
 * input	- nickname
 * output	- none
 * side effects - walks through the nickname, returning 0 if erroneous
 */
static int
clean_nick_name(const char *nick)
{
	s_assert(nick);
	if(nick == NULL)
		return 0;

	/* nicks cant start with a digit or -, and must have a length */
	if(*nick == '-' || IsDigit(*nick) || !*nick)
		return 0;

	for (; *nick; nick++)
	{
		if(!IsNickChar(*nick))
			return 0;
	}

	return 1;
}

/* clean_user_name()
 *
 * input	- username
 * output	- none
 * side effects - walks through the username, returning 0 if erroneous
 */
static int
clean_user_name(const char *user)
{
	s_assert(user);
	if(user == NULL)
		return 0;

	for (; *user; user++)
	{
		if(!IsUserChar(*user))
			return 0;

	}

	return 1;
}

/* clean_host_name()
 * input	- hostname
 * output	- none
 * side effects - walks through the hostname, returning 0 if erroneous
 */
static int
clean_host_name(const char *host)
{
	s_assert(host);
	if(host == NULL)
		return 0;
	for (; *host; host++)
	{
		if(!IsHostChar(*host))
			return 0;
	}

	return 1;
}


/*
 * nick_from_server()
 */
static int
nick_from_server(struct Client *client_p, struct Client *source_p, int parc,
		 const char *parv[], time_t newts, const char *nick)
{
	if(IsServer(source_p))
	{
		/* A server introducing a new client, change source */
		source_p = make_client(client_p);
		dlinkAddTail(source_p, &source_p->node, &global_client_list);

		if(parc > 2)
			source_p->hopcount = atoi(parv[2]);
		if(newts)
			source_p->tsinfo = newts;
		else
		{
			newts = source_p->tsinfo = CurrentTime;
			ts_warn("Remote nick %s (%s) introduced without a TS", nick, parv[0]);
		}

		/* copy the nick in place */
		(void) strcpy(source_p->name, nick);
		add_to_client_hash(nick, source_p);

		if(parc > 8)
		{
			int flag;
			const char *m;

			/* parse usermodes */
			m = &parv[4][1];
			while (*m)
			{
				flag = user_modes_from_c_to_bitmask[(unsigned char) *m];
				if(!(source_p->umodes & UMODE_INVISIBLE)
				   && (flag & UMODE_INVISIBLE))
					Count.invisi++;
				if(!(source_p->umodes & UMODE_OPER) && (flag & UMODE_OPER))
					Count.oper++;

				source_p->umodes |= flag & SEND_UMODES;
				m++;
			}

			return do_remote_user(nick, client_p, source_p, parv[5], parv[6],
					      parv[7], parv[8], NULL);
		}
	}
	else if(source_p->name[0])
	{
		/* client changing their nick */
		if(irccmp(parv[0], nick))
			source_p->tsinfo = newts ? newts : CurrentTime;

		sendto_common_channels_local(source_p, ":%s!%s@%s NICK :%s",
					     source_p->name, source_p->username, source_p->host,
					     nick);

		if(source_p->user)
		{
			add_history(source_p, 1);
			sendto_server(client_p, NULL, NOCAPS, NOCAPS,
				      ":%s NICK %s :%lu",
				      parv[0], nick, (unsigned long) source_p->tsinfo);
		}
	}

	/* set the new nick name */
	if(source_p->name[0])
		del_from_client_hash(source_p->name, source_p);

	strcpy(source_p->name, nick);
	add_to_client_hash(nick, source_p);

	/* remove all accepts pointing to the client */
	del_all_accepts(source_p);

	return 0;
}


/*
 * client_from_server()
 */
static int
client_from_server(struct Client *client_p, struct Client *source_p, int parc,
		   const char *parv[], time_t newts, const char *nick)
{
	const char *name;
	const char *id;
	int flag;
	const char *m;

	id = parv[8];
	name = parv[9];

	source_p = make_client(client_p);
	dlinkAddTail(source_p, &source_p->node, &global_client_list);

	source_p->hopcount = atoi(parv[2]);
	source_p->tsinfo = newts;

	/* copy the nick in place */
	(void) strcpy(source_p->name, nick);
	add_to_client_hash(nick, source_p);
	add_to_id_hash(id, source_p);

	/* parse usermodes */
	m = &parv[4][1];
	while (*m)
	{
		flag = user_modes_from_c_to_bitmask[(unsigned char) *m];
		if(flag & UMODE_INVISIBLE)
			Count.invisi++;
		if(flag & UMODE_OPER)
			Count.oper++;

		source_p->umodes |= flag & SEND_UMODES;
		m++;

	}

	return do_remote_user(nick, client_p, source_p, parv[5], parv[6], parv[7], name, id);
}

static int
perform_nick_collides(struct Client *source_p, struct Client *client_p,
		      struct Client *target_p, int parc, const char *parv[], time_t newts, const char *nick)
{
	int sameuser;

	/* server introducing new nick */
	if(IsServer(source_p))
	{
		/* if we dont have a ts, or their TS's are the same, kill both */
		if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo))
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Nick collision on %s(%s <- %s)(both killed)",
					     target_p->name, target_p->from->name, client_p->name);

			kill_client_serv_butone(NULL, target_p,
						"%s (Nick collision (new))", me.name);
			ServerStats->is_kill++;
			sendto_one(target_p, form_str(ERR_NICKCOLLISION),
				   me.name, target_p->name, target_p->name);

			target_p->flags |= FLAGS_KILLED;
			exit_client(client_p, target_p, &me, "Nick collision (new)");
			return 0;
		}
		/* the timestamps are different */
		else
		{
			sameuser = (target_p->user) && !irccmp(target_p->username, parv[5])
				&& !irccmp(target_p->host, parv[6]);

			/* if the users are the same (loaded a client on a different server)
			 * and the new users ts is older, or the users are different and the
			 * new users ts is newer, ignore the new client and let it do the kill
			 */
			if((sameuser && newts < target_p->tsinfo) ||
			   (!sameuser && newts > target_p->tsinfo))
			{
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

				ServerStats->is_kill++;
				sendto_one(target_p, form_str(ERR_NICKCOLLISION),
					   me.name, target_p->name, target_p->name);

				/* if it came from a LL server, itd have been source_p,
				 * so we dont need to mark target_p as known
				 */
				kill_client_serv_butone(source_p, target_p,
							"%s (Nick collision (new))", me.name);

				target_p->flags |= FLAGS_KILLED;
				(void) exit_client(client_p, target_p, &me, "Nick collision");

				if(parc == 9)
					nick_from_server(client_p, source_p, parc, parv, newts,
							 nick);
				else if(parc == 10)
					client_from_server(client_p, source_p, parc, parv, newts,
							   nick);

				return 0;
			}
		}
	}

	/* its a client changing nick and causing a collide */
	if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo) || !source_p->user)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "Nick change collision from %s to %s(%s <- %s)(both killed)",
				     source_p->name, target_p->name, target_p->from->name,
				     client_p->name);

		ServerStats->is_kill++;
		sendto_one(target_p, form_str(ERR_NICKCOLLISION),
			   me.name, target_p->name, target_p->name);

		/* if we got the message from a LL, it knows about source_p */
		kill_client_serv_butone(NULL, source_p, "%s (Nick change collision)", me.name);

		ServerStats->is_kill++;

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

			ServerStats->is_kill++;

			/* this won't go back to the incoming link, so LL doesnt matter */
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

			kill_client_serv_butone(source_p, target_p, "%s (Nick collision)", me.name);

			ServerStats->is_kill++;
			sendto_one(target_p, form_str(ERR_NICKCOLLISION),
				   me.name, target_p->name, target_p->name);

			target_p->flags |= FLAGS_KILLED;
			(void) exit_client(client_p, target_p, &me, "Nick collision");
		}
	}

	/*
	   if(HasID(source_p))
	   client_from_server(client_p,source_p,parc,parv,newts,nick);
	   else
	 */

	/* we should only ever call nick_from_server() here, as
	 * this is a client changing nick, not a new client
	 */
	nick_from_server(client_p, source_p, parc, parv, newts, nick);

	return 0;
}
