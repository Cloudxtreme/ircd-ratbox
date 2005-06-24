/************************************************************************
 *   IRC - Internet Relay Chat, ircd/m_info.h
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
#ifndef INCLUDED_m_info_h
#define INCLUDED_m_info_h
#ifndef INCLUDED_config_h
#include "config.h"
#endif

typedef struct Information
{
  char* name;        /* name of item */
  char* strvalue;    /* value of item if it's a boolean */
  int   intvalue;    /* value of item if it's an integer */
  char* desc;        /* short description of item */
} Info;

Info MyInformation[] = {

#ifdef ANTI_SPAM_EXIT_MESSAGE
  { "ANTI_SPAM_EXIT_MESSAGE", "ON", 0, "Do not broadcast Spam Bots' exit messages" },
#else
  { "ANTI_SPAM_EXIT_MESSAGE", "OFF", 0, "Do not broadcast Spam Bots' exit messages" },
#endif /* ANTI_SPAM_EXIT_MESSAGE */

#ifdef ANTI_SPAM_EXIT_MESSAGE_TIME
  { "ANTI_SPAM_EXIT_MESSAGE_TIME", "", ANTI_SPAM_EXIT_MESSAGE_TIME, "Delay before Allowing Spam Bot Exit Messages" },
#else
  { "ANTI_SPAM_EXIT_MESSAGE_TIME", "NONE", 0, "Delay before Allowing Spam Bot Exit Messages" },
#endif /* ANTI_SPAM_EXIT_MESSAGE_TIME */

#ifdef CLIENT_FLOOD
  { "CLIENT_FLOOD", "", CLIENT_FLOOD, "Client Excess Flood Threshold" },
#else
  { "CLIENT_FLOOD", "OFF", 0, "Client Excess Flood Threshold" },
#endif /* CLIENT_FLOOD */

#ifdef CMDLINE_CONFIG
  { "CMDLINE_CONFIG", "ON", 0, "Allow Command Line Specification of Config File" },
#else
  { "CMDLINE_CONFIG", "OFF", 0, "Allow Command Line Specification of Config File" },
#endif /* CMDLINE_CONFIG */

#ifdef CPATH
  { "CPATH", CPATH, 0, "Path to Main Configuration File" },
#else
  { "CPATH", "NONE", 0, "Path to Main Configuration File" },
#endif /* CPATH */

#ifdef CRYPT_OPER_PASSWORD
  { "CRYPT_OPER_PASSWORD", "ON", 0, "Encrypt Operator Passwords" },
#else
  { "CRYPT_OPER_PASSWORD", "OFF", 0, "Encrypt Operator Passwords" },
#endif /* CRYPT_OPER_PASSWORD */

#ifdef CRYPT_LINK_PASSWORD
  { "CRYPT_LINK_PASSWORD", "ON", 0, "Encrypt Server Passwords" },
#else
  { "CRYPT_LINK_PASSWORD", "OFF", 0, "Encrypt Server Passwords" },
#endif /* CRYPT_LINK_PASSWORD */

#ifdef DEBUGMODE
  { "DEBUGMODE", "ON", 0, "Debugging Mode" },
#else
  { "DEBUGMODE", "OFF", 0, "Debugging Mode" },
#endif /* DEBUGMODE */

#ifdef DPATH
  { "DPATH", DPATH, 0, "Directory Containing Configuration Files" },
#else
  { "DPATH", "NONE", 0, "Directory Containing Configuration Files" },
#endif /* DPATH */

#ifdef DLPATH
  { "DLPATH", DLPATH, 0, "Path to D-line File" },
#else
  { "DLPATH", "NONE", 0, "Path to D-line File" },
#endif /* DLPATH */

  { "HARD_FDLIMIT_", "", HARD_FDLIMIT_, "Maximum Number of File Descriptors Available" },

#ifdef HPATH
  { "HPATH", HPATH, 0, "Path to Operator Help File" },
#else
  { "HPATH", "NONE", 0, "Path to Operator Help File" },
#endif /* HPATH */

#ifdef SOMAXCONN
  { "HYBRID_SOMAXCONN", "", SOMAXCONN, "Maximum Queue Length of Pending Connections" },
#else
  { "HYBRID_SOMAXCONN", "", HYBRID_SOMAXCONN, "Maximum Queue Length of Pending Connections" },
#endif /* SOMAXCONN */

  { "INIT_MAXCLIENTS", "", INIT_MAXCLIENTS, "Maximum Clients" },
  { "INITIAL_DBUFS", "", INITIAL_DBUFS, "Number of Dbufs to PreAllocate" },

  { "JOIN_LEAVE_COUNT_EXPIRE_TIME", "", JOIN_LEAVE_COUNT_EXPIRE_TIME, "Anti SpamBot Parameter" },

  { "KILLCHASETIMELIMIT", "", KILLCHASETIMELIMIT, "Nick Change Tracker for KILL" },

#ifdef KPATH
  { "KPATH", KPATH, 0, "Path to K-line File" },
#else
  { "KPATH", "NONE", 0, "Path to K-line File" },
#endif /* KPATH */

#ifdef LPATH
  { "LPATH", LPATH, 0, "Path to Log File" },
#else
  { "LPATH", "NONE", 0, "Path to Log File" },
#endif /* LPATH */

  { "MAX_BUFFER", "", MAX_BUFFER, "Maximum Buffer Connections Allowed" },

  { "MAX_JOIN_LEAVE_COUNT", "", MAX_JOIN_LEAVE_COUNT, "Anti SpamBot Parameter" },

  { "MAXCHANNELSPERUSER", "", MAXCHANNELSPERUSER, "Maximum Channels per User" },
  { "MAXIMUM_LINKS_DEFAULT", "", MAXIMUM_LINKS_DEFAULT, "Default Maximum Links for Class 0" },

  { "MIN_JOIN_LEAVE_TIME", "", MIN_JOIN_LEAVE_TIME, "Anti SpamBot Parameter" },

#ifdef MPATH
  { "MPATH", MPATH, 0, "Path to MOTD File" },
#else
  { "MPATH", "NONE", 0, "Path to MOTD File" },
#endif /* MPATH */

  { "NICKNAMEHISTORYLENGTH", "", NICKNAMEHISTORYLENGTH, "Size of WHOWAS Array" },

#ifdef OPATH
  { "OPATH", OPATH, 0, "Path to Operator MOTD File" },
#else
  { "OPATH", "NONE", 0, "Path to Operator MOTD File" },
#endif /* OPATH */

  { "OPER_SPAM_COUNTDOWN", "", OPER_SPAM_COUNTDOWN, "Anti SpamBot Parameter" },

#ifdef OPENSSL
  { "OPENSSL", "ON", 0, "Enable OpenSSL CHALLENGE Support" },
#else
  { "OPENSSL", "OFF", 0, "Enable OpenSSL CHALLENGE Support" },
#endif /* OPENSSL */

#ifdef PPATH
  { "PPATH", PPATH, 0, "Path to Pid File" },
#else
  { "PPATH", "NONE", 0, "Path to Pid File" },
#endif /* PPATH */

#ifdef REPORT_DLINE_TO_USER
  { "REPORT_DLINE_TO_USER", "ON", 0, "Inform Clients They are D-lined" },
#else
  { "REPORT_DLINE_TO_USER", "OFF", 0, "Inform Clients They are D-lined" },
#endif /* REPORT_DLINE_TO_USER */

  { "SELECT_TYPE", SELECT_TYPE, 0, "Method of Multiplexed I/O" },

#ifdef SPATH
  { "SPATH", SPATH, 0, "Path to Server Executable" },
#else
  { "SPATH", "NONE", 0, "Path to Server Executable" },
#endif /* SPATH */

  { "TS_MAX_DELTA_DEFAULT", "", TS_MAX_DELTA_DEFAULT, "Maximum Allowed TS Delta from another Server" },
  { "TS_WARN_DELTA_DEFAULT", "", TS_WARN_DELTA_DEFAULT, "Maximum TS Delta before Sending Warning" },

#ifdef USE_GETTEXT
  { "USE_GETTEXT", "ON", 0, "Use gettext() for message customisation at runtime" },
#else
  { "USE_GETTEXT", "OFF", 0, "Use gettext() for message customisation at runtime" },
#endif /* USE_GETTEXT */
  
#ifdef USE_RCS
  { "USE_RCS", "ON", 0, "Use \"ci\" to Keep RCS Control" },
#else
  { "USE_RCS", "OFF", 0, "Use \"ci\" to Keep RCS Control" },
#endif /* USE_RCS */

#ifdef USE_SYSLOG
  { "USE_SYSLOG", "ON", 0, "Log Errors to syslog file" },
#else
  { "USE_SYSLOG", "OFF", 0, "Log Errors to syslog file" },
#endif /* USE_SYSLOG */

  /*
   * since we don't want to include the world here, NULL probably
   * isn't defined by the time we read this, just use plain 0 instead
   * 0 is guaranteed by the language to be assignable to ALL built
   * in types with the correct results.
   */
  { 0, 0, 0, 0 }
};


#endif /* INCLUDED_m_info_h */

