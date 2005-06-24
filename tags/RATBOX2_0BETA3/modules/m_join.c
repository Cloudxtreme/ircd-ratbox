/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_join.c: Joins a channel.
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
#include "channel.h"
#include "channel_mode.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_serv.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "sprintf_irc.h"
#include "packet.h"

static int m_join(struct Client *, struct Client *, int, const char **);
static int ms_join(struct Client *, struct Client *, int, const char **);

struct Message join_msgtab = {
	"JOIN", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_join, ms_join, m_join}
};

mapi_clist_av1 join_clist[] = { &join_msgtab, NULL };
DECLARE_MODULE_AV1(join, NULL, NULL, join_clist, NULL, NULL, NULL, "$Revision$");

static void do_join_0(struct Client *client_p, struct Client *source_p);
void check_spambot_warning(struct Client *source_p, const char *name);


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
	char *name;
	char *key = NULL;
	int i, flags = 0;
	char *p = NULL, *p2 = NULL;
	char *chanlist;
	char *mykey;
	int successful_join_count = 0;	/* Number of channels successfully joined */

	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "JOIN");
		return 0;
	}

	jbuf[0] = '\0';

	/* rebuild the list of channels theyre supposed to be joining.
	 * this code has a side effect of losing keys, but..
	 */
	chanlist = LOCAL_COPY(parv[1]);
	for(i = 0, name = strtoken(&p, chanlist, ","); name;
	    name = strtoken(&p, NULL, ","))
	{

		/* check the length and name of channel is ok */
		if(!check_channel_name(name) || (strlen(name) > CHANNELLEN))
		{
			sendto_one(source_p, form_str(ERR_BADCHANNAME),
				   me.name, source_p->name, (unsigned char *) name);
			continue;
		}

		/* join 0 parts all channels */
		if(*name == '0' && !atoi(name))
		{
			(void) strcpy(jbuf, "0");
			continue;
		}

		/* check it begins with # or &, and local chans are disabled */
		else if(!IsChannelName(name) ||
			(ConfigServerHide.disable_local_channels && (*name == '&')))
		{
			sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
				   me.name, source_p->name, name);
			continue;
		}

		/* see if its resv'd */
		if(find_channel_resv(name) && (!IsOper(source_p) || !ConfigChannel.no_oper_resvs))
		{
			sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
				   me.name, source_p->name, name);
			sendto_realops_flags(UMODE_SPY, L_ALL,
					     "User %s (%s@%s) is attempting to join locally juped channel %s",
					     source_p->name, source_p->username, source_p->host,
					     name);
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
					ConfigChannel.max_chans_per_user) &&
		   (!IsOper(source_p) || 
		    (dlink_list_length(&source_p->user->channel) >=
				 ConfigChannel.max_chans_per_user * 3)))
		{
			sendto_one(source_p, form_str(ERR_TOOMANYCHANNELS), me.name, parv[0], name);
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
				sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE), me.name, parv[0], name);
				if(successful_join_count > 0)
					successful_join_count--;
				continue;
			}
		}

		if(!IsOper(source_p))
			check_spambot_warning(source_p, name);

		/* can_join checks for +i key, bans etc */
		if((i = can_join(source_p, chptr, key)))
		{
			sendto_one(source_p, form_str(i), me.name, parv[0], name);
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

		/* if theyre joining opped (ie, new chan or joining one thats
		 * persisting) then set timestamp to current, set +nt and
		 * broadcast the sjoin with its old modes, or +nt.
		 */
		if(flags & CHFL_CHANOP)
		{
			char mbuf[MODEBUFLEN];
			char pbuf[MODEBUFLEN];

			chptr->channelts = CurrentTime;
			chptr->mode.mode |= MODE_TOPICLIMIT;
			chptr->mode.mode |= MODE_NOPRIVMSGS;

			sendto_channel_local(ONLY_CHANOPS, chptr, ":%s MODE %s +nt",
					     me.name, chptr->chname);

			if(*chptr->chname == '#')
			{
				channel_modes(chptr, source_p, mbuf, pbuf);

				strlcat(mbuf, " ", sizeof(mbuf));

				if(pbuf[0] != '\0')
					strlcat(mbuf, pbuf, sizeof(mbuf));

				/* note: mbuf here will have a trailing space.  we add one above,
				 * and channel_modes() will leave a trailing space on pbuf if
				 * its used --fl
				 */
				sendto_server(client_p, chptr, NOCAPS, NOCAPS,
					      ":%s SJOIN %lu %s %s:@%s",
					      me.name, (unsigned long) chptr->channelts,
					      chptr->chname, mbuf, parv[0]);
			}

			/* drop our +beI modes */
			free_channel_list(&chptr->banlist);
			free_channel_list(&chptr->exceptlist);
			free_channel_list(&chptr->invexlist);
			chptr->num_mask = 0;
		}
		else
		{
			sendto_server(client_p, chptr, NOCAPS, NOCAPS,
				      ":%s SJOIN %lu %s + :%s",
				      me.name, (unsigned long) chptr->channelts,
				      chptr->chname, parv[0]);
		}

		del_invite(chptr, source_p);

		if(chptr->topic != NULL)
		{
			sendto_one(source_p, form_str(RPL_TOPIC), me.name,
				   parv[0], chptr->chname, chptr->topic);

			sendto_one(source_p, form_str(RPL_TOPICWHOTIME),
				   me.name, parv[0], chptr->chname,
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
	const char *name;
	int new_ts;

	if(!(source_p->user))
		return 0;

	name = parv[1];

	if((name[0] == '0') && (name[1] == '\0'))
	{
		do_join_0(client_p, source_p);
	}
	else
	{
		if(parc > 2)
		{
			new_ts = atoi(parv[2]);
		}
		else
		{
			ts_warn("User on %s remotely JOINing new channel with no TS",
				source_p->user->server);
		}
	}

	return 0;
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

	if(source_p->user->channel.head && MyConnect(source_p) && !IsOper(source_p))
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
