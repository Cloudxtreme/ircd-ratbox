/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_join.c: Joins a channel.
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
#include "client.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_serv.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "sprintf_irc.h"
#include "packet.h"

static int m_join(struct Client *, struct Client *, int, const char **);
static int ms_join(struct Client *, struct Client *, int, const char **);
static int ms_sjoin(struct Client *, struct Client *, int, const char **);
static int m_kick(struct Client *, struct Client *, int, const char **);
static int m_part(struct Client *, struct Client *, int, const char **);
static int m_names(struct Client *, struct Client *, int, const char **);
static int m_invite(struct Client *, struct Client *, int, const char **);


#define mg_kick { m_kick, 3 }

struct Message join_msgtab = {
	"JOIN", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_join, 2}, {ms_join, 2}, mg_ignore, mg_ignore, {m_join, 2}}
};

struct Message part_msgtab = {
	"PART", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_part, 2}, {m_part, 2}, mg_ignore, mg_ignore, {m_part, 2}}
};

struct Message sjoin_msgtab = {
	"SJOIN", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_ignore, mg_ignore, {ms_sjoin, 0}, mg_ignore, mg_ignore}
};

struct Message kick_msgtab = {
	"KICK", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_kick, mg_kick, mg_kick, mg_ignore, mg_kick}
};

struct Message names_msgtab = {
	"NAMES", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_names, 0}, mg_ignore, mg_ignore, mg_ignore, {m_names, 0}}
};

struct Message invite_msgtab = {
	"INVITE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_invite, 3}, {m_invite, 3}, mg_ignore, mg_ignore, {m_invite, 3}}
};

mapi_clist_av1 channels_clist[] = { &join_msgtab, &sjoin_msgtab, &kick_msgtab, &part_msgtab, &names_msgtab, 
				    &invite_msgtab, 
				    NULL };
DECLARE_MODULE_AV1(channels, NULL, NULL, channels_clist, NULL, NULL, "$Revision$");

static void do_join_0(struct Client *client_p, struct Client *source_p);
static int check_channel_name_loc(struct Client *source_p, const char *name);

static int can_join(struct Client *source_p, struct Channel *chptr, char *key);

static void set_final_mode(struct Channel *chptr, const char *name,
			struct Mode *mode, struct Mode *oldmode);
static void remove_our_modes(struct Channel *chptr, struct Client *source_p);
static void remove_ban_list(struct Channel *chptr, struct Client *source_p,
				dlink_list *list, char c, int cap);
static void add_user_to_channel(struct Channel *chptr, struct Client *client_p, int flags);
static void remove_user_from_channel(struct membership *msptr);
static void part_one_client(struct Client *client_p, struct Client *source_p, char *name, char *reason);

static void channel_member_names(struct Channel *chptr, struct Client *client_p, int show_eon);
static void names_global(struct Client *source_p);

/*
 * m_join
 *      parv[0] = sender prefix
 *      parv[1] = channel
 *      parv[2] = channel password (key)
 */
static int
m_join(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char jbuf[BUFSIZE];
	struct Channel *chptr = NULL;
	struct ConfItem *aconf;
	char *name;
	char *key = NULL;
	int i, flags = 0;
	char *p = NULL, *p2 = NULL;
	char *chanlist;
	char *mykey;
	int successful_join_count = 0;	/* Number of channels successfully joined */

	jbuf[0] = '\0';

	/* rebuild the list of channels theyre supposed to be joining.
	 * this code has a side effect of losing keys, but..
	 */
	chanlist = LOCAL_COPY(parv[1]);
	for(i = 0, name = strtoken(&p, chanlist, ","); name;
	    name = strtoken(&p, NULL, ","))
	{
		/* check the length and name of channel is ok */
		if(!check_channel_name_loc(source_p, name) || (strlen(name) > LOC_CHANNELLEN))
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME,
					   form_str(ERR_BADCHANNAME),
					   (unsigned char *) name);
			continue;
		}

		/* join 0 parts all channels */
		if(*name == '0' && !atoi(name))
		{
			(void) strcpy(jbuf, "0");
			continue;
		}

		/* check it begins with # or &, and local chans are disabled */
		else if(!IsChannelName(name))
		{
			sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
					   form_str(ERR_NOSUCHCHANNEL), name);
			continue;
		}

		/* see if its resv'd */
		if((aconf = hash_find_resv(name)) && (!IsOper(source_p) || !ConfigChannel.no_oper_resvs))
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME,
					form_str(ERR_BADCHANNAME), name);
			sendto_realops_flags(UMODE_SPY, L_ALL,
					     "User %s (%s@%s) is attempting to join locally juped channel %s (%s)",
					     source_p->name, source_p->username, source_p->host,
					     name, aconf->passwd);
			continue;
		}

		if(splitmode && !IsOper(source_p) && (*name != '&') &&
		   ConfigChannel.no_join_on_split)
		{
			sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
				   me.name, source_p->name, name);
			continue;
		}

		if(*jbuf)
			(void) strcat(jbuf, ",");
		(void) strlcat(jbuf, name, sizeof(jbuf) - i - 1);
		i += strlen(name)+1;
	}

	if(parc > 2)
	{
		mykey = LOCAL_COPY(parv[2]);
		key = strtoken(&p2, mykey, ",");
	}

	for(name = strtoken(&p, jbuf, ","); name;
	    key = (key) ? strtoken(&p2, NULL, ",") : NULL, name = strtoken(&p, NULL, ","))
	{
		/* JOIN 0 simply parts all channels the user is in */
		if(*name == '0' && !atoi(name))
		{
			if(source_p->user->channel.head == NULL)
				continue;

			do_join_0(&me, source_p);
			continue;
		}

		/* look for the channel */
		if((chptr = find_channel(name)) != NULL)
		{
			if(IsMember(source_p, chptr))
				continue;
		
			if(dlink_list_length(&chptr->members) == 0)
				flags = CHFL_CHANOP;
			else
				flags = 0;
		}
		else
		{
			if(splitmode && !IsOper(source_p) && (*name != '&') &&
			   ConfigChannel.no_create_on_split)
			{
				sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
					   me.name, source_p->name, name);
				continue;
			}

			flags = CHFL_CHANOP;
		}

		if((dlink_list_length(&source_p->user->channel) >= 
					(unsigned long)ConfigChannel.max_chans_per_user) &&
		   (!IsOper(source_p) || 
		    (dlink_list_length(&source_p->user->channel) >=
				 (unsigned long)ConfigChannel.max_chans_per_user * 3)))
		{
			sendto_one(source_p, form_str(ERR_TOOMANYCHANNELS),
				   me.name, source_p->name, name);
			if(successful_join_count)
				source_p->localClient->last_join_time = CurrentTime;
			return 0;
		}

		if(flags == 0)	/* if channel doesn't exist, don't penalize */
			successful_join_count++;

		if(chptr == NULL)	/* If I already have a chptr, no point doing this */
		{
			chptr = get_or_create_channel(source_p, name, NULL);

			if(chptr == NULL)
			{
				sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
					   me.name, source_p->name, name);
				if(successful_join_count > 0)
					successful_join_count--;
				continue;
			}
		}

		if(!IsOper(source_p) && !IsExemptSpambot(source_p))
			check_spambot_warning(source_p, name);

		/* can_join checks for +i key, bans etc */
		if((i = can_join(source_p, chptr, key)))
		{
			sendto_one(source_p, form_str(i),
				   me.name, source_p->name, name);
			if(successful_join_count > 0)
				successful_join_count--;
			continue;
		}

		/* add the user to the channel */
		add_user_to_channel(chptr, source_p, flags);

		/* we send the user their join here, because we could have to
		 * send a mode out next.
		 */
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
				     source_p->name,
				     source_p->username, source_p->host, chptr->chname);

		/* its a new channel, set +nt and burst. */
		if(flags & CHFL_CHANOP)
		{
			chptr->channelts = CurrentTime;
			chptr->mode.mode |= MODE_TOPICLIMIT;
			chptr->mode.mode |= MODE_NOPRIVMSGS;

			sendto_channel_local(ONLY_CHANOPS, chptr, ":%s MODE %s +nt",
					     me.name, chptr->chname);

			if(*chptr->chname == '#')
			{
				sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
					      ":%s SJOIN %ld %s +nt :@%s",
					      me.id, (long) chptr->channelts,
					      chptr->chname, source_p->id);
				sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
					      ":%s SJOIN %ld %s +nt :@%s",
					      me.name, (long) chptr->channelts,
					      chptr->chname, source_p->name);
			}
		}
		else
		{
			/* shortcut to &me here, save us checking member
			 * lists because we've just joined the thing.. --fl
			 */
			const char *mbuf = channel_modes(chptr, &me);

			sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
				      ":%s JOIN %ld %s %s",
				      use_id(source_p), (long) chptr->channelts,
				      chptr->chname, mbuf);

			sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
				      ":%s SJOIN %ld %s %s :%s",
				      me.name, (long) chptr->channelts,
				      chptr->chname, mbuf, source_p->name);
		}

		del_invite(chptr, source_p);

		if(chptr->topic != NULL)
		{
			sendto_one(source_p, form_str(RPL_TOPIC), me.name,
				   source_p->name, chptr->chname, chptr->topic);

			sendto_one(source_p, form_str(RPL_TOPICWHOTIME),
				   me.name, source_p->name, chptr->chname,
				   chptr->topic_info, chptr->topic_time);
		}

		channel_member_names(chptr, source_p, 1);

		if(successful_join_count)
			source_p->localClient->last_join_time = CurrentTime;
	}

	return 0;
}

