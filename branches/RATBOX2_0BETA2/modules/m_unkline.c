/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_unkline.c: Unklines a user.
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
#include "handlers.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "fileio.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_log.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"
#include "cluster.h"

static void mo_unkline(struct Client *, struct Client *, int, char **);
static void ms_unkline(struct Client *, struct Client *, int, char **);

struct Message unkline_msgtab = {
	"UNKLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, ms_unkline, mo_unkline}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&unkline_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&unkline_msgtab);
}
const char *_version = "$Revision$";
#endif

static void remove_permkline_match(struct Client *, char *, char *, int);
static int flush_write(struct Client *, FBFILE *, char *, char *);
static int remove_temp_kline(char *, char *);

/*
** mo_unkline
** Added Aug 31, 1997 
** common (Keith Fralick) fralick@gate.net
**
**      parv[0] = sender
**      parv[1] = address to remove
*
*
*/
static void
mo_unkline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	char *user, *host;
	char splat[] = "*";
	if(!IsOperUnkline(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You need unkline = yes;", me.name, parv[0]);
		return;
	}

	if(parc < 2)
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "UNKLINE");
		return;
	}

	if((host = strchr(parv[1], '@')) || *parv[1] == '*')
	{
		/* Explicit user@host mask given */

		if(host)	/* Found user@host */
		{
			user = parv[1];	/* here is user part */
			*(host++) = '\0';	/* and now here is host */
		}
		else
		{
			user = splat;	/* no @ found, assume its *@somehost */
			host = parv[1];
		}
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid parameters", me.name, source_p->name);
		return;
	}

	/* possible remote kline.. */
	if((parc > 3) && (irccmp(parv[2], "ON") == 0))
	{
		sendto_match_servs(source_p, parv[3], CAP_UNKLN,
				   "UNKLINE %s %s %s", parv[3], user, host);

		if(match(parv[3], me.name) == 0)
			return;
	}
	else if(dlink_list_length(&cluster_list) > 0)
	{
		cluster_unkline(source_p, user, host);
	}

	if(remove_temp_kline(user, host))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Un-klined [%s@%s] from temporary k-lines",
			   me.name, parv[0], user, host);
		sendto_realops_flags(UMODE_ALL, L_ALL,
				     "%s has removed the temporary K-Line for: [%s@%s]",
				     get_oper_name(source_p), user, host);
		ilog(L_NOTICE, "%s removed temporary K-Line for [%s@%s]", parv[0], user, host);
		return;
	}

	remove_permkline_match(source_p, host, user, 0);
}

/* ms_unkline()
 *
 * input	- pointer to servere
 * 		- pointer to client
 * 		- parm count
 * 		- params
 * output	- none
 * side effects - kline is removed if matching shared {} is found.
 */
static void
ms_unkline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
	char *kuser;
	char *khost;

	if(parc != 4)
		return;

	/* parv[0]  parv[1]        parv[2]  parv[3]
	 * oper     target server  user     host    */
	sendto_match_servs(source_p, parv[1], CAP_UNKLN,
			   "UNKLINE %s %s %s", parv[1], parv[2], parv[3]);

	kuser = parv[2];
	khost = parv[3];

	if(!match(parv[1], me.name))
		return;

	if(!IsPerson(source_p))
		return;

	if(find_cluster(source_p->user->server, CLUSTER_UNKLINE))
	{
		if(remove_temp_kline(kuser, khost))
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s has removed the temporary K-Line for: [%s@%s]",
					     get_oper_name(source_p), kuser, khost);
			ilog(L_NOTICE, "%s removed temporary K-Line for [%s@%s]",
			     source_p->name, kuser, khost);
			return;
		}

		remove_permkline_match(source_p, khost, kuser, 1);
	}
	else if(find_shared(source_p->username, source_p->host,
			    source_p->user->server, OPER_UNKLINE))
	{
		if(remove_temp_kline(kuser, khost))
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "*** Received Un-kline for [%s@%s], from %s",
					     kuser, khost, get_oper_name(source_p));

			sendto_one(source_p,
				   ":%s NOTICE %s :Un-klined [%s@%s] from temporary k-lines",
				   me.name, parv[0], kuser, khost);

			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "%s has removed the temporary K-Line for: [%s@%s]",
					     get_oper_name(source_p), kuser, khost);

			ilog(L_NOTICE, "%s removed temporary K-Line for [%s@%s]",
			     source_p->name, kuser, khost);
			return;
		}

		remove_permkline_match(source_p, khost, kuser, 0);
	}
}

/* remove_permkline_match()
 *
 * hunts for a permanent kline, and removes it.
 */
