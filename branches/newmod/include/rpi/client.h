/*
 *  ircd-ratbox: A slightly useful ircd.
 *  client.h: The ircd client header.
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

#ifndef RPI_CLIENT_H_
#define RPI_CLIENT_H_

#include "config.h"
#include "ircd_defs.h"

#include <rpi/api.h>

#if !defined(CONFIG_RATBOX_LEVEL_1)
#error Incorrect config.h for this revision of ircd.
#endif

#include "ircd_defs.h"
#include "linebuf.h"
#include "channel.h"
#include "res.h"
#ifdef IPV6
#define HOSTIPLEN	53	/* sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255.ipv6") */
#else
#define HOSTIPLEN       16	/* Length of dotted quad form of IP        */
#endif
#define PASSWDLEN       128
#define CIPHERKEYLEN    64	/* 512bit */
#define CLIENT_BUFSIZE 512	/* must be at least 512 bytes */

#define IDLEN		10

/*
 * pre declare structs
 */
struct ConfItem;
struct Whowas;
struct DNSReply;
struct Listener;
struct Client;
struct LocalUser;

struct rpi_user;
struct rpi_server;
struct rpi_client;
struct rpi_localclient;

/** Client */

typedef struct _rpi_client_vtable {
	void 			(*close_connection)		(struct rpi_client *);
	const char *	(*get_name)				(struct rpi_client *, int);
	int 			(*accept_message)		(struct rpi_client *, struct rpi_client *);
	void 			(*del_from_accept)		(struct rpi_client *, struct rpi_client *);
	void 			(*del_all_accepts)		(struct rpi_client *);
	const char *	(*server_name)			(struct rpi_client *, int);
	const char *	(*log_name)				(struct rpi_client *, int);
	void 			(*dead_link)			(struct rpi_client *);
	int 			(*show_ip)				(struct rpi_client *, struct rpi_client *);

} _rpi_client_vtable;

typedef struct rpi_client {
	RPI_INHERIT(_rpi_base);
#define rpi_client_offset (sizeof(_rpi_base))
	_rpi_client_vtable *v;

	dlink_node node;
	dlink_node lnode;
	struct rpi_user *user;
	struct rpi_server *serv;

	struct rpi_client *servptr;	/* Points to server this Client is on */
	struct rpi_client *from;	/* == self, if Local Client, *NEVER* NULL! */

	struct Whowas *whowas;	/* Pointers to whowas structs */
	time_t lasttime;	/* ...should be only LOCAL clients? --msa */
	time_t firsttime;	/* time client was created */
	time_t since;		/* last time we parsed something */
	time_t tsinfo;		/* TS on the nick, SVINFO on server */

	unsigned int umodes;    /* opers, normal users subset */
	unsigned int flags;     /* client flags */
	unsigned int flags2;    /* ugh. overflow */


	int hopcount;		/* number of servers to this 0 = local */

	enum {	STAT_CONNECTING,
			STAT_HANDSHAKE,
			STAT_ME,
			STAT_UNKNOWN,
			STAT_REJECT,
			STAT_SERVER,
			STAT_CLIENT
	} status; /* client type */

	unsigned char handler;	/* Handler index */
	unsigned long serial;	/* used to enforce 1 send per nick */

	char name[HOSTLEN + 1];
	char username[USERLEN + 1];	/* client's username */
	char host[HOSTLEN + 1];	/* client's hostname */
	char sockhost[HOSTIPLEN + 1]; /* clients ip */
	char info[REALLEN + 1];	/* Free form additional client info */

	char id[IDLEN + 1];	/* UID/SID, unique on the network */

	dlink_list allow_list;	/* clients I'll allow to talk to me */
	dlink_list on_allow_list;	/* clients that have =me= on their allow list */

	struct rpi_localclient *localClient;
} rpi_client;

/** User */

typedef struct rpi_user {
	dlink_list channel;	/* chain of channel pointer blocks */
	dlink_list invited;	/* chain of invite pointer blocks */
	char *away;		/* pointer to away message */
	time_t last_away;	/* Away since... */
	time_t last;
	int refcnt;		/* Number of times this block is referenced */
	const char *server;	/* pointer to scached server name */
	char *response;		/* expected response from client */
	char *auth_oper;	/* Operator to become if they supply the response. */
} rpi_user;

/** Server */

typedef struct rpi_server {
	struct rpi_user *user;	/* who activated this connection */
	const char *up;		/* Pointer to scache name */
	const char *upid;
	char by[NICKLEN];
	dlink_list servers;
	dlink_list users;
	int caps;		/* capabilities bit-field */
	char *fullcaps;
} rpi_server;

