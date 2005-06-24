/*
 *  ircd-ratbox: A slightly useful ircd.
 *  client.h: The ircd client header.
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

#ifndef INCLUDED_client_h
#define INCLUDED_client_h

#include "config.h"

#if !defined(CONFIG_RATBOX_LEVEL_1)
#error Incorrect config.h for this revision of ircd.
#endif

#include "ircd_defs.h"
#include "ircd_handler.h"
#include "linebuf.h"
#include "channel.h"
#include "res.h"
#ifdef IPV6
#define HOSTIPLEN	53	/* sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255.ipv6") */
#else
#define HOSTIPLEN       16	/* Length of dotted quad form of IP        */
#endif
#define PASSWDLEN       20
#define CIPHERKEYLEN    64	/* 512bit */

#define IDLEN           12	/* this is the maximum length, not the actual
				   generated length; DO NOT CHANGE! */
#define COOKIELEN       IDLEN

#define CLIENT_BUFSIZE 512	/* must be at least 512 bytes */

/*
 * pre declare structs
 */
struct ConfItem;
struct Whowas;
struct DNSReply;
struct Listener;
struct Client;
struct LocalUser;
struct AuthRequest;
/*
 * Client structures
 */
struct User
{
	dlink_list channel;	/* chain of channel pointer blocks */
	dlink_list invited;	/* chain of invite pointer blocks */
	char *away;		/* pointer to away message */
	time_t last_away;	/* Away since... */
	time_t last;
	int refcnt;		/* Number of times this block is referenced */
	const char *server;	/* pointer to scached server name */
	char *response;		/* expected response from client */
	char *auth_oper;	/* Operator to become if they supply the response. */
	char id[IDLEN + 1];	/* client ID, unique ID per client */
};

struct Server
{
	struct User *user;	/* who activated this connection */
	const char *up;		/* Pointer to scache name */
	char by[NICKLEN];
	struct ConfItem *sconf;	/* connect{} pointer for this server */
	dlink_list servers;
	dlink_list users;
};

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

struct Client
{
	dlink_node node;
	dlink_node lnode;

	struct User *user;	/* ...defined, if this is a User */
	struct Server *serv;	/* ...defined, if this is a server */
	struct Client *servptr;	/* Points to server this Client is on */
	struct Client *from;	/* == self, if Local Client, *NEVER* NULL! */

	struct Whowas *whowas;	/* Pointers to whowas structs */
	time_t lasttime;	/* ...should be only LOCAL clients? --msa */
	time_t firsttime;	/* time client was created */
	time_t since;		/* last time we parsed something */
	time_t tsinfo;		/* TS on the nick, SVINFO on server */
	unsigned int umodes;	/* opers, normal users subset */
	unsigned int flags;	/* client flags */
	unsigned int flags2;	/* ugh. overflow */

	int hopcount;		/* number of servers to this 0 = local */
	unsigned short status;	/* Client type */
	unsigned char handler;	/* Handler index */
	unsigned long serial;	/* used to enforce 1 send per nick */

	/* client->name is the unique name for a client nick or host */
	char name[HOSTLEN + 1];

	/* 
	 * client->username is the username from ident or the USER message, 
	 * If the client is idented the USER message is ignored, otherwise 
	 * the username part of the USER message is put here prefixed with a 
	 * tilde depending on the I:line, Once a client has registered, this
	 * field should be considered read-only.
	 */
	char username[USERLEN + 1];	/* client's username */
	/*
	 * client->host contains the resolved name or ip address
	 * as a string for the user, it may be fiddled with for oper spoofing etc.
	 * once it's changed the *real* address goes away. This should be
	 * considered a read-only field after the client has registered.
	 */
	char host[HOSTLEN + 1];	/* client's hostname */
	/*
	 * client->info for unix clients will normally contain the info from the 
	 * gcos field in /etc/passwd but anything can go here.
	 */
	char info[REALLEN + 1];	/* Free form additional client info */

	/* caller ID allow list */
	/* This has to be here, since a client on an on_allow_list could
	 * be a remote client. simpler to keep both here.
	 */
	dlink_list allow_list;	/* clients I'll allow to talk to me */
	dlink_list on_allow_list;	/* clients that have =me= on their allow list */

