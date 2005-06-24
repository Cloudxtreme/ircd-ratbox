/*
 *  ircd-ratbox: A slightly useful ircd.
 *  channel.c: Controls channels.
 *
 * Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center 
 * Copyright (C) 1996-2002 Hybrid Development Team 
 * Copyright (C) 2002 ircd-ratbox development team 
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
#include "channel_mode.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"		/* captab */
#include "s_user.h"
#include "send.h"
#include "whowas.h"
#include "s_conf.h"		/* ConfigFileEntry, ConfigChannel */
#include "event.h"
#include "memory.h"
#include "balloc.h"
#include "resv.h"


#include "s_log.h"

struct config_channel_entry ConfigChannel;
dlink_list global_channel_list;
BlockHeap *channel_heap;
BlockHeap *ban_heap;
BlockHeap *topic_heap;
static BlockHeap *member_heap;

static void destroy_channel(struct Channel *);

static void send_mode_list(struct Client *client_p, char *chname, dlink_list * top, char flag);
static int check_banned(struct Channel *chptr, struct Client *who, char *s, char *s2);

static char buf[BUFSIZE];
static char modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];

/* 
 * init_channels
 *
 * Initializes the channel blockheap
 */
void
init_channels(void)
{
	channel_heap = BlockHeapCreate(sizeof(struct Channel), CHANNEL_HEAP_SIZE);
	ban_heap = BlockHeapCreate(sizeof(struct Ban), BAN_HEAP_SIZE);
	topic_heap = BlockHeapCreate(TOPICLEN + 1 + USERHOST_REPLYLEN, TOPIC_HEAP_SIZE);
	member_heap = BlockHeapCreate(sizeof(struct membership), MEMBER_HEAP_SIZE);
}

struct membership *
find_channel_membership(struct Channel *chptr, struct Client *client_p)
{
	struct membership *msptr;
	dlink_node *ptr;

	if(!IsClient(client_p))
		return NULL;

	DLINK_FOREACH(ptr, client_p->user->channel.head)
	{
		msptr = ptr->data;
		if(msptr->chptr == chptr)
			return msptr;
	}

	return NULL;
}

const char *
find_channel_status(struct membership *msptr, int combine)
{
	static char buffer[3];
	char *p;

	p = buffer;

	if(is_chanop(msptr))
	{
		if(!combine)
			return "@";
		*p++ = '@';
	}

	if(is_voiced(msptr))
		*p++ = '+';

	*p = '\0';
	return buffer;
}

void
add_user_to_channel(struct Channel *chptr, struct Client *client_p, int flags)
{
	struct membership *msptr;

	if(client_p->user == NULL)
		return;

	msptr = BlockHeapAlloc(member_heap);
	memset(msptr, 0, sizeof(struct membership));

	msptr->chptr = chptr;
	msptr->client_p = client_p;
	msptr->flags = flags;

	dlinkAdd(msptr, &msptr->usernode, &client_p->user->channel);
	dlinkAdd(msptr, &msptr->channode, &chptr->members);

	if(MyClient(client_p))
		dlinkAdd(msptr, &msptr->locchannode, &chptr->locmembers);
}


/*
 * remove_user_from_channel
 * 
 * inputs       - pointer to channel to remove client from
 *              - pointer to client (who) to remove
 * output       - did the channel get destroyed
 * side effects - deletes an user from a channel by removing a link in the
 *                channels member chain.
 */
int
remove_user_from_channel(struct membership *msptr)
{
	struct Client *client_p;
	struct Channel *chptr;

	if(msptr == NULL)
		return 0;

	client_p = msptr->client_p;
	chptr = msptr->chptr;

	dlinkDelete(&msptr->usernode, &client_p->user->channel);
	dlinkDelete(&msptr->channode, &chptr->members);

	if(client_p->servptr == &me)
		dlinkDelete(&msptr->locchannode, &chptr->locmembers);

	chptr->users_last = CurrentTime;

	BlockHeapFree(member_heap, msptr);

	if(dlink_list_length(&chptr->members) <= 0)
	{
		destroy_channel(chptr);
		return 1;
	}

	return 0;
}