/** Localuser */

/* these shouldn't be here (?).		-larne */
struct SlinkRpl
{
	int command;
	int datalen;
	int gotdatalen;
	int readdata;
	unsigned char *data;
};

struct ZipStats
{
	unsigned long in;
	unsigned long in_wire;
	unsigned long out;
	unsigned long out_wire;
	double in_ratio;
	double out_ratio;
};

struct exit_client_hook
{
	struct rpi_client *client_p;
	char exit_message[TOPICLEN];
};

/* um.. not sure what to do about this.
   it should probably derive user or client, but we don't have
   a mechanism to support that yet.	-larne */
typedef struct rpi_localclient {
	dlink_node tnode;	/* This is the node for the local list type the client is on*/
	time_t last_join_time;
	time_t last_leave_time;
	int join_leave_count;
	int oper_warn_count_down;
	time_t last_caller_id_time;
	time_t first_received_message_time;
	int received_number_of_privmsgs;
	int flood_noticed;
	buf_head_t buf_sendq;
	buf_head_t buf_recvq;
	unsigned int sendM;	/* Statistics: protocol messages send */
	unsigned int sendK;	/* Statistics: total k-bytes send */
	unsigned int receiveM;	/* Statistics: protocol messages received */
	unsigned int receiveK;	/* Statistics: total k-bytes received */
	unsigned short sendB;	/* counters to count upto 1-k lots of bytes */
	unsigned short receiveB;	/* sent and received. */
	unsigned int lastrecvM;	/* to check for activity --Mika */
	int priority;
	struct Listener *listener;	/* listener accepted from */
	struct ConfItem *att_conf;	/* attached conf */
	struct server_conf *att_sconf;

	struct sockaddr_storage ip;
	unsigned long serverMask;	/* Only used for Lazy Links */
	time_t last_nick_change;
	int number_of_nick_changes;
	char *passwd;
	char *opername;
	char *fullcaps;

	int caps;		/* capabilities bit-field */
	int fd;			/* >= 0, for local clients */

	int ctrlfd;
	struct SlinkRpl slinkrpl;	/* slink reply being parsed */
	unsigned char *slinkq;	/* sendq for control data */
	int slinkq_ofs;		/* ofset into slinkq */
	int slinkq_len;		/* length remaining after slinkq_ofs */

	struct ZipStats zipstats;
	int allow_read;		/* how many we're allowed to read in this second */
	int actually_read;	/* how many we've actually read in this second */
	int sent_parsed;	/* how many messages we've parsed in this second */
	time_t last_knock;	/* time of last knock */
	unsigned long random_ping;
	struct AuthRequest	*auth_request;
} rpi_localclient;

#define IsRegisteredUser(x)     ((x)->status == STAT_CLIENT)
#define IsRegistered(x)         (((x)->status  > STAT_UNKNOWN) && ((x)->status != STAT_REJECT))
#define IsConnecting(x)         ((x)->status == STAT_CONNECTING)
#define IsHandshake(x)          ((x)->status == STAT_HANDSHAKE)
#define IsMe(x)                 ((x)->status == STAT_ME)
#define IsUnknown(x)            ((x)->status == STAT_UNKNOWN)
#define IsServer(x)             ((x)->status == STAT_SERVER)
#define IsClient(x)             ((x)->status == STAT_CLIENT)
#define IsAnyServer(x)          (IsServer(x) || IsHandshake(x) || IsConnecting(x))

#define SetReject(x)			do {(x)->status = STAT_REJECT; \
								    (x)->handler = UNREGISTERED_HANDLER; } while(0)
#define SetConnecting(x)        do {(x)->status = STAT_CONNECTING; \
								 (x)->handler = UNREGISTERED_HANDLER; } while(0)
#define SetHandshake(x)         do {(x)->status = STAT_HANDSHAKE; \
								 (x)->handler = UNREGISTERED_HANDLER; } while(0)
#define SetMe(x)                do {(x)->status = STAT_ME; \
								 (x)->handler = UNREGISTERED_HANDLER; } while(0)
#define SetUnknown(x)           do {(x)->status = STAT_UNKNOWN; \
								 (x)->handler = UNREGISTERED_HANDLER; } while(0)
#define SetServer(x)            do {(x)->status = STAT_SERVER; \
								 (x)->handler = SERVER_HANDLER; } while(0)
#define SetClient(x)            do {(x)->status = STAT_CLIENT; \
								    (x)->handler = IsOper((x)) ? \
									   OPER_HANDLER : CLIENT_HANDLER; } while(0)
