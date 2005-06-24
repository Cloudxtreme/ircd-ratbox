/* modules/m_operspy.c
 *  Copyright (C) 2003 Lee Hardy <lee -at- leeh.co.uk>
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
#include "tools.h"
#include "client.h"
#include "send.h"
#include "modules.h"
#include "parse.h"
#include "msg.h"
#include "handlers.h"
#include "sprintf_irc.h"

static void ms_operspy(struct Client *, struct Client *, int, char **);

struct Message operspy_msgtab = {
	"OPERSPY", 0, 0, 0, 0, MFLG_SLOW, 0,
	{m_ignore, m_ignore, m_ignore, ms_operspy, m_ignore}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&operspy_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&operspy_msgtab);
}

const char *_version = "$Revision$";
#endif

static void
ms_operspy(struct Client *client_p, struct Client *source_p,
		           int parc, char *parv[])
{
	static char buffer[BUFSIZE];
	char *ptr;
	int cur_len = 0;
	int len, i;

	if(parc < 2 || !IsPerson(source_p))
		return;

	if(parc < 4)
	{
		log_operspy(source_p, parv[1],
				parc < 3 ? NULL : parv[2]);
	}
	/* buffer all remaining into one param */
	else
	{
		ptr = buffer;
		cur_len = 0;

		for(i = 2; i < parc; i++)
		{
			len = strlen(parv[i]) + 1;

			if((size_t)(cur_len + len) >= sizeof(buffer))
				return;

			snprintf(ptr, sizeof(buffer) - cur_len, "%s ",
					parv[i]);
			ptr += len;
			cur_len += len;
		}

		log_operspy(source_p, parv[1], buffer);
	}

	return;
}

