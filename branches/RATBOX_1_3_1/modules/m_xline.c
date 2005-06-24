/* modules/m_xline.c
 * 
 *  Copyright (C) 2002-2003 Lee Hardy <lee@leeh.co.uk>
 *  Copyright (C) 2002-2003 ircd-ratbox development team
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
#include "whowas.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "cluster.h"

static int mo_xline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int ms_xline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int mo_unxline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int ms_unxline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message xline_msgtab = {
	"XLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_xline, 5}, {ms_xline, 5}, {mo_xline, 3}}
};
struct Message unxline_msgtab = {
	"UNXLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_unxline, 3}, {ms_unxline, 3}, {mo_unxline, 2}}
};

mapi_clist_av1 xline_clist[] =  { &xline_msgtab, &unxline_msgtab, NULL };
DECLARE_MODULE_AV1(xline, NULL, NULL, xline_clist, NULL, NULL, "$Revision$");

static int valid_xline(struct Client *, const char *, const char *);
static void write_xline(struct Client *source_p, const char *gecos, 
			const char *reason, int xtype);
static void remove_xline(struct Client *source_p, const char *gecos);


/* m_xline()
 *
 * parv[1] - thing to xline
 * parv[2] - optional type/reason
 * parv[3] - reason
 */
static int
mo_xline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct rxconf *xconf;
	const char *reason;
	const char *target_server = NULL;
	int xtype = 1;

	if(!IsOperXline(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need xline = yes;",
			   me.name, source_p->name);
		return 0;
	}

	if((xconf = find_xline(parv[1])) != NULL)
	{
		sendto_one(source_p, ":%s NOTICE %s :[%s] already X-Lined by [%s] - %s",
			   me.name, source_p->name, parv[1], xconf->name, xconf->reason);
		return 0;
	}

	/* XLINE <gecos> <type> ON <server> :<reason> */
	if(parc == 6)
	{
		if(irccmp(parv[3], "ON") == 0)
		{
			target_server = parv[4];
			reason = parv[5];
			xtype = atoi(parv[2]);
		}
		else
		{
			/* as good a numeric as any other I suppose --fl */
			sendto_one(source_p, form_str(ERR_NORECIPIENT),
				   me.name, source_p->name, "XLINE");
			return 0;
		}
	}
	
	/* XLINE <gecos> <type> :<reason> */
	else if(parc == 4)
	{
		reason = parv[3];
		xtype = atoi(parv[2]);
	}

	/* XLINE <gecos> :<reason> */
	else if(parc == 3)
	{
		reason = parv[2];
	}

	/* XLINE <something I cant be bothered to parse> */
	else
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "XLINE");
		return 0;
	}

	if(target_server != NULL)
	{
		sendto_match_servs(source_p, target_server, CAP_CLUSTER,
				   "XLINE %s %s %d :%s",
				   target_server, parv[1], xtype, reason);

		if(!match(target_server, me.name))
			return 0;
	}
	else if(dlink_list_length(&cluster_list) > 0)
		cluster_xline(source_p, parv[1], xtype, reason);

	if(!valid_xline(source_p, parv[1], reason))
		return 0;
	
	write_xline(source_p, parv[1], reason, xtype);

	return 0;
}

/* ms_xline()
 *
 * handles a remote xline
 */
static int
ms_xline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct rxconf *xconf;

	/* parv[0]  parv[1]      parv[2]  parv[3]  parv[4]
	 * oper     target serv  xline    type     reason
	 */
	sendto_match_servs(source_p, parv[1], CAP_CLUSTER,
			   "XLINE %s %s %s :%s",
			   parv[1], parv[2], parv[3], parv[4]);

	if(!IsPerson(source_p))
		return 0;

	/* destined for me? */
	if(!match(parv[1], me.name))
		return 0;

	/* first look to see if we're clustering with the server */
	if(find_cluster(source_p->user->server, CLUSTER_XLINE) ||
	   find_shared(source_p->username, source_p->host, 
		       source_p->user->server, OPER_XLINE))
	{
		if(!valid_xline(source_p, parv[2], parv[4]))
			return 0;

		/* already xlined */
		if((xconf = find_xline(parv[2])) != NULL)
		{
			sendto_one(source_p, ":%s NOTICE %s :[%s] already X-Lined by [%s] - %s",
				   me.name, source_p->name, parv[1], 
				   xconf->name, xconf->reason);
			return 0;
		}

		write_xline(source_p, parv[2], parv[4], atoi(parv[3]));
	}

	return 0;
}

/* valid_xline()
 *
 * inputs	- client xlining, gecos, reason and whether to warn
 * outputs	-
 * side effects - checks the xline for validity, erroring if needed
 */
static int
valid_xline(struct Client *source_p, const char *gecos,
	    const char *reason)
{
	if(EmptyString(reason))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   get_id(&me, source_p), 
			   get_id(source_p, source_p), "XLINE");
		return 0;
	}

	if(strchr(reason, ':') != NULL)
	{
		sendto_one_notice(source_p,
				  ":Invalid character ':' in comment");
		return 0;
	}

	if(!valid_wild_card_simple(gecos))
	{
		sendto_one_notice(source_p,
				  ":Please include at least %d non-wildcard "
				  "characters with the xline",
				  ConfigFileEntry.min_nonwildcard_simple);
		return 0;
	}

	return 1;
}

