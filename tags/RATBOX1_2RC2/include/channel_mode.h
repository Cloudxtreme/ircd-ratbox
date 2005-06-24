/*
 *  ircd-ratbox: A slightly useful ircd.
 *  channel_mode.h: The ircd channel mode header.
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


#ifndef INCLUDED_channel_mode_h
#define INCLUDED_channel_mode_h
#include "config.h"		/* config settings */
#include "ircd_defs.h"		/* buffer sizes */

#define MODEBUFLEN      200

/* Maximum mode changes allowed per client, per server is different */
#define MAXMODEPARAMS   4

struct membership;

extern void set_channel_mode(struct Client *, struct Client *,
			     struct Channel *, struct membership *,
			     int, char **, char *);

extern void init_chcap_usage_counts(void);
extern void set_chcap_usage_counts(struct Client *serv_p);
extern void unset_chcap_usage_counts(struct Client *serv_p);

/*
** Channel Related macros follow
*/

/* can_send results */
#define CAN_SEND_NO	0
#define CAN_SEND_NONOP  1
#define CAN_SEND_OPV	2


/* channel status flags */
#define CHFL_PEON		0x0000	/* normal member of channel */
#define CHFL_CHANOP     	0x0001	/* Channel operator */
#define CHFL_VOICE      	0x0002	/* the power to speak */
#define CHFL_DEOPPED    	0x0004	/* deopped by us, bounce modes */
/* channel bantype flags */
#define CHFL_BAN        	0x0010	/* ban channel flag */
#define CHFL_EXCEPTION  	0x0020	/* exception to ban channel flag */
#define CHFL_INVEX      	0x0080	/* exception to invite-only */
/* channel send flags */
#define ONLY_SERVERS		0x0100	  /* send to servers only */
#define ALL_MEMBERS		CHFL_PEON /* send to all members */
#define ONLY_CHANOPS		CHFL_CHANOP
#define ONLY_CHANOPSVOICED	(CHFL_CHANOP|CHFL_VOICE)

#define is_chanop(x)   ((x) && (x)->flags & CHFL_CHANOP)
#define is_voiced(x)   ((x) && (x)->flags & CHFL_VOICE)
#define is_chanop_voiced(x) ((x) && (x)->flags & (CHFL_CHANOP|CHFL_VOICE))
#define is_deop(x)    ((x) && (x)->flags & CHFL_DEOPPED)

/* channel modes ONLY */
#define MODE_PRIVATE    0x0008
#define MODE_SECRET     0x0010
#define MODE_MODERATED  0x0020
#define MODE_TOPICLIMIT 0x0040
#define MODE_INVITEONLY 0x0080
#define MODE_NOPRIVMSGS 0x0100
#define MODE_BAN        0x0400
#define MODE_EXCEPTION  0x0800
#define MODE_INVEX	0x2000
#define MODE_HIDEOPS    0x4000

/*
 * mode flags which take another parameter (With PARAmeterS)
 */

#define MODE_QUERY     0
#define MODE_ADD       1
#define MODE_DEL       -1

/* name invisible */
#define SecretChannel(x)        ((x) && ((x)->mode.mode & MODE_SECRET))
/* channel not shown but names are */
#define HiddenChannel(x)        ((x) && ((x)->mode.mode & MODE_PRIVATE))
#define PubChannel(x)           ((!x) || ((x)->mode.mode &\
                                 (MODE_PRIVATE | MODE_SECRET)) == 0)
#define ParanoidChannel(x)	((x) && ((x)->mode.mode &\
			        (MODE_PRIVATE|MODE_INVITEONLY))==\
		                (MODE_PRIVATE|MODE_INVITEONLY))

struct ChModeChange
{
	char letter;
	const char *arg;
	char *id;
	int dir;
	int caps;
	int nocaps;
	int mems;
	struct Client *client;
};

struct ChCapCombo
{
	int count;
	int cap_yes;
	int cap_no;
};

#endif /* INCLUDED_channel_mode_h */
