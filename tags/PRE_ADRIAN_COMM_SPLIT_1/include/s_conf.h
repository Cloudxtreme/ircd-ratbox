#ifndef INCLUDED_s_conf_h
#define INCLUDED_s_conf_h
/************************************************************************
 *   IRC - Internet Relay Chat, include/s_conf.h
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
 */

/*
 * $Id$
 *
 * $Log$
 * Revision 7.16  2000/09/29 17:16:55  ejb
 * merged toot's patch to resync with -6rc4
 *
 * Revision 7.15  2000/08/13 22:35:00  ejb
 * Large commit, folding in a number of changes i made over the weekend.
 * - add n!u@h for topic_info ala ircnet.
 * - stats p notice.
 * - removed some old non-TS cruft.
 * - unreg users can't send version.
 * - added +a umode, shows oper is admin in whois
 *   if they are 'admin=yes'.
 * - users can see the topic of a -s channel they aren't
 *   on (but can't set it even if it is -t, to prevent
 *   topic floods *sigh*)
 * - many changes in parse.c .. fixed a few bugs, rewrote
 *   the argument parser code, added support for catching
 *   users passing not enough parms before the m_ function
 *   is even called.
 * - you can now set in ircd.conf whether channels have to
 *   be +i for an invite to work.
 * - added a numeric (504) for sending to users trying to
 *   invite remote users to +i channels.
 * - started changing m_* functions to not check parc anymore,
 *   in most cases parse() has already handled this.
 * - move glines from config.h to ircd.conf
 * plus a lot of other stuff i probably forgot.
 *
 * Revision 7.14  2000/03/31 02:38:27  db
 * - large resync , folding in a number of changes contributed by is
 *   This moves some config.h items into the conf file
 *
 * Revision 7.13  2000/02/01 04:30:01  db
 * - improvements to connect section. allow multiple hub_masks etc.
 *
 * Revision 7.12  2000/01/23 04:53:36  db
 * - more cleanup on new conf file parser..
 *   using "auth" instead of "client" as per Kev's document..
 *
 * Revision 7.11  2000/01/21 04:21:32  db
 * - CONNECT in new parser works, needs full testing
 *
 * Revision 7.10  2000/01/17 03:21:24  db
 * - bunch of changes in order to have class names not numbers. *sigh*
 *   I toyed with the idea of keeping class #'s and having class_names
 *   to class #' mapping somewhere, but thats just getting silly.. sooo....
 *   Here we are...
 *
 * Revision 7.9  2000/01/16 22:16:46  db
 * - checkpoint with operator section mostly working...
 *   class code has to be rewritten still
 *
 * Revision 7.8  2000/01/16 04:35:06  db
 * - first very rough working lex/yacc under ircd-hybrid..
 *   parser doesn't actually do anything.... yet...
 *   Tom, we could use a fileio pushback, to push back a line thats
 *   "not for us"
 *
 * Revision 7.7  2000/01/15 22:46:22  db
 * - more cleanups to parse code
 *
 * Revision 7.6  2000/01/14 01:16:47  db
 * - externalized the old parser, unfortunately, this means a lot
 *   more of the s_conf interface is visible ;-(, can do better in the future...
 *   This is in preparation for new parser inclusion.
 *   This will (ick) allow us to use both old conf files and new format conf files
 *   for backwards compatibility. (I'm not terribly happy about this, but...
 *   Hi Tom!)
 *
 * Revision 7.5  2000/01/06 03:19:32  db
 * - removed HUB from config.h etc. now in a config entry
 *
 * Revision 7.4  2000/01/02 05:34:53  db
 * - Preliminary rough cut at Lazy Links, still a lot of mopping up to do
 *   'n' in c/n's means try lazy link, 'N' means do normal.
 *
 * Revision 7.3  1999/12/30 20:35:36  db
 * resync with current ircd-hybrid-6 tree
 *
 * Revision 1.43  1999/08/10 03:32:14  lusky
 * remove <sys/syslog.h> check from configure, assume <syslog.h> exists (sw)
 * cleaned up attach_Iline some more (db)
 *
 * Revision 1.41  1999/07/29 07:06:48  tomh
 * new m_commands
 *
 * Revision 1.40  1999/07/28 05:00:41  tomh
 * Finish net cleanup of connects (mostly).
 * NOTE: Please check this carefully to make sure it still works right.
 * The original code was entirely too twisted to be sure I got everything right.
 *
 * Revision 1.39  1999/07/27 00:51:53  tomh
 * more connect cleanups
 *
 * Revision 1.38  1999/07/26 05:46:35  tomh
 * new functions for s_conf cleaning up connect
 *
 * Revision 1.37  1999/07/25 18:05:06  tomh
 * untangle m_commands
 *
 * Revision 1.36  1999/07/25 17:27:40  db
 * - moved aConfItem defs from struct.h to s_conf.h
 *
 * Revision 1.35  1999/07/24 02:55:45  wnder
 * removed #ifdef for obsolete R_LINES (CONF_RESTRICT as well).
 *
 * Revision 1.34  1999/07/23 02:45:39  db
 * - include file fixes
 *
 * Revision 1.33  1999/07/23 02:38:30  db
 * - more include file fixes
 *
 * Revision 1.32  1999/07/22 03:19:11  tomh
 * work on socket code
 *
 * Revision 1.31  1999/07/22 02:44:22  db
 * - built m_gline.h, scache.h , moved more stuff from h.h
 *
 * Revision 1.30  1999/07/21 23:12:10  db
 * - more h.h pruning
 *
 * Revision 1.29  1999/07/21 21:54:28  db
 * - yet more h.h cleanups, the nightmare that never ends
 *
 * Revision 1.28  1999/07/21 05:45:05  tomh
 * untabify headers
 *
 * Revision 1.27  1999/07/20 09:11:21  db
 * - moved getfield from parse.c to s_conf.c which is the only place its used
 * - removed duplicate prototype from h.h , it was in dline_conf.h already
 * - send.c needs s_zip.h included to know about ziplinks
 *
 * Revision 1.26  1999/07/20 08:28:03  db
 * - more removal of stuff from h.h
 *
 * Revision 1.25  1999/07/20 08:20:33  db
 * - more cleanups from h.h
 *
 * Revision 1.24  1999/07/20 04:37:11  tomh
 * more cleanups
 *
 * Revision 1.23  1999/07/19 09:05:14  tomh
 * Work on char attributes for nick names, changed isvalid macro
 * Const correctness changes
 * Fixed file close bug on successful read
 * Header cleanups
 * Checked all strncpy_irc usage added terminations where needed
 *
 * Revision 1.22  1999/07/18 17:50:52  db
 * - more header cleanups
 *
 * Revision 1.21  1999/07/18 17:27:02  db
 * - a few more header cleanups
 * - motd.c included channel.h, no need
 *
 * Revision 1.20  1999/07/18 07:00:24  tomh
 * add new file
 *
 * Revision 1.19  1999/07/17 03:23:15  db
 * - my bad.
 * - fixed prototype in s_conf.h
 * - fixed typo of password for passwd in s_conf.c
 *
 * Revision 1.18  1999/07/17 03:13:03  db
 * - corrected type casting problems, mainly const char *
 * - moved prototype for safe_write into s_conf.h
 *
 * Revision 1.17  1999/07/16 11:57:31  db
 * - more cleanups
 * - removed unused function in FLUD code
 *
 * Revision 1.16  1999/07/16 09:57:54  db
 * - even more cleanups. moved prototype from h.h to s_conf.h
 *
 * Revision 1.15  1999/07/16 09:36:00  db
 * - rename some function names to make function clearer
 * - moved prototypes into headers
 * - made some functions static
 * - added some needed comments
 *
 * Revision 1.14  1999/07/16 04:16:59  db
 * - optimized get_conf_name
 * - replaced char * with const char * for filename
 *
 * Revision 1.13  1999/07/15 22:26:43  db
 * - fixed core bug in m_kline.c, probably should add extra sanity test there
 *   REDUNDANT_KLINES was using aconf->name instead of aconf->user
 * - cleaning up conf file generation etc.
 *
 * Revision 1.12  1999/07/15 02:45:07  db
 * - added conf_connect_allowed()
 *
 * Revision 1.11  1999/07/15 02:34:18  db
 * - redid m_kline, moved conf file writing from m_kline into s_conf.c
 *   thus "hiding" the details of where the kline gets written..
 *   Temporarily removed Shadowfax's LOCKFILE code until this settles down.
 *
 * Revision 1.10  1999/07/13 01:42:58  db
 * - cleaned up conf file handling, handled by read_conf_files()
 *
 * Revision 1.9  1999/07/11 21:09:35  tomh
 * sockhost cleanup and a lot of other stuff
 *
 * Revision 1.8  1999/07/11 02:44:17  db
 * - redid motd handling completely. most of the motd handling is now
 *   done in motd.c
 *   motd handling includes, motd, oper motd, help file
 *
 * Revision 1.7  1999/07/09 06:55:45  tomh
 * Changed resolver code to use reference counting instead of blind hostent
 * removal. This will ensure that if a client resolved we will always get
 * it's hostent. Currently we are saving the hostent for the life of the client,
 * but it can be released once the access checks are finished so the resolver
 * cache stays reasonably sized.
 *
 * Revision 1.6  1999/07/08 23:04:06  db
 * - fixed goof in s_conf.h
 *
 * Revision 1.5  1999/07/08 22:46:22  db
 * - changes to centralize config.h ircd config files to one struct
 *
 * Revision 1.4  1999/07/04 09:00:48  tomh
 * more cleanup, only call delete_resolver_queries when there are outstanding requests
 *
 * Revision 1.3  1999/07/03 20:24:20  tomh
 * clean up class macros, includes
 *
 * Revision 1.2  1999/07/03 08:13:09  tomh
 * cleanup dependencies
 *
 * Revision 1.1  1999/06/23 00:28:37  tomh
 * added fileio module, changed file read/write code to use fileio, removed dgets, new header s_conf.h, new module files fileio.h fileio.c
 *
 */
