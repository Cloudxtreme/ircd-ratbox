/*
 *  ircd-ratbox: A slightly useful ircd.
 *  config.h: The ircd compile-time-configurable header.
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

#ifndef INCLUDED_config_h
#define INCLUDED_config_h

#include "setup.h"

#ifndef __vms
/* 
 * Directory paths and filenames for UNIX systems.
 * IRCD_PREFIX is set using ./configure --prefix, see INSTALL.
 * The other defaults should be fine.
 *
 * NOTE: CHANGING THESE WILL NOT ALTER THE DIRECTORY THAT FILES WILL
 *       BE INSTALLED TO.  IF YOU CHANGE THESE, DO NOT USE MAKE INSTALL,
 *       BUT COPY THE FILES MANUALLY TO WHERE YOU WANT THEM.
 *
 * IRCD_PREFIX = prefix for all directories,
 * DPATH       = root directory of installation,
 * BINPATH     = directory for binary files,
 * ETCPATH     = directory for configuration files,
 * LOGPATH     = directory for logfiles,
 * MODPATH     = directory for modules,
 * AUTOMODPATH = directory for autoloaded modules
 */

/* dirs */
#define DPATH   IRCD_PREFIX
#define BINPATH IRCD_PREFIX "/bin/"
#define MODPATH MODULE_DIR
#define AUTOMODPATH MODULE_DIR "/autoload/"
#define ETCPATH ETC_DIR 
#define LOGPATH LOG_DIR
#define UHPATH   HELP_DIR "/users"
#define HPATH  HELP_DIR "/opers"

/* files */
#define SPATH    BINPATH "/ircd"	/* ircd executable */
#define SLPATH   BINPATH "/servlink"	/* servlink executable */
#define CPATH    ETCPATH "/ircd.conf"	/* ircd.conf file */
#define KPATH    ETCPATH "/kline.conf"	/* kline file */
#define DLPATH   ETCPATH "/dline.conf"	/* dline file */
#define XPATH	 ETCPATH "/xline.conf"	/* xline file */
#define RESVPATH ETCPATH "/resv.conf"	/* resv file */
#define RPATH    ETCPATH "/ircd.rsa"	/* ircd rsa private keyfile */
#define MPATH    ETCPATH "/ircd.motd"	/* MOTD file */
#define LPATH    LOGPATH "/ircd.log"	/* ircd logfile */
#define PPATH    LOGPATH "/ircd.pid"	/* pid file */
#define OPATH    ETCPATH "/opers.motd"	/* oper MOTD file */

#else /* !__vms */

/* *PATH - directory locations and filenames for VMS.
 *
 * Non VMS systems see below.
 *
 * IRCD_PREFIX = prefix for all directories,
 * DPATH       = root directory of installation,
 * BINPATH     = directory for binary files,
 * ETCPATH     = directory for configuration files,
 * LOGPATH     = directory for logfiles,
 * MODPATH     = directory for autoloaded modules (disabled in VMS),
 */

/* IRCD_PREFIX not needed in VMS -larne */
/*#define IRCD_PREFIX     "IRCD$BASEDIR:"*/
#define DPATH           "IRCD$BASEDIR:"
#define BINPATH         "IRCD$BINDIR:"
#define ETCPATH         "IRCD$CONFDIR:"
#define LOGPATH         "IRCD$LOGDIR:"

#undef  MODPATH
#undef  AUTOMODPATH

#define SPATH    BINPATH "IRCD.EXE"	/* server executable */
#define SLPATH   BINPATH "SERVLINK.EXE"	/* servlink executable */
#define CPATH    ETCPATH "IRCD.CONF"	/* config file */
#define KPATH    ETCPATH "KLINE.CONF"	/* kline file */
#define DLPATH   ETCPATH "DLINE.CONF"	/* dline file */
#define XPATH	 ETCPATH "XLINE.CONF"	/* xline file */
#define RESVPATH ETCPATH "RESV.CONF"	/* resv file */
#define RPATH    ETCPATH "IRCD.RSA"	/* RSA private key file */
#define MPATH    ETCPATH "IRCD.MOTD"	/* MOTD filename */
#define LPATH    LOGPATH "IRCD.LOG"	/* logfile */
#define PPATH    ETCPATH "IRCD.PID"	/* pid file */
#define HPATH    ETCPATH "OPERS.TXT"	/* oper help file */
#define UHPATH   ETCPATH "USERS.TXT"	/* user help file */
#define OPATH    ETCPATH "OPERS.MOTD"	/* oper MOTD file */

#endif /* !__vms */

/* IGNORE_BOGUS_TS
 * Ignore bogus timestamps from other servers. Yes this will desync
 * the network, but it will allow chanops to resync with a valid non TS 0
 *
 * This should be enabled network wide, or not at all.
 */
#undef  IGNORE_BOGUS_TS

/* HIDE_SERVERS_IPS
 *
 * If this is undefined, anyone can see a servers ip.  If it is defined,
 * noone can.
 */
#define  HIDE_SERVERS_IPS

/* HIDE_SPOOF_IPS
 *
 * If this is undefined, opers will be allowed to see the real IP of spoofed
 * users in /trace etc.  If this is defined they will be shown a masked IP.
 */
#define HIDE_SPOOF_IPS

/* TS6_ONLY
 *
 * If this is defined only TS6 servers may link to the network.  See
 * doc/TS6.txt for more information.  If your network has old servers
 * (hyb7.0, ircd-ratbox-1.x, +CSr) or hybserv you should NOT define this.
 */
#undef TS6_ONLY

/* USE_LOGFILE - log errors and such to LPATH
 * If you wish to have the server send 'vital' messages about server
 * to a logfile, define USE_LOGFILE.
 */
#define USE_LOGFILE

/* CLIENT_FLOOD - client excess flood threshold(in messages)
 * The number of messages that we can receive before we disconnect the
 * remote client...
 */
#define CLIENT_FLOOD 20

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
#define HANGONRETRYDELAY 60	/* Recommended value: 30-60 seconds */
#define HANGONGOODLINK 3600	/* Recommended value: 30-60 minutes */

/* KILLCHASETIMELIMIT -
 * Max time from the nickname change that still causes KILL
 * automatically to switch for the current nick of that user. (seconds)
 */
#define KILLCHASETIMELIMIT 90	/* Recommended value: 90 */

/* RATBOX_SOMAXCONN
 * Use SOMAXCONN if OS has it, otherwise use this value for the 
 * listen(); backlog.  5 for AIX/SUNOS, 25 for other OSs.
 */
#define RATBOX_SOMAXCONN 25

/* ----------------------------------------------------------------
 * STOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOP
 * ----------------------------------------------------------------
 * The options below this line should NOT be modified.
 * ----------------------------------------------------------------
 */

/* MAX_BUFFER
 * The amount of fds to reserve for clients exempt from limits
 * and dns lookups.
 */
#define MAX_BUFFER      60

/* HARD_FDLIMIT_
 * The maximum amount of FDs to use.  MAX_CLIENTS is set in ./configure.
 */
#define HARD_FDLIMIT_    MAX_CLIENTS + MAX_BUFFER + 20

#define CONFIG_RATBOX_LEVEL_2

#include "defaults.h"
#endif /* INCLUDED_config_h */
