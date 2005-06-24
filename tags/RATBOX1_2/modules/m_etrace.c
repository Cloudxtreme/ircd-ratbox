/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_etrace.c: Gives local opers a trace output with added info.
 *
 *  Copyright (C) 2002 ircd-ratbox development team
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1.Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  2.Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  3.The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  $Id$
 */

#include "stdinc.h"
#include "handlers.h"
#include "class.h"
#include "hook.h"
#include "client.h"
#include "hash.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_serv.h"
#include "s_conf.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void m_etrace(struct Client *, struct Client *, int, char **);

struct Message etrace_msgtab = {
	"ETRACE", 0, 0, 0, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, m_ignore, m_etrace}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&etrace_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&etrace_msgtab);
}
const char *_version = "$Revision$";
#endif


/*
 * m_etrace
 *      parv[0] = sender prefix
 *      parv[1] = servername
 */
static void
m_etrace(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	struct Client *target_p;
	dlink_node *ptr;
	char ip[HOSTIPLEN];
	const char *ip_ptr;

	if(!IsClient(source_p))
		return;

	/* report all direct connections */
	DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = ptr->data;

#ifdef HIDE_SPOOF_IPS
		if(IsIPSpoof(target_p))
		{
			ip_ptr = "255.255.255.255";
		}
		else
#endif
		{
			inetntop(target_p->localClient->aftype, &IN_ADDR(target_p->localClient->ip), ip, HOSTIPLEN);
			ip_ptr = ip;
		}

		sendto_one(source_p, form_str(RPL_ETRACE),
			   me.name, source_p->name, 
			   IsOper(target_p) ? "Oper" : "User", 
			   get_client_class(target_p),
			   target_p->name, target_p->username, target_p->host,
			   ip_ptr, target_p->info);
	}

	sendto_one(source_p, form_str(RPL_ENDOFTRACE), me.name, parv[0], me.name);
}

