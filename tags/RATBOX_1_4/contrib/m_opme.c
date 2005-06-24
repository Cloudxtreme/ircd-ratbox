/*   contrib/m_opme.c
 *   Copyright (C) 2002 Hybrid Development Team
 *   Copyright (C) 2003-2004 ircd-ratbox development team
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
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void mo_opme(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);

struct Message opme_msgtab = {
	"OPME", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, m_ignore, mo_opme}
};

void
_modinit(void)
{
	mod_add_cmd(&opme_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&opme_msgtab);
}

const char *_version = "$Revision$";

/*
** mo_opme
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static void mo_opme(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
	struct Channel *chptr;
	struct membership *msptr;
	dlink_node *ptr;
  
	/* admins only */
	if (!IsAdmin(source_p))
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

	DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;

		if(is_chanop(msptr))
		{
			sendto_one(source_p, ":%s NOTICE %s :%s Channel is not opless",
					me.name, parv[0], parv[1]);
			return;
		}
	}

	msptr = find_channel_membership(chptr, source_p);

	if(msptr == NULL)
		return;

	msptr->flags |= CHFL_CHANOP;

	sendto_wallops_flags(UMODE_WALLOP, &me,
			     "OPME called for [%s] by %s!%s@%s",
			     parv[1], source_p->name, source_p->username,
			     source_p->host);
	ilog(L_NOTICE, "OPME called for [%s] by %s!%s@%s",
	     parv[1], source_p->name, source_p->username, source_p->host);

	/* dont send stuff for local channels remotely. */
	if(*chptr->chname != '&')
	{
		sendto_server(NULL, NULL, NOCAPS, NOCAPS, 
			      ":%s WALLOPS :OPME called for [%s] by %s!%s@%s",
			      me.name, parv[1], source_p->name, source_p->username,
			      source_p->host);
		sendto_server(NULL, chptr, NOCAPS, NOCAPS, 
			      ":%s PART %s", source_p->name, parv[1]);
		sendto_server(NULL, chptr, NOCAPS, NOCAPS, 
			      ":%s SJOIN %ld %s + :@%s",
			      me.name, (signed long) chptr->channelts,
			      parv[1], source_p->name);
	}

	sendto_channel_local(ALL_MEMBERS, chptr,
			     ":%s MODE %s +o %s",
			     me.name, parv[1], source_p->name);
}
