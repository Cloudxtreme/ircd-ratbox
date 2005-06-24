/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_info.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *   
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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
#include "tools.h"
#include "m_info.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"
#include "handlers.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <time.h>
#include <string.h>

struct Message info_msgtab = {
  MSG_INFO, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_info, ms_info, mo_info}
};

void
_modinit(void)
{
  mod_add_cmd(&info_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&info_msgtab);
}

void send_info_text(struct Client *sptr);
void send_birthdate_online_time(struct Client *sptr);
void send_conf_options(struct Client *sptr);

char *_version = "20010101";

/*
** m_info
**  parv[0] = sender prefix
**  parv[1] = servername
*/

int m_info(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  static time_t last_used=0L;

  if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME)
  {
    sendto_realops_flags(FLAGS_SPY, "info requested by %s (%s@%s) [%s]",
      sptr->name, sptr->username, sptr->host,
      sptr->user->server);

    if ((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
      {
        /* safe enough to give this on a local connect only */
        sendto_one(sptr,form_str(RPL_LOAD2HI),me.name,parv[0]);
        return 0;
      }
      else
        last_used = CurrentTime;

    send_info_text(sptr);
    send_birthdate_online_time(sptr);

    sendto_one(sptr, form_str(RPL_ENDOFINFO), me.name, parv[0]);
  } /* if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME) */

  return 0;
} /* m_info() */

/*
** mo_info
**  parv[0] = sender prefix
**  parv[1] = servername
*/
int mo_info(struct Client *cptr, struct Client *sptr, int parc, char *parv[])

{
  if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME)
  {
    sendto_realops_flags(FLAGS_SPY, "info requested by %s (%s@%s) [%s]",
      sptr->name, sptr->username, sptr->host,
      sptr->user->server);

    send_info_text(sptr);
    send_conf_options(sptr);
    send_birthdate_online_time(sptr);

    sendto_one(sptr, form_str(RPL_ENDOFINFO), me.name, parv[0]);
  } /* if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME) */

  return 0;
} /* mo_info() */

/*
** ms_info
**  parv[0] = sender prefix
**  parv[1] = servername
*/
int ms_info(struct Client *cptr, struct Client *sptr, int parc, char *parv[])

{
  if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME)
    {
      if(IsOper(sptr))
	mo_info(cptr,sptr,parc,parv);
      else
	m_info(cptr,sptr,parc,parv);
    }
  return 0;
} /* ms_info() */


/*
 * send_info_text
 *
 * inputs	- client pointer to send info text to
 * output	- none
 * side effects	- info text is sent to client
 */
void send_info_text(struct Client *sptr)
{
  char **text = infotext;

  while (*text)
    sendto_one(sptr, form_str(RPL_INFO), me.name, sptr->name, *text++);
  
  sendto_one(sptr, form_str(RPL_INFO), me.name, sptr->name, "");
}

/*
 * send_birthdate_online_time
 *
 * inputs	- client pointer to send to
 * output	- none
 * side effects	- birthdate and online time are sent
 */
void send_birthdate_online_time(struct Client *sptr)
{
  sendto_one(sptr,
	     ":%s %d %s :Birth Date: %s, compile # %s",
	     me.name,
	     RPL_INFO,
	     sptr->name,
	     creation,
	     generation);

  sendto_one(sptr,
	     ":%s %d %s :On-line since %s",
	     me.name,
	     RPL_INFO,
	     sptr->name,
	     myctime(me.firsttime));
}

/*
 * send_conf_options
 *
 * inputs	- client pointer to send to
 * output	- none
 * side effects	- send config options to client
 */
void send_conf_options(struct Client *sptr)
{
  Info *infoptr;

  /*
   * Now send them a list of all our configuration options
   * (mostly from config.h)
   */
  
  for (infoptr = MyInformation; infoptr->name; infoptr++)
    {
      if (infoptr->intvalue)
	sendto_one(sptr,
		   ":%s %d %s :%-30s %-5d [%-30s]",
		   me.name,
		   RPL_INFO,
		   sptr->name,
		   infoptr->name,
		   infoptr->intvalue,
		   infoptr->desc);
      else
	sendto_one(sptr,
		   ":%s %d %s :%-30s %-5s [%-30s]",
		   me.name,
		   RPL_INFO,
		   sptr->name,
		   infoptr->name,
		   infoptr->strvalue,
		   infoptr->desc);
    }

   /* now ircd.conf options */

   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "network_name",
              ConfigFileEntry.network_name ? 
                ConfigFileEntry.network_name :
                NETWORK_NAME_DEFAULT,
              "Name of the Network");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "network_desc",
              ConfigFileEntry.network_desc ?
                ConfigFileEntry.network_desc :
                NETWORK_DESC_DEFAULT,
              "Description of the network");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "fname_operlog",
              ConfigFileEntry.fname_operlog ?
                ConfigFileEntry.fname_operlog :
                "NONE",
              "Oper Log File");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "fname_foperlog",
              ConfigFileEntry.fname_foperlog ?
                ConfigFileEntry.fname_foperlog :
                "NONE",
              "Failed Oper Log File");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "fname_userlog",
              ConfigFileEntry.fname_userlog ?
                ConfigFileEntry.fname_userlog :
                "NONE",
              "User Log File");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "hub",
              ConfigFileEntry.hub ? "ON" : "OFF",
              "Server can connect to more than one server");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "failed_oper_notice",
              ConfigFileEntry.failed_oper_notice ? "ON" : "OFF",
              "Show opers a notice if someone uses oper with the wrong password");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "show_failed_oper_id",
              ConfigFileEntry.show_failed_oper_id ? "ON" : "OFF",
              "Also show a notice if the oper has the wrong user@host");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "dots_in_ident",
              ConfigFileEntry.dots_in_ident,
              "How many dots are allowed in idents");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "anti_nick_flood",
              ConfigFileEntry.anti_nick_flood ? "ON" : "OFF",
              "Enable anti nick flood code");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "max_nick_time",
              ConfigFileEntry.max_nick_time,
              "Anti nick flood time setting");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "max_nick_changes",
              ConfigFileEntry.max_nick_changes,
              "How many nick changes to allow");
    sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "links_delay",
              ConfigFileEntry.links_delay,
              "How often the links file is rehashed");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "ts_max_delta",
              ConfigFileEntry.ts_max_delta ?
                ConfigFileEntry.ts_max_delta :
                TS_MAX_DELTA_DEFAULT,
              "Maximum Allowed TS Delta from another Server");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "ts_warn_delta",
              ConfigFileEntry.ts_warn_delta ?
                ConfigFileEntry.ts_warn_delta :
                TS_WARN_DELTA_DEFAULT,
              "Maximum TS Delta before Sending Warning");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "vchans_oper_only",
              ConfigFileEntry.vchans_oper_only ? "YES" : "NO",
              "Restrict use of /CJOIN to opers");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "kline_with_reason",
              ConfigFileEntry.kline_with_reason ? "YES" : "NO",
              "Show K-line Reason to Client on Exit");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "kline_with_connection_closed",
              ConfigFileEntry.kline_with_connection_closed ? "YES" : "NO",
              "Signoff reason: Connection closed");

  sendto_one(sptr,
	     ":%s %d %s :Compiled on [%s]",
	     me.name, 
	     RPL_INFO,
	     sptr->name,
	     platform); 

  sendto_one(sptr, form_str(RPL_INFO), me.name, sptr->name, "");
}

