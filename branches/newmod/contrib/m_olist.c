/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_olist.c: List channels.  olist is an oper only command
 *             that shows channels regardless of modes.  This
 *             is kinda evil, and might be morally wrong, but
 *             somebody will likely need it.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *  Copyright (C) 2004 ircd-ratbox Development Team
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
#include "channel.h"
#include "rpi/client.h"
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
#include "s_newconf.h"
#include "sprintf_irc.h"

#ifndef OPER_SPY
#define OPER_SPY 0x000400
#define IsOperSpy(x) ((x)->flags2 & OPER_SPY)
#endif

static int mo_olist(struct rpi_client *, struct rpi_client *, int parc, const char *parv[]);
static int list_all_channels(struct rpi_client *);


#ifndef STATIC_MODULES

struct Message olist_msgtab = {
	"OLIST", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_olist, 1}}
};

mapi_clist_av1 olist_clist[] = { &olist_msgtab, NULL };

DECLARE_MODULE_AV1(okick, NULL, NULL, olist_clist, NULL, NULL, "$Revision$");

#endif

static int list_all_channels(struct rpi_client *source_p);
static int list_named_channel(struct rpi_client *source_p, char *name);

/*
** mo_olist
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static int
mo_olist(struct rpi_client *client_p, struct rpi_client *source_p, int parc, const char *parv[])
{
	if(!IsOperSpy(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need oper_spy", me->name, source_p->name);
		return 0;
	}

	/* If no arg, do all channels *whee*, else just one channel */
	if(parc < 2 || EmptyString(parv[1]))
	{
		list_all_channels(source_p);
	}
	else
	{
		list_named_channel(source_p, LOCAL_COPY(parv[1]));
	}
	return 0;
}


/*
 * list_all_channels
 * inputs	- pointer to client requesting list
 * output	- 0/1
 * side effects	- list all channels to source_p
 */
static int
list_all_channels(struct rpi_client *source_p)
{
	struct Channel *chptr;
	dlink_node *ptr;
	sendto_one(source_p, form_str(RPL_LISTSTART), me->name, source_p->name);

	DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = (struct Channel *) ptr->data;

		sendto_one(source_p, form_str(RPL_LIST),
			   me->name, source_p->name, chptr->chname,
			   dlink_list_length(&chptr->members),
			   chptr->topic == NULL ? "" : chptr->topic);
	}

	sendto_one(source_p, form_str(RPL_LISTEND), me->name, source_p->name);
	return 0;
}

/*
 * list_named_channel
 * inputs       - pointer to client requesting list
 * output       - 0/1
 * side effects	- list all channels to source_p
 */
static int
list_named_channel(struct rpi_client *source_p, char *name)
{
	struct Channel *chptr;
	char id_and_topic[TOPICLEN + NICKLEN + 6];	/* <!!>, space and null */
	char *p;

	sendto_one(source_p, form_str(RPL_LISTSTART), me->name, source_p->name);

	if((p = strchr(name, ',')))
		*p = '\0';

	if(*name == '\0')
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), name);
		sendto_one(source_p, form_str(RPL_LISTEND), me->name, source_p->name);
		return 0;
	}

	if((chptr = find_channel(name)) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), name);
		sendto_one(source_p, form_str(RPL_LISTEND), me->name, source_p->name);
		return 0;
	}

	ircsprintf(id_and_topic, "%s", chptr->topic == NULL ? "" : chptr->topic);

	sendto_one(source_p, form_str(RPL_LIST), me->name, source_p->name,
		   chptr->chname, chptr->members, id_and_topic);

	sendto_one(source_p, form_str(RPL_LISTEND), me->name, source_p->name);
	return 0;
}