/* write_xline()
 *
 * inputs	- gecos, reason, xline type
 * outputs	- writes an xline to the config
 * side effects - 
 */
static void
write_xline(struct Client *source_p, const char *gecos, 
	    const char *reason, int xtype)
{
	char buffer[BUFSIZE * 2];
	FBFILE *out;
	struct rxconf *xconf;
	const char *filename;

	xconf = make_rxconf(gecos, reason, xtype, CONF_XLINE);
	collapse(xconf->name);

	filename = ConfigFileEntry.xlinefile;

	if((out = fbopen(filename, "a")) == NULL)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem opening %s ", filename);
		free_rxconf(xconf);
		return;
	}

	ircsprintf(buffer, "\"%s\",\"%d\",\"%s\",\"%s\",%lu\n",
		   xconf->name, xconf->type, xconf->reason, get_oper_name(source_p), CurrentTime);

	if(fbputs(buffer, out) == -1)
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s", filename);
		free_rxconf(xconf);
		fbclose(out);
		return;
	}

	fbclose(out);

	sendto_realops_flags(UMODE_ALL, L_ALL, "%s added X-Line for [%s] [%s]",
			     get_oper_name(source_p), xconf->name, xconf->reason);
	sendto_one_notice(source_p, ":Added X-Line for [%s] [%s]",
			  xconf->name, xconf->reason);
	ilog(L_KLINE, "%s added X-Line for [%s] [%s]",
	     get_oper_name(source_p), xconf->name, xconf->reason);

	add_rxconf(xconf);
	check_xlines();
}

/* mo_unxline()
 *
 * parv[1] - thing to unxline
 */
static int
mo_unxline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(!IsOperXline(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need xline = yes;",
			   me.name, source_p->name);
		return 0;
	}

	remove_xline(source_p, parv[1]);

	return 0;
}

/* ms_unxline()
 *
 * handles a remote unxline
 */
static int
ms_unxline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* parv[0]  parv[1]        parv[2]
	 * oper     target server  gecos
	 */
	sendto_match_servs(source_p, parv[1], CAP_CLUSTER,
			   "UNXLINE %s %s",
			   parv[1], parv[2]);

	if(!match(parv[1], me.name))
		return 0;

	if(!IsPerson(source_p))
		return 0;

	if(find_cluster(source_p->user->server, CLUSTER_UNXLINE) ||
	   find_shared(source_p->username, source_p->host, 
		       source_p->user->server, OPER_XLINE))
	{
		remove_xline(source_p, parv[2]);
	}

	return 0;
}
	
/* remove_xline()
 *
 * inputs	- gecos to remove
 * outputs	- 
 * side effects - removes xline from conf, if exists
 */
static void
remove_xline(struct Client *source_p, const char *huntgecos)
{
	FBFILE *in, *out;
	char buf[BUFSIZE];
	char buff[BUFSIZE];
	char temppath[BUFSIZE];
	const char *filename;
	const char *gecos;
	mode_t oldumask;
	char *p;
	int error_on_write = 0;
	int found_xline = 0;

	filename = ConfigFileEntry.xlinefile;
	ircsnprintf(temppath, sizeof(temppath),
		 "%s.tmp", ConfigFileEntry.xlinefile);

	if((in = fbopen(filename, "r")) == NULL)
	{
		sendto_one_notice(source_p, ":Cannot open %s", filename);
		return;
	}

	oldumask = umask(0);

	if((out = fbopen(temppath, "w")) == NULL)
	{
		sendto_one_notice(source_p, ":Cannot open %s", temppath);
		fbclose(in);
		umask(oldumask);
		return;
	}

	umask(oldumask);

	while (fbgets(buf, sizeof(buf), in))
	{
		if(error_on_write)
		{
			if(temppath != NULL)
				(void) unlink(temppath);

			break;
		}

		strlcpy(buff, buf, sizeof(buff));

		if((p = strchr(buff, '\n')) != NULL)
			*p = '\0';

		if((*buff == '\0') || (*buff == '#'))
		{
			error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
			continue;
		}

		if((gecos = getfield(buff)) == NULL)
		{
			error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
			continue;
		}

		/* matching.. */
		if(irccmp(gecos, huntgecos) == 0)
			found_xline++;
		else
			error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
	}

	fbclose(in);
	fbclose(out);

	if(error_on_write)
	{
		sendto_one_notice(source_p,
				  ":Couldn't write temp xline file, aborted");
		return;
	}
	else if(found_xline == 0)
	{
		sendto_one_notice(source_p, ":No X-Line for %s", huntgecos);

		if(temppath != NULL)
			(void) unlink(temppath);
		return;
	}

	(void) rename(temppath, filename);
	rehash(0);

	sendto_one_notice(source_p, ":X-Line for [%s] is removed", huntgecos);
	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s has removed the X-Line for: [%s]",
			     get_oper_name(source_p), huntgecos);
	ilog(L_KLINE, "%s has removed the X-Line for [%s]", get_oper_name(source_p), huntgecos);
}
