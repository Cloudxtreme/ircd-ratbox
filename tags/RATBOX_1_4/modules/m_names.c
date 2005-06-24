/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_names.c: Shows the users who are online.
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


static void names_global(struct Client *source_p);

static void m_names(struct Client *, struct Client *, int, char **);

struct Message names_msgtab = {
	"NAMES", 0, 0, 0, 0, MFLG_SLOW, 0,
	{m_unregistered, m_names, m_ignore, m_names}
};
#ifndef STATIC_MODULES

void
_modinit(void)
{
	mod_add_cmd(&names_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&names_msgtab);
}

const char *_version = "$Revision$";
#endif

/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 ************************************************************************/

/*
** m_names
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = vkey
*/
static void
m_names(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	static time_t last_used = 0;
	struct Channel *chptr = NULL;
	char *s;
	char *para = parc > 1 ? parv[1] : NULL;

	if(!BadPtr(para))
	{
		if((s = strchr(para, ',')))
			*s = '\0';

		if(!check_channel_name(para))
		{
			sendto_one(source_p, form_str(ERR_BADCHANNAME),
				   me.name, parv[0], (unsigned char *) para);
			return;
		}

		if((chptr = hash_find_channel(para)) != NULL)
			channel_member_names(chptr, source_p, 1);
		else
			sendto_one(source_p, form_str(RPL_ENDOFNAMES), me.name, parv[0], para);
	}
	else
	{
		if(!IsOper(source_p))
		{
			if((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
			{
				sendto_one(source_p, form_str(RPL_LOAD2HI),
						me.name, source_p->name, "NAMES");
				sendto_one(source_p, form_str(RPL_ENDOFNAMES), me.name, parv[0], "*");
				return;
			}
			else
				last_used = CurrentTime;
		}

		names_global(source_p);
		sendto_one(source_p, form_str(RPL_ENDOFNAMES), me.name, parv[0], "*");
	}
}

/*
 * names_global
 *
 * inputs       - pointer to client struct requesting names
 * output       - none
 * side effects - lists all non public non secret channels
 */
static void
names_global(struct Client *source_p)
{
	int mlen;
	int tlen;
	int cur_len;
	int dont_show = NO;
	dlink_node *lp, *ptr;
	struct Client *target_p;
	struct Channel *chptr = NULL;
	struct membership *msptr;
	char buf[BUFSIZE];
	char *t;

	/* first do all visible channels */
	DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = (struct Channel *) ptr->data;
		channel_member_names(chptr, source_p, 0);
	}

	cur_len = mlen = ircsprintf(buf, form_str(RPL_NAMREPLY), me.name, source_p->name, "*", "*");
	t = buf + mlen;

	/* Second, do all clients in one big sweep */
	DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = ptr->data;
		dont_show = NO;

		if(!IsPerson(target_p) || IsInvisible(target_p))
			continue;
		/* we want to show -i clients that are either:
		 *   a) not on any channels
		 *   b) only on +p channels
		 *
		 * both were missed out above.  if the target is on a
		 * common channel with source, its already been shown.
		 */
		DLINK_FOREACH(lp, target_p->user->channel.head)
		{
			msptr = lp->data;
			chptr = msptr->chptr;

			if(PubChannel(chptr) || IsMember(source_p, chptr) ||
			   SecretChannel(chptr))
			{
				dont_show = YES;
				break;
			}
		}

		if(dont_show == YES)
			continue;

		if((cur_len + NICKLEN + 2) > (BUFSIZE - 3))
		{
			sendto_one(source_p, "%s", buf);
			cur_len = mlen;
			t = buf + mlen;
		}

		ircsprintf(t, "%s ", target_p->name);

		tlen = strlen(t);
		cur_len += tlen;
		t += tlen;
	}

	if(cur_len > mlen)
		sendto_one(source_p, "%s", buf);
}
