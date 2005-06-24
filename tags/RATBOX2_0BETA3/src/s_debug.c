/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_debug.c: Some assorted debug functions.
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
#include "s_debug.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "fdlist.h"
#include "fileio.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "res.h"
#include "s_conf.h"
#include "s_log.h"
#include "scache.h"
#include "send.h"
#include "whowas.h"
#include "linebuf.h"
#include "memory.h"

/*
 * Option string.  Must be before #ifdef DEBUGMODE.
 */
const char serveropts[] = {
	' ',
	'T',
	'S',
#ifdef TS_CURRENT
	'0' + TS_CURRENT,
#endif
/* ONLY do TS */
/* ALWAYS do TS_WARNINGS */
	'o',
	'w',
	'\0'
};

void
debug(int level, char *format, ...)
{
	static char debugbuf[1024];
	va_list args;
	int err = errno;

	if((debuglevel >= 0) && (level <= debuglevel))
	{
		va_start(args, format);

		vsprintf(debugbuf, format, args);
		va_end(args);

		ilog(L_DEBUG, "%s", debugbuf);
	}
	errno = err;
}				/* debug() */

/*
 * This is part of the STATS replies. There is no offical numeric for this
 * since this isnt an official command, in much the same way as HASH isnt.
 * It is also possible that some systems wont support this call or have
 * different field names for "struct rusage".
 * -avalon
 */
void
send_usage(struct Client *source_p)
{
#ifndef __VMS
	struct rusage rus;
#endif
	time_t secs;
	time_t rup;
#ifdef  hz
# define hzz hz
#else
# ifdef HZ
#  define hzz HZ
# else
	int hzz = 1;
# endif
#endif

#ifdef __VMS
	sendto_one(source_p, 
		   ":%s NOTICE %s :getrusage not supported on this system",
		   me.name, source_p->name);
	return;
#else
	if(getrusage(RUSAGE_SELF, &rus) == -1)
	{
		sendto_one(source_p, ":%s NOTICE %s :Getruseage error: %s.",
			   me.name, source_p->name, strerror(errno));
		return;
	}
	secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	if(0 == secs)
		secs = 1;

	rup = (CurrentTime - me.since) * hzz;
	if(0 == rup)
		rup = 1;


	sendto_one(source_p,
		   ":%s %d %s R :CPU Secs %d:%d User %d:%d System %d:%d",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   (int) (secs / 60), (int) (secs % 60),
		   (int) (rus.ru_utime.tv_sec / 60),
		   (int) (rus.ru_utime.tv_sec % 60),
		   (int) (rus.ru_stime.tv_sec / 60), (int) (rus.ru_stime.tv_sec % 60));
	sendto_one(source_p,
		   ":%s %d %s R :RSS %ld ShMem %ld Data %ld Stack %ld",
		   me.name, RPL_STATSDEBUG, source_p->name, rus.ru_maxrss,
		   (rus.ru_ixrss / rup), (rus.ru_idrss / rup), (rus.ru_isrss / rup));
	sendto_one(source_p, ":%s %d %s R :Swaps %d Reclaims %d Faults %d",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   (int) rus.ru_nswap, (int) rus.ru_minflt, (int) rus.ru_majflt);
	sendto_one(source_p, ":%s %d %s R :Block in %d out %d", me.name,
		   RPL_STATSDEBUG, source_p->name, (int) rus.ru_inblock, (int) rus.ru_oublock);
	sendto_one(source_p, ":%s %d %s R :Msg Rcv %d Send %d", me.name,
		   RPL_STATSDEBUG, source_p->name, (int) rus.ru_msgrcv, (int) rus.ru_msgsnd);
	sendto_one(source_p,
		   ":%s %d %s R :Signals %d Context Vol. %d Invol %d", me.name,
		   RPL_STATSDEBUG, source_p->name, (int) rus.ru_nsignals,
		   (int) rus.ru_nvcsw, (int) rus.ru_nivcsw);
#endif /* __VMS */
}