/*
 * ms_join
 *
 * inputs	-
 * output	- none
 * side effects	- handles remote JOIN's sent by servers. In TSora
 *		  remote clients are joined using SJOIN, hence a 
 *		  JOIN sent by a server on behalf of a client is an error.
 *		  here, the initial code is in to take an extra parameter
 *		  and use it for the TimeStamp on a new channel.
 */
static int
ms_join(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;
	static struct Mode mode, *oldmode;
	const char *s;
	const char *mbuf;
	time_t oldts;
	time_t newts;
	int isnew;
	int args = 0;
	int keep_our_modes = YES;
	int keep_new_modes = YES;

	/* special case for join 0 */
	if((parv[1][0] == '0') && (parv[1][1] == '\0') && parc == 2)
	{
		do_join_0(client_p, source_p);
		return 0;
	}

	if(parc < 4)
		return 0;

	if(!IsChannelName(parv[2]) || !check_channel_name(parv[2]))
		return 0;

	/* joins for local channels cant happen. */
	if(parv[2][0] == '&')
		return 0;

	mode.key[0] = '\0';
	mode.mode = mode.limit = 0;

	s = parv[3];
	while (*s)
	{
		switch (*(s++))
		{
		case 'i':
			mode.mode |= MODE_INVITEONLY;
			break;
		case 'n':
			mode.mode |= MODE_NOPRIVMSGS;
			break;
		case 'p':
			mode.mode |= MODE_PRIVATE;
			break;
		case 's':
			mode.mode |= MODE_SECRET;
			break;
		case 'm':
			mode.mode |= MODE_MODERATED;
			break;
		case 't':
			mode.mode |= MODE_TOPICLIMIT;
			break;
		case 'k':
			/* sent a +k without a key, eek. */
			if(parc < 5 + args)
				return 0;
			strlcpy(mode.key, parv[4 + args], sizeof(mode.key));
			args++;
			break;
		case 'l':
			/* sent a +l without a limit. */
			if(parc < 5 + args)
				return 0;
			mode.limit = atoi(parv[4 + args]);
			args++;
			break;
		}
	}

	if((chptr = get_or_create_channel(source_p, parv[2], &isnew)) == NULL)
		return 0;

	newts = atol(parv[1]);
	oldts = chptr->channelts;
	oldmode = &chptr->mode;

	/* making a channel TS0 */
	if(!isnew && !newts && oldts)
	{
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to 0",
				     me.name, chptr->chname, chptr->chname, (long) oldts);
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "Server %s changing TS on %s from %ld to 0",
				     source_p->name, chptr->chname, (long) oldts);
	}

	if(isnew)
		chptr->channelts = newts;
	else if(newts == 0 || oldts == 0)
		chptr->channelts = 0;
	else if(newts == oldts)
		;
	else if(newts < oldts)
	{
		keep_our_modes = NO;
		chptr->channelts = newts;
	}
	else
		keep_new_modes = NO;

	if(!keep_new_modes)
		mode = *oldmode;
	else if(keep_our_modes)
	{
		mode.mode |= oldmode->mode;
		if(oldmode->limit > mode.limit)
			mode.limit = oldmode->limit;
		if(strcmp(mode.key, oldmode->key) < 0)
			strcpy(mode.key, oldmode->key);
	}

	/* Lost the TS, other side wins, so remove modes on this side */
	if(!keep_our_modes)
	{
		remove_our_modes(chptr, source_p);
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to %ld",
				     me.name, chptr->chname, chptr->chname, (long) oldts, (long) newts);
	}

	/* only if the modes are actually changing.. */
	if(keep_new_modes || !keep_our_modes)
		set_final_mode(chptr, source_p->user->server, &mode, oldmode);

	chptr->mode = mode;

	mbuf = channel_modes(chptr, client_p);

	if(!IsMember(source_p, chptr))
	{
		add_user_to_channel(chptr, source_p, CHFL_PEON);
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
				     source_p->name, source_p->username,
				     source_p->host, chptr->chname);
	}

	sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
		      ":%s JOIN %ld %s %s",
		      source_p->id, (long) chptr->channelts, chptr->chname, mbuf);
	sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
		      ":%s SJOIN %ld %s %s :%s",
		      source_p->user->server, (long) chptr->channelts,
		      chptr->chname, mbuf, source_p->name);
	return 0;
}