#define SetRemoteClient(x)		do {(x)->status = STAT_CLIENT; \
								    (x)->handler = RCLIENT_HANDLER; } while(0)


#define STAT_CLIENT_PARSE (STAT_UNKNOWN | STAT_CLIENT)
#define STAT_SERVER_PARSE (STAT_CONNECTING | STAT_HANDSHAKE | STAT_SERVER)

#define PARSE_AS_CLIENT(x)      ((x)->status & STAT_CLIENT_PARSE)
#define PARSE_AS_SERVER(x)      ((x)->status & STAT_SERVER_PARSE)

#define TS_CURRENT	6

#ifdef TS6_ONLY
# define TS_MIN          6
#else
# define TS_MIN          3
#endif

#define TS_DOESTS       0x10000000
#define DoesTS(x)       ((x)->tsinfo & TS_DOESTS)

#define has_id(source)	((source)->id[0] != '\0')
#define use_id(source)	((source)->id[0] != '\0' ? (source)->id : (source)->name)

/* if target is TS6, use id if it has one, else name */
#define get_id(source, target) ((IsServer(target->from) && has_id(target->from)) ? \
				use_id(source) : (source)->name)

/* housekeeping flags */
/* this is broken.  don't know how to fix it atm.	-larne */
#define SEND_UMODES  (UMODE_INVISIBLE | UMODE_OPER | UMODE_WALLOP | \
                      UMODE_ADMIN)
#define DEFAULT_OPER_UMODES (UMODE_SERVNOTICE | UMODE_OPERWALL | \
                             UMODE_WALLOP | UMODE_LOCOPS)
#define ALL_UMODES   (SEND_UMODES | UMODE_SERVNOTICE | UMODE_CCONN | \
                      UMODE_REJ | UMODE_SKILL | UMODE_FULL | UMODE_SPY | \
                      UMODE_NCHANGE | UMODE_OPERWALL | UMODE_DEBUG | \
                      UMODE_BOTS | UMODE_EXTERNAL | UMODE_LOCOPS | \
 		      UMODE_ADMIN | UMODE_UNAUTH | UMODE_CALLERID | \
		      UMODE_OPERSPY)

#define FLAGS_ID     (FLAGS_NEEDID | FLAGS_GOTID)

#define FLAGS_PINGSENT     0x0001       /* Unreplied ping sent */
#define FLAGS_DEAD         0x0002       /* Local socket is dead--Exiting soon */
#define FLAGS_KILLED       0x0004       /* Prevents "QUIT" from being sent for this */
#define FLAGS_CLOSING      0x0020       /* set when closing to suppress errors */
#define FLAGS_CHKACCESS    0x0040       /* ok to check clients access if set */
#define FLAGS_GOTID        0x0080       /* successful ident lookup achieved */
#define FLAGS_NEEDID       0x0100       /* I-lines say must use ident return */
#define FLAGS_NORMALEX     0x0400       /* Client exited normally */
#define FLAGS_SENDQEX      0x0800       /* Sendq exceeded */
#define FLAGS_SERVLINK     0x10000      /* servlink has servlink process */
#define FLAGS_MARK         0x20000      /* marked client */
#define FLAGS_HIDDEN       0x40000      /* hidden server */
#define FLAGS_EOB          0x80000      /* EOB */
#define FLAGS_MYCONNECT    0x100000     /* MyConnect */
#define FLAGS_IOERROR	   0x200000

#define UMODE_SERVNOTICE   0x0001       /* server notices such as kill */
#define UMODE_CCONN        0x0002       /* Client Connections */
#define UMODE_REJ          0x0004       /* Bot Rejections */
#define UMODE_SKILL        0x0008       /* Server Killed */
#define UMODE_FULL         0x0010       /* Full messages */
#define UMODE_SPY          0x0020       /* see STATS / LINKS */
#define UMODE_DEBUG        0x0040       /* 'debugging' info */
#define UMODE_NCHANGE      0x0080       /* Nick change notice */
#define UMODE_WALLOP       0x0100       /* send wallops to them */
#define UMODE_OPERWALL     0x0200       /* Operwalls */
#define UMODE_INVISIBLE    0x0400       /* makes user invisible */
#define UMODE_BOTS         0x0800       /* shows bots */
#define UMODE_EXTERNAL     0x1000       /* show servers introduced and splitting */
#define UMODE_CALLERID     0x2000       /* block unless caller id's */
#define UMODE_UNAUTH       0x4000       /* show unauth connects here */
#define UMODE_LOCOPS       0x8000       /* show locops */
#define UMODE_OPERSPY      0x10000
#define UMODE_OPER         0x20000      /* Operator */
#define UMODE_ADMIN        0x40000      /* Admin on server */

