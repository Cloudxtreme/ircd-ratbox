/*
 *  ircd-ratbox: A slightly useful ircd.
 *  spy_whois_notice.c: Sends a notice when someone uses WHOIS.
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
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

int show_whois(struct hook_mfunc_data *);

mapi_hfn_list_av1 whois_hfnlist[] = {
	{"doing_whois", (hookfn) show_whois},
	{NULL, NULL}
};

DECLARE_MODULE_AV1(whois_spy, NULL, NULL, NULL, NULL, whois_hfnlist, "$Revision$");

int
show_whois(struct hook_mfunc_data *data)
{
	if(MyConnect(data->source_p) && MyConnect(data->client_p) &&
	   IsOper(data->client_p) && (data->client_p != data->source_p)
	   && data->client_p->umodes & UMODE_SPY)
	{
		sendto_one(data->client_p,
			   ":%s NOTICE %s :*** Notice -- %s (%s@%s) is doing a whois on you [%s]",
			   me.name, data->client_p->name, data->source_p->name,
			   data->source_p->username, data->source_p->host,
			   data->source_p->user->server);
	}

	return 0;
}