void
count_memory(struct Client *source_p)
{
	struct Client *target_p;
	struct Channel *chptr;
	struct ConfItem *aconf;
	struct Ban *actualBan;
	dlink_node *dlink;
	dlink_node *ptr;
	int channel_count = 0;
	int local_client_conf_count = 0;	/* local client conf links */
	int users_counted = 0;	/* user structs */

	int channel_users = 0;
	int channel_invites = 0;
	int channel_bans = 0;
	int channel_except = 0;
	int channel_invex = 0;

	size_t wwu = 0;		/* whowas users */
	int class_count = 0;	/* classes */
	int conf_count = 0;	/* conf lines */
	int users_invited_count = 0;	/* users invited */
	int user_channels = 0;	/* users in channels */
	int aways_counted = 0;
	size_t number_servers_cached;	/* number of servers cached by scache */

	size_t channel_memory = 0;
	size_t channel_ban_memory = 0;
	size_t channel_except_memory = 0;
	size_t channel_invex_memory = 0;

	size_t away_memory = 0;	/* memory used by aways */
	size_t wwm = 0;		/* whowas array memory used */
	size_t conf_memory = 0;	/* memory used by conf lines */
	size_t mem_servers_cached;	/* memory used by scache */

	size_t linebuf_count = 0;
	size_t linebuf_memory_used = 0;

	size_t client_hash_table_size = 0;
	size_t channel_hash_table_size = 0;
	size_t total_channel_memory = 0;
	size_t totww = 0;

	size_t local_client_count = 0;
	size_t local_client_memory_used = 0;

	size_t remote_client_count = 0;
	size_t remote_client_memory_used = 0;

	size_t total_memory = 0;

	count_whowas_memory(&wwu, &wwm);

	DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = ptr->data;
		if(MyConnect(target_p))
		{
			local_client_conf_count++;
		}

		if(target_p->user)
		{
			users_counted++;
			users_invited_count += dlink_list_length(&target_p->user->invited);
			user_channels += dlink_list_length(&target_p->user->channel);
			if(target_p->user->away)
			{
				aways_counted++;
				away_memory += (strlen(target_p->user->away) + 1);
			}
		}
	}

	/* Count up all channels, ban lists, except lists, Invex lists */
	DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = (struct Channel *) ptr->data;
		channel_count++;
		channel_memory += (strlen(chptr->chname) + sizeof(struct Channel));

		channel_users += dlink_list_length(&chptr->members);
		channel_invites += dlink_list_length(&chptr->invites);

		DLINK_FOREACH(dlink, chptr->banlist.head)
		{
			actualBan = dlink->data;
			channel_bans++;

			channel_ban_memory += sizeof(dlink_node) + sizeof(struct Ban);
		}

		DLINK_FOREACH(dlink, chptr->exceptlist.head)
		{
			actualBan = dlink->data;
			channel_except++;

			channel_except_memory += (sizeof(dlink_node) + sizeof(struct Ban));
		}

		DLINK_FOREACH(dlink, chptr->invexlist.head)
		{
			actualBan = dlink->data;
			channel_invex++;

			channel_invex_memory += (sizeof(dlink_node) + sizeof(struct Ban));
		}
	}

	/* count up all config items */

	for (aconf = ConfigItemList; aconf; aconf = aconf->next)
	{
		conf_count++;
		conf_memory += aconf->host ? strlen(aconf->host) + 1 : 0;
		conf_memory += aconf->passwd ? strlen(aconf->passwd) + 1 : 0;
		conf_memory += aconf->name ? strlen(aconf->name) + 1 : 0;
		conf_memory += sizeof(struct ConfItem);
	}

	/* count up all classes */

	class_count = dlink_list_length(&class_list) + 1;

	count_linebuf_memory(&linebuf_count, &linebuf_memory_used);

	sendto_one(source_p, ":%s %d %s z :Users %u(%lu) Invites %u(%lu)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   users_counted,
		   (unsigned long) users_counted * sizeof(struct User),
		   users_invited_count, (unsigned long) users_invited_count * sizeof(dlink_node));

	sendto_one(source_p, ":%s %d %s z :User channels %u(%lu) Aways %u(%d)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   user_channels,
		   (unsigned long) user_channels * sizeof(dlink_node),
		   aways_counted, (int) away_memory);

	sendto_one(source_p, ":%s %d %s z :Attached confs %u(%lu)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   local_client_conf_count,
		   (unsigned long) local_client_conf_count * sizeof(dlink_node));

	sendto_one(source_p, ":%s %d %s z :Conflines %u(%d)",
		   me.name, RPL_STATSDEBUG, source_p->name, conf_count, (int) conf_memory);

	sendto_one(source_p, ":%s %d %s z :Classes %u(%lu)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   class_count, (unsigned long) class_count * sizeof(struct Class));

	sendto_one(source_p, ":%s %d %s z :Channels %u(%d)",
		   me.name, RPL_STATSDEBUG, source_p->name, channel_count, (int) channel_memory);

	sendto_one(source_p, ":%s %d %s z :Bans %u(%d)",
		   me.name, RPL_STATSDEBUG, source_p->name, channel_bans, (int) channel_ban_memory);

	sendto_one(source_p, ":%s %d %s z :Exceptions %u(%d)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   channel_except, (int) channel_except_memory);

	sendto_one(source_p, ":%s %d %s z :Invex %u(%d)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   channel_invex, (int) channel_invex_memory);

	sendto_one(source_p,
		   ":%s %d %s z :Channel members %u(%lu) invite %u(%lu)",
		   me.name, RPL_STATSDEBUG, source_p->name, channel_users,
		   (unsigned long) channel_users * sizeof(dlink_node),
		   channel_invites, (unsigned long) channel_invites * sizeof(dlink_node));

	total_channel_memory = channel_memory +
		channel_ban_memory +
		channel_users * sizeof(dlink_node) + channel_invites * sizeof(dlink_node);

	sendto_one(source_p, ":%s %d %s z :Whowas users %ld(%ld)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   (long)wwu, (long) (wwu * sizeof(struct User)));

	sendto_one(source_p, ":%s %d %s z :Whowas array %u(%d)",
		   me.name, RPL_STATSDEBUG, source_p->name, NICKNAMEHISTORYLENGTH, (int) wwm);

	totww = wwu * sizeof(struct User) + wwm;

	client_hash_table_size = hash_get_client_table_size();
	channel_hash_table_size = hash_get_channel_table_size();

	sendto_one(source_p, ":%s %d %s z :Hash: client %u(%ld) chan %u(%ld)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   U_MAX, (long)client_hash_table_size, CH_MAX, (long)channel_hash_table_size);

	sendto_one(source_p, ":%s %d %s z :linebuf %ld(%ld)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   (long)linebuf_count, (long)linebuf_memory_used);

	count_scache(&number_servers_cached, &mem_servers_cached);

	sendto_one(source_p, ":%s %d %s z :scache %ld(%ld)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   (long)number_servers_cached, (long)mem_servers_cached);

	sendto_one(source_p, ":%s %d %s z :hostname hash %d(%ld)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   HOST_MAX, (long)HOST_MAX * sizeof(dlink_list));

	total_memory = totww + total_channel_memory + conf_memory +
		class_count * sizeof(struct Class);
	total_memory += client_hash_table_size;
	total_memory += channel_hash_table_size;

	total_memory += mem_servers_cached;
	sendto_one(source_p,
		   ":%s %d %s z :Total: whowas %d channel %d conf %d", me.name,
		   RPL_STATSDEBUG, source_p->name, (int) totww,
		   (int) total_channel_memory, (int) conf_memory);

	count_local_client_memory(&local_client_count, &local_client_memory_used);
	total_memory += local_client_memory_used;
	sendto_one(source_p, ":%s %d %s z :Local client Memory in use: %ld(%ld)",
		   me.name, RPL_STATSDEBUG, source_p->name,
		   (long)local_client_count, (long)local_client_memory_used);


	count_remote_client_memory(&remote_client_count, &remote_client_memory_used);
	total_memory += remote_client_memory_used;
	sendto_one(source_p,
		   ":%s %d %s z :Remote client Memory in use: %ld(%ld)", me.name,
		   RPL_STATSDEBUG, source_p->name, (long)remote_client_count,
		   (long)remote_client_memory_used);

	sendto_one(source_p,
		   ":%s %d %s z :TOTAL: %d Available:  Current max RSS: %lu",
		   me.name, RPL_STATSDEBUG, source_p->name, (int) total_memory, get_maxrss());
}
