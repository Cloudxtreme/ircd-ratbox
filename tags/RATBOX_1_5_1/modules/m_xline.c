/* modules/m_xline.c
 *  Copyright (C) 2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
#include "whowas.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "resv.h"
#include "cluster.h"

static void mo_xline(struct Client *, struct Client *, int, char **);
static void ms_xline(struct Client *, struct Client *, int, char **);
static void me_xline(struct Client *, struct Client *, int, char **);
static void mo_unxline(struct Client *, struct Client *, int, char **);
static void ms_unxline(struct Client *, struct Client *, int, char **);
static void me_unxline(struct Client *, struct Client *, int, char **);

static int valid_xline(struct Client *source_p, const char *gecos, 
		       const char *reason);
static time_t valid_txline(const char *p);
static void remove_xline(struct Client *source_p, const char *gecos);

static void handle_remote_xline(struct Client *source_p, time_t temp_time,
			const char *name, const char *type, const char *reason);

static void propagate_xline(struct Client *source_p, const char *target, 
		time_t temp_time, const char *name, int type, const char *reason);
static void cluster_xline(struct Client *source_p, time_t temp_time,
			const char *name, int type, const char *reason);

struct Message xline_msgtab = {
	"XLINE", 0, 0, 3, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, ms_xline, me_xline, mo_xline}
};

struct Message unxline_msgtab = {
	"UNXLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, ms_unxline, me_unxline, mo_unxline}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&xline_msgtab);
	mod_add_cmd(&unxline_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&xline_msgtab);
	mod_del_cmd(&unxline_msgtab);
}

const char *_version = "$Revision$";
#endif

/* m_xline()
 *
 * parv[1] - thing to xline
 * parv[2] - optional type/reason
 * parv[3] - reason
 */
void
mo_xline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	struct ConfItem *aconf;
	const char *reason;
	const char *target_server = NULL;
	char *gecos;
	int xtype = 1;
	time_t txline_time;
	
	if(!IsOperXline(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need xline = yes;",
			   me.name, source_p->name);
		return;
	}
	/* Gecos will always be parv[1] unless its a temp xline */
	gecos = parv[1];
	txline_time = valid_txline(parv[1]);

	/* Deal with temp xlines first */
	if(txline_time >= 0)
	{
		gecos = parv[2];
		/* XLINE <time> <gecos> <type> :<reason> */
		if(parc == 5)
		{
			reason = parv[4];
			xtype = atoi(parv[3]);
		}
	
		/* XLINE <time> <gecos> :<reason> */
		else if(parc == 4)
		{
			reason = parv[3];
		}
	
		/* XLINE <something I cant be bothered to parse> */
		else
		{
			sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
				   me.name, source_p->name, "XLINE");
			return;
		}
	} 

	/* XLINE <gecos> <type> ON <server> :<reason> */
	else if(parc == 6)
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
			return;
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
		reason = parv[2];

	/* XLINE <something I cant be bothered to parse> */
	else
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "XLINE");
		return;
	}

	if(target_server != NULL)
	{
		propagate_xline(source_p, target_server, txline_time,
				gecos, xtype, reason);

		if(!match(target_server, me.name))
			return;
	}
	else if(dlink_list_length(&cluster_list) > 0)
		cluster_xline(source_p, txline_time, gecos, xtype, reason);

	if(valid_xline(source_p, gecos, reason) == 0)
		return;

	aconf = find_x_conf_exact(gecos);
	if(aconf != NULL)
	{
		sendto_one(source_p, ":%s NOTICE %s :[%s] already X-Lined by [%s] - %s",
			   me.name, source_p->name, gecos, aconf->name, aconf->passwd);
		return;
	}

	aconf = make_conf();
	aconf->status = CONF_XLINE;
	DupString(aconf->host, gecos);
	aconf->port = xtype;
	
	if(txline_time > 0)
	{
		aconf->hold = CurrentTime + txline_time;
		aconf->flags = CONF_FLAGS_TEMPORARY;
		
	}

	if(reason != NULL)
		DupString(aconf->passwd, reason);
	else
		DupString(aconf->passwd, "No Reason");

	collapse(aconf->host);

	/* conf_add_x_conf must be done last, due to it messing about
	 * with aconf --fl
	 */
	if(txline_time <= 0) 
	{
		write_confitem(XLINE_TYPE, source_p, NULL, aconf->host, aconf->passwd, 
			       NULL, NULL, aconf->port);
	}
	else
	{
		sendto_realops_flags(UMODE_ALL, L_ALL, "%s added temporary %d min. X-Line for [%s] [%s]",
			     get_oper_name(source_p), (int)(txline_time / 60),
			     aconf->host, aconf->passwd);
		ilog(L_TRACE, "%s added temporary %d min. X-line for [%s] [%s]", 
		     		source_p->name, (int)(txline_time / 60), 
	     			aconf->host, aconf->passwd);
		sendto_one(source_p, ":%s NOTICE %s :Added temporary %d min. X-Line for [%s] [%s]",
			   me.name, source_p->name, (int)(txline_time / 60),
			   aconf->host, aconf->passwd);

	}
	conf_add_x_conf(aconf);
	check_xlines();
}


