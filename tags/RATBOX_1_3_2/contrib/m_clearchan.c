/*
 *   IRC - Internet Relay Chat, contrib/m_clearchan.c
 *   Copyright (C) 2002 Hybrid Development Team
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   $Id$
 */
#include "stdinc.h"
#include "tools.h"
#include "handlers.h"
#include "channel.h"
#include "channel_mode.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_log.h"
#include "s_serv.h"
#include "send.h"
#include "whowas.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void mo_clearchan(struct Client *client_p, struct Client *source_p,
                         int parc, char *parv[]);

struct Message clearchan_msgtab = {
	"CLEARCHAN", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, m_ignore, mo_clearchan}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&clearchan_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&clearchan_msgtab);
}

const char *_version = "$Revision$";
#endif

/*
** mo_clearchan
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static void mo_clearchan(struct Client *client_p, struct Client *source_p,
                        int parc, char *parv[])
{
	struct Channel *chptr;
	struct membership *msptr;
	struct Client *target_p;
	dlink_node *ptr;
	dlink_node *next_ptr;

	/* admins only */
	if (!IsOperAdmin(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You have no A flag", 
			   me.name, parv[0]);
		return;
	}

	chptr = hash_find_channel(parv[1]);

	if(chptr == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
			   me.name, parv[0], parv[1]);
		return;
	}

	if(IsMember(source_p, chptr))
	{
		sendto_one(source_p, ":%s NOTICE %s :*** Please part %s before using CLEARCHAN",
			   me.name, source_p->name, parv[1]);
		return;
	}

	/* quickly make everyone a peon.. */
	DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;
		msptr->flags &= ~CHFL_CHANOP|CHFL_VOICE;
	}

	sendto_wallops_flags(UMODE_WALLOP, &me, 
			     "CLEARCHAN called for [%s] by %s!%s@%s",
			     parv[1], source_p->name, source_p->username, source_p->host);
	ilog(L_NOTICE, "CLEARCHAN called for [%s] by %s!%s@%s",
	     parv[1], source_p->name, source_p->username, source_p->host);

	if(*chptr->chname != '&')
	{
		sendto_server(NULL, NULL, NOCAPS, NOCAPS, 
			      ":%s WALLOPS :CLEARCHAN called for [%s] by %s!%s@%s",
			      me.name, parv[1], source_p->name, source_p->username,
			      source_p->host);

		/* SJOIN the user to give them ops, and lock the channel */
		sendto_server(client_p, chptr, NOCAPS, NOCAPS,
				":%s SJOIN %lu %s +ntsi :@%s",
				me.name, (unsigned long) (chptr->channelts - 1),
				chptr->chname, source_p->name);
	}

	sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN %s",
			     source_p->name, source_p->username,
			     source_p->host, chptr->chname);
	sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +o %s",
			     me.name, chptr->chname, source_p->name);

	add_user_to_channel(chptr, source_p, CHFL_CHANOP);

	/* Take the TS down by 1, so we don't see the channel taken over
	 * again. */
	if (chptr->channelts)
		chptr->channelts--;

	chptr->mode.mode = MODE_SECRET|MODE_TOPICLIMIT|MODE_INVITEONLY|MODE_NOPRIVMSGS;
	free_topic(chptr);
	chptr->mode.key[0] = '\0';

	DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->members.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		/* skip the person we just added.. */
		if(is_chanop(msptr))
			continue;

		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s KICK %s %s :CLEARCHAN",
				     source_p->name, chptr->chname, 
				     target_p->name);

		if(*chptr->chname != '&')
			sendto_server(NULL, chptr, NOCAPS, NOCAPS, 
				      ":%s KICK %s %s :CLEARCHAN", 
				      source_p->name, chptr->chname, 
				      target_p->name);

		remove_user_from_channel(msptr);
	}

	/* Join the user themselves to the channel down here, so they dont see a nicklist 
	 * or people being kicked */
	sendto_one(source_p, ":%s!%s@%s JOIN %s",
		   source_p->name, source_p->username,
		   source_p->host, chptr->chname);

	channel_member_names(chptr, source_p, 1);
}

