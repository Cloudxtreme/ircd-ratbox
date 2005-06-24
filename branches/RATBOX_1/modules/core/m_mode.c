/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_mode.c: Sets a user or channel mode.
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
#include "handlers.h"
#include "channel.h"
#include "channel_mode.h"
#include "client.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_user.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "sprintf_irc.h"
#include "s_log.h"

static void m_mode(struct Client *, struct Client *, int, char **);

struct Message mode_msgtab = {
	"MODE", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_mode, m_mode, m_ignore, m_mode}
};
#ifndef STATIC_MODULES

void
_modinit(void)
{
	mod_add_cmd(&mode_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&mode_msgtab);
}


const char *_version = "$Revision$";
#endif

static void
loc_channel_modes(struct Channel *chptr, struct Client *client_p, char *mbuf, char *pbuf)
{
	int len;
	*mbuf++ = '+';
	*pbuf = '\0';

	if(chptr->mode.mode & MODE_SECRET)
		*mbuf++ = 's';
	if(chptr->mode.mode & MODE_PRIVATE)
		*mbuf++ = 'p';
	if(chptr->mode.mode & MODE_MODERATED)
		*mbuf++ = 'm';
	if(chptr->mode.mode & MODE_TOPICLIMIT)
		*mbuf++ = 't';
	if(chptr->mode.mode & MODE_INVITEONLY)
		*mbuf++ = 'i';
	if(chptr->mode.mode & MODE_NOPRIVMSGS)
		*mbuf++ = 'n';

	if(chptr->mode.limit)
	{
		*mbuf++ = 'l';
		len = ircsprintf(pbuf, "%d ", chptr->mode.limit);
		pbuf += len;
	}

	if(*chptr->mode.key)
	{
		*mbuf++ = 'k';
		ircsprintf(pbuf, "%s ", chptr->mode.key);
	}

	*mbuf++ = '\0';
}

/*
 * m_mode - MODE command handler
 * parv[0] - sender
 * parv[1] - channel
 */
static void
m_mode(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	struct Channel *chptr = NULL;
	struct membership *msptr;
	static char modebuf[MODEBUFLEN];
	static char parabuf[MODEBUFLEN];
	int n = 2;
	char *my_parv;
	int whois_cheat = 0;

	my_parv = parv[1];

	if(IsOperSpy(source_p) && *my_parv == '!')
	{
		my_parv++;
		whois_cheat = 1;
	}

	if(EmptyString(my_parv))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "MODE");
		return;
	}

	/* Now, try to find the channel in question */
	if(!IsChanPrefix(*my_parv))
	{
		/* if here, it has to be a non-channel name */
		user_mode(client_p, source_p, parc, parv);
		return;
	}

	if(!check_channel_name(my_parv))
	{
		sendto_one(source_p, form_str(ERR_BADCHANNAME),
			   me.name, parv[0], (unsigned char *) parv[1]);
		return;
	}

	chptr = hash_find_channel(my_parv);

	if(chptr == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL), me.name, parv[0], parv[1]);
		return;
	}

	/* Now know the channel exists */
	if(parc < n + 1)
	{
		if(whois_cheat)
		{
			log_operspy(source_p, "MODE", chptr->chname);
			loc_channel_modes(chptr, source_p, modebuf, parabuf);
		}
		else
			channel_modes(chptr, source_p, modebuf, parabuf);

		sendto_one(source_p, form_str(RPL_CHANNELMODEIS),
			   me.name, parv[0], parv[1], modebuf, parabuf);

		sendto_one(source_p, form_str(RPL_CREATIONTIME),
			   me.name, parv[0], parv[1], chptr->channelts);
	}
	else if(IsServer(source_p))
	{
		set_channel_mode(client_p, source_p, chptr, NULL,
				 parc - n, parv + n, chptr->chname);
	}
	else
	{
		msptr = find_channel_membership(chptr, source_p);

		if(is_deop(msptr))
			return;

		/* Finish the flood grace period... */
		if(MyClient(source_p) && !IsFloodDone(source_p))
		{
			if((parc == 3) && (parv[2][0] == 'b') && (parv[2][1] == '\0'))
				;
			else
				flood_endgrace(source_p);
		}

		set_channel_mode(client_p, source_p, chptr, msptr, 
				 parc - n, parv + n, chptr->chname);
	}
}
