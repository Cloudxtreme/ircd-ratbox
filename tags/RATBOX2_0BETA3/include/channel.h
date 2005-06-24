/*
 *  ircd-ratbox: A slightly useful ircd.
 *  channel.h: The ircd channel header.
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

#ifndef INCLUDED_channel_h
#define INCLUDED_channel_h
#include "config.h"		/* config settings */
#include "ircd_defs.h"		/* buffer sizes */

struct Client;

/* mode structure for channels */

struct Mode
{
	unsigned int mode;
	int limit;
	char key[KEYLEN];
};

/* channel structure */

struct Channel
{
	dlink_node node;
	struct Mode mode;
	char *topic;
	char *topic_info;
	time_t topic_time;
	time_t users_last;	/* when last user was in channel */
	time_t last_knock;	/* don't allow knock to flood */

	dlink_list members;	/* channel members */
	dlink_list locmembers;	/* local channel members */

	dlink_list invites;
	dlink_list banlist;
	dlink_list exceptlist;
	dlink_list invexlist;

	time_t first_received_message_time;	/* channel flood control */
	int received_number_of_privmsgs;
	int flood_noticed;

	int num_mask;		/* number of bans+exceptions+invite exceptions */
	time_t channelts;
	char chname[CHANNELLEN + 1];
};

struct membership
{
	dlink_node channode;
	dlink_node locchannode;
	dlink_node usernode;

	struct Channel *chptr;
	struct Client *client_p;
	unsigned int flags;
};

extern dlink_list global_channel_list;
void init_channels(void);
void cleanup_channels(void *);

extern int can_send(struct Channel *chptr, struct Client *who, 
		    struct membership *);
extern int is_banned(struct Channel *chptr, struct Client *who);

extern int can_join(struct Client *source_p, struct Channel *chptr, char *key);

extern struct membership *find_channel_membership(struct Channel *, struct Client *);
extern const char *find_channel_status(struct membership *msptr, int combine);
extern void add_user_to_channel(struct Channel *, struct Client *, int flags);
extern int remove_user_from_channel(struct membership *);
extern int qs_user_from_channel(struct Channel *, struct Client *);

extern void free_channel_list(dlink_list *);

extern int check_channel_name(const char *name);

extern void channel_member_names(struct Channel *chptr, struct Client *,
				 int show_eon);
extern const char *channel_pub_or_secret(struct Channel *chptr);

extern void add_invite(struct Channel *chptr, struct Client *who);
extern void del_invite(struct Channel *chptr, struct Client *who);

extern void send_channel_modes(struct Client *, struct Channel *);
extern void channel_modes(struct Channel *chptr, struct Client *who, char *, char *);

extern void check_spambot_warning(struct Client *source_p, const char *name);

extern void check_splitmode(void *);

/*
** Channel Related macros follow
*/

#define HoldChannel(x)          (!(x))
/* channel visible */
#define ShowChannel(v,c)        (PubChannel(c) || IsMember((v),(c)))

#define IsMember(who, chan) ((who && who->user && \
                find_channel_membership(chan, who)) ? 1 : 0)

#define IsChannelName(name) ((name) && (*(name) == '#' || *(name) == '&'))

struct Ban			/* also used for exceptions -orabidoo */
{
	char banstr[NICKLEN+USERLEN+HOSTLEN+6];
	char who[NICKLEN+USERLEN+HOSTLEN+6];
	time_t when;
};

#define CLEANUP_CHANNELS_TIME (30*60)

void set_channel_topic(struct Channel *chptr, const char *topic,
		       const char *topic_info, time_t topicts);
void free_topic(struct Channel *);
int allocate_topic(struct Channel *);

#endif /* INCLUDED_channel_h */