/* remove_user_from_channels()
 *
 * input	- user to remove from all channels
 * output	-
 * side effects - user is removed from all channels
 */
void
remove_user_from_channels(struct Client *client_p)
{
	struct Channel *chptr;
	struct membership *msptr;
	dlink_node *ptr;
	dlink_node *next_ptr;

	if(client_p == NULL)
		return;

	DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->user->channel.head)
	{
		msptr = ptr->data;
		chptr = msptr->chptr;

		dlinkDelete(&msptr->channode, &chptr->members);

		if(client_p->servptr == &me)
			dlinkDelete(&msptr->locchannode, &chptr->locmembers);
		
		chptr->users_last = CurrentTime;

		if(dlink_list_length(&chptr->members) <= 0)
			destroy_channel(chptr);

		BlockHeapFree(member_heap, msptr);
	}

	client_p->user->channel.head = client_p->user->channel.tail = NULL;
	client_p->user->channel.length = 0;
}

/* qs_user_from_channels()
 *
 * inputs       - user to remove from all channels
 * outputs      -
 * side effects - user is removed from channel, made persisting if last
 *                user to leave.
 */
void
qs_user_from_channels(struct Client *client_p)
{
	struct Channel *chptr;
	struct membership *msptr;
	dlink_node *ptr;
	dlink_node *next_ptr;

	if(client_p == NULL)
		return;

	DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->user->channel.head)
	{
		msptr = ptr->data;
		chptr = msptr->chptr;

		/* note, a QS can never be done for our own users */
		dlinkDelete(&msptr->channode, &chptr->members);

		chptr->users_last = CurrentTime;

		if(dlink_list_length(&chptr->members) <= 0)
		{
			/* persistent channel - must be 12h old */
			if(!ConfigChannel.persist_time ||
					((chptr->channelts + (60 * 60 * 12)) > CurrentTime))
				destroy_channel(chptr);
		}

		BlockHeapFree(member_heap, msptr);
	}

	client_p->user->channel.head = client_p->user->channel.tail = NULL;
	client_p->user->channel.length = 0;
}

/*
 * inputs       -
 * output       - NONE
 * side effects -
 */
static void
send_members(struct Channel *chptr, struct Client *client_p,
	     const char *lmodebuf, const char *lparabuf)
{
	struct membership *msptr;
	dlink_node *ptr;
	int tlen;		/* length of t (temp pointer) */
	int mlen;		/* minimum length */
	int cur_len = 0;	/* current length */
	char *t;		/* temp char pointer */

	if(chptr->members.head == NULL)
		return;
	
	cur_len = mlen = ircsprintf(buf, ":%s SJOIN %lu %s %s %s:", me.name,
				    (unsigned long) chptr->channelts,
				    chptr->chname, lmodebuf, lparabuf);

	t = buf + mlen;

	DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;

		tlen = strlen(msptr->client_p->name) + 1;
		if(is_chanop(msptr))
			tlen++;
		if(is_voiced(msptr))
			tlen++;

		if(cur_len + tlen >= BUFSIZE - 4)
		{
			t--;
			*t = '\0';
			sendto_one(client_p, "%s", buf);
			cur_len = mlen;
			t = buf + mlen;
		}

		ircsprintf(t, "%s%s ", find_channel_status(msptr, 1),
			   msptr->client_p->name);

		cur_len += tlen;
		t += tlen;
	}

	t--;
	*t = '\0';
	sendto_one(client_p, "%s", buf);
}

/*
 * send_channel_modes
 *
 * inputs       - pointer to client client_p
 *              - pointer to channel pointer
 * output       - NONE
 * side effects - send "client_p" a full list of the modes for channel chptr.
 */
