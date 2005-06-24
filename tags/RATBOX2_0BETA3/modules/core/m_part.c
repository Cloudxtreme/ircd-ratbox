/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_part.c: Parts a user from a channel.
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
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "packet.h"

static int m_part(struct Client *, struct Client *, int, const char **);
void check_spambot_warning(struct Client *source_p, const char *name);

struct Message part_msgtab = {
	"PART", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_part, m_part, m_part}
};

mapi_clist_av1 part_clist[] = { &part_msgtab, NULL };
DECLARE_MODULE_AV1(part, NULL, NULL, part_clist, NULL, NULL, NULL, "$Revision$");

static void part_one_client(struct Client *client_p,
			    struct Client *source_p, char *name, char *reason);


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
	char reason[TOPICLEN + 1];
	char *s = LOCAL_COPY(parv[1]);

	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "PART");
		return 0;
	}

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
		sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL), me.name, source_p->name, name);
		return;
	}

	msptr = find_channel_membership(chptr, source_p);
	if(msptr == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOTONCHANNEL), me.name, source_p->name, name);
		return;
	}

	if(MyConnect(source_p) && !IsOper(source_p))
		check_spambot_warning(source_p, NULL);

	/*
	 *  Remove user from the old channel (if any)
	 *  only allow /part reasons in -m chans
	 */
	if(reason[0] && (is_chanop(msptr) || !MyConnect(source_p) ||
			 ((can_send(chptr, source_p, msptr) > 0 &&
			   (source_p->firsttime + ConfigFileEntry.anti_spam_exit_message_time)
			   < CurrentTime))))
	{
		sendto_server(client_p, chptr, CAP_UID, NOCAPS,
			      ":%s PART %s :%s", ID(source_p), chptr->chname, reason);
		sendto_server(client_p, chptr, NOCAPS, CAP_UID,
			      ":%s PART %s :%s", source_p->name, chptr->chname, reason);

		sendto_channel_local(ALL_MEMBERS,
				     chptr, ":%s!%s@%s PART %s :%s",
				     source_p->name,
				     source_p->username, source_p->host, chptr->chname, reason);
	}
	else
	{
		sendto_server(client_p, chptr, CAP_UID, NOCAPS,
			      ":%s PART %s", ID(source_p), chptr->chname);
		sendto_server(client_p, chptr, NOCAPS, CAP_UID,
			      ":%s PART %s", source_p->name, chptr->chname);

		sendto_channel_local(ALL_MEMBERS,
				     chptr, ":%s!%s@%s PART %s",
				     source_p->name,
				     source_p->username, source_p->host, chptr->chname);
	}
	remove_user_from_channel(msptr);
}