#define UMODE_ALL          UMODE_SERVNOTICE

#define FLAGS2_EXEMPTGLINE      0x0100000
#define FLAGS2_EXEMPTKLINE      0x0200000
#define FLAGS2_EXEMPTFLOOD      0x0400000
#define FLAGS2_NOLIMIT          0x0800000
#define FLAGS2_IDLE_LINED       0x1000000
#define FLAGS2_RESTRICTED       0x2000000
#define FLAGS2_PING_COOKIE      0x4000000
#define FLAGS2_IP_SPOOFING      0x8000000
#define FLAGS2_FLOODDONE        0x10000000
#define FLAGS2_EXEMPTSPAMBOT    0x20000000
#define FLAGS2_EXEMPTSHIDE      0x40000000

/*
 * flags macros.
 */
#define IsPerson(x)             (IsClient(x) && (x)->user)
#define DoAccess(x)             ((x)->flags & FLAGS_CHKACCESS)
#define SetAccess(x)            ((x)->flags |= FLAGS_CHKACCESS)
#define ClearAccess(x)          ((x)->flags &= ~FLAGS_CHKACCESS)
#define HasServlink(x)          ((x)->flags &  FLAGS_SERVLINK)
#define SetServlink(x)          ((x)->flags |= FLAGS_SERVLINK)
#define MyConnect(x)            ((x)->flags & FLAGS_MYCONNECT)
#define SetMyConnect(x)         ((x)->flags |= FLAGS_MYCONNECT)
#define ClearMyConnect(x)       ((x)->flags &= ~FLAGS_MYCONNECT)

#define MyClient(x)             (MyConnect(x) && IsClient(x))
#define SetMark(x)              ((x)->flags |= FLAGS_MARK)
#define ClearMark(x)            ((x)->flags &= ~FLAGS_MARK)
#define IsMarked(x)             ((x)->flags & FLAGS_MARK)
#define SetHidden(x)            ((x)->flags |= FLAGS_HIDDEN)
#define ClearHidden(x)          ((x)->flags &= ~FLAGS_HIDDEN)
#define IsHidden(x)             ((x)->flags & FLAGS_HIDDEN)
#define ClearEob(x)             ((x)->flags &= ~FLAGS_EOB)
#define SetEob(x)               ((x)->flags |= FLAGS_EOB)
#define HasSentEob(x)           ((x)->flags & FLAGS_EOB)
#define IsDead(x)               ((x)->flags &  FLAGS_DEAD)
#define SetDead(x)              ((x)->flags |= FLAGS_DEAD)
#define IsIOError(x)            ((x)->flags & FLAGS_IOERROR)
#define SetIOError(x)           ((x)->flags |= FLAGS_IOERROR)
#define IsDeadorAborted(x)      (IsAborted(x) || IsDead(x))
#define IsKilled(x)				((x)->flags & FLAGS_KILLED)
#define SetKilled(x)			((x)->flags |= FLAGS_KILLED)

#define IsClosing(x)			((x)->flags & FLAGS_CLOSING)
#define SetClosing(x)			((x)->flags |= FLAGS_CLOSING)
#define IsAnyDead(x)			(IsIOError(x) || IsDead(x) || IsClosing(x))
#define IsIODead(x)				(IsIOError(x) || IsDead(x))

#define IsExemptKline(x)        ((x)->flags2 & FLAGS2_EXEMPTKLINE)
#define SetExemptKline(x)       ((x)->flags2 |= FLAGS2_EXEMPTKLINE)
#define IsExemptLimits(x)       ((x)->flags2 & FLAGS2_NOLIMIT)
#define SetExemptLimits(x)      ((x)->flags2 |= FLAGS2_NOLIMIT)
#define IsExemptGline(x)        ((x)->flags2 & FLAGS2_EXEMPTGLINE)
#define SetExemptGline(x)       ((x)->flags2 |= FLAGS2_EXEMPTGLINE)
#define IsExemptFlood(x)        ((x)->flags2 & FLAGS2_EXEMPTFLOOD)
#define SetExemptFlood(x)       ((x)->flags2 |= FLAGS2_EXEMPTFLOOD)
#define IsExemptSpambot(x)      ((x)->flags2 & FLAGS2_EXEMPTSPAMBOT)
#define SetExemptSpambot(x)     ((x)->flags2 |= FLAGS2_EXEMPTSPAMBOT)
#define IsExemptShide(x)        ((x)->flags2 & FLAGS2_EXEMPTSHIDE)
#define SetExemptShide(x)       ((x)->flags2 |= FLAGS2_EXEMPTSHIDE)
#define IsIPSpoof(x)            ((x)->flags2 & FLAGS2_IP_SPOOFING)
#define SetIPSpoof(x)           ((x)->flags2 |= FLAGS2_IP_SPOOFING)

