/************************************************************************
 *   IRC - Internet Relay Chat, include/serv.h
 *   Copyright (C) 1992 Darren Reed
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * $Id$
 *
 */
#ifndef INCLUDED_serv_h
#define INCLUDED_serv_h
#ifndef INCLUDED_config_h
#include "config.h"
#endif
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif

/*
 * The number of seconds between calls to try_connections(). Fiddle with
 * this ONLY if you KNOW what you're doing!
 */
#define TRY_CONNECTIONS_TIME	60
/*
 * number of seconds to wait after server starts up, before
 * starting try_connections()
 * TOO SOON and you can nick collide like crazy. 
 */
#define STARTUP_CONNECTIONS_TIME 300

struct Client;
struct ConfItem;
struct Channel;

struct Capability
{
  char*        name;      /* name of capability */
  unsigned int cap;       /* mask value */
};

#define CAP_CAP         0x00000001      /* received a CAP to begin with */
#define CAP_QS          0x00000002      /* Can handle quit storm removal */
#define CAP_EX          0x00000008      /* Can do channel +e exemptions */
#define CAP_CHW         0x00000010      /* Can do channel wall @# */
#define CAP_LL          0x00000020      /* Can do lazy links */
#define CAP_IE          0x00000040      /* Can do invite exceptions */
#define CAP_VCHAN       0x00000080      /* Can do vchans */
#define CAP_EOB	        0x00000100      /* Can do EOB message */
#define CAP_KLN	        0x00000200      /* Can do KLINE message */
#define CAP_GLN	        0x00000400      /* Can do GLINE message */
#define CAP_HOPS        0x00000800      /* can do half ops (+h) */
#define CAP_HUB         0x00001000      /* This server is a HUB */
#define CAP_AOPS        0x00002000      /* Can do anon ops (+a) */
#define CAP_UID         0x00004000      /* Can do UIDs */

#define CAP_MASK        CAP_QS|CAP_EX|CAP_CHW|\
                        CAP_IE|CAP_VCHAN|CAP_EOB|CAP_KLN|CAP_GLN|\
                        CAP_HOPS|CAP_AOPS|CAP_UID

#define DoesCAP(x)      ((x)->caps)

/*
 * Capability macros.
 */
#define IsCapable(x, cap)       ((x)->localClient->caps & (cap))
#define SetCapable(x, cap)      ((x)->localClient->caps |= (cap))
#define ClearCap(x, cap)        ((x)->localClient->caps &= ~(cap))

/*
 * Globals
 *
 *
 * list of recognized server capabilities.  "TS" is not on the list
 * because all servers that we talk to already do TS, and the kludged
 * extra argument to "PASS" takes care of checking that.  -orabidoo
 */
extern struct Capability captab[];

extern int MaxClientCount;     /* GLOBAL - highest number of clients */
extern int MaxConnectionCount; /* GLOBAL - highest number of connections */

/*
 * return values for hunt_server() 
 */
#define HUNTED_NOSUCH   (-1)    /* if the hunted server is not found */
#define HUNTED_ISME     0       /* if this server should execute the command */
#define HUNTED_PASS     1       /* if message passed onwards successfully */


extern int         check_server(const char* name, struct Client* server);
extern int         hunt_server(struct Client* cptr, struct Client* sptr,
                               char* command, int server, 
                               int parc, char** parv);
extern const char* my_name_for_link(const char* name, struct ConfItem* conf);
extern void        send_capabilities(struct Client*, int);
extern int         server_estab(struct Client* cptr);
extern void        set_autoconn(struct Client *,char *,char *,int);
extern const char* show_capabilities(struct Client* client);
extern void        show_servers(struct Client *);
extern void        try_connections(void *unused);

extern void        initServerMask(void);
extern void        burst_channel(struct Client *cptr, struct Channel *chptr);
extern void	   sendnick_TS(struct Client*, struct Client* );
extern int         serv_connect(struct ConfItem *, struct Client *);
extern unsigned long nextFreeMask(void);

extern struct Client *uplink; /* NON NULL if leaf and is this servers uplink */

void add_lazylinkchannel(struct Client *cptr, struct Channel *chptr);
void add_lazylinkclient(struct Client *cptr, struct Client *sptr);
void remove_lazylink_flags(unsigned long mask);
void client_burst_if_needed(struct Client *cptr, struct Client *acptr);

#endif /* INCLUDED_s_serv_h */



