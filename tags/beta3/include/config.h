/************************************************************************
 *   IRC - Internet Relay Chat, include/config.h
 *   Copyright (C) 1990 Jarkko Oikarinen
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
 * $Id$
 */
#ifndef INCLUDED_config_h
#define INCLUDED_config_h

#include "setup.h"

/* PLEASE READ SECTION:
 */

/***************** MAKE SURE THIS IS CORRECT!!!!!!!!! **************/
/* ONLY EDIT "HARD_FDLIMIT_" and "INIT_MAXCLIENTS" */

/* You may also need to hand edit the Makefile to increase
 * the value of FD_SETSIZE 
 */

/* These ultra low values pretty much guarantee the server will
 * come up initially, then you can increase these values to fit your
 * system limits. If you know what you are doing, increase them now
 */

/* VMS NOTE - VMS is basically unlimited with FDs, so just set this
   to any suitable value.. */

#define HARD_FDLIMIT_   256
#define INIT_MAXCLIENTS 200

/*
 * This is how many 'buffer connections' we allow... 
 * Remember, MAX_BUFFER + MAX_CLIENTS can't exceed HARD_FDLIMIT :)
 */
#define MAX_BUFFER      60

/* Don't change this... */
#define HARD_FDLIMIT    (HARD_FDLIMIT_ - 10)
#define MASTER_MAX      (HARD_FDLIMIT - MAX_BUFFER)
/*******************************************************************/

/* *PATH - directory and files locations
 *
 * Full pathnames and defaults of irc system's support files. Please note that
 * these are only the recommended names and paths. Change as needed.
 *
 * DPATH   = root directory of installation,
 * MODPATH = path to module directory,
 * MSGPATH = path to directory containing .mo message files,
 * SPATH   = server executable,
 * CPATH   = conf file,
 * KPATH   = kline conf file,
 * DLPATH  = dline conf file,
 * RPATH   = RSA keyfile,
 * MPATH   = MOTD,
 * PPATH   = pid file,
 * HPATH   = oper help file as seen by /quote help, 
 * OMOTD   = path to MOTD for opers.
 * 
 * For /restart to work, SPATH needs to be a full pathname
 * (unless "." is in your exec path). -Rodder
 *
 * Leave KPATH undefined if you want klines in main conf file.
 *
 * Leave MSGPATH undefined if you don't want to include gettext() support
 * for alternate message files -dt
 *
 * All other *PATHs should be defined.
 * DPATH, MODPATH and MSGPATH must have a trailing /'s
 * Do not use ~'s in any of these paths
 *
 */

#define DPATH   IRCD_PREFIX
#define MODPATH IRCD_PREFIX "/modules/autoload/"
#define MSGPATH IRCD_PREFIX "/messages/"
#define SPATH   IRCD_PREFIX "/bin/ircd"
/* i really don't like the below, but I'll figure the rest out! -- adrian */
#define ETCPATH	IRCD_PREFIX "/etc"
#define LOGPATH IRCD_PREFIX "/logs"
#define CPATH   ETCPATH "/ircd.conf"
#define KPATH   ETCPATH "/kline.conf"
#define DLPATH  ETCPATH "/kline.conf"
#define GPATH   LOGPATH "/gline.log"
#define RPATH   ETCPATH "/ircd.rsa"
#define MPATH   ETCPATH "/ircd.motd"
#define LPATH   LOGPATH "/ircd.log"
#define PPATH   ETCPATH "/ircd.pid"
#define HPATH   ETCPATH "/opers.txt"
#define OPATH   ETCPATH "/opers.motd"
#define LIPATH   ETCPATH "/links.txt"

/* TS_MAX_DELTA_DEFAULT and TS_WARN_DELTA_DEFAULT -
 * allowed delta for TS when another server connects.
 *
 * If the difference between my clock and the other server's clock is
 * greater than TS_MAX_DELTA, I send out a warning and drop the links.
 * 
 * If the difference is less than TS_MAX_DELTA, I just sends out a warning
 * but don't drop the link.
 *
 * TS_MAX_DELTA_DEFAULT currently set to 30 minutes to deal with older
 * timedelta implementation.  Once pre-hybrid5.2 servers are eradicated, 
 * we can drop this down to 90 seconds or so. --Rodder
 */