static void
remove_permkline_match(struct Client *source_p, char *host, char *user, int cluster)
{
	FBFILE *in, *out;
	int pairme = 0;
	int error_on_write = NO;
	char buf[BUFSIZE];
	char buff[BUFSIZE];
	char temppath[BUFSIZE];
	const char *filename;
	mode_t oldumask;
	char *p;

	ircsprintf(temppath, "%s.tmp", ConfigFileEntry.klinefile);

	filename = get_conf_name(KLINE_TYPE);

	if((in = fbopen(filename, "r")) == 0)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s", me.name, source_p->name,
			   filename);
		return;
	}

	oldumask = umask(0);
	if((out = fbopen(temppath, "w")) == 0)
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
		char *found_host, *found_user;

		strlcpy(buff, buf, sizeof(buff));

		if((p = strchr(buff, '\n')) != NULL)
			*p = '\0';

		if((*buff == '\0') || (*buff == '#'))
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
			continue;
		}

		if((found_user = getfield(buff)) == NULL)
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
			continue;
		}

		if((found_host = getfield(NULL)) == NULL)
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
			continue;
		}

		if((irccmp(host, found_host) == 0) && (irccmp(user, found_user) == 0))
		{
			pairme++;
		}
		else
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
		}
	}
	fbclose(in);
	fbclose(out);

/* The result of the rename should be checked too... oh well */
/* If there was an error on a write above, then its been reported
 * and I am not going to trash the original kline /conf file
 */
	if(!error_on_write)
	{
		(void) rename(temppath, filename);
		rehash(0);
	}
	else
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Couldn't write temp kline file, aborted",
			   me.name, source_p->name);
		return;
	}

	if(!pairme && !cluster)
	{
		sendto_one(source_p, ":%s NOTICE %s :No K-Line for %s@%s",
			   me.name, source_p->name, user, host);
		return;
	}

	if(!cluster)
	{
		if(!MyClient(source_p))
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "*** Received Un-kline for [%s@%s], from %s",
					     user, host, get_oper_name(source_p));

		sendto_one(source_p, ":%s NOTICE %s :K-Line for [%s@%s] is removed",
			   me.name, source_p->name, user, host);
	}

	sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s has removed the K-Line for: [%s@%s]",
			     get_oper_name(source_p), user, host);

	ilog(L_NOTICE, "%s removed K-Line for [%s@%s]", source_p->name, user, host);
	return;
}



/*
 * flush_write()
 *
 * inputs       - pointer to client structure of oper requesting unkline
 *              - out is the file descriptor
 *              - buf is the buffer to write
 *              - ntowrite is the expected number of character to be written
 *              - temppath is the temporary file name to be written
 * output       - YES for error on write
 *              - NO for success
 * side effects - if successful, the buf is written to output file
 *                if a write failure happesn, and the file pointed to
 *                by temppath, if its non NULL, is removed.
 *
 * The idea here is, to be as robust as possible when writing to the 
 * kline file.
 *
 * -Dianora
 */

static int
flush_write(struct Client *source_p, FBFILE * out, char *buf, char *temppath)
{
	int error_on_write = (fbputs(buf, out) < 0) ? YES : NO;

	if(error_on_write)
	{
		sendto_one(source_p, ":%s NOTICE %s :Unable to write to %s",
			   me.name, source_p->name, temppath);
		fbclose(out);
		if(temppath != NULL)
			(void) unlink(temppath);
	}
	return (error_on_write);
}

static dlink_list *tkline_list[] = {
	&tkline_hour,
	&tkline_day,
	&tkline_min,
	&tkline_week,
	NULL
};

/* remove_temp_kline()
 *
 * inputs       - username, hostname to unkline
 * outputs      -
 * side effects - tries to unkline anything that matches
 */
static int
remove_temp_kline(char *user, char *host)
{
	dlink_list *tklist;
	struct ConfItem *aconf;
	dlink_node *ptr;
	struct sockaddr_storage addr, caddr;
	int bits, cbits;
	int i;

	parse_netmask(host, &addr, &bits);

	for (i = 0; tkline_list[i] != NULL; i++)
	{
		tklist = tkline_list[i];

		DLINK_FOREACH(ptr, tklist->head)
		{
			aconf = (struct ConfItem *) ptr->data;

			parse_netmask(aconf->host, &caddr, &cbits);

			if(user && irccmp(user, aconf->user))
				continue;

			if(!irccmp(aconf->host, host) || (bits == cbits
							  && comp_with_mask_sock(&addr,
									    &caddr, bits)))
			{
				dlinkDestroy(ptr, tklist);
				delete_one_address_conf(aconf->host, aconf);
				return YES;
			}
		}
	}

	return NO;
}
