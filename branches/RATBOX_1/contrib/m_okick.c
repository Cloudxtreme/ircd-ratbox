/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_okick.c: Kicks a user from a channel with much prejudice.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
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
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "modules.h"
#include "parse.h"
#include "hash.h"
#include "packet.h"

static void m_okick(struct Client*, struct Client*, int, char**);

struct Message okick_msgtab = {
	"OKICK", 0, 0, 3, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, m_ignore, m_ignore, m_okick}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&okick_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&okick_msgtab);
}

const char *_version = "$Revision$";
#endif

/*
** m_okick
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = client to kick
**      parv[3] = kick comment
*/
static void m_okick(struct Client *client_p,
                  struct Client *source_p,
                  int parc,
                  char *parv[])
{
	struct Client *who;
	struct Channel *chptr;
	struct membership *msptr;
	static char buf[BUFSIZE];
	int chasing = 0;
	char *comment;
	char *name;
	char *p = (char *)NULL;
	char *user;

	if(EmptyString(parv[2]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "KICK");
		return;
	}

	comment = (BadPtr(parv[3])) ? parv[2] : parv[3];
	if(strlen(comment) > (size_t) TOPICLEN)
		comment[TOPICLEN-1] = '\0';

	*buf = '\0';
	if((p = strchr(parv[1],',')))
		*p = '\0';

	name = parv[1];

	chptr = hash_find_channel(name);
	if(chptr == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
			   me.name, parv[0], name);
		return;
	}


	if((p = strchr(parv[2],',')))
		*p = '\0';

	user = parv[2];

	if((who = find_chasing(source_p, user, &chasing)) == NULL)
		return;

	msptr = find_channel_membership(chptr, who);
	
	if(msptr != NULL)
	{
		sendto_wallops_flags(UMODE_WALLOP, &me, 
				     "OKICK called for [%s] [%s] by %s!%s@%s",
				     parv[1], parv[2], source_p->name, 
				     source_p->username, source_p->host);
		ilog(L_NOTICE, "OKICK called for [%s] [%s] by %s!%s@%s",
		     parv[1], parv[2], source_p->name, 
		     source_p->username, source_p->host);

		if(*chptr->chname != '&')
		{
			sendto_server(NULL, NULL, NOCAPS, NOCAPS, 
				      ":%s WALLOPS :OKICK called for [%s] [%s] by %s!%s@%s",
				      me.name, parv[1], parv[2], source_p->name, 
				      source_p->username, source_p->host);

			sendto_server(&me, chptr, NOCAPS, NOCAPS,
				      ":%s KICK %s %s :%s",
				      me.name, chptr->chname,
				      who->name, comment);
		}

		sendto_channel_local(ALL_MEMBERS, chptr, ":%s KICK %s %s :%s",
				     me.name, chptr->chname, who->name, comment);
		remove_user_from_channel(msptr);
	}
}