#define TS_MAX_DELTA_MIN      10
#define TS_MAX_DELTA_DEFAULT  600
#define TS_WARN_DELTA_MIN     10
#define TS_WARN_DELTA_DEFAULT 30

/* NETWORK_NAME_DEFAULT and NETWORK_DESC_DEFAULT - these are used
 * instead of a servers name/description if you enable server hiding.
 */
#define NETWORK_NAME_DEFAULT "EFnet"
#define NETWORK_DESC_DEFAULT "Eris Free Network"

/* MAXIMUM LINKS - max links for class 0 if no Y: line configured
 *
 * This define is useful for leaf nodes and gateways. It keeps you from
 * connecting to too many places. It works by keeping you from
 * connecting to more than "n" nodes which you have C:blah::blah:6667
 * lines for.
 *
 * Note that any number of nodes can still connect to you. This only
 * limits the number that you actively reach out to connect to.
 *
 * Leaf nodes are nodes which are on the edge of the tree. If you want
 * to have a backup link, then sometimes you end up connected to both
 * your primary and backup, routing traffic between them. To prevent
 * this, #define MAXIMUM_LINKS 1 and set up both primary and
 * secondary with C:blah::blah:6667 lines. THEY SHOULD NOT TRY TO
 * CONNECT TO YOU, YOU SHOULD CONNECT TO THEM.
 *
 * Gateways such as the server which connects Australia to the US can
 * do a similar thing. Put the American nodes you want to connect to
 * in with C:blah::blah:6667 lines, and the Australian nodes with
 * C:blah::blah lines. Have the Americans put you in with C:blah::blah
 * lines. Then you will only connect to one of the Americans.
 *
 * This value is only used if you don't have server classes defined, and
 * a server is in class 0 (the default class if none is set).
 *
 */
#define MAXIMUM_LINKS_DEFAULT 1

/* CMDLINE_CONFIG - allow conf-file to be specified on command line
 * NOTE: defining CMDLINE_CONFIG and installing ircd SUID or SGID is a MAJOR
 * security problem - they can use the "-f" option to read any files
 * that the 'new' access lets them.
 */
#define CMDLINE_CONFIG

/* INIT_LOG_LEVEL - what level of information is logged to ircd.log
 * options are:
 *   L_CRIT, L_ERROR, L_WARN, L_NOTICE, L_TRACE, L_INFO, L_DEBUG
 */
#define INIT_LOG_LEVEL L_NOTICE

/* USE_LOGFILE - log errors and such to LPATH
 * If you wish to have the server send 'vital' messages about server
 * to a logfile, define USE_LOGFILE.
 */
#define USE_LOGFILE

/* USE_SYSLOG - log errors and such to syslog()
 * If you wish to have the server send 'vital' messages about server
 * through syslog, define USE_SYSLOG. Only system errors and events critical
 * to the server are logged although if this is defined with FNAME_USERLOG,
 * syslog() is used instead of the above file. It is not recommended that
 * this option is used unless you tell the system administrator beforehand
 * and obtain their permission to send messages to the system log files.
 */
#undef USE_SYSLOG

#ifdef  USE_SYSLOG
/* SYSLOG_KILL SYSLOG_SQUIT SYSLOG_CONNECT SYSLOG_USERS SYSLOG_OPER
 * If you use syslog above, you may want to turn some (none) of the
 * spurious log messages for KILL,SQUIT,etc off.
 */
#undef  SYSLOG_KILL     /* log all operator kills to syslog */
#undef  SYSLOG_SQUIT    /* log all remote squits for all servers to syslog */
#undef  SYSLOG_CONNECT  /* log remote connect messages for other all servs */
#undef  SYSLOG_USERS    /* send userlog stuff to syslog */
#undef  SYSLOG_OPER     /* log all users who successfully become an Op */
#undef  SYSLOG_BLOCK_ALLOCATOR /* debug block allocator */