#ifndef INCLUDED_config_h
#include "config.h"             /* defines */
#endif
#ifndef INCLUDED_fileio_h
#include "fileio.h"             /* FBFILE */
#endif
#ifndef INCLUDED_netinet_in_h
#include <netinet/in.h>         /* in_addr */
#define INCLUDED_netinet_in_h
#endif
#ifndef INCLUDED_ircd_defs_h
#include "ircd_defs.h"
#endif
#ifndef INCLUDED_motd_h
#include "motd.h"               /* MessageFile */
#endif

struct Client;
struct SLink;
struct DNSReply;
struct hostent;

/* used by new parser */
/* yacc/lex love globals!!! */

struct ip_value {
  unsigned long ip;
  unsigned long ip_mask;
};

extern FBFILE* conf_fbfile_in;
extern char conf_line_in[256];
extern struct ConfItem* yy_aconf;

struct ConfItem
{
  struct ConfItem* next;     /* list node pointer */
  unsigned int     status;   /* If CONF_ILLEGAL, delete when no clients */
  unsigned int     flags;
  int              clients;  /* Number of *LOCAL* clients using this */
  struct in_addr   ipnum;    /* ip number of host field */
  unsigned long    ip;       /* only used for I D lines etc. */
  unsigned long    ip_mask;
  char*            name;     /* IRC name, nick, server name, or original u@h */
  char*            host;     /* host part of user@host */
  char*            passwd;
  char*            user;     /* user part of user@host */
  int              port;
  time_t           hold;     /* Hold action until this time (calendar time) */
  char*		   className;   /* Name of class */
  struct Class*    c_class;     /* Class of connection */
  int              dns_pending; /* 1 if dns query pending, 0 otherwise */
};

