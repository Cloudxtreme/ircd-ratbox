/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_whois.c: Shows who a user is.
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
#include "common.h"
#include "handlers.h"
#include "client.h"
#include "hash.h"
#include "channel.h"
#include "channel_mode.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "hook.h"

static void do_whois(struct Client *client_p, struct Client *source_p, int parc, char *parv[]);
static void single_whois(struct Client *source_p, struct Client *target_p, int);

static void m_whois(struct Client *, struct Client *, int, char **);
static void ms_whois(struct Client *, struct Client *, int, char **);
static void mo_whois(struct Client *, struct Client *, int, char **);

struct Message whois_msgtab = {
	"WHOIS", 0, 0, 0, 0, MFLG_SLOW, 0L,
	{m_unregistered, m_whois, ms_whois, mo_whois}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&whois_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&whois_msgtab);
}

const char *_version = "$Revision$";
#endif
/*
** m_whois
**      parv[0] = sender prefix
**      parv[1] = nickname masklist
*/
static void
m_whois(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	static time_t last_used = 0;

	if(parc < 2 || BadPtr(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN), me.name, parv[0]);
		return;
	}

	if(parc > 2)
	{
		/* seeing as this is going across servers, we should limit it */
		if((last_used + ConfigFileEntry.pace_wait_simple) > CurrentTime)
		{
			sendto_one(source_p, form_str(RPL_LOAD2HI),
				   me.name, source_p->name, "WHOIS");
			return;
		}
		else
			last_used = CurrentTime;

		/* if we have serverhide enabled, they can either ask the clients
		 * server, or our server.. I dont see why they would need to ask
		 * anything else for info about the client.. --fl_
		 */
		if(ConfigServerHide.disable_remote)
			parv[1] = parv[2];

		if(hunt_server(client_p, source_p, ":%s WHOIS %s :%s", 1, parc, parv) !=
		   HUNTED_ISME)
		{
			return;
		}
		parv[1] = parv[2];

	}
	do_whois(client_p, source_p, parc, parv);
}

/*
** mo_whois
**      parv[0] = sender prefix
**      parv[1] = nickname masklist
*/
static void
mo_whois(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if(parc < 2 || BadPtr(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN), me.name, parv[0]);
		return;
	}

	if(parc > 2)
	{
		if(hunt_server(client_p, source_p, ":%s WHOIS %s :%s", 1, parc, parv) !=
		   HUNTED_ISME)
		{
			return;
		}
		parv[1] = parv[2];
	}

	do_whois(client_p, source_p, parc, parv);
}

/*
** ms_whois
**      parv[0] = sender prefix
**      parv[1] = nickname masklist
*/
static void
ms_whois(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	struct Client *target_p;

	/* its a misconfigured server */
	if(parc < 3 || BadPtr(parv[2]))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN), me.name, parv[0]);
		return;
	}

	if(!IsClient(source_p))
		return;

	/* check if parv[1] exists */
	if((target_p = find_client(parv[1])) == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOSUCHSERVER), me.name, parv[0], parv[1]);
		return;
	}

	/* if parv[1] isnt my client, or me, someone else is supposed
	 * to be handling the request.. so send it to them 
	 */
	if(!MyClient(target_p) && !IsMe(target_p))
	{
		sendto_one(target_p->from, ":%s WHOIS %s :%s", parv[0], parv[1], parv[2]);
		return;
	}

	/* ok, the target is either us, or a client on our server, so perform the whois
	 * but first, parv[1] == server to perform the whois on, parv[2] == person
	 * to whois, so make parv[1] = parv[2] so do_whois is ok -- fl_
	 */
	parv[1] = parv[2];
	do_whois(client_p, source_p, parc, parv);
	return;
}

/* do_whois
 *
 * inputs	- pointer to 
 * output	- 
 * side effects -
 */
static void
do_whois(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	struct Client *target_p;
	char *nick;
	char *p = NULL;
	int glob = 0;

	/* This lets us make all "whois nick" queries look the same, and all
	 * "whois nick nick" queries look the same.  We have to pass it all
	 * the way down to whois_person() though -- fl
	 */
	if(parc > 2)
		glob = 1;

	nick = parv[1];
	if((p = strchr(parv[1], ',')))
		*p = '\0';

	(void) collapse(nick);
	if(((target_p = find_client(nick)) != NULL) && IsPerson(target_p))
	{
		/* cant give detailed whois on a remote client */
		if(!MyClient(target_p))
			glob = 0;

		single_whois(source_p, target_p, glob);
	}
	else
		sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, parv[0], nick);

	sendto_one(source_p, form_str(RPL_ENDOFWHOIS), me.name, parv[0], parv[1]);

	return;
}