void
send_channel_modes(struct Client *client_p, struct Channel *chptr)
{
	if(*chptr->chname != '#')
		return;

	*modebuf = *parabuf = '\0';
	channel_modes(chptr, client_p, modebuf, parabuf);
	send_members(chptr, client_p, modebuf, parabuf);

	send_mode_list(client_p, chptr->chname, &chptr->banlist, 'b');

	if(IsCapable(client_p, CAP_EX))
		send_mode_list(client_p, chptr->chname, &chptr->exceptlist, 'e');

	if(IsCapable(client_p, CAP_IE))
		send_mode_list(client_p, chptr->chname, &chptr->invexlist, 'I');
}

/*
 * send_mode_list
 * inputs       - client pointer to server
 *              - pointer to channel
 *              - pointer to top of mode link list to send
 *              - char flag flagging type of mode i.e. 'b' 'e' etc.
 * output       - NONE
 * side effects - sends +b/+e/+I
 *
 */
static void
send_mode_list(struct Client *client_p, char *chname, dlink_list * top, char flag)
{
	dlink_node *lp;
	struct Ban *banptr;
	char mbuf[MODEBUFLEN];
	char pbuf[BUFSIZE];
	int tlen;
	int mlen;
	int cur_len;
	char *mp;
	char *pp;
	int count = 0;

	mlen = ircsprintf(buf, ":%s MODE %s +", me.name, chname);
	cur_len = mlen;

	mp = mbuf;
	pp = pbuf;

	DLINK_FOREACH(lp, top->head)
	{
		banptr = lp->data;
		tlen = strlen(banptr->banstr) + 3;

		/* uh oh */
		if(tlen > MODEBUFLEN)
			continue;

		if((count >= MAXMODEPARAMS) || ((cur_len + tlen + 2) > (BUFSIZE - 3)))
		{
			sendto_one(client_p, "%s%s %s", buf, mbuf, pbuf);

			mp = mbuf;
			pp = pbuf;
			cur_len = mlen;
			count = 0;
		}

		*mp++ = flag;
		*mp = '\0';
		pp += ircsprintf(pp, "%s ", banptr->banstr);
		cur_len += tlen;
		count++;
	}

	if(count != 0)
		sendto_one(client_p, "%s%s %s", buf, mbuf, pbuf);
}


/*
 * check_channel_name
 * inputs       - channel name
 * output       - true (1) if name ok, false (0) otherwise
 * side effects - check_channel_name - check channel name for
 *                invalid characters
 */
int
check_channel_name(const char *name)
{
	s_assert(name != NULL);
	if(name == NULL)
		return 0;

	for (; *name; ++name)
	{
		if(!IsChanChar(*name))
			return 0;
	}

	return 1;
}

/*
 * free_channel_list
 *
 * inputs       - pointer to dlink_list
 * output       - NONE
 * side effects -
 */
void
free_channel_list(dlink_list * list)
{
	dlink_node *ptr;
	dlink_node *next_ptr;
	struct Ban *actualBan;

	DLINK_FOREACH_SAFE(ptr, next_ptr, list->head)
	{
		actualBan = ptr->data;
		MyFree(actualBan->banstr);
		MyFree(actualBan->who);
		BlockHeapFree(ban_heap, actualBan);

		free_dlink_node(ptr);
	}

	list->head = list->tail = NULL;
	list->length = 0;
}

/*
 * cleanup_channels
 *
 * inputs       - not used
 * output       - none
 * side effects - persistent channels... 
 */
void
cleanup_channels(void *unused)
{
	struct Channel *chptr;
	dlink_node *ptr, *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, global_channel_list.head)
	{
		chptr = ptr->data;
		if(dlink_list_length(&chptr->members) == 0)
		{
			if((chptr->users_last + ConfigChannel.persist_time) < CurrentTime)
			{
				destroy_channel(chptr);
			}
		}
	}
}

/*
 * destroy_channel
 * inputs       - channel pointer
 * output       - none
 * side effects - walk through this channel, and destroy it.
 */