typedef struct QlineItem {
  char      *name;
  struct    ConfItem *confList;
  struct    QlineItem *next;
}aQlineItem;

#define CONF_ILLEGAL            0x80000000
#define CONF_MATCH              0x40000000
#define CONF_QUARANTINED_NICK   0x0001
#define CONF_CLIENT             0x0002
#define CONF_CONNECT_SERVER     0x0004
#define CONF_NOCONNECT_SERVER   0x0008
#define CONF_LOCOP              0x0010
#define CONF_OPERATOR           0x0020
#define CONF_ME                 0x0040
#define CONF_KILL               0x0080
#define CONF_ADMIN              0x0100
#define CONF_GENERAL            0x0200
/*
 * R_LINES are no more
 * -wnder
 *
 * #ifdef  R_LINES
 * #define CONF_RESTRICT           0x0200
 * #endif
 */
#define CONF_CLASS              0x0400
#define CONF_LEAF               0x0800
#define CONF_LISTEN_PORT        0x1000
#define CONF_HUB                0x2000
#define CONF_ELINE              0x4000
#define CONF_FLINE              0x8000
#define CONF_BLINE              0x10000
#define CONF_DLINE              0x20000
#define CONF_XLINE              0x40000
#define CONF_ULINE              0x80000

#define CONF_OPS                (CONF_OPERATOR | CONF_LOCOP)
#define CONF_SERVER_MASK        (CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER)
#define CONF_CLIENT_MASK        (CONF_CLIENT | CONF_OPS | CONF_SERVER_MASK)

#define IsIllegal(x)    ((x)->status & CONF_ILLEGAL)

/* aConfItem->flags */

#define CONF_FLAGS_LIMIT_IP             0x0001
#define CONF_FLAGS_NO_TILDE             0x0002
#define CONF_FLAGS_NEED_IDENTD          0x0004
#define CONF_FLAGS_PASS_IDENTD          0x0008
#define CONF_FLAGS_NOMATCH_IP           0x0010
#define CONF_FLAGS_E_LINED              0x0020
#define CONF_FLAGS_B_LINED              0x0040
#define CONF_FLAGS_F_LINED              0x0080