/*
 * ms_sjoin
 * parv[0] - sender
 * parv[1] - TS
 * parv[2] - channel
 * parv[3] - modes + n arguments (key and/or limit)
 * parv[4+n] - flags+nick list (all in one parameter)
 * 
 * process a SJOIN, taking the TS's into account to either ignore the
 * incoming modes or undo the existing ones or merge them, and JOIN
 * all the specified users while sending JOIN/MODEs to local clients
 */
static int
ms_sjoin(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char modebuf[MODEBUFLEN];
	static char buf_nick[BUFSIZE];
	static char buf_uid[BUFSIZE];
	static const char *para[MAXMODEPARAMS];
	struct Channel *chptr;
	struct Client *target_p;
	time_t newts;
	time_t oldts;
	static struct Mode mode, *oldmode;
	char *mbuf;
	int args = 0;
	int keep_our_modes = 1;
	int keep_new_modes = 1;
	int fl;
	int isnew;
	int mlen;
	int len_nick = 0;
	int len_uid = 0;
	int len;
	int joins = 0;
	const char *s;
	char *ptr_nick;
	char *ptr_uid;
	char *p;
	int i;
	int pargs = 0;
	static char empty[] = "";

	/* I dont trust servers *not* to end up sending us a blank sjoin, so
	 * its better not to make a big deal about it. --fl
	 */
	if(parc < 5 || EmptyString(parv[4]))
		return 0;

	if(!IsChannelName(parv[2]) || !check_channel_name(parv[2]))
		return 0;

	/* SJOIN's for local channels can't happen. */
	if(*parv[2] == '&')
		return 0;

	modebuf[0] = mode.key[0] = '\0';
	mode.mode = mode.limit = 0;

	newts = atol(parv[1]);

	s = parv[3];
	while (*s)
	{
		switch (*(s++))
		{
		case 'i':
			mode.mode |= MODE_INVITEONLY;
			break;
		case 'n':
			mode.mode |= MODE_NOPRIVMSGS;
			break;
		case 'p':
			mode.mode |= MODE_PRIVATE;
			break;
		case 's':
			mode.mode |= MODE_SECRET;
			break;
		case 'm':
			mode.mode |= MODE_MODERATED;
			break;
		case 't':
			mode.mode |= MODE_TOPICLIMIT;
			break;
		case 'k':
			strlcpy(mode.key, parv[4 + args], sizeof(mode.key));
			args++;
			if(parc < 5 + args)
				return 0;
			break;
		case 'l':
			mode.limit = atoi(parv[4 + args]);
			args++;
			if(parc < 5 + args)
				return 0;
			break;
		}
	}

	s = parv[args + 4];

	/* remove any leading spaces */
	while (*s == ' ')
		s++;

	if(EmptyString(s))
		return 0;

	if((chptr = get_or_create_channel(source_p, parv[2], &isnew)) == NULL)
		return 0;		/* channel name too long? */


	oldts = chptr->channelts;
	oldmode = &chptr->mode;

	if(!isnew && !newts && oldts)
	{
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s "
                                     "changed from %ld to 0",
				     me.name, chptr->chname, chptr->chname, (long) oldts);
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "Server %s changing TS on %s from %ld to 0",
				     source_p->name, chptr->chname, (long) oldts);
	}

	if(isnew)
		chptr->channelts = newts;
	else if(newts == 0 || oldts == 0)
		chptr->channelts = 0;
	else if(newts == oldts)
		;
	else if(newts < oldts)
	{
		keep_our_modes = NO;
		chptr->channelts = newts;
	}
	else
		keep_new_modes = NO;

	if(!keep_new_modes)
		mode = *oldmode;
	else if(keep_our_modes)
	{
		mode.mode |= oldmode->mode;
		if(oldmode->limit > mode.limit)
			mode.limit = oldmode->limit;
		if(strcmp(mode.key, oldmode->key) < 0)
			strcpy(mode.key, oldmode->key);
	}

	/* Lost the TS, other side wins, so remove modes on this side */
	if(!keep_our_modes)
	{
		remove_our_modes(chptr, source_p);
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to %ld",
				     me.name, chptr->chname, chptr->chname,
				     (long) oldts, (long) newts);
	}

	if(keep_new_modes || !keep_our_modes)
		set_final_mode(chptr, source_p->name, &mode, oldmode);

	chptr->mode = mode;

	*modebuf = '\0';

	mlen = ircsprintf(buf_nick, ":%s SJOIN %ld %s %s :",
			  source_p->name, (long) chptr->channelts,
			  parv[2], 
			  (parv[3][0] != '0' && keep_new_modes) ? channel_modes(chptr, source_p) : "0");
	strlcpy(buf_uid, buf_nick, sizeof(buf_uid));
	ptr_nick = buf_nick + mlen;;
	ptr_uid = buf_uid + mlen;

	mbuf = modebuf;
	para[0] = para[1] = para[2] = para[3] = empty;

	*mbuf++ = '+';

	/* if theres a space, theres going to be more than one nick, change the
	 * first space to \0, so s is just the first nick, and point p to the
	 * second nick
	 */
	if((p = strchr(s, ' ')) != NULL)
	{
		*p++ = '\0';
	}

	while (s)
	{
		fl = 0;

		for (i = 0; i < 2; i++)
		{
			if(*s == '@')
			{
				fl |= CHFL_CHANOP;
				s++;
			}
			else if(*s == '+')
			{
				fl |= CHFL_VOICE;
				s++;
			}
		}

		/* if the client doesnt exist or is fake direction, skip. */
		if(!(target_p = find_client(s)) ||
		   (target_p->from != client_p) || !IsPerson(target_p))
			goto nextnick;

		/* check we can fit another status+nick+space into buffer,
		 * if we're converting nicks to uids and vice versa this may
		 * not fit..
		 */
		if((mlen + len_nick + NICKLEN + 3) > (BUFSIZE - 3))
		{
			*(ptr_nick - 1) = '\0';
			sendto_server(client_p->from, NULL, NOCAPS, CAP_TS6,
					"%s", buf_nick);
			ptr_nick = buf_nick + mlen;
			len_nick = 0;
		}

		if((mlen + len_uid + IDLEN + 3) > (BUFSIZE - 3))
		{
			*(ptr_uid - 1) = '\0';
			sendto_server(client_p->from, NULL, NOCAPS, CAP_TS6,
					"%s", buf_uid);
			ptr_uid = buf_uid + mlen;
			len_uid = 0;
		}

		if(keep_new_modes)
		{
			if(fl & CHFL_CHANOP)
			{
				*ptr_nick++ = '@';
				*ptr_uid++ = '@';
				len_nick++;
				len_uid++;
			}
			if(fl & CHFL_VOICE)
			{
				*ptr_nick++ = '+';
				*ptr_uid++ = '+';
				len_nick++;
				len_uid++;
			}
		}

		/* copy the nick to the two buffers */
		len = ircsprintf(ptr_nick, "%s ", target_p->name);
		ptr_nick += len;
		len_nick += len;
		len = ircsprintf(ptr_uid, "%s ", use_id(target_p));
		ptr_uid += len;
		len_uid += len;

		if(!keep_new_modes)
		{
			if(fl & CHFL_CHANOP)
				fl = CHFL_DEOPPED;
			else
				fl = 0;
		}

		if(!IsMember(target_p, chptr))
		{
			add_user_to_channel(chptr, target_p, fl);
			sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
					     target_p->name,
					     target_p->username, target_p->host, parv[2]);
			joins++;
		}

		if(fl & CHFL_CHANOP)
		{
			*mbuf++ = 'o';
			para[pargs++] = target_p->name;

			/* a +ov user.. bleh */
			if(fl & CHFL_VOICE)
			{
				/* its possible the +o has filled up MAXMODEPARAMS, if so, start
				 * a new buffer
				 */
				if(pargs >= MAXMODEPARAMS)
				{
					*mbuf = '\0';
					sendto_channel_local(ALL_MEMBERS, chptr,
							     ":%s MODE %s %s %s %s %s %s",
							     source_p->name, chptr->chname,
							     modebuf,
							     para[0], para[1], para[2], para[3]);
					mbuf = modebuf;
					*mbuf++ = '+';
					para[0] = para[1] = para[2] = para[3] = NULL;
					pargs = 0;
				}

				*mbuf++ = 'v';
				para[pargs++] = target_p->name;
			}
		}
		else if(fl & CHFL_VOICE)
		{
			*mbuf++ = 'v';
			para[pargs++] = target_p->name;
		}

		if(pargs >= MAXMODEPARAMS)
		{
			*mbuf = '\0';
			sendto_channel_local(ALL_MEMBERS, chptr,
					     ":%s MODE %s %s %s %s %s %s",
					     source_p->name,
					     chptr->chname,
					     modebuf, para[0], para[1], para[2], para[3]);
			mbuf = modebuf;
			*mbuf++ = '+';
			para[0] = para[1] = para[2] = para[3] = NULL;
			pargs = 0;
		}

	      nextnick:
		/* p points to the next nick */
		s = p;

		/* if there was a trailing space and p was pointing to it, then we
		 * need to exit.. this has the side effect of breaking double spaces
		 * in an sjoin.. but that shouldnt happen anyway
		 */
		if(s && (*s == '\0'))
			s = p = NULL;

		/* if p was NULL due to no spaces, s wont exist due to the above, so
		 * we cant check it for spaces.. if there are no spaces, then when
		 * we next get here, s will be NULL
		 */
		if(s && ((p = strchr(s, ' ')) != NULL))
		{
			*p++ = '\0';
		}
	}

	*mbuf = '\0';
	if(pargs)
	{
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s MODE %s %s %s %s %s %s",
				     source_p->name, chptr->chname, modebuf,
				     para[0], CheckEmpty(para[1]), CheckEmpty(para[2]), CheckEmpty(para[3]));
	}

	if(!joins)
	{
		if(isnew)
			destroy_channel(chptr);

		return 0;
	}

	*(ptr_nick - 1) = '\0';
	*(ptr_uid - 1) = '\0';

	sendto_server(client_p->from, NULL, CAP_TS6, NOCAPS,
		      "%s", buf_uid);
	sendto_server(client_p->from, NULL, NOCAPS, CAP_TS6,
		      "%s", buf_nick);

	/* if the source does TS6 we have to remove our bans.  Its now safe
	 * to issue -b's to the non-ts6 servers, as the sjoin we've just
	 * sent will kill any ops they have.
	 */
	if(!keep_our_modes && source_p->id[0] != '\0')
	{
		if(dlink_list_length(&chptr->banlist) > 0)
			remove_ban_list(chptr, source_p, &chptr->banlist,
					'b', NOCAPS);

		if(dlink_list_length(&chptr->exceptlist) > 0)
			remove_ban_list(chptr, source_p, &chptr->exceptlist,
					'e', CAP_EX);

		if(dlink_list_length(&chptr->invexlist) > 0)
			remove_ban_list(chptr, source_p, &chptr->invexlist,
					'I', CAP_IE);
	}

	
	return 0;
}