/* LOG_FACILITY - facility to use for syslog()
 * Define the facility you want to use for syslog().  Ask your
 * sysadmin which one you should use.
 */
#define LOG_FACILITY LOG_LOCAL4

#endif /* USE_SYSLOG */

/* CRYPT_OPER_PASSWORD - use crypted oper passwords in the ircd.conf
 * define this if you want to use crypted passwords for operators in your
 * ircd.conf file.
 */
#define CRYPT_OPER_PASSWORD

/* MAXSENDQLENGTH - Max amount of internal send buffering
 * Max amount of internal send buffering when socket is stuck (bytes)
 */
#define MAXSENDQLENGTH 7050000    /* Recommended value: 7050000 for EFnet */

/* CLIENT_FLOOD - client excess flood threshold
 * this controls the number of bytes the server will allow a client to
 * send to the server without processing before disconnecting the client for
 * flooding it.  Values greater than 8000 make no difference to the server.
 */
#define CLIENT_FLOOD    2560

/*   STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP  */

/* You shouldn't change anything below this line, unless absolutely needed. */

#define MAX_TARGETS_DEFAULT 4

/* INITIAL_DBUFS - how many dbufs to preallocate
 */
#define INITIAL_DBUFS 1000 /* preallocate 4 megs of dbufs */ 

/* MAXBUFFERS - increase socket buffers
 *
 * Increase send & receive socket buffer up to 64k,
 * keeps clients at 8K and only raises servers to 64K
 */
#define MAXBUFFERS

/* PORTNUM - default port where ircd resides
 * Port where ircd resides. NOTE: This *MUST* be greater than 1024 if you
 * plan to run ircd under any other UID than root.
 */
#define PORTNUM 6667

/* MAXCONNECTIONS - don't touch - change the HARD_FDLIMIT_ instead
 * Maximum number of network connections your server will allow.  This should
 * never exceed max. number of open file descriptors and wont increase this.
 * Should remain LOW as possible. Most sites will usually have under 30 or so
 * connections. A busy hub or server may need this to be as high as 50 or 60.
 * Making it over 100 decreases any performance boost gained from it being low.
 * if you have a lot of server connections, it may be worth splitting the load
 * over 2 or more servers.
 * 1 server = 1 connection, 1 user = 1 connection.
 * This should be at *least* 3: 1 listen port, 1 DNS port + 1 client
 */
/* change the HARD_FDLIMIT_ instead */
#define MAXCONNECTIONS  HARD_FDLIMIT

/* NICKNAMEHISTORYLENGTH - size of WHOWAS array
 * this defines the length of the nickname history.  each time a user changes
 * nickname or signs off, their old nickname is added to the top of the list.
 * NOTE: this is directly related to the amount of memory ircd will use whilst
 *       resident and running - it hardly ever gets swapped to disk!  Memory
 *       will be preallocated for the entire whowas array when ircd is started.
 */
#define NICKNAMEHISTORYLENGTH 15000

/* PINGFREQUENCY - ping frequency for idle connections
 * If daemon doesn't receive anything from any of its links within
 * PINGFREQUENCY seconds, then the server will attempt to check for
 * an active link with a PING message. If no reply is received within
 * (PINGFREQUENCY * 2) seconds, then the connection will be closed.
 */
#define PINGFREQUENCY    120    /* Recommended value: 120 */

/* CONNECTFREQUENCY - time to wait before auto-reconnecting
 * If the connection to to uphost is down, then attempt to reconnect every 
 * CONNECTFREQUENCY  seconds.
 */
#define CONNECTFREQUENCY 600    /* Recommended value: 600 */

/* HANGONGOODLINK and HANGONGOODLINK
 * Often net breaks for a short time and it's useful to try to
 * establishing the same connection again faster than CONNECTFREQUENCY
 * would allow. But, to keep trying on bad connection, we require
 * that connection has been open for certain minimum time
 * (HANGONGOODLINK) and we give the net few seconds to steady
 * (HANGONRETRYDELAY). This latter has to be long enough that the
 * other end of the connection has time to notice it broke too.
 * 1997/09/18 recommended values by ThemBones for modern EFnet
 */