#ifdef IDLE_CHECK
#define CONF_FLAGS_IDLE_LINED           0x0100
#endif

#define CONF_FLAGS_DO_IDENTD            0x0200
#define CONF_FLAGS_ALLOW_AUTO_CONN      0x0400
#define CONF_FLAGS_ZIP_LINK             0x0800
#define CONF_FLAGS_SPOOF_IP             0x1000
#define CONF_FLAGS_LAZY_LINK            0x2000

#ifdef LITTLE_I_LINES
#define CONF_FLAGS_LITTLE_I_LINE        0x8000
#endif



/* Macros for aConfItem */

#define IsLimitIp(x)            ((x)->flags & CONF_FLAGS_LIMIT_IP)
#define IsNoTilde(x)            ((x)->flags & CONF_FLAGS_NO_TILDE)
#define IsNeedIdentd(x)         ((x)->flags & CONF_FLAGS_NEED_IDENTD)
#define IsPassIdentd(x)         ((x)->flags & CONF_FLAGS_PASS_IDENTD)
#define IsNoMatchIp(x)          ((x)->flags & CONF_FLAGS_NOMATCH_IP)
#define IsConfElined(x)         ((x)->flags & CONF_FLAGS_E_LINED)
#define IsConfBlined(x)         ((x)->flags & CONF_FLAGS_B_LINED)
#define IsConfFlined(x)         ((x)->flags & CONF_FLAGS_F_LINED)

#ifdef IDLE_CHECK
#define IsConfIdlelined(x)      ((x)->flags & CONF_FLAGS_IDLE_LINED)
#endif

#define IsConfDoIdentd(x)       ((x)->flags & CONF_FLAGS_DO_IDENTD)
#define IsConfDoSpoofIp(x)      ((x)->flags & CONF_FLAGS_SPOOF_IP)
#ifdef LITTLE_I_LINES
#define IsConfLittleI(x)        ((x)->flags & CONF_FLAGS_LITTLE_I_LINE)
#endif

/* port definitions for Opers */

#define CONF_OPER_GLOBAL_KILL 1
#define CONF_OPER_REMOTE      2
#define CONF_OPER_UNKLINE     4
#define CONF_OPER_GLINE       8
#define CONF_OPER_N          16
#define CONF_OPER_K          32
#define CONF_OPER_REHASH     64
#define CONF_OPER_DIE       128
#define CONF_OPER_ADMIN     256

typedef struct
{
  char *dpath;          /* DPATH if set from command line */
  char *configfile;
  char *klinefile;
  char *dlinefile;

  char  *glinefile;

  char* logpath;
  char* operlog;
  char* glinelog;

  MessageFile helpfile;
  MessageFile motd;
  MessageFile opermotd;
  int         hub; /* YES or NO */
  int         quiet_on_ban; /* YES or NO */
  int         failed_oper_notice;
  int         show_failed_oper_passwd;
  int         show_failed_oper_id;
  int         anti_nick_flood;
  int         max_nick_time;
  int         max_nick_changes;
  int         ts_max_delta;
  int         ts_warn_delta;
  int         kline_with_reason;
  int         kline_with_connection_closed;
  int         warn_no_nline;
  int         non_redundant_klines;
  int         botcheck;
  int         b_lines_oper_only;
  int         e_lines_oper_only;
  int         f_lines_oper_only;
  int         stats_notice;
  int         whois_notice;
  int         pace_wait;
  int         whois_wait;
  int         knock_delay;
  int         wallops_wait;
  int         pace_wallops;
  int         short_motd;
  int         no_oper_flood;
  int         stats_p_notice;
  int         invite_plus_i_only;
  int         glines;
  int         topic_uh;
  int         gline_time;
} ConfigFileEntryType;

/* bleh. have to become global. */
extern int ccount;
extern int ncount;

/* struct ConfItems */
/* conf uline link list root */
extern struct ConfItem *u_conf;
/* conf xline link list root */
extern struct ConfItem *x_conf;
/* conf qline link list root */
extern struct QlineItem *q_conf;

extern struct ConfItem* ConfigItemList;        /* GLOBAL - conf list head */
extern int              specific_virtual_host; /* GLOBAL - used in s_bsd.c */
extern struct ConfItem *temporary_klines;
extern struct ConfItem *temporary_ip_klines;
extern ConfigFileEntryType ConfigFileEntry;    /* GLOBAL - defined in ircd.c */