	struct LocalUser *localClient;
};

struct LocalUser
{
	dlink_node tnode;
	/*
	 * The following fields are allocated only for local clients
	 * (directly connected to *this* server with a socket.
	 */
	/* Anti flooding part, all because of lamers... */
	time_t last_join_time;	/* when this client last 
				   joined a channel */
	time_t last_leave_time;	/* when this client last 
				 * left a channel */
	int join_leave_count;	/* count of JOIN/LEAVE in less than 
				   MIN_JOIN_LEAVE_TIME seconds */
	int oper_warn_count_down;	/* warn opers of this possible 
					   spambot every time this gets to 0 */
	time_t last_caller_id_time;
	time_t first_received_message_time;
	int received_number_of_privmsgs;
	int flood_noticed;

	/* Send and receive linebuf queues .. */
	buf_head_t buf_sendq;
	buf_head_t buf_recvq;
	/*
	 * we want to use unsigned int here so the sizes have a better chance of
	 * staying the same on 64 bit machines. The current trend is to use
	 * I32LP64, (32 bit ints, 64 bit longs and pointers) and since ircd
	 * will NEVER run on an operating system where ints are less than 32 bits, 
	 * it's a relatively safe bet to use ints. Since right shift operations are
	 * performed on these, it's not safe to allow them to become negative, 
	 * which is possible for long running server connections. Unsigned values 
	 * generally overflow gracefully. --Bleep
	 */
	unsigned int sendM;	/* Statistics: protocol messages send */
	unsigned int sendK;	/* Statistics: total k-bytes send */
	unsigned int receiveM;	/* Statistics: protocol messages received */
	unsigned int receiveK;	/* Statistics: total k-bytes received */
	unsigned short sendB;	/* counters to count upto 1-k lots of bytes */
	unsigned short receiveB;	/* sent and received. */
	unsigned int lastrecvM;	/* to check for activity --Mika */
	int priority;
	struct Listener *listener;	/* listener accepted from */
	dlink_list confs;	/* Configuration record associated */

	struct irc_inaddr ip;
	unsigned short port;	/* and the remote port# too :-) */
	int aftype;		/* Makes life easier for DNS res in IPV6 */
	unsigned long serverMask;	/* Only used for Lazy Links */
	time_t last_nick_change;
	int number_of_nick_changes;
	/*
	 * client->sockhost contains the ip address gotten from the socket as a
	 * string, this field should be considered read-only once the connection
	 * has been made. (set in s_bsd.c only)
	 */
	char sockhost[HOSTIPLEN + 1];	/* This is the host name from the 
					   socket ip address as string */
	/*
	 * XXX - there is no reason to save this, it should be checked when it's
	 * received and not stored, this is not used after registration
	 */
	char passwd[PASSWDLEN + 1];
	int caps;		/* capabilities bit-field */
	int enc_caps;		/* cipher capabilities bit-field */

#ifdef HAVE_LIBCRYPTO
	struct EncCapability *in_cipher;
	struct EncCapability *out_cipher;

	char in_key[CIPHERKEYLEN];
	char out_key[CIPHERKEYLEN];
#endif

	int fd;			/* >= 0, for local clients */
#ifndef HAVE_SOCKETPAIR
	int fd_r;		/* fd for reading */
#endif

	int ctrlfd;		/* For servers:
				   control fd used for sending commands
				   to servlink */
#ifndef HAVE_SOCKETPAIR
	int ctrlfd_r;		/* control fd for reading */
#endif

	struct SlinkRpl slinkrpl;	/* slink reply being parsed */
	unsigned char *slinkq;	/* sendq for control data */
	int slinkq_ofs;		/* ofset into slinkq */
	int slinkq_len;		/* length remaining after slinkq_ofs */

	struct ZipStats zipstats;

	/*
	 * Anti-flood stuff. We track how many messages were parsed and how
	 * many we were allowed in the current second, and apply a simple decay
	 * to avoid flooding.
	 *   -- adrian
	 */
	int allow_read;		/* how many we're allowed to read in this second */
	int actually_read;	/* how many we've actually read in this second */
	int sent_parsed;	/* how many messages we've parsed in this second */
	time_t last_knock;	/* time of last knock */
	unsigned long random_ping;
	struct AuthRequest *auth_request;
};