#define HANGONRETRYDELAY 60     /* Recommended value: 30-60 seconds */
#define HANGONGOODLINK 3600     /* Recommended value: 30-60 minutes */

/* CONNECTTIMEOUT -
 * Number of seconds to wait for a connect(2) call to complete.
 * NOTE: this must be at *LEAST* 10.  When a client connects, it has
 * CONNECTTIMEOUT - 10 seconds for its host to respond to an ident lookup
 * query and for a DNS answer to be retrieved.
 */
#define CONNECTTIMEOUT  30      /* Recommended value: 30 */

/* KILLCHASETIMELIMIT -
 * Max time from the nickname change that still causes KILL
 * automatically to switch for the current nick of that user. (seconds)
 */
#define KILLCHASETIMELIMIT 90   /* Recommended value: 90 */

/* MAXCHANNELSPERUSER -
 * Max number of channels a user is allowed to join.
 */
#define MAXCHANNELSPERUSER  15  /* Recommended value: 15 */

/* 
 * anti spambot code
 * The defaults =should= be fine for the initial timers/counters etc.
 * they are all changeable at run time anyway
 *
 * join/leave 25 times and stay on each channel less than 60 seconds
 * opers will get a warning every 5 
 * if a client stays on a channel for at least 2 minutes, their
 * spam JOIN_LEAVE_COUNT goes down by 1
 */
#define MIN_JOIN_LEAVE_TIME  60
#define MAX_JOIN_LEAVE_COUNT  25
#define OPER_SPAM_COUNTDOWN   5 
#define JOIN_LEAVE_COUNT_EXPIRE_TIME 120

/*
 * Links rehash delay
 */
#define LINKS_DELAY_DEFAULT 300
/*
 * If the OS has SOMAXCONN use that value, otherwise
 * Use the value in HYBRID_SOMAXCONN for the listen(); backlog
 * try 5 or 25. 5 for AIX and SUNOS, 25 should work better for other OS's
*/
#define HYBRID_SOMAXCONN 25

/* DEBUGMODE is used mostly for internal development, it is likely
 * to make your client server very sluggish.
 * You usually shouldn't need this. -Dianora
*/
#undef DEBUGMODE               /* define DEBUGMODE to enable debugging mode.*/

/*
 * viconf option, if USE_RCS is defined, viconf will use rcs "ci"
 * to keep the conf file  under RCS control.
 */
#define USE_RCS

/*
 * this checks for various things that should never happen, but
 * might do due to bugs.  ircd might be slightly more efficient with 
 * these disabled, who knows. keep this enabled during development.
 */
#define INVARIANTS
/* ----------------- archaic and/or broken section -------------------- */
#undef DNS_DEBUG

/* ------------------------- END CONFIGURATION SECTION -------------------- */
#define MAX_CLIENTS INIT_MAXCLIENTS

#if defined(CLIENT_FLOOD) && ((CLIENT_FLOOD > 8000) || (CLIENT_FLOOD < 512))
error CLIENT_FLOOD needs redefining.
#endif

#if !defined(CLIENT_FLOOD)
error CLIENT_FLOOD undefined.
#endif

#if defined(DEBUGMODE) || defined(DNS_DEBUG)
#  define Debug(x) debug x
#  define LOGFILE LPATH
#else
#  define Debug(x) ;
#  define LOGFILE "/dev/null"
#endif

#define MIN_SPAM_NUM 5
#define MIN_SPAM_TIME 60

/* This may belong elsewhere... -dt */
#if defined( HAVE_LIBINTL ) && !defined ( HAVE_GETTEXT )
#define HAVE_GETTEXT 1
#endif
#if defined( HAVE_GETTEXT ) && defined( MSGPATH )
#define USE_GETTEXT 1
#define _(a)       (gettext(a))
#else
#undef USE_GETTEXT
#define _(a)       (a)
#endif

#define CONFIG_H_LEVEL_7

#endif /* INCLUDED_config_h */