/*
** m_kick
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = client to kick
**      parv[3] = kick comment
*/
static int
m_kick(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct membership *msptr;
	struct Client *who;
	struct Channel *chptr;
	int chasing = 0;
	char *comment;
	const char *name;
	char *p = NULL;
	const char *user;
	static char buf[BUFSIZE];

	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	comment = LOCAL_COPY((EmptyString(parv[3])) ? parv[2] : parv[3]);
	if(strlen(comment) > (size_t) REASONLEN)
		comment[REASONLEN] = '\0';

	*buf = '\0';
	if((p = strchr(parv[1], ',')))
		*p = '\0';

	name = parv[1];

	chptr = find_channel(name);
	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), name);
		return 0;
	}

	if(!IsServer(source_p))
	{
		msptr = find_channel_membership(chptr, source_p);

		if((msptr == NULL) && MyConnect(source_p))
		{
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
					   form_str(ERR_NOTONCHANNEL), name);
			return 0;
		}

		if(!is_chanop(msptr))
		{
			if(MyConnect(source_p))
			{
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					   me.name, source_p->name, name);
				return 0;
			}

			/* If its a TS 0 channel, do it the old way */
			if(chptr->channelts == 0)
			{
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					   get_id(&me, source_p), 
					   get_id(source_p, source_p), name);
				return 0;
			}
		}

		/* Its a user doing a kick, but is not showing as chanop locally
		 * its also not a user ON -my- server, and the channel has a TS.
		 * There are two cases we can get to this point then...
		 *
		 *     1) connect burst is happening, and for some reason a legit
		 *        op has sent a KICK, but the SJOIN hasn't happened yet or 
		 *        been seen. (who knows.. due to lag...)
		 *
		 *     2) The channel is desynced. That can STILL happen with TS
		 *        
		 *     Now, the old code roger wrote, would allow the KICK to 
		 *     go through. Thats quite legit, but lets weird things like
		 *     KICKS by users who appear not to be chanopped happen,
		 *     or even neater, they appear not to be on the channel.
		 *     This fits every definition of a desync, doesn't it? ;-)
		 *     So I will allow the KICK, otherwise, things are MUCH worse.
		 *     But I will warn it as a possible desync.
		 *
		 *     -Dianora
		 */
	}

	if((p = strchr(parv[2], ',')))
		*p = '\0';

	user = parv[2];		/* strtoken(&p2, parv[2], ","); */

	if(!(who = find_chasing(source_p, user, &chasing)))
	{
		return 0;
	}

	msptr = find_channel_membership(chptr, who);

	if(msptr != NULL)
	{
		/* jdc
		 * - In the case of a server kicking a user (i.e. CLEARCHAN),
		 *   the kick should show up as coming from the server which did
		 *   the kick.
		 * - Personally, flame and I believe that server kicks shouldn't
		 *   be sent anyways.  Just waiting for some oper to abuse it...
		 */
		if(IsServer(source_p))
			sendto_channel_local(ALL_MEMBERS, chptr, ":%s KICK %s %s :%s",
					     source_p->name, name, who->name, comment);
		else
			sendto_channel_local(ALL_MEMBERS, chptr,
					     ":%s!%s@%s KICK %s %s :%s",
					     source_p->name, source_p->username,
					     source_p->host, name, who->name, comment);

		sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
			      ":%s KICK %s %s :%s", 
			      use_id(source_p), chptr->chname, 
			      use_id(who), comment);
		sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
			      ":%s KICK %s %s :%s", 
			      source_p->name, chptr->chname, who->name, comment);
		remove_user_from_channel(msptr);
	}
	else
		sendto_one_numeric(source_p, ERR_USERNOTINCHANNEL,
				   form_str(ERR_USERNOTINCHANNEL), user, name);

	return 0;
}

