/* modules/m_services.c
 *   Copyright (C) 2005 Lee Hardy <lee -at- leeh.co.uk>
 *   Copyright (C) 2005 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#include "stdinc.h"

#ifdef ENABLE_SERVICES
#include "tools.h"
#include "send.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "ircd.h"
#include "numeric.h"
#include "memory.h"
#include "s_conf.h"
#include "s_serv.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "sprintf_irc.h"

static int me_su(struct Client *, struct Client *, int, const char **);
static int me_login(struct Client *, struct Client *, int, const char **);

static int h_sent_client_burst(struct Client *);
static int h_server_link(struct Client *);
static int h_svc_whois(struct hook_mfunc_data *hd);

struct Message su_msgtab = {
	"SU", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_su, 2}, mg_ignore}
};
struct Message login_msgtab = {
	"LOGIN", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_login, 2}, mg_ignore}
};

mapi_clist_av1 services_clist[] = { &su_msgtab, &login_msgtab, NULL };
mapi_hfn_list_av1 services_hfnlist[] = {
	{ "sent_client_burst",	(hookfn) h_sent_client_burst },
	{ "server_link",	(hookfn) h_server_link },
	{ "doing_whois",	(hookfn) h_svc_whois },
	{ "doing_whois_global",	(hookfn) h_svc_whois },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(services, NULL, NULL, services_clist, NULL, services_hfnlist, "$Revision$");

static int
me_su(struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	struct Client *target_p;

	if(!(source_p->flags & FLAGS_SERVICE))
		return 0;

	if((target_p = find_client(parv[1])) == NULL)
		return 0;

	/* we only care about all clients if we're a hub */
	if(!IsPerson(target_p) || (!ServerInfo.hub && !MyClient(target_p)))
		return 0;

	if(EmptyString(parv[2]))
		target_p->user->suser[0] = '\0';
	else
		strlcpy(target_p->user->suser, parv[2], sizeof(target_p->user->suser));

	return 0;
}

static int
me_login(struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	if(!IsPerson(source_p) || !ServerInfo.hub)
		return 0;

	/* this command is *only* accepted from bursting servers */
	if(HasSentEob(source_p->servptr))
		return 0;

	strlcpy(source_p->user->suser, parv[1], sizeof(source_p->user->suser));
	return 0;
}

static int
h_sent_client_burst(struct Client *target_p)
{
	struct Client *client_p;
	dlink_node *ptr;

	DLINK_FOREACH(ptr, global_client_list.head)
	{
		client_p = ptr->data;

		if(!IsPerson(client_p))
			continue;
		
		if(EmptyString(client_p->user->suser))
			continue;

		sendto_one(target_p, ":%s ENCAP * LOGIN %s",
				get_id(client_p, target_p), client_p->user->suser);
	}

	return 0;
}

static int
h_server_link(struct Client *target_p)
{
	dlink_node *ptr;

	DLINK_FOREACH(ptr, service_list.head)
	{
		if(!irccmp((const char *) ptr->data, target_p->name))
		{
			target_p->flags |= FLAGS_SERVICE;
			return 0;
		}
	}

	return 0;
}

static int
h_svc_whois(struct hook_mfunc_data *data)
{
	if(!EmptyString(data->client_p->user->suser))
	{
		sendto_one(data->source_p, form_str(RPL_WHOISLOGGEDIN),
				me.name, data->source_p->name,
				data->client_p->name,
				data->client_p->user->suser);
	}

	return 0;
}

#endif