static void
destroy_channel(struct Channel *chptr)
{
	dlink_node *ptr, *next;

	DLINK_FOREACH_SAFE(ptr, next, chptr->invites.head)
	{
		del_invite(chptr, ptr->data);
	}
	/* free all bans/exceptions/denies */
	free_channel_list(&chptr->banlist);
	free_channel_list(&chptr->exceptlist);
	free_channel_list(&chptr->invexlist);

	/* Free the topic */
	free_topic(chptr);

	dlinkDelete(&chptr->node, &global_channel_list);

	del_from_channel_hash_table(chptr->chname, chptr);
	BlockHeapFree(channel_heap, chptr);
	Count.chan--;
}

/*
 * channel_member_names
 *
 * inputs       - pointer to client struct requesting names
 *              - pointer to channel block
 *              - pointer to name of channel
 *              - show ENDOFNAMES numeric or not
 *                (don't want it with /names with no params)
 * output       - none
 * side effects - lists all names on given channel
 */
void
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

			if(cur_len + tlen >= BUFSIZE - 4)
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
 * channel_pub_or_secret
 *
 * inputs       - pointer to channel
 * output       - string pointer "=" if public, "@" if secret else "*"
 * side effects - NONE
 */
const char *
channel_pub_or_secret(struct Channel *chptr)
{
	if(PubChannel(chptr))
		return ("=");
	else if(SecretChannel(chptr))
		return ("@");
	return ("*");
}

/*
 * add_invite
 *
 * inputs       - pointer to channel block
 *              - pointer to client to add invite to
 * output       - none
 * side effects - adds client to invite list
 *
 * This one is ONLY used by m_invite.c
 */
void
add_invite(struct Channel *chptr, struct Client *who)
{
	dlink_node *ptr;

	DLINK_FOREACH(ptr, who->user->invited.head)
	{
		if(ptr->data == chptr)
			return;
	}

	if((int)dlink_list_length(&who->user->invited) >= ConfigChannel.max_chans_per_user)
	{
		ptr = who->user->invited.tail;
		del_invite(ptr->data, who);
	}

	/*
	 * add client to channel invite list
	 */
	dlinkAddAlloc(who, &chptr->invites);

	/*
	 * add channel to the end of the client invite list
	 */
	dlinkAddAlloc(chptr, &who->user->invited);
}

/*
 * del_invite
 *
 * inputs       - pointer to dlink_list
 *              - pointer to client to remove invites from
 * output       - none
 * side effects - Delete Invite block from channel invite list
 *                and client invite list
 *
 */
void
del_invite(struct Channel *chptr, struct Client *who)
{
	dlinkFindDestroy(&chptr->invites, who);
	dlinkFindDestroy(&who->user->invited, chptr);
}

/*
 * is_banned
 *
 * inputs       - pointer to channel block
 *              - pointer to client to check access fo
 * output       - returns an int 0 if not banned,
 *                CHFL_BAN if banned
 *
 * IP_BAN_ALL from comstud
 * always on...
 *
 * +e code from orabidoo
 */
int
is_banned(struct Channel *chptr, struct Client *who)
{
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];

	if(!IsPerson(who))
		return (0);

	ircsprintf(src_host, "%s!%s@%s", who->name, who->username, who->host);
	ircsprintf(src_iphost, "%s!%s@%s", who->name, who->username, who->localClient->sockhost);

	return (check_banned(chptr, who, src_host, src_iphost));
}

/*
 * check_banned
 *
 * inputs       - pointer to channel block
 *              - pointer to client to check access fo
 *              - pointer to pre-formed nick!user@host
 *              - pointer to pre-formed nick!user@ip
 * output       - returns an int 0 if not banned,
 *                CHFL_BAN if banned
 *
 * IP_BAN_ALL from comstud
 * always on...
 *
 * +e code from orabidoo
 */