/*
** m_part
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = reason
*/
static int
m_part(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	char *p, *name;
	char reason[REASONLEN + 1];
	char *s = LOCAL_COPY(parv[1]);

	reason[0] = '\0';

	if(parc > 2)
		strlcpy(reason, parv[2], sizeof(reason));

	name = strtoken(&p, s, ",");

	/* Finish the flood grace period... */
	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	while (name)
	{
		part_one_client(client_p, source_p, name, reason);
		name = strtoken(&p, NULL, ",");
	}
	return 0;
}

static int
m_names(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	struct Channel *chptr = NULL;
	char *s;

	if(parc > 1 && !EmptyString(parv[1]))
	{
		char *p = LOCAL_COPY(parv[1]);
		if((s = strchr(p, ',')))
			*s = '\0';

		if(!check_channel_name(p))
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME,
					   form_str(ERR_BADCHANNAME),
					   (unsigned char *) p);
			return 0;
		}

		if((chptr = find_channel(p)) != NULL)
			channel_member_names(chptr, source_p, 1);
		else
			sendto_one(source_p, form_str(RPL_ENDOFNAMES), 
				   me.name, source_p->name, p);
	}
	else
	{
		if(!IsOper(source_p))
		{
			if((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
			{
				sendto_one(source_p, form_str(RPL_LOAD2HI),
					   me.name, source_p->name, "NAMES");
				sendto_one(source_p, form_str(RPL_ENDOFNAMES),
					   me.name, source_p->name, "*");
				return 0;
			}
			else
				last_used = CurrentTime;
		}

		names_global(source_p);
		sendto_one(source_p, form_str(RPL_ENDOFNAMES), 
			   me.name, source_p->name, "*");
	}

	return 0;
}


/* m_invite()
 *      parv[0] - sender prefix
 *      parv[1] - user to invite
 *      parv[2] - channel name
 */
static int
m_invite(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	struct Channel *chptr;
	struct membership *msptr;
	int store_invite = 0;

	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	if((target_p = find_person(parv[1])) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, 
				   form_str(ERR_NOSUCHNICK), 
				   IsDigit(parv[1][0]) ? "*" : parv[1]);
		return 0;
	}

	if(check_channel_name(parv[2]) == 0)
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME,
				   form_str(ERR_BADCHANNAME),
				   parv[2]);
		return 0;
	}

	if(!IsChannelName(parv[2]))
	{
		if(MyClient(source_p))
			sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
					   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return 0;
	}

	/* Do not send local channel invites to users if they are not on the
	 * same server as the person sending the INVITE message. 
	 */
	if(parv[2][0] == '&' && !MyConnect(target_p))
	{
		sendto_one(source_p, form_str(ERR_USERNOTONSERV),
			   me.name, source_p->name, parv[1]);
		return 0;
	}

	if((chptr = find_channel(parv[2])) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return 0;
	}

	msptr = find_channel_membership(chptr, source_p);
	if(MyClient(source_p) && (msptr == NULL))
	{
		sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
				   form_str(ERR_NOTONCHANNEL), parv[2]);
		return 0;
	}

	if(IsMember(target_p, chptr))
	{
		sendto_one_numeric(source_p, ERR_USERONCHANNEL,
				   form_str(ERR_USERONCHANNEL), parv[1], parv[2]);
		return 0;
	}

	/* only store invites for +i channels */
	if(ConfigChannel.invite_ops_only || (chptr->mode.mode & MODE_INVITEONLY))
	{
		/* treat remote clients as chanops */
		if(MyClient(source_p) && !is_chanop(msptr))
		{
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
				   me.name, source_p->name, parv[2]);
			return 0;
		}

		if(chptr->mode.mode & MODE_INVITEONLY)
			store_invite = 1;
	}

	if(MyConnect(source_p))
	{
		sendto_one(source_p, form_str(RPL_INVITING), 
			   me.name, source_p->name,
			   target_p->name, parv[2]);
		if(target_p->user->away)
			sendto_one_numeric(source_p, RPL_AWAY, form_str(RPL_AWAY),
					   target_p->name, target_p->user->away);
	}

	if(MyConnect(target_p))
	{
		sendto_one(target_p, ":%s!%s@%s INVITE %s :%s", 
			   source_p->name, source_p->username, source_p->host, 
			   target_p->name, chptr->chname);

		if(store_invite)
		{
			dlink_node *ptr;
			/* already invited? */
			DLINK_FOREACH(ptr, target_p->user->invited.head)
			{
				if(ptr->data == chptr)
					return 0;
			}

			/* ok, if their invite list is too long, remove the tail */
			if((int)dlink_list_length(&target_p->user->invited) >= 
			   ConfigChannel.max_chans_per_user)
			{
				ptr = target_p->user->invited.tail;
				del_invite(ptr->data, target_p);
			}
			
			/* add user to channel invite list */
			dlinkAddAlloc(target_p, &chptr->invites);
				
			/* add channel to user invite list */
			dlinkAddAlloc(chptr, &target_p->user->invited);
				
		}
	}
	else if(target_p->from != client_p)
	{
		sendto_one_prefix(target_p, source_p, "INVITE", ":%s",
				  chptr->chname);
	}

	return 0;
}



