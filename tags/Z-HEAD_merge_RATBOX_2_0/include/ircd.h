/*
 *  ircd-ratbox: A slightly useful ircd.
 *  ircd.h: A header for the ircd startup routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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

#ifndef INCLUDED_ircd_h
#define INCLUDED_ircd_h

#include "config.h"
#include "tools.h"
#include "memory.h"

struct Client;
struct dlink_list;

struct SetOptions
{
	int maxclients;		/* max clients allowed */
	int autoconn;		/* autoconn enabled for all servers? */

	int floodcount;		/* Number of messages in 1 second */
	int ident_timeout;	/* timeout for identd lookups */

	int spam_num;
	int spam_time;
	int split_delay;

	char operstring[REALLEN];
	char adminstring[REALLEN];
};

struct Counter
{
	int oper;		/* Opers */
	int total;		/* total clients */
	int invisi;		/* invisible clients */
	int max_loc;		/* MAX local clients */
	int max_tot;		/* MAX global clients */
	unsigned long totalrestartcount;	/* Total client count ever */
};

extern struct SetOptions GlobalSetOptions;	/* defined in ircd.c */

struct ServerStatistics
{
	unsigned int is_cl;	/* number of client connections */
	unsigned int is_sv;	/* number of server connections */
	unsigned int is_ni;	/* connection but no idea who it was */
	unsigned short is_cbs;	/* bytes sent to clients */
	unsigned short is_cbr;	/* bytes received to clients */
	unsigned short is_sbs;	/* bytes sent to servers */
	unsigned short is_sbr;	/* bytes received to servers */
	unsigned long is_cks;	/* k-bytes sent to clients */
	unsigned long is_ckr;	/* k-bytes received to clients */
	unsigned long is_sks;	/* k-bytes sent to servers */
	unsigned long is_skr;	/* k-bytes received to servers */
	time_t is_cti;		/* time spent connected by clients */
	time_t is_sti;		/* time spent connected by servers */
	unsigned int is_ac;	/* connections accepted */
	unsigned int is_ref;	/* accepts refused */
	unsigned int is_unco;	/* unknown commands */
	unsigned int is_wrdi;	/* command going in wrong direction */
	unsigned int is_unpf;	/* unknown prefix */
	unsigned int is_empt;	/* empty message */
	unsigned int is_num;	/* numeric message */
	unsigned int is_kill;	/* number of kills generated on collisions */
	unsigned int is_asuc;	/* successful auth requests */
	unsigned int is_abad;	/* bad auth requests */
	unsigned int is_rej;	/* rejected from cache */
};

extern struct ServerStatistics ServerStats;

extern const char *creation;
extern const char *generation;
extern const char *infotext[];
extern const char *serno;
extern const char *ircd_version;
extern const char *logFileName;
extern const char *pidFileName;
extern int dorehash;
extern int dorehashban;
extern int doremotd;
extern int kline_queued;
extern int server_state_foreground;

extern struct Client me;
extern dlink_list global_client_list;
extern struct Client *local[];
extern struct Counter Count;
#if 0
extern time_t CurrentTime;
#endif
extern struct timeval SystemTime;
#define CurrentTime SystemTime.tv_sec
extern time_t nextconnect;
extern int default_server_capabs;

extern time_t startup_time;

extern int splitmode;
extern int splitchecking;
extern int split_users;
extern int split_servers;

extern dlink_list unknown_list;
extern dlink_list lclient_list;
extern dlink_list serv_list;
extern dlink_list global_serv_list;
extern dlink_list oper_list;
extern dlink_list dead_list;
extern dlink_list abort_list;

extern unsigned long get_maxrss(void);
extern void set_time(void);

extern int testing_conf;

#endif