/*
 * status macros.
 */
#define STAT_CONNECTING         0x01
#define STAT_HANDSHAKE          0x02
#define STAT_ME                 0x04
#define STAT_UNKNOWN            0x08
#define STAT_REJECT		0x10
#define STAT_SERVER             0x20
#define STAT_CLIENT             0x40
#define HasID(x) (!IsServer(x) && (x)->user && (x)->user->id[0] != '\0')
#define ID(source_p) (HasID(source_p) ? source_p->user->id : source_p->name)

#define IsRegisteredUser(x)     ((x)->status == STAT_CLIENT)
#define IsRegistered(x)         (((x)->status  > STAT_UNKNOWN) && ((x)->status != STAT_REJECT))
#define IsConnecting(x)         ((x)->status == STAT_CONNECTING)
#define IsHandshake(x)          ((x)->status == STAT_HANDSHAKE)
#define IsMe(x)                 ((x)->status == STAT_ME)
#define IsUnknown(x)            ((x)->status == STAT_UNKNOWN)
#define IsServer(x)             ((x)->status == STAT_SERVER)
#define IsClient(x)             ((x)->status == STAT_CLIENT)
#define IsAnyServer(x)          (IsServer(x) || IsHandshake(x) || IsConnecting(x))

#define IsOper(x)		((x)->umodes & UMODE_OPER)
#define IsAdmin(x)		((x)->umodes & UMODE_ADMIN)

#define SetReject(x)		{(x)->status = STAT_REJECT; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetConnecting(x)        {(x)->status = STAT_CONNECTING; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetHandshake(x)         {(x)->status = STAT_HANDSHAKE; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetMe(x)                {(x)->status = STAT_ME; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetUnknown(x)           {(x)->status = STAT_UNKNOWN; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetServer(x)            {(x)->status = STAT_SERVER; \
				 (x)->handler = SERVER_HANDLER; }

#define SetClient(x)            {(x)->status = STAT_CLIENT; \
				 (x)->handler = IsOper((x)) ? \
					OPER_HANDLER : CLIENT_HANDLER; }

#define STAT_CLIENT_PARSE (STAT_UNKNOWN | STAT_CLIENT)
#define STAT_SERVER_PARSE (STAT_CONNECTING | STAT_HANDSHAKE | STAT_SERVER)

#define PARSE_AS_CLIENT(x)      ((x)->status & STAT_CLIENT_PARSE)
#define PARSE_AS_SERVER(x)      ((x)->status & STAT_SERVER_PARSE)


/*
 * ts stuff
 */
#define TS_CURRENT      5	/* current TS protocol version */
#ifdef TS5_ONLY
#define TS_MIN          5
#else
#define TS_MIN          3	/* minimum supported TS protocol version */
#endif
#define TS_DOESTS       0x20000000
#define DoesTS(x)       ((x)->tsinfo == TS_DOESTS)


/* housekeeping flags */

#define FLAGS_PINGSENT     0x0001	/* Unreplied ping sent */
#define FLAGS_DEADSOCKET   0x0002	/* Local socket is dead--Exiting soon */
#define FLAGS_KILLED       0x0004	/* Prevents "QUIT" from being sent for this */
#define FLAGS_CLOSING      0x0020	/* set when closing to suppress errors */
#define FLAGS_CHKACCESS    0x0040	/* ok to check clients access if set */
#define FLAGS_GOTID        0x0080	/* successful ident lookup achieved */
#define FLAGS_NEEDID       0x0100	/* I-lines say must use ident return */
#define FLAGS_NORMALEX     0x0400	/* Client exited normally */
#define FLAGS_SENDQEX      0x0800	/* Sendq exceeded */
#define FLAGS_CRYPTIN      0x2000	/* incoming data must be decrypted */
#define FLAGS_CRYPTOUT     0x4000	/* outgoing data must be encrypted */
#define FLAGS_WAITAUTH     0x8000	/* waiting for CRYPTLINK AUTH command */
#define FLAGS_SERVLINK     0x10000	/* servlink has servlink process */
#define FLAGS_MARK	   0x20000	/* marked client */
#define FLAGS_HIDDEN       0x40000	/* hidden server */
#define FLAGS_EOB          0x80000	/* EOB */
/* umodes, settable flags */