/* channel_pub_or_secret()
 *
 * input	- channel
 * output	- "=" if public, "@" if secret, else "*"
 * side effects	-
 */
static const char *
channel_pub_or_secret(struct Channel *chptr)
{
	if(PubChannel(chptr))
		return ("=");
	else if(SecretChannel(chptr))
		return ("@");
	return ("*");
}

/* channel_member_names()
 *
 * input	- channel to list, client to list to, show endofnames
 * output	-
 * side effects - client is given list of users on channel
 */
static void
channel_member_names(struct Channel *chptr, struct Client *client_p, int show_eon)
{
	struct membership *msptr;
	struct Client *target_p;
	dlink_node *ptr;
	char lbuf[BUFSIZE];
	char *t;
	int mlen;
	int tlen;
	int cur_len;
	int is_member;

	if(ShowChannel(client_p, chptr))
	{
		is_member = IsMember(client_p, chptr);

		cur_len = mlen = ircsprintf(lbuf, form_str(RPL_NAMREPLY),
					    me.name, client_p->name, 
					    channel_pub_or_secret(chptr),
					    chptr->chname);

		t = lbuf + cur_len;

		DLINK_FOREACH(ptr, chptr->members.head)
		{
			msptr = ptr->data;
			target_p = msptr->client_p;

			if(IsInvisible(target_p) && !is_member)
				continue;

			tlen = strlen(target_p->name) + 1;
			if(is_chanop_voiced(msptr))
				tlen++;

			if(cur_len + tlen >= BUFSIZE - 3)
			{
				*(t - 1) = '\0';
				sendto_one(client_p, "%s", lbuf);
				cur_len = mlen;
				t = lbuf + mlen;
			}

			ircsprintf(t, "%s%s ", find_channel_status(msptr, 0),
				   target_p->name);

			cur_len += tlen;
			t += tlen;
		}

		*(t - 1) = '\0';
		sendto_one(client_p, "%s", lbuf);
	}

	if(show_eon)
		sendto_one(client_p, form_str(RPL_ENDOFNAMES),
			   me.name, client_p->name, chptr->chname);
}


/*
* names_global
*
* inputs       - pointer to client struct requesting names
* output       - none
* side effects - lists all non public non secret channels
*/
static void
names_global(struct Client *source_p)
{
       int mlen;
       int tlen;
       int cur_len;
       int dont_show = NO;
       dlink_node *lp, *ptr;
       struct Client *target_p;
       struct Channel *chptr = NULL;
       struct membership *msptr;
       char buf[BUFSIZE];
       char *t;

       /* first do all visible channels */
       DLINK_FOREACH(ptr, global_channel_list.head)
       {
	       chptr = ptr->data;
	       channel_member_names(chptr, source_p, 0);
       }
       cur_len = mlen = ircsprintf(buf, form_str(RPL_NAMREPLY), 
				   me.name, source_p->name, "*", "*");
       t = buf + mlen;

       /* Second, do all clients in one big sweep */
       DLINK_FOREACH(ptr, global_client_list.head)
       {
	       target_p = ptr->data;
	       dont_show = NO;

	       if(!IsPerson(target_p) || IsInvisible(target_p))
		       continue;

	       /* we want to show -i clients that are either:
		*   a) not on any channels
		*   b) only on +p channels
		*
		* both were missed out above.  if the target is on a
		* common channel with source, its already been shown.
		*/
	       DLINK_FOREACH(lp, target_p->user->channel.head)
	       {
		       msptr = lp->data;
		       chptr = msptr->chptr;

		       if(PubChannel(chptr) || IsMember(source_p, chptr) ||
			  SecretChannel(chptr))
		       {
			       dont_show = YES;
			       break;
		       }
	       }

	       if(dont_show)
		       continue;

	       if((cur_len + NICKLEN + 2) > (BUFSIZE - 3))
	       {
		       sendto_one(source_p, "%s", buf);
		       cur_len = mlen;
		       t = buf + mlen;
	       }

	       tlen = ircsprintf(t, "%s ", target_p->name);
	       cur_len += tlen;
	       t += tlen;
       }

       if(cur_len > mlen)
	       sendto_one(source_p, "%s", buf);
}


/*
 * part_one_client
 *
 * inputs	- pointer to server
 * 		- pointer to source client to remove
 *		- char pointer of name of channel to remove from
 * output	- none
 * side effects	- remove ONE client given the channel name 
 */
