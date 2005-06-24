/************************************************************************
 *   IRC - Internet Relay Chat, include/msg.h
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
 *
 * $Id$
 */
#ifndef INCLUDED_msg_h
#define INCLUDED_msg_h
#ifndef INCLUDED_config_h
#include "config.h"
#endif

#ifndef INCLUDED_ircd_handler_h
#define INCLUDED_ircd_handler_h
#include "ircd_handler.h"
#endif

struct Client;

/* 
 * Message table structure 
 */
struct  Message
{
  char  *cmd;
  unsigned int  count;                  /* number of times command used */
  unsigned int  parameters; /* at least this many args must be passed or an error
			       will be sent to the user before the m_func is even called */
  unsigned int  flags;  /* bit 0 set means that this command is allowed
			   to be used only on the average of once per 2
			   seconds -SRB */
  unsigned long bytes;  /* bytes received for this message */
  /*
   * cptr = Connected client ptr
   * sptr = Source client ptr
   * parc = parameter count
   * parv = parameter variable array
   */
  /* handlers:
   * UNREGISTERED, CLIENT, SERVER, OPER, LAST
   */
  MessageHandler handlers[LAST_HANDLER_TYPE];
};

struct MessageTree
{
  char*               final;
  struct Message*     msg;
  struct MessageTree* pointers[26];
}; 

typedef struct MessageTree MESSAGE_TREE;

#ifdef DBOP
#define MSG_DBOP     "DBOP"
#endif

#define MSG_PRIVMSG  "PRIVMSG"  /* PRIV */
#define MSG_CBURST   "CBURST"   /* LazyLink channel burst */
#define MSG_DROP     "DROP"     /* LazyLink channel drop */
#define MSG_LLJOIN   "LLJOIN"   /* LazyLink join */
#define MSG_WHO      "WHO"      /* WHO  -> WHOC */
#define MSG_WHOIS    "WHOIS"    /* WHOI */
#define MSG_WHOWAS   "WHOWAS"   /* WHOW */
#define MSG_USER     "USER"     /* USER */
#define MSG_NICK     "NICK"     /* NICK */
#define MSG_SERVER   "SERVER"   /* SERV */
#define MSG_LIST     "LIST"     /* LIST */
#define MSG_TOPIC    "TOPIC"    /* TOPI */
#define MSG_INVITE   "INVITE"   /* INVI */
#define MSG_VERSION  "VERSION"  /* VERS */
#define MSG_QUIT     "QUIT"     /* QUIT */
#define MSG_SQUIT    "SQUIT"    /* SQUI */
#define MSG_KILL     "KILL"     /* KILL */
#define MSG_INFO     "INFO"     /* INFO */
#define MSG_LINKS    "LINKS"    /* LINK */
#define MSG_STATS    "STATS"    /* STAT */
#define MSG_USERS    "USERS"    /* USER -> USRS */
#define MSG_HELP     "HELP"     /* HELP */
#define MSG_ERROR    "ERROR"    /* ERRO */
#define MSG_AWAY     "AWAY"     /* AWAY */
#define MSG_CONNECT  "CONNECT"  /* CONN */
#define MSG_PING     "PING"     /* PING */
#define MSG_PONG     "PONG"     /* PONG */
#define MSG_OPER     "OPER"     /* OPER */
#define MSG_PASS     "PASS"     /* PASS */
#define MSG_WALLOPS  "WALLOPS"  /* WALL */
#define MSG_TIME     "TIME"     /* TIME */
#define MSG_NAMES    "NAMES"    /* NAME */
#define MSG_ADMIN    "ADMIN"    /* ADMI */
#define MSG_TRACE    "TRACE"    /* TRAC */
#define MSG_LTRACE   "LTRACE"   /* LTRA */
#define MSG_NOTICE   "NOTICE"   /* NOTI */
#define MSG_JOIN     "JOIN"     /* JOIN */
#define MSG_CJOIN    "CJOIN"    /* CJOIN */
#define MSG_PART     "PART"     /* PART */
#define MSG_LUSERS   "LUSERS"   /* LUSE */
#define MSG_MOTD     "MOTD"     /* MOTD */
#define MSG_MODE     "MODE"     /* MODE */
#define MSG_KICK     "KICK"     /* KICK */
#define MSG_USERHOST "USERHOST" /* USER -> USRH */
#define MSG_ISON     "ISON"     /* ISON */
#define MSG_REHASH   "REHASH"   /* REHA */
#define MSG_RESTART  "RESTART"  /* REST */
#define MSG_CLOSE    "CLOSE"    /* CLOS */
#define MSG_SVINFO   "SVINFO"   /* SVINFO */
#define MSG_SJOIN    "SJOIN"    /* SJOIN */
#define MSG_CAPAB    "CAPAB"    /* CAPAB */
#define MSG_DIE      "DIE"      /* DIE */
#define MSG_HASH     "HASH"     /* HASH */
#define MSG_DNS      "DNS"      /* DNS  -> DNSS */
#define MSG_KLINE    "KLINE"    /* KLINE */
#define MSG_UNKLINE  "UNKLINE"  /* UNKLINE */
#define MSG_DLINE    "DLINE"    /* DLINE */
#define MSG_HTM      "HTM"      /* HTM */
#define MSG_SET      "SET"      /* SET */
#define MSG_GLINE    "GLINE"    /* GLINE */
#define MSG_KNOCK    "KNOCK"    /* KNOCK */
#define MSG_OPERWALL "OPERWALL" /* OPERWALL */
#define MSG_ZIP      "ZIP"      /* ZIP */

/*
 * Constants
 */
#define   MFLG_SLOW              0x01   /* Command can be executed roughly    *
                                         * once per 2 seconds.                */
#define   MFLG_UNREG             0x02   /* Command available to unregistered  *
                                         * clients.                           */
#define   MFLG_IGNORE            0x04   /* silently ignore command from
                                         * unregistered clients */
#define MAXPARA    15 

#define MSG_TESTLINE "TESTLINE"

extern struct Message msgtab[];

#endif /* INCLUDED_msg_h */