/* ms_xline()
 *
 * handles an xline from a remote server
 */
static void
ms_xline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if(parc != 5 || EmptyString(parv[4]))
		return;

	/* parv[0]  parv[1]      parv[2]  parv[3]  parv[4]
	 * oper     target serv  xline    type     reason
	 */
	propagate_xline(source_p, parv[1], 0, parv[2], atoi(parv[3]), parv[4]);

	if(!IsPerson(source_p))
		return;

	/* destined for me? */
	if(!match(parv[1], me.name))
		return;

	handle_remote_xline(source_p, 0, parv[2], parv[3], parv[4]);
}

static void
me_xline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if(parc != 5 || EmptyString(parv[3]))
		return;

	if(!IsPerson(source_p))
		return;

	handle_remote_xline(source_p, valid_txline(parv[1]), parv[2], parv[3], parv[4]);
}

static void
handle_remote_xline(struct Client *source_p, time_t temp_time, const char *name,
			const char *type, const char *reason)
{
	struct ConfItem *aconf;

	/* look to see if we're clustering or oper can kline */
	if(find_cluster(source_p->user->server, CLUSTER_XLINE) ||
	   find_u_conf((char *) source_p->user->server,
		       source_p->username, source_p->host, OPER_XLINE))
	{
		if(valid_xline(source_p, name, reason) == 0)
			return;

		if((aconf = find_x_conf_exact(name)) != NULL)
		{
			sendto_one(source_p, ":%s NOTICE %s :[%s] already X-Lined by [%s] - %s",
				   me.name, source_p->name, name, aconf->name, aconf->passwd);
			return;
		}

		aconf = make_conf();
		aconf->status = CONF_XLINE;
		DupString(aconf->host, name);
		DupString(aconf->passwd, reason);
		aconf->port = atoi(type);

		if(temp_time > 0)
		{
			aconf->hold = CurrentTime + temp_time;
			aconf->flags = CONF_FLAGS_TEMPORARY;	
		}

		collapse(aconf->host);

		if(temp_time <= 0) 
		{
			write_confitem(XLINE_TYPE, source_p, NULL, aconf->host, aconf->passwd, 
					NULL, NULL, aconf->port);
		}
		else
		{
			sendto_realops_flags(UMODE_ALL, L_ALL, "%s added temporary %d min. X-Line for [%s] [%s]",
					get_oper_name(source_p), (int)(temp_time / 60),
					aconf->host, aconf->passwd);
			ilog(L_TRACE, "%s added temporary %d min. X-line for [%s] [%s]", 
					source_p->name, (int)(temp_time / 60), 
					aconf->host, aconf->passwd);
			sendto_one(source_p, ":%s NOTICE %s :Added temporary %d min. X-Line for [%s] [%s]",
					me.name, source_p->name, (int)(temp_time / 60),
					aconf->host, aconf->passwd);

		}

		conf_add_x_conf(aconf);
		check_xlines();
	}
}

