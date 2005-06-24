/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_links.c: Shows what servers are currently connected.
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
#include "sprintf_irc.h"
#include "tools.h"
#include "handlers.h"
#include "channel.h"
#include "channel_mode.h"
#include "client.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "linebuf.h"


static void m_list(struct Client *, struct Client *, int, char **);
static void mo_list(struct Client *, struct Client *, int, char **);
static int list_all_channels(struct Client *);

struct Message list_msgtab = {
	"LIST", 0, 0, 0, 0, MFLG_SLOW, 0,
	{m_unregistered, m_list, m_ignore, mo_list}
};
#ifndef STATIC_MODULES

void
_modinit(void)
{
	mod_add_cmd(&list_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&list_msgtab);
}
const char *_version = "$Revision$";
#endif
static int list_all_channels(struct Client *source_p);
static int list_named_channel(struct Client *source_p, char *name);


/*
** m_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static void
m_list(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	static time_t last_used = 0L;

	/* If we're forwarding this to uplinks.. it should be paced due to the
	 * traffic involved in /list.. -- fl_ */
	if(((last_used + ConfigFileEntry.pace_wait) > CurrentTime))
	{
		sendto_one(source_p, form_str(RPL_LOAD2HI),
			   me.name, parv[0], "LIST");
		sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
		return;
	}
	else
		last_used = CurrentTime;


	/* If no arg, do all channels *whee*, else just one channel */
	if(parc < 2 || BadPtr(parv[1]))
		list_all_channels(source_p);
	else
		list_named_channel(source_p, parv[1]);
}


/*
** mo_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static void
mo_list(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	/* If no arg, do all channels *whee*, else just one channel */
	if(parc < 2 || BadPtr(parv[1]))
		list_all_channels(source_p);
	else
		list_named_channel(source_p, parv[1]);
}


/*
 * list_all_channels
 * inputs	- pointer to client requesting list
 * output	- 0/1
 * side effects	- list all channels to source_p
 */
static int
list_all_channels(struct Client *source_p)
{
	struct Channel *chptr;
	dlink_node *ptr;
	int sendq_limit;

	/* give them an output limit of 90% of their sendq. --fl */
	sendq_limit = (int) get_sendq(source_p);
	sendq_limit /= 10;
	sendq_limit *= 9;

	sendto_one(source_p, form_str(RPL_LISTSTART), me.name, source_p->name);

	DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = ptr->data;

		/* if theyre overflowing their sendq, stop. --fl */
		if(linebuf_len(&source_p->localClient->buf_sendq) > sendq_limit)
		{
			sendto_one(source_p, form_str(ERR_TOOMANYMATCHES),
				   me.name, source_p->name, "LIST");
			break;
		}

		if(SecretChannel(chptr) && !IsMember(source_p, chptr))
			continue;

		sendto_one(source_p, form_str(RPL_LIST),
			   me.name, source_p->name, chptr->chname, 
			   dlink_list_length(&chptr->members), 
			   chptr->topic == NULL ? "" : chptr->topic);
	}

	sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
	return 0;
}

/*
 * list_named_channel
 * inputs       - pointer to client requesting list
 * output       - 0/1
 * side effects	- list all channels to source_p
 */
static int
list_named_channel(struct Client *source_p, char *name)
{
	struct Channel *chptr;
	char *p;

	sendto_one(source_p, form_str(RPL_LISTSTART), me.name, source_p->name);

	if((p = strchr(name, ',')))
		*p = '\0';

	if(*name == '\0')
	{
		sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, source_p->name, name);
		sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
		return 0;
	}

	chptr = hash_find_channel(name);

	if(chptr == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, source_p->name, name);
		sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
		return 0;
	}

	if(ShowChannel(source_p, chptr))
		sendto_one(source_p, form_str(RPL_LIST), 
			   me.name, source_p->name, chptr->chname, 
			   dlink_list_length(&chptr->members),
			   chptr->topic == NULL ? "" : chptr->topic);

	sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
	return 0;
}

