/* modules/m_testline.c
 * 
 *  Copyright (C) 2004 Lee Hardy <lee@leeh.co.uk>
 *  Copyright (C) 2004 ircd-ratbox development team
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
#include "send.h"
#include "client.h"
#include "modules.h"
#include "msg.h"
#include "hostmask.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "sprintf_irc.h"

static int mo_testline(struct Client *, struct Client *, int, const char **);
static int mo_testgecos(struct Client *, struct Client *, int, const char **);

struct Message testline_msgtab = {
	"TESTLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_ignore, mg_ignore, mg_ignore, mg_ignore, {mo_testline, 2}}
};
struct Message testgecos_msgtab = {
	"TESTGECOS", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_ignore, mg_ignore, mg_ignore, mg_ignore, {mo_testgecos, 2}}
};

mapi_clist_av1 testline_clist[] = { &testline_msgtab, &testgecos_msgtab, NULL };
DECLARE_MODULE_AV1(testline, NULL, NULL, testline_clist, NULL, NULL, "$Revision$");

static int
mo_testline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct ConfItem *aconf;
	struct ConfItem *resv_p;
	struct sockaddr_storage ip;
	const char *name = NULL;
	const char *username = NULL;
	const char *host = NULL;
	char *mask;
	char *p;
	int host_mask;
	int type;

	mask = LOCAL_COPY(parv[1]);

	if((p = strchr(mask, '!')))
	{
		*p++ = '\0';
		name = mask;
		mask = p;

		if(EmptyString(mask))
			return 0;
	}

	if((p = strchr(mask, '@')))
	{
		*p++ = '\0';
		username = mask;
		host = p;

		if(EmptyString(host))
			return 0;
	}
	else
		host = mask;

	/* parses as an IP, check for a dline */
	if((type = parse_netmask(host, &ip, &host_mask)) != HM_HOST)
	{
#ifdef IPV6
		if(type == HM_IPV6)
			aconf = find_dline(&ip, AF_INET6);
		else
#endif
			aconf = find_dline(&ip, AF_INET);

		if(aconf && aconf->status & CONF_DLINE)
		{
			sendto_one(source_p, form_str(RPL_TESTLINE),
				me.name, source_p->name,
				(aconf->flags & CONF_FLAGS_TEMPORARY) ? 'd' : 'D',
				(aconf->flags & CONF_FLAGS_TEMPORARY) ? 
				 (long) ((aconf->hold - CurrentTime) / 60) : 0L, 
				aconf->host, aconf->passwd);

			return 0;
		}
	}

	/* now look for a matching I/K/G */
	if((aconf = find_address_conf(host, username ? username : "dummy",
				(type != HM_HOST) ? &ip : NULL,
				(type != HM_HOST) ? (
#ifdef IPV6
				 (type == HM_IPV6) ? AF_INET6 : 
#endif
				  AF_INET) : 0)))
	{
		static char buf[HOSTLEN+USERLEN+2];

		if(aconf->status & CONF_KILL)
		{
			ircsnprintf(buf, sizeof(buf), "%s@%s", 
					aconf->user, aconf->host);
			sendto_one(source_p, form_str(RPL_TESTLINE),
				me.name, source_p->name,
				(aconf->flags & CONF_FLAGS_TEMPORARY) ? 'k' : 'K',
				(aconf->flags & CONF_FLAGS_TEMPORARY) ? 
				 (long) ((aconf->hold - CurrentTime) / 60) : 0L,
				buf, aconf->passwd);
			return 0;
		}
		else if(aconf->status & CONF_GLINE)
		{
			ircsnprintf(buf, sizeof(buf), "%s@%s",
					aconf->user, aconf->host);
			sendto_one(source_p, form_str(RPL_TESTLINE),
				me.name, source_p->name,
				'G', (long) ((aconf->hold - CurrentTime) / 60),
				buf, aconf->passwd);
			return 0;
		}
	}

	/* they asked us to check a nick, so hunt for resvs.. */
	if(name && (resv_p = find_nick_resv(name)))
	{
		sendto_one(source_p, form_str(RPL_TESTLINE),
				me.name, source_p->name,
				resv_p->hold ? 'q' : 'Q',
				resv_p->hold ? (long) ((resv_p->hold - CurrentTime) / 60) : 0L,
				resv_p->name, resv_p->passwd);
		return 0;
	}

	/* no matching resv, we can print the I: if it exists */
	if(aconf && aconf->status & CONF_CLIENT)
	{
		sendto_one_numeric(source_p, RPL_STATSILINE, form_str(RPL_STATSILINE),
				aconf->name, show_iline_prefix(source_p, aconf, aconf->user),
				aconf->host, aconf->port, aconf->className);
		return 0;
	}

	/* nothing matches.. */
	sendto_one(source_p, form_str(RPL_NOTESTLINE),
			me.name, source_p->name, parv[1]);
	return 0;
}

static int
mo_testgecos(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct ConfItem *aconf;

	if(!(aconf = find_xline(parv[1], 0)))
	{
		sendto_one(source_p, form_str(RPL_NOTESTLINE),
				me.name, source_p->name, parv[1]);
		return 0;
	}

	sendto_one(source_p, form_str(RPL_TESTLINE),
			me.name, source_p->name,
			aconf->hold ? 'x' : 'X',
			aconf->hold ? (long) ((aconf->hold - CurrentTime) / 60) : 0L,
			aconf->name, aconf->passwd);
	return 0;
}