static int
check_banned(struct Channel *chptr, struct Client *who, char *s, char *s2)
{
	dlink_node *ban;
	dlink_node *except;
	struct Ban *actualBan = NULL;
	struct Ban *actualExcept = NULL;

	DLINK_FOREACH(ban, chptr->banlist.head)
	{
		actualBan = ban->data;
		if(match(actualBan->banstr, s) ||
		   match(actualBan->banstr, s2) || match_cidr(actualBan->banstr, s2))
			break;
		else
			actualBan = NULL;
	}

	if((actualBan != NULL) && ConfigChannel.use_except)
	{
		DLINK_FOREACH(except, chptr->exceptlist.head)
		{
			actualExcept = except->data;

			if(match(actualExcept->banstr, s) ||
			   match(actualExcept->banstr, s2) || match_cidr(actualExcept->banstr, s2))
			{
				return CHFL_EXCEPTION;
			}
		}
	}

	return ((actualBan ? CHFL_BAN : 0));
}

/* small series of "helper" functions */

/*
 * can_join
 *
 * inputs       -
 * output       -
 * side effects - NONE
 */
int
can_join(struct Client *source_p, struct Channel *chptr, char *key)
{
	dlink_node *lp;
	dlink_node *ptr;
	struct Ban *invex = NULL;
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];

	s_assert(source_p->localClient != NULL);

	ircsprintf(src_host, "%s!%s@%s", source_p->name, source_p->username, source_p->host);
	ircsprintf(src_iphost, "%s!%s@%s", source_p->name, source_p->username,
		   source_p->localClient->sockhost);

	if((check_banned(chptr, source_p, src_host, src_iphost)) == CHFL_BAN)
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

	if(*chptr->mode.key && (BadPtr(key) || irccmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	if(chptr->mode.limit && 
	   dlink_list_length(&chptr->members) >= chptr->mode.limit)
		return (ERR_CHANNELISFULL);

	return 0;
}

/*
 * can_send
 *
 * inputs       - pointer to channel
 *              - pointer to client
 * outputs      - CAN_SEND_OPV if op or voiced on channel
 *              - CAN_SEND_NONOP if can send to channel but is not an op
 *                CAN_SEND_NO if they cannot send to channel
 *                Just means they can send to channel.
 * side effects - NONE
 */
int
can_send(struct Channel *chptr, struct Client *source_p,
	 struct membership *msptr)
{
	if(IsServer(source_p))
		return CAN_SEND_OPV;

	if(MyClient(source_p) && find_channel_resv(chptr->chname) &&
	   (!IsOper(source_p) || !ConfigChannel.no_oper_resvs))
		return CAN_SEND_NO;

	if(msptr == NULL)
	{
		msptr = find_channel_membership(chptr, source_p);

		if(msptr == NULL)
		{
			if(chptr->mode.mode & MODE_NOPRIVMSGS)
				return CAN_SEND_NO;
		}
	}

	if(is_chanop_voiced(msptr))
		return CAN_SEND_OPV;

	if(chptr->mode.mode & MODE_MODERATED)
		return CAN_SEND_NO;

	if(ConfigChannel.quiet_on_ban && MyClient(source_p) &&
	   (is_banned(chptr, source_p) == CHFL_BAN))
	{
		return (CAN_SEND_NO);
	}

	return CAN_SEND_NONOP;
}

/* void check_spambot_warning(struct Client *source_p)
 * Input: Client to check, channel name or NULL if this is a part.
 * Output: none
 * Side-effects: Updates the client's oper_warn_count_down, warns the
 *    IRC operators if necessary, and updates join_leave_countdown as
 *    needed.
 */
void
check_spambot_warning(struct Client *source_p, const char *name)
{
	int t_delta;
	int decrement_count;
	if((GlobalSetOptions.spam_num &&
	    (source_p->localClient->join_leave_count >= GlobalSetOptions.spam_num)))
	{
		if(source_p->localClient->oper_warn_count_down > 0)
			source_p->localClient->oper_warn_count_down--;
		else
			source_p->localClient->oper_warn_count_down = 0;
		if(source_p->localClient->oper_warn_count_down == 0)
		{
			/* Its already known as a possible spambot */
			if(name != NULL)
				sendto_realops_flags(UMODE_BOTS, L_ALL,
						     "User %s (%s@%s) trying to join %s is a possible spambot",
						     source_p->name, source_p->username,
						     source_p->host, name);
			else
				sendto_realops_flags(UMODE_BOTS, L_ALL,
						     "User %s (%s@%s) is a possible spambot",
						     source_p->name, source_p->username,
						     source_p->host);
			source_p->localClient->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
		}
	}
	else
	{
		if((t_delta = (CurrentTime - source_p->localClient->last_leave_time))
		   > JOIN_LEAVE_COUNT_EXPIRE_TIME)
		{
			decrement_count = (t_delta / JOIN_LEAVE_COUNT_EXPIRE_TIME);
			if(decrement_count > source_p->localClient->join_leave_count)
				source_p->localClient->join_leave_count = 0;
			else
				source_p->localClient->join_leave_count -= decrement_count;
		}
		else
		{
			if((CurrentTime - (source_p->localClient->last_join_time)) <
			   GlobalSetOptions.spam_time)
			{
				/* oh, its a possible spambot */
				source_p->localClient->join_leave_count++;
			}
		}
		if(name != NULL)
			source_p->localClient->last_join_time = CurrentTime;
		else
			source_p->localClient->last_leave_time = CurrentTime;
	}
}

/* finish_splitmode()
 *
 * input	-
 * output	-
 * side effects - splitmode is finished
 */
static void
finish_splitmode(void *unused)
{
	splitmode = 0;

	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "Network rejoined, deactivating splitmode");
}