/*
 * single_whois()
 *
 * Inputs	- source_p client to report to
 *		- target_p client to report on
 * Output	- if found return 1
 * Side Effects	- do a single whois on given client
 * 		  writing results to source_p
 */
static void
single_whois(struct Client *source_p, struct Client *target_p, int glob)
{
	char buf[BUFSIZE];
	const char *name;
	char *server_name;
	dlink_node *lp;
	struct Client *a2client_p;
	struct Channel *chptr;
	struct membership *msptr;
	int cur_len = 0;
	int mlen;
	char *t;
	int tlen;
	struct hook_mfunc_data hd;

	if(target_p->name[0] == '\0')
		name = "?";
	else
		name = target_p->name;

	if(target_p->user == NULL)
	{
		sendto_one(source_p, form_str(RPL_WHOISUSER), me.name,
			   source_p->name, name,
			   target_p->username, target_p->host, target_p->info);
		sendto_one(source_p, form_str(RPL_WHOISSERVER),
			   me.name, source_p->name, name, "<Unknown>", "*Not On This Net*");
		return;
	}

	a2client_p = find_server(target_p->user->server);

	sendto_one(source_p, form_str(RPL_WHOISUSER), me.name,
		   source_p->name, target_p->name,
		   target_p->username, target_p->host, target_p->info);
	server_name = (char *) target_p->user->server;

	cur_len = mlen = ircsprintf(buf, form_str(RPL_WHOISCHANNELS), me.name, source_p->name, target_p->name);
	t = buf + mlen;

	DLINK_FOREACH(lp, target_p->user->channel.head)
	{
		msptr = lp->data;
		chptr = msptr->chptr;

		if(ShowChannel(source_p, chptr))
		{
			if((cur_len + strlen(chptr->chname) + 3) > (BUFSIZE - 4))
			{
				sendto_one(source_p, "%s", buf);
				cur_len = mlen;
				t = buf + mlen;
			}

			ircsprintf(t, "%s%s ", 
				   find_channel_status(msptr, 1), 
				   chptr->chname);

			tlen = strlen(t);
			t += tlen;
			cur_len += tlen;
		}
	}

	if(cur_len > mlen)
		sendto_one(source_p, "%s", buf);

	if((IsOper(source_p) || !ConfigServerHide.hide_servers) || target_p == source_p)
		sendto_one(source_p, form_str(RPL_WHOISSERVER),
			   me.name, source_p->name, target_p->name, server_name,
			   a2client_p ? a2client_p->info : "*Not On This Net*");
	else
		sendto_one(source_p, form_str(RPL_WHOISSERVER),
			   me.name, source_p->name, target_p->name,
			   ServerInfo.network_name, ServerInfo.network_desc);

	if(target_p->user->away)
		sendto_one(source_p, form_str(RPL_AWAY), me.name,
			   source_p->name, target_p->name, target_p->user->away);

	if(IsOper(target_p))
	{
			sendto_one(source_p, form_str(RPL_WHOISOPERATOR),
				   me.name, source_p->name, target_p->name,
				   IsAdmin(target_p) ? GlobalSetOptions.adminstring :
				   GlobalSetOptions.operstring);
	}

	if(MyClient(target_p))
	{
	  /* send idle if its global, source == target, or source and target
	   * are both local and theres no serverhiding or source is opered
	   */
	  if((glob == 1) || (MyClient(source_p) && 
	     (IsOper(source_p) || !ConfigServerHide.hide_servers)) || 
	     (source_p == target_p))
	  {
		if(ConfigFileEntry.use_whois_actually && show_ip(source_p, target_p))
			sendto_one(source_p, form_str(RPL_WHOISACTUALLY),
				   me.name, source_p->name, target_p->name,
				   target_p->localClient->sockhost);

		sendto_one(source_p, form_str(RPL_WHOISIDLE),
			   me.name, source_p->name, target_p->name,
			   CurrentTime - target_p->user->last, target_p->firsttime);
	  }
	}

	hd.client_p = target_p;
	hd.source_p = source_p;

	/* although we should fill in parc and parv, we don't ..
	 be careful of this when writing whois hooks */
	if(MyClient(source_p))
		hook_call_event("doing_whois_local", &hd);
	else
		hook_call_event("doing_whois_global", &hd);

	return;
}

