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
  unsigned int  count;      /* number of times command used */
  unsigned int  parameters; /* at least this many args must be passed
                             * or an error will be sent to the user 
                             * before the m_func is even called 
                             */
  unsigned int  maxpara;    /* maximum permitted parameters */
  unsigned int  flags;      /* bit 0 set means that this command is allowed
			     * to be used only on the average of once per 2
			     * seconds -SRB
			     */
  unsigned long bytes;  /* bytes received for this message */
  /*
   * client_p = Connected client ptr
   * source_p = Source client ptr
   * parc = parameter count
   * parv = parameter variable array
   */
  /* handlers:
   * UNREGISTERED, CLIENT, SERVER, OPER, LAST
   */
  MessageHandler handlers[LAST_HANDLER_TYPE];
};

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

#endif /* INCLUDED_msg_h */