/* valid_xline()
 *
 * checks xline for validity
 */
static int
valid_xline(struct Client *source_p, const char *gecos, const char *reason)
{
	if(EmptyString(reason))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "XLINE");
		return 0;
	}

	if(strchr(reason, ':') != NULL)
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Invalid character ':' in comment",
			   me.name, source_p->name);
		return 0;
	}


	if(!valid_wild_card_simple(gecos))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Please include at least %d non-wildcard characters with the xline",
			   me.name, source_p->name, 
			   ConfigFileEntry.min_nonwildcard_simple);
		return 0;
	}

	return 1;
}

static time_t
valid_txline(const char *p)
{
	time_t result = 0;

	while(*p)
	{
		if(IsDigit(*p))
		{
			result *= 10;
			result += ((*p) & 0xF);
			p++;
		}
		else
			return -1;
	}

	if(result > (24 * 60 * 7 * 4))
		result = (24 * 60 * 7 * 4);

	return(result * 60);
}

static void
propagate_xline(struct Client *source_p, const char *target, 
		time_t temp_time, const char *name, int type,
		const char *reason)
{
	if(temp_time <= 0)
	{
		sendto_match_servs(source_p, target, CAP_CLUSTER, NOCAPS,
				"XLINE %s %s %d :%s",
				target, name, type, reason);
		sendto_match_servs(source_p, target, CAP_ENCAP, CAP_CLUSTER,
				"ENCAP %s XLINE 0 %s %d :%s",
				target, name, type, reason);
	}
	else
		sendto_match_servs(source_p, target, CAP_ENCAP, NOCAPS,
				"ENCAP %s XLINE %ld %s %d :%s",
				target, (long) temp_time, name, type, reason);
}

static void
cluster_xline(struct Client *source_p, time_t temp_time, const char *name,
		int type, const char *reason)
{
	struct cluster *clptr;
	dlink_node *ptr;

	DLINK_FOREACH(ptr, cluster_list.head)
	{
		clptr = ptr->data;

		if(!(clptr->type & CLUSTER_XLINE))
			continue;

		if(temp_time <= 0)
		{
			sendto_match_servs(source_p, clptr->name, CAP_CLUSTER, NOCAPS,
					"XLINE %s %s %d :%s",
					clptr->name, name, type, reason);
			sendto_match_servs(source_p, clptr->name, CAP_ENCAP, CAP_CLUSTER,
					"ENCAP %s XLINE 0 %s %d :%s",
					clptr->name, name, type, reason);
		}
		else
			sendto_match_servs(source_p, clptr->name, CAP_ENCAP, NOCAPS,
					"ENCAP %s XLINE %ld %s %d :%s",
					clptr->name, temp_time, name, type, reason);
	}
}

		
/* mo_unxline()
 *
 * parv[1] - thing to unxline
 */
static void
mo_unxline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	if(!IsOperXline(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need xline = yes;",
			   me.name, source_p->name);
		return;
	}

	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "UNXLINE");
		return;
	}

	if((parc == 4) && (irccmp(parv[2], "ON") == 0))
	{
		propagate_generic(source_p, "UNXLINE", parv[3], CAP_CLUSTER,
				"%s", parv[1]);

		if(match(parv[3], me.name) == 0)
			return;
	}
	else if(dlink_list_length(&cluster_list) > 0)
		cluster_generic(source_p, "UNXLINE", CLUSTER_UNXLINE, CAP_CLUSTER,
				"%s", parv[1]);

	remove_xline(source_p, parv[1]);
}

/* ms_unxline()
 *
 * parv[1] - server to unxline on
 * parv[2] - thing to unxline
 */