/* check_splitmode()
 *
 * input	-
 * output	-
 * side effects - compares usercount and servercount against their split
 *                values and adjusts splitmode accordingly
 */
void
check_splitmode(void *unused)
{
	if(splitchecking && (ConfigChannel.no_join_on_split || ConfigChannel.no_create_on_split))
	{
		if(!splitmode && ((Count.server < split_servers) || 
		   (Count.total < split_users)))
		{
			splitmode = 1;

			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "Network split, activating splitmode");
			eventAddIsh("check_splitmode", check_splitmode, NULL, 10);
		}
		else if(splitmode && (Count.server >= split_servers) &&
			(Count.total >= split_users))
		{
			/* splitmode ended, if we're delaying the
			 * endofsplit, add an event, else finish it
			 */
			if(GlobalSetOptions.split_delay > 0)
				eventAddOnce("finish_splitmode", finish_splitmode,
					     NULL, GlobalSetOptions.split_delay);
			else
				finish_splitmode(NULL);

			eventDelete(check_splitmode, NULL);
		}
	}
}

/*
 * input	- Channel to allocate a new topic for
 * output	- Success or failure
 * side effects - Allocates a new topic
 */

int
allocate_topic(struct Channel *chptr)
{
	void *ptr;
	if(chptr == NULL)
		return FALSE;

	ptr = BlockHeapAlloc(topic_heap);
	/* Basically we allocate one large block for the topic and
	 * the topic info.  We then split it up into two and shove it
	 * in the chptr 
	 */
	chptr->topic = ptr;
	chptr->topic_info = (char *) ptr + TOPICLEN + 1;
	*chptr->topic = '\0';
	*chptr->topic_info = '\0';
	return TRUE;

}

void
free_topic(struct Channel *chptr)
{
	void *ptr;

	if(chptr == NULL)
		return;
	if(chptr->topic == NULL)
		return;
	/* This is safe for now - If you change allocate_topic you
	 * MUST change this as well
	 */
	ptr = chptr->topic;
	BlockHeapFree(topic_heap, ptr);
	chptr->topic = NULL;
	chptr->topic_info = NULL;
}

/*
 * set_channel_topic - Sets the channel topic
 */
void
set_channel_topic(struct Channel *chptr, const char *topic, const char *topic_info, time_t topicts)
{
	if(strlen(topic) > 0)
	{
		if(chptr->topic == NULL)
			allocate_topic(chptr);
		strlcpy(chptr->topic, topic, TOPICLEN + 1);
		strlcpy(chptr->topic_info, topic_info, USERHOST_REPLYLEN);
		chptr->topic_time = topicts;
	}
	else
	{
		if(chptr->topic != NULL)
			free_topic(chptr);
		chptr->topic_time = 0;
	}
}