extern void clear_ip_hash_table(void);
extern void iphash_stats(struct Client *,struct Client *,int,char **,int);

#ifdef LIMIT_UH
void remove_one_ip(struct Client *);
#else
void remove_one_ip(unsigned long);
#endif

extern struct ConfItem* make_conf(void);
extern void             free_conf(struct ConfItem*);

extern void             read_conf_files(int cold);

extern struct DNSReply* conf_dns_lookup(struct ConfItem* aconf);
extern int              attach_conf(struct Client*, struct ConfItem *);
extern int              attach_confs(struct Client* client, 
                                     const char* name, int statmask);
extern int              attach_cn_lines(struct Client* client, 
                                        const char* host);
extern int              attach_Iline(struct Client* client, 
                                     const char* username, char** reason);
extern struct ConfItem* find_me(void);
extern struct ConfItem* find_admin(void);
extern struct ConfItem* find_first_nline(struct SLink* lp);
extern void             det_confs_butmask (struct Client *, int);
extern int              detach_conf (struct Client *, struct ConfItem *);
extern struct ConfItem* det_confs_butone (struct Client *, struct ConfItem *);
extern struct ConfItem* find_conf_exact(const char* name, const char* user, 
                                        const char* host, int statmask);
extern struct ConfItem* find_conf_name(struct SLink* lp, const char* name, 
                                       int statmask);
extern struct ConfItem* find_conf_host(struct SLink* lp, const char* host, 
                                       int statmask);
extern struct ConfItem* find_conf_ip(struct SLink* lp, char* ip, char* name, 
                                     int);
extern struct ConfItem* find_conf_by_name(const char* name, int status);
extern struct ConfItem* find_conf_by_host(const char* host, int status);
extern struct ConfItem* find_kill (struct Client *);
extern int conf_connect_allowed(struct in_addr addr);
extern char *oper_flags_as_string(int);
extern char *oper_privs_as_string(struct Client *, int);
extern int rehash_dump(struct Client *);
extern int find_q_line(char*, char*, char *);
extern struct ConfItem* find_special_conf(char *,int );
extern struct ConfItem* find_is_klined(const char* host, 
                                       const char* name,
                                       unsigned long ip);
extern char* show_iline_prefix(struct Client *,struct ConfItem *,char *);
extern void get_printable_conf(struct ConfItem *,
                                    char **, char **, char **,
                                    char **, int *,char **);
extern void report_configured_links(struct Client* cptr, int mask);
extern void report_specials(struct Client* sptr, int flags, int numeric);
extern void report_qlines(struct Client* cptr);

typedef enum {
  CONF_TYPE,
  KLINE_TYPE,
  DLINE_TYPE
} KlineType;

extern void write_kline_or_dline_to_conf_and_notice_opers(
                                                          KlineType,
                                                          struct Client *,
                                                          struct Client *,
                                                          char *,
                                                          char *,
                                                          char *,
                                                          char *
                                                          );
extern const char *get_conf_name(KlineType);
extern int safe_write(struct Client *, const char *, int ,char *);
extern void add_temp_kline(struct ConfItem *);
extern  void    flush_temp_klines(void);
extern  void    report_temp_klines(struct Client *);
extern  void    show_temp_klines(struct Client *, struct ConfItem *);
extern  int     is_address(char *,unsigned long *,unsigned long *); 
extern  int     rehash (struct Client *, struct Client *, int);

/* BLAH, pity I extern'ed all these. later will do better -db */
extern struct ConfItem* conf_add_server(struct ConfItem *,int ,int );
extern struct ConfItem* conf_add_o_line(struct ConfItem *);
extern void conf_add_port(struct ConfItem *);
extern void conf_add_class_to_conf(struct ConfItem *);
extern void conf_delist_old_conf(struct ConfItem *);
extern void conf_add_i_line(struct ConfItem *);
extern void conf_add_me(struct ConfItem *);
extern void conf_add_hub_or_leaf(struct ConfItem *);
extern void conf_add_class(struct ConfItem *,int );
extern void conf_add_k_line(struct ConfItem *);
extern void conf_add_d_line(struct ConfItem *);
extern void conf_add_x_line(struct ConfItem *);
extern void conf_add_u_line(struct ConfItem *);
extern void conf_add_q_line(struct ConfItem *);
extern void conf_add_fields(struct ConfItem*, char*, char *, char*, char *,char *);
extern void conf_add_conf(struct ConfItem *);
extern void oldParseOneLine(char* ,struct ConfItem*,int*,int*);

extern unsigned long cidr_to_bitmask[];

#endif /* INCLUDED_s_conf_h */