static void
ms_unxline(struct Client *client_p, struct Client *source_p, 
	   int parc, char *parv[])
{
	if(parc != 3)
		return;

	/* parv[0]  parv[1]        parv[2] 
	 * oper     target server  gecos
	 */
	propagate_generic(source_p, "UNXLINE", parv[1], CAP_CLUSTER,
			"%s", parv[2]);

	if(!match(parv[1], me.name))
		return;

	if(!IsPerson(source_p))
		return;

	if(find_cluster(source_p->user->server, CLUSTER_UNXLINE) ||
	   find_u_conf((char *) source_p->user->server, source_p->username,
			    source_p->host, OPER_XLINE))
	{
		remove_xline(source_p, parv[2]);
	}
}

static void
me_unxline(struct Client *client_p, struct Client *source_p,
		int parc, char *parv[])
{
	if(parc != 2 || EmptyString(parv[1]))
		return;

	if(!IsPerson(source_p))
		return;

	if(find_cluster(source_p->user->server, CLUSTER_UNXLINE) ||
	   find_u_conf((char *) source_p->user->server, source_p->username,
			    source_p->host, OPER_XLINE))
	{
		remove_xline(source_p, parv[1]);
	}
}

static void
remove_xline(struct Client *source_p, const char *gecos)
{
	struct ConfItem *aconf;
	FBFILE *in, *out;
	char buf[BUFSIZE];
	char buff[BUFSIZE];
	char temppath[BUFSIZE];
	const char *filename;
	const char *conf_gecos;
	int error_on_write = 0;
	int found_xline = 0;
	mode_t oldumask;
	char *p;

	if((aconf = find_x_conf_exact(gecos)) != NULL)
	{
		if(aconf->flags & CONF_FLAGS_TEMPORARY)
		{
			aconf->hold = 0; /* This is a ugly hack */
			cleanup_temp_xlines(NULL);
			sendto_one(source_p, ":%s NOTICE %s :X-Line for [%s] is removed",
				   me.name, source_p->name, gecos);
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s has removed the temporary X-Line for: [%s]",
					     get_oper_name(source_p), gecos);
			ilog(L_NOTICE, "%s has removed the temporary X-Line for [%s]", 
			     get_oper_name(source_p), gecos);
			return;
		}
	} 
	else 
	{
		sendto_one(source_p, ":%s NOTICE %s :No X-Line for %s",
			   me.name, source_p->name, gecos);
		return;
	}

	
	filename = ConfigFileEntry.xlinefile;
	snprintf(temppath, sizeof(temppath), 
		 "%s.tmp", ConfigFileEntry.xlinefile);

	if((in = fbopen(filename, "r")) == NULL)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
			   me.name, source_p->name, filename);
		return;
	}

	oldumask = umask(0);

	if((out = fbopen(temppath, "w")) == NULL)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
			   me.name, source_p->name, temppath);
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

		if((conf_gecos = getfield(buff)) == NULL)
		{
			error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
			continue;
		}

		/* matching.. */
		if(irccmp(conf_gecos, gecos) == 0)
			found_xline++;
		else
			error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
	}

	fbclose(in);
	fbclose(out);

	if(error_on_write)
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Couldn't write temp xline file, aborted",
			   me.name, source_p->name);
		return;
	}
	else
	{
		(void) rename(temppath, filename);
		rehash(0);
	}

	if(!found_xline)
	{
		sendto_one(source_p, ":%s NOTICE %s :No X-Line for %s",
			   me.name, source_p->name, gecos);
		return;
	}

	sendto_one(source_p, ":%s NOTICE %s :X-Line for [%s] is removed",
		   me.name, source_p->name, gecos);
	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s has removed the X-Line for: [%s]",
			     get_oper_name(source_p), gecos);
	ilog(L_NOTICE, "%s has removed the X-Line for [%s]", get_oper_name(source_p), gecos);
}
