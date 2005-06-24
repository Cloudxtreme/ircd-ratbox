/* modules/m_tb.c
 * 
 *  Copyright (C) 2003 Lee Hardy <lee@leeh.co.uk>
 *  Copyright (C) 2003-2004 ircd-ratbox development team
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
#include "handlers.h"
#include "tools.h"
#include "send.h"
#include "channel.h"
#include "channel_mode.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "ircd.h"
#include "s_conf.h"
#include "msg.h"
#include "modules.h"
#include "hash.h"
#include "s_serv.h"

static void ms_tb(struct Client *client_p, struct Client *source_p, int parc, char *parv[]);

struct Message tb_msgtab = {
	"TB", 0, 0, 4, 0, MFLG_SLOW, 0,
	{m_unregistered, m_ignore, ms_tb, m_ignore, m_ignore}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&tb_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&tb_msgtab);
}

const char *_version = "$Revision$";
#endif

/* m_tb()
 *
 * parv[1] - channel
 * parv[2] - topic ts
 * parv[3] - optional topicwho/topic
 * parv[4] - topic
 */
static void
ms_tb(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	struct Channel *chptr;
	const char *newtopic;
	const char *newtopicwho;
	time_t newtopicts;

	if(!IsServer(source_p))
		return;

	chptr = hash_find_channel(parv[1]);

	if(chptr == NULL)
		return;

	newtopicts = atol(parv[2]);

	if(parc == 5)
	{
		newtopic = parv[4];
		newtopicwho = parv[3];
	}
	else
	{
		newtopic = parv[3];
		newtopicwho = source_p->name;
	}

	if(EmptyString(newtopic))
		return;

	if(chptr->topic == NULL || chptr->topic_time > newtopicts)
	{
		/* its possible the topicts is a few seconds out on some
		 * servers, due to lag when propagating it, so if theyre the
		 * same topic just drop the message --fl
		 */
		if(chptr->topic != NULL && strcmp(chptr->topic, newtopic) == 0)
			return;

		set_channel_topic(chptr, newtopic, newtopicwho, newtopicts);
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s TOPIC %s :%s",
				     source_p->name, chptr->chname, newtopic);
		sendto_server(client_p, chptr, CAP_TB, NOCAPS,
			      ":%s TB %s %ld %s%s:%s",
			      me.name, chptr->chname, chptr->topic_time,
			      ConfigChannel.burst_topicwho ? chptr->topic_info : "",
			      ConfigChannel.burst_topicwho ? " " : "", chptr->topic);
	}
}
