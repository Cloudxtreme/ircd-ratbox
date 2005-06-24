/* m_classdisable.c
 *  Copyright (C) 2004 ircd-ratbox development team
 *  Copyright (C) 2004 Lee Hardy <lee -at- leeh.co.uk>
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
#include "tools.h"
#include "handlers.h"
#include "send.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "class.h"
#include "ircd.h"
#include "numeric.h"
#include "memory.h"
#include "s_log.h"
#include "s_serv.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"

static void mo_classdisable(struct Client *, struct Client *, int, char **);

struct Message classdisable_msgtab = {
	"CLASSDISABLE", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, m_ignore, m_ignore, mo_classdisable}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&classdisable_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&classdisable_msgtab);
}

const char *_version = "$Revision$";
#endif

void
mo_classdisable(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	struct Class *clptr;

	if(!IsOperRehash(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need rehash = yes;",
				me.name, source_p->name);
		return;
	}

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
				 me.name, source_p->name, "CLASSDISABLE");
		return;
	}

	if(!irccmp(parv[1], "default") ||
	   (clptr = find_class(parv[1])) == NULL)
	{
		sendto_one(source_p, ":%s NOTICE %s :No such class",
				me.name, source_p->name);
		return;
	}

	MaxUsers(clptr) = 1;

	sendto_realops_flags(UMODE_ALL, L_ALL, "%s is disabling class %s",
				get_oper_name(source_p), parv[1]);
}