#define UMODE_SERVNOTICE   0x0001	/* server notices such as kill */
#define UMODE_CCONN        0x0002	/* Client Connections */
#define UMODE_REJ          0x0004	/* Bot Rejections */
#define UMODE_SKILL        0x0008	/* Server Killed */
#define UMODE_FULL         0x0010	/* Full messages */
#define UMODE_SPY          0x0020	/* see STATS / LINKS */
#define UMODE_DEBUG        0x0040	/* 'debugging' info */
#define UMODE_NCHANGE      0x0080	/* Nick change notice */
#define UMODE_WALLOP       0x0100	/* send wallops to them */
#define UMODE_OPERWALL     0x0200	/* Operwalls */
#define UMODE_INVISIBLE    0x0400	/* makes user invisible */
#define UMODE_BOTS         0x0800	/* shows bots */
#define UMODE_EXTERNAL     0x1000	/* show servers introduced and splitting */
#define UMODE_CALLERID     0x2000	/* block unless caller id's */
#define UMODE_UNAUTH       0x4000	/* show unauth connects here */
#define UMODE_LOCOPS       0x8000	/* show locops */
#define UMODE_OPERSPY      0x10000	/* show operspy notices */

/* user information flags, only settable by remote mode or local oper */
#define UMODE_OPER         0x20000	/* Operator */
#define UMODE_ADMIN        0x40000	/* Admin on server */

#define UMODE_ALL	   UMODE_SERVNOTICE


/* overflow flags */
#define OPER_GLOBAL_KILL        0x000001
#define OPER_REMOTE             0x000002
#define OPER_UNKLINE            0x000004
#define OPER_GLINE              0x000008
#define OPER_N                  0x000010
#define OPER_K                  0x000020
#define OPER_DIE                0x000040
#define OPER_REHASH             0x000080
#define OPER_ADMIN              0x000100
#define OPER_XLINE              0x000200
#define OPER_SPY                0x000400
#define OPER_LOCAL_KILL		0x000800
#define OPER_RESV		0x001000 /* XXX - only here for shared {}; */
#define OPER_OPERWALL		0x002000
#define OPER_HIDDENADMIN        0x004000
#define OPER_FLAGS      (OPER_GLOBAL_KILL | OPER_REMOTE | OPER_UNKLINE |\
                         OPER_GLINE | OPER_N | OPER_K | OPER_DIE |\
                         OPER_REHASH | OPER_ADMIN | OPER_XLINE | OPER_OPERWALL|\
                         OPER_SPY | OPER_HIDDENADMIN | OPER_LOCAL_KILL)

#define FLAGS2_EXEMPTGLINE      0x008000
#define FLAGS2_EXEMPTKLINE      0x010000
#define FLAGS2_EXEMPTFLOOD      0x020000
#define FLAGS2_NOLIMIT          0x040000
#define FLAGS2_IDLE_LINED       0x080000
#define FLAGS2_RESTRICTED       0x100000
#define FLAGS2_PING_COOKIE      0x200000
#define FLAGS2_IP_SPOOFING      0x400000
#define FLAGS2_FLOODDONE        0x800000
#define FLAGS2_EXEMPTSPAMBOT	0x1000000

#define SEND_UMODES  (UMODE_INVISIBLE | UMODE_OPER | UMODE_WALLOP | \
                      UMODE_ADMIN)
#define ALL_UMODES   (SEND_UMODES | UMODE_SERVNOTICE | UMODE_CCONN | \
                      UMODE_REJ | UMODE_SKILL | UMODE_FULL | UMODE_SPY | \
                      UMODE_NCHANGE | UMODE_OPERWALL | UMODE_DEBUG | \
                      UMODE_BOTS | UMODE_EXTERNAL | UMODE_LOCOPS | \
 		      UMODE_ADMIN | UMODE_UNAUTH | UMODE_CALLERID | UMODE_OPERSPY)

#define FLAGS_ID     (FLAGS_NEEDID | FLAGS_GOTID)

/*
 * flags macros.
 */