static void
part_one_client(struct Client *client_p, struct Client *source_p, char *name, char *reason)
{
	struct Channel *chptr;
	struct membership *msptr;

	if((chptr = find_channel(name)) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), name);
		return;
	}

	msptr = find_channel_membership(chptr, source_p);
	if(msptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
				   form_str(ERR_NOTONCHANNEL), name);
		return;
	}

	if(MyConnect(source_p) && !IsOper(source_p) && !IsExemptSpambot(source_p))
		check_spambot_warning(source_p, NULL);

	/*
	 *  Remove user from the old channel (if any)
	 *  only allow /part reasons in -m chans
	 */
	if(reason[0] && (is_chanop(msptr) || !MyConnect(source_p) ||
			 ((can_send(chptr, source_p, msptr) > 0 &&
			   (source_p->localClient->firsttime + ConfigFileEntry.anti_spam_exit_message_time)
			   < CurrentTime))))
	{
		sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
				":%s PART %s :%s",
				use_id(source_p), chptr->chname, reason);
		sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
				":%s PART %s :%s",
				source_p->name, chptr->chname, reason);
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s :%s",
					source_p->name, source_p->username,
					source_p->host, chptr->chname, reason);
	}
	else
	{
		sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
				":%s PART %s",
				use_id(source_p), chptr->chname);
		sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
				":%s PART %s",
				source_p->name, chptr->chname);
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s",
					source_p->name, source_p->username,
					source_p->host, chptr->chname);
	}
	remove_user_from_channel(msptr);
}



/*
 * do_join_0
 *
 * inputs	- pointer to client doing join 0
 * output	- NONE
 * side effects	- Use has decided to join 0. This is legacy
 *		  from the days when channels were numbers not names. *sigh*
 *		  There is a bunch of evilness necessary here due to
 * 		  anti spambot code.
 */
static void
do_join_0(struct Client *client_p, struct Client *source_p)
{
	struct membership *msptr;
	struct Channel *chptr = NULL;
	dlink_node *ptr;

	/* Finish the flood grace period... */
	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);


	sendto_server(client_p, NULL, NOCAPS, NOCAPS, ":%s JOIN 0", source_p->name);

	if(source_p->user->channel.head && MyConnect(source_p) && 
	   !IsOper(source_p) && !IsExemptSpambot(source_p))
		check_spambot_warning(source_p, NULL);

	while ((ptr = source_p->user->channel.head))
	{
		msptr = ptr->data;
		chptr = msptr->chptr;
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s",
				     source_p->name,
				     source_p->username, source_p->host, chptr->chname);
		remove_user_from_channel(msptr);
	}
}

static int
check_channel_name_loc(struct Client *source_p, const char *name)
{
	s_assert(name != NULL);
	if(EmptyString(name))
		return 0;

	if(ConfigFileEntry.disable_fake_channels && !IsOper(source_p))
	{
		for (; *name; ++name)
		{
			if(!IsChanChar(*name) || IsFakeChanChar(*name))
				return 0;
		}
	}
	else
	{
		for(; *name; ++name)
		{
			if(!IsChanChar(*name))
				return 0;
		}
	}

	return 1;
}

/* can_join()
 *
 * input	- client to check, channel to check for, key
 * output	- reason for not being able to join, else 0
 * side effects -
 */
static int
can_join(struct Client *source_p, struct Channel *chptr, char *key)
{
	dlink_node *lp;
	dlink_node *ptr;
	struct Ban *invex = NULL;
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];

	s_assert(source_p->localClient != NULL);

	ircsprintf(src_host, "%s!%s@%s", 
		   source_p->name, source_p->username, source_p->host);
	ircsprintf(src_iphost, "%s!%s@%s",
		   source_p->name, source_p->username, source_p->sockhost);

	if((is_banned(chptr, source_p, NULL, src_host, src_iphost)) == CHFL_BAN)
		return (ERR_BANNEDFROMCHAN);

	if(chptr->mode.mode & MODE_INVITEONLY)
	{
		DLINK_FOREACH(lp, source_p->user->invited.head)
		{
			if(lp->data == chptr)
				break;
		}
		if(lp == NULL)
		{
			if(!ConfigChannel.use_invex)
				return (ERR_INVITEONLYCHAN);
			DLINK_FOREACH(ptr, chptr->invexlist.head)
			{
				invex = ptr->data;
				if(match(invex->banstr, src_host)
				   || match(invex->banstr, src_iphost)
				   || match_cidr(invex->banstr, src_iphost))
					break;
			}
			if(ptr == NULL)
				return (ERR_INVITEONLYCHAN);
		}
	}

	if(*chptr->mode.key && (EmptyString(key) || irccmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	if(chptr->mode.limit && 
	   dlink_list_length(&chptr->members) >= (unsigned long)chptr->mode.limit)
		return (ERR_CHANNELISFULL);

	return 0;
}


struct mode_letter
{
	int mode;
	char letter;
};

static struct mode_letter flags[] = {
	{MODE_NOPRIVMSGS,	'n'},
	{MODE_TOPICLIMIT,	't'},
	{MODE_SECRET,		's'},
	{MODE_MODERATED,	'm'},
	{MODE_INVITEONLY,	'i'},
	{MODE_PRIVATE,		'p'},
	{0,			0}
};

static void
set_final_mode(struct Channel *chptr, const char *name,
		struct Mode *mode, struct Mode *oldmode)
{
	static char lmodebuf[MODEBUFLEN];
	static char lparabuf[BUFSIZE];
	int dir = MODE_QUERY;
	char *mbuf, *pbuf;
	int i;

	lmodebuf[0] = lparabuf[0] = '\0';
	mbuf = lmodebuf;
	pbuf = lparabuf;

	/* ok, first get a list of modes we need to add */
	for (i = 0; flags[i].letter; i++)
	{
		if((mode->mode & flags[i].mode) && !(oldmode->mode & flags[i].mode))
		{
			if(dir != MODE_ADD)
			{
				*mbuf++ = '+';
				dir = MODE_ADD;
			}
			*mbuf++ = flags[i].letter;
		}
	}

	/* now the ones we need to remove. */
	for (i = 0; flags[i].letter; i++)
	{
		if((oldmode->mode & flags[i].mode) && !(mode->mode & flags[i].mode))
		{
			if(dir != MODE_DEL)
			{
				*mbuf++ = '-';
				dir = MODE_DEL;
			}
			*mbuf++ = flags[i].letter;
		}
	}

	if(oldmode->limit && !mode->limit)
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'l';
	}
	else if(mode->limit && oldmode->limit != mode->limit)
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'l';
		pbuf += ircsprintf(pbuf, "%d ", mode->limit);
	}

	if(oldmode->key[0] && !mode->key[0])
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'k';
		pbuf += ircsprintf(pbuf, "%s ", oldmode->key);
	}
	if(mode->key[0] && strcmp(oldmode->key, mode->key))
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'k';
		pbuf += ircsprintf(pbuf, "%s ", mode->key);
	}

	*mbuf = '\0';

	/* remove trailing space.. */
	if(*pbuf)
		*(pbuf-1) = '\0';

	if(lmodebuf[0])
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s %s %s",
				     name, chptr->chname, lmodebuf, lparabuf);
}