#define SetIdlelined(x)         ((x)->flags2 |= FLAGS2_IDLE_LINED)
#define IsIdlelined(x)          ((x)->flags2 & FLAGS2_IDLE_LINED)

#define IsRestricted(x)         ((x)->flags2 &  FLAGS2_RESTRICTED)
#define SetRestricted(x)        ((x)->flags2 |= FLAGS2_RESTRICTED)

#define IsFloodDone(x)          ((x)->flags2 & FLAGS2_FLOODDONE)
#define SetFloodDone(x)         ((x)->flags2 |= FLAGS2_FLOODDONE)

#define SetNeedId(x)            ((x)->flags |= FLAGS_NEEDID)
#define IsNeedId(x)             ((x)->flags & FLAGS_NEEDID)
#define SetGotId(x)             ((x)->flags |= FLAGS_GOTID)
#define IsGotId(x)              ((x)->flags & FLAGS_GOTID)

/* oper flags */
#define MyOper(x)               (MyConnect(x) && IsOper(x))
#define SetOper(x)              do {(x)->umodes |= UMODE_OPER; \
								 if (MyClient((x))) (x)->handler = OPER_HANDLER;} while(0)
#define ClearOper(x)            do {(x)->umodes &= ~UMODE_OPER; (x)->umodes &= ~UMODE_ADMIN; \
								  if (MyClient((x)) && !IsOper((x)) && !IsServer((x))) \
								  (x)->handler = CLIENT_HANDLER; } while(0)

#define IsPrivileged(x)         (IsOper(x) || IsServer(x))

/* umode flags */
#define IsInvisible(x)          ((x)->umodes & UMODE_INVISIBLE)
#define SetInvisible(x)         ((x)->umodes |= UMODE_INVISIBLE)
#define ClearInvisible(x)       ((x)->umodes &= ~UMODE_INVISIBLE)
#define SendWallops(x)          ((x)->umodes & UMODE_WALLOP)
#define ClearWallops(x)         ((x)->umodes &= ~UMODE_WALLOP)
#define SendLocops(x)           ((x)->umodes & UMODE_LOCOPS)
#define SendServNotice(x)       ((x)->umodes & UMODE_SERVNOTICE)
#define SendOperwall(x)         ((x)->umodes & UMODE_OPERWALL)
#define SendCConnNotice(x)      ((x)->umodes & UMODE_CCONN)
#define SendRejNotice(x)        ((x)->umodes & UMODE_REJ)
#define SendSkillNotice(x)      ((x)->umodes & UMODE_SKILL)
#define SendFullNotice(x)       ((x)->umodes & UMODE_FULL)
#define SendSpyNotice(x)        ((x)->umodes & UMODE_SPY)
#define SendDebugNotice(x)      ((x)->umodes & UMODE_DEBUG)
#define SendNickChange(x)       ((x)->umodes & UMODE_NCHANGE)
#define SetWallops(x)           ((x)->umodes &= ~UMODE_WALLOP)
#define SetCallerId(x)			((x)->umodes &= ~UMODE_CALLERID)
#define IsSetCallerId(x)		((x)->umodes & UMODE_CALLERID)
#define IsOper(x)				((x)->umodes & UMODE_OPER)
#define IsAdmin(x)				((x)->umodes & UMODE_ADMIN)

/*
 * definitions for get_client_name
 */
#define HIDE_IP 0
#define SHOW_IP 1
#define MASK_IP 2

/* Functions below here need to become members.		-larne */
int exit_client(rpi_client *, rpi_client *, rpi_client *, const char *);
void error_exit_client(rpi_client *, int);
void free_user(rpi_user *, rpi_client *);

/* functions below here need to go away or become statics */
void count_local_client_memory(size_t * count, size_t * memory);
void count_remote_client_memory(size_t * count, size_t * memory);
rpi_client *find_chasing(rpi_client *, const char *, int *);
rpi_client *find_person(const char *);
rpi_client *find_named_person(const char *);
rpi_user *next_client(rpi_client *, const char *);

/* i don't know what to do with functions below here */
void init_client(void);
void init_uid(void);
char *generate_uid(void);
void check_banned_lines(void);
void check_klines_event(void *unused);
void check_klines(void);
void check_glines(void);
void check_dlines(void);
void check_xlines(void);
void initUser(void);
rpi_user *make_user(rpi_client *);
rpi_server *make_server(rpi_client *);

#endif