#define IsPerson(x)             (IsClient(x) && (x)->user)
#define DoAccess(x)             ((x)->flags & FLAGS_CHKACCESS)
#define IsDead(x)               ((x)->flags & FLAGS_DEADSOCKET)
#define SetDead(x)              ((x)->flags |= FLAGS_DEADSOCKET)
#define SetAccess(x)            ((x)->flags |= FLAGS_CHKACCESS)
#define IsClosing(x)		((x)->flags & FLAGS_CLOSING)
#define SetClosing(x)		((x)->flags |= FLAGS_CLOSING)
#define ClearAccess(x)          ((x)->flags &= ~FLAGS_CHKACCESS)
#define IsCryptIn(x)            ((x)->flags &  FLAGS_CRYPTIN)
#define SetCryptIn(x)           ((x)->flags |= FLAGS_CRYPTIN)
#define IsCryptOut(x)           ((x)->flags &  FLAGS_CRYPTOUT)
#define SetCryptOut(x)          ((x)->flags |= FLAGS_CRYPTOUT)
#define IsWaitAuth(x)           ((x)->flags &  FLAGS_WAITAUTH)
#define SetWaitAuth(x)          ((x)->flags |= FLAGS_WAITAUTH)
#define ClearWaitAuth(x)        ((x)->flags &= ~FLAGS_WAITAUTH)
#define HasServlink(x)          ((x)->flags &  FLAGS_SERVLINK)
#define SetServlink(x)          ((x)->flags |= FLAGS_SERVLINK)
#define MyConnect(x)            ((x)->localClient != NULL)
#define MyClient(x)             (MyConnect(x) && IsClient(x))
#define SetMark(x)		((x)->flags |= FLAGS_MARK)
#define ClearMark(x)		((x)->flags &= ~FLAGS_MARK)
#define IsMarked(x)		((x)->flags & FLAGS_MARK)
#define SetHidden(x)		((x)->flags |= FLAGS_HIDDEN)
#define ClearHidden(x)		((x)->flags &= ~FLAGS_HIDDEN)
#define IsHidden(x)		((x)->flags & FLAGS_HIDDEN)
#define ClearEob(x)		((x)->flags &= ~FLAGS_EOB)
#define SetEob(x)		((x)->flags |= FLAGS_EOB)
#define HasSentEob(x)		((x)->flags & FLAGS_EOB)


/* oper flags */
#define MyOper(x)               (MyConnect(x) && IsOper(x))

#define SetOper(x)              {(x)->umodes |= UMODE_OPER; \
				 if (!IsServer((x))) (x)->handler = OPER_HANDLER;}

#define ClearOper(x)            {(x)->umodes &= ~(UMODE_OPER|UMODE_ADMIN); \
				 if (!IsOper((x)) && !IsServer((x))) \
				  (x)->handler = CLIENT_HANDLER; }

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
#define SetWallops(x)           ((x)->umodes |= UMODE_WALLOP)
#define SetCallerId(x)		((x)->umodes |= UMODE_CALLERID)
#define IsSetCallerId(x)	((x)->umodes & UMODE_CALLERID)

#define SetNeedId(x)            ((x)->flags |= FLAGS_NEEDID)
#define IsNeedId(x)             (((x)->flags & FLAGS_NEEDID) != 0)

#define SetGotId(x)             ((x)->flags |= FLAGS_GOTID)
#define IsGotId(x)              (((x)->flags & FLAGS_GOTID) != 0)

/*
 * flags2 macros.
 */
#define IsExemptKline(x)        ((x)->flags2 & FLAGS2_EXEMPTKLINE)
#define SetExemptKline(x)       ((x)->flags2 |= FLAGS2_EXEMPTKLINE)
#define IsExemptLimits(x)       ((x)->flags2 & FLAGS2_NOLIMIT)
#define SetExemptLimits(x)      ((x)->flags2 |= FLAGS2_NOLIMIT)
#define IsExemptGline(x)        ((x)->flags2 & FLAGS2_EXEMPTGLINE)
#define SetExemptGline(x)       ((x)->flags2 |= FLAGS2_EXEMPTGLINE)
#define IsExemptFlood(x)        ((x)->flags2 & FLAGS2_EXEMPTFLOOD)
#define SetExemptFlood(x)       ((x)->flags2 |= FLAGS2_EXEMPTFLOOD)
#define IsExemptSpambot(x)	((x)->flags2 & FLAGS2_EXEMPTSPAMBOT)
#define SetExemptSpambot(x)	((x)->flags2 |= FLAGS2_EXEMPTSPAMBOT)
#define SetIPSpoof(x)           ((x)->flags2 |= FLAGS2_IP_SPOOFING)
#define IsIPSpoof(x)            ((x)->flags2 & FLAGS2_IP_SPOOFING)