/*
 * remove_our_modes
 *
 * inputs	-
 * output	- 
 * side effects	- 
 */
static void
remove_our_modes(struct Channel *chptr, struct Client *source_p)
{
	struct membership *msptr;
	dlink_node *ptr;
	char lmodebuf[MODEBUFLEN];
	char *lpara[MAXMODEPARAMS];
	char *mbuf;
	int count = 0;
	int i;

	mbuf = lmodebuf;
	*mbuf++ = '-';

	for(i = 0; i < MAXMODEPARAMS; i++)
		lpara[i] = NULL;

	DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;

		if(is_chanop(msptr))
		{
			msptr->flags &= ~CHFL_CHANOP;
			lpara[count++] = msptr->client_p->name;
			*mbuf++ = 'o';

			/* +ov, might not fit so check. */
			if(is_voiced(msptr))
			{
				if(count >= MAXMODEPARAMS)
				{
					*mbuf = '\0';
					sendto_channel_local(ALL_MEMBERS, chptr,
							     ":%s MODE %s %s %s %s %s %s",
							     me.name, chptr->chname,
							     lmodebuf, lpara[0], lpara[1],
							     lpara[2], lpara[3]);

					/* preserve the initial '-' */
					mbuf = lmodebuf;
					*mbuf++ = '-';
					count = 0;

					for(i = 0; i < MAXMODEPARAMS; i++)
						lpara[i] = NULL;
				}

				msptr->flags &= ~CHFL_VOICE;
				lpara[count++] = msptr->client_p->name;
				*mbuf++ = 'v';
			}
		}
		else if(is_voiced(msptr))
		{
			msptr->flags &= ~CHFL_VOICE;
			lpara[count++] = msptr->client_p->name;
			*mbuf++ = 'v';
		}
		else
			continue;

		if(count >= MAXMODEPARAMS)
		{
			*mbuf = '\0';
			sendto_channel_local(ALL_MEMBERS, chptr,
					     ":%s MODE %s %s %s %s %s %s",
					     me.name, chptr->chname, lmodebuf,
					     lpara[0], lpara[1], lpara[2], lpara[3]);
			mbuf = lmodebuf;
			*mbuf++ = '-';
			count = 0;

			for(i = 0; i < MAXMODEPARAMS; i++)
				lpara[i] = NULL;
		}
	}

	if(count != 0)
	{
		*mbuf = '\0';
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s MODE %s %s %s %s %s %s",
				     me.name, chptr->chname, lmodebuf,
				     EmptyString(lpara[0]) ? "" : lpara[0],
				     EmptyString(lpara[1]) ? "" : lpara[1],
				     EmptyString(lpara[2]) ? "" : lpara[2],
				     EmptyString(lpara[3]) ? "" : lpara[3]);

	}
}

/* remove_ban_list()
 *
 * inputs	- channel, source, list to remove, char of mode, caps needed
 * outputs	-
 * side effects - given list is removed, with modes issued to local clients
 * 		  and non-TS6 servers.
 */
static void
remove_ban_list(struct Channel *chptr, struct Client *source_p,
		dlink_list *list, char c, int cap)
{
	static char lmodebuf[BUFSIZE];
	static char lparabuf[BUFSIZE];
	struct Ban *banptr;
	dlink_node *ptr;
	dlink_node *next_ptr;
	char *mbuf;
	char *pbuf;
	int count = 0;
	int cur_len, mlen, plen;

	pbuf = lparabuf;

	cur_len = mlen = ircsprintf(lmodebuf, ":%s MODE %s -", 
				    source_p->name, chptr->chname);
	mbuf = lmodebuf + mlen;

	DLINK_FOREACH_SAFE(ptr, next_ptr, list->head)
	{
		banptr = ptr->data;

		/* trailing space, and the mode letter itself */
		plen = strlen(banptr->banstr) + 2;

		if(count >= MAXMODEPARAMS || (cur_len + plen) > BUFSIZE - 4)
		{
			/* remove trailing space */
			*(pbuf - 1) = '\0';

			sendto_channel_local(ALL_MEMBERS, chptr, "%s %s",
					     lmodebuf, lparabuf);
			sendto_server(source_p, chptr, cap, CAP_TS6,
				      "%s %s", lmodebuf, lparabuf);

			cur_len = mlen;
			mbuf = lmodebuf + mlen;
			count = 0;
		}

		*mbuf++ = c;
		cur_len += plen;
		pbuf += ircsprintf(pbuf, "%s ", banptr->banstr);
		count++;

		free_ban(banptr);
	}

	*(pbuf - 1) = '\0';
	sendto_channel_local(ALL_MEMBERS, chptr, "%s %s", lmodebuf, lparabuf);
	sendto_server(source_p, chptr, cap, CAP_TS6,
		      "%s%s", lmodebuf, lparabuf);

	list->head = list->tail = NULL;
	list->length = 0;
}

/* add_user_to_channel()
 *
 * input	- channel to add client to, client to add, channel flags
 * output	- 
 * side effects - user is added to channel
 */
static void
add_user_to_channel(struct Channel *chptr, struct Client *client_p, int xflags)
{
	struct membership *msptr;

	s_assert(client_p->user != NULL);
	if(client_p->user == NULL)
		return;

	msptr = allocate_membership();

	msptr->chptr = chptr;
	msptr->client_p = client_p;
	msptr->flags = xflags;

	dlinkAdd(msptr, &msptr->usernode, &client_p->user->channel);
	dlinkAdd(msptr, &msptr->channode, &chptr->members);

	if(MyClient(client_p))
		dlinkAdd(msptr, &msptr->locchannode, &chptr->locmembers);
}

/* remove_user_from_channel()
 *
 * input	- membership pointer to remove from channel
 * output	-
 * side effects - membership (thus user) is removed from channel
 */
static void
remove_user_from_channel(struct membership *msptr)
{
	struct Client *client_p;
	struct Channel *chptr;
	s_assert(msptr != NULL);
	if(msptr == NULL)
		return;

	client_p = msptr->client_p;
	chptr = msptr->chptr;

	dlinkDelete(&msptr->usernode, &client_p->user->channel);
	dlinkDelete(&msptr->channode, &chptr->members);

	if(client_p->servptr == &me)
		dlinkDelete(&msptr->locchannode, &chptr->locmembers);

	chptr->users_last = CurrentTime;

	if(dlink_list_length(&chptr->members) <= 0)
	{
		destroy_channel(chptr);
		return;
	}

	free_membership(msptr);

	return;
}