#define SetIdlelined(x)         ((x)->flags2 |= FLAGS2_IDLE_LINED)
#define IsIdlelined(x)          ((x)->flags2 & FLAGS2_IDLE_LINED)

#define IsRestricted(x)         ((x)->flags2 &  FLAGS2_RESTRICTED)
#define SetRestricted(x)        ((x)->flags2 |= FLAGS2_RESTRICTED)

#define IsOperGlobalKill(x)     ((x)->flags2 & OPER_GLOBAL_KILL)
#define IsOperLocalKill(x)	((x)->flags2 & OPER_LOCAL_KILL)
#define IsOperRemote(x)         ((x)->flags2 & OPER_REMOTE)
#define IsOperUnkline(x)        ((x)->flags2 & OPER_UNKLINE)
#define IsOperGline(x)          ((x)->flags2 & OPER_GLINE)
#define IsOperN(x)              ((x)->flags2 & OPER_N)
#define IsOperK(x)              ((x)->flags2 & OPER_K)
#define IsOperXline(x)          ((x)->flags2 & OPER_XLINE)
#define IsOperDie(x)            ((x)->flags2 & OPER_DIE)
#define IsOperRehash(x)         ((x)->flags2 & OPER_REHASH)
#define IsOperHiddenAdmin(x)    ((x)->flags2 & OPER_HIDDENADMIN)
#define IsOperAdmin(x)          (((x)->flags2 & OPER_ADMIN) || \
                                 ((x)->flags2 & OPER_HIDDENADMIN))
#define IsOperOperwall(x)	((x)->flags2 & OPER_OPERWALL)
#define IsOperSpy(x)		((x)->flags2 & OPER_SPY)

#define IsFloodDone(x)          ((x)->flags2 & FLAGS2_FLOODDONE)
#define SetFloodDone(x)         ((x)->flags2 |= FLAGS2_FLOODDONE)

/*
 * definitions for get_client_name
 */
#define HIDE_IP 0
#define SHOW_IP 1
#define MASK_IP 2

extern void check_banned_lines(void);
extern void check_klines_event(void *unused);
extern void check_klines(void);
extern void check_glines(void);
extern void check_dlines(void);
extern void check_xlines(void);

extern const char *get_client_name(struct Client *client, int show_ip);
extern const char *log_client_name(struct Client *, int);
extern void init_client(void);
extern struct Client *make_client(struct Client *from);
extern void free_client(struct Client *client);
/*
 * extern void           add_client_to_list(struct Client* client);
 * This used to be a function..but now it is a macro
 */
#define add_client_to_list(client) dlinkAddTail(client, &client->node, &global_client_list)

extern void remove_client_from_list(struct Client *);
extern int exit_client(struct Client *, struct Client *, struct Client *, const char *);


extern void count_local_client_memory(int *count, size_t *memory);
extern void count_remote_client_memory(int *count, size_t *memory);

extern struct Client *find_chasing(struct Client *, char *, int *);
extern struct Client *find_person(char *);
extern struct Client *next_client(struct Client *, const char *);
extern int accept_message(struct Client *source, struct Client *target);
extern void del_from_accept(struct Client *source, struct Client *target);
extern void del_all_accepts(struct Client *client_p);

extern int set_initial_nick(struct Client *client_p, struct Client *source_p, char *nick);
extern int change_local_nick(struct Client *client_p, struct Client *source_p, char *nick);
extern void dead_link(struct Client *client_p);
extern int show_ip(struct Client *source_p, struct Client *target_p);

extern void initUser(void);
extern void free_user(struct User *, struct Client *);

extern struct User *make_user(struct Client *);
extern struct Server *make_server(struct Client *);


#endif /* INCLUDED_client_h */
