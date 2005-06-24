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

static void send_conf_options(struct Client *sptr);
static void send_birthdate_online_time(struct Client *sptr);
static void send_info_text(struct Client *sptr);

static void m_info(struct Client*, struct Client*, int, char**);
static void ms_info(struct Client*, struct Client*, int, char**);
static void mo_info(struct Client*, struct Client*, int, char**);

struct Message info_msgtab = {
  "INFO", 0, 0, 0, MFLG_SLOW, 0,
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

char *_version = "20010109";

/*
** m_info
**  parv[0] = sender prefix
**  parv[1] = servername
*/

static void m_info(struct Client *cptr, struct Client *sptr,
                  int parc, char *parv[])
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
        return;
      }
      else
        last_used = CurrentTime;

    send_info_text(sptr);
    send_birthdate_online_time(sptr);

    sendto_one(sptr, form_str(RPL_ENDOFINFO), me.name, parv[0]);
  } /* if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME) */
} /* m_info() */

/*
** mo_info
**  parv[0] = sender prefix
**  parv[1] = servername
*/
static void mo_info(struct Client *cptr, struct Client *sptr,
                   int parc, char *parv[])

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
} /* mo_info() */

/*
** ms_info
**  parv[0] = sender prefix
**  parv[1] = servername
*/
static void ms_info(struct Client *cptr, struct Client *sptr,
                   int parc, char *parv[])

{
  if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME)
    {
      if(IsOper(sptr))
	mo_info(cptr,sptr,parc,parv);
      else
	m_info(cptr,sptr,parc,parv);
    }
} /* ms_info() */


/*
 * send_info_text
 *
 * inputs	- client pointer to send info text to
 * output	- none
 * side effects	- info text is sent to client
 */
static void send_info_text(struct Client *sptr)
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
static void send_birthdate_online_time(struct Client *sptr)
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
static void send_conf_options(struct Client *sptr)
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
   /* This DESPERATELY needs tabularized... */

   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "network_name",
              ServerInfo.network_name ? 
                ServerInfo.network_name :
                NETWORK_NAME_DEFAULT,
              "Name of the Network");
   sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name,
              RPL_INFO,
              sptr->name,
              "network_desc",
              ServerInfo.network_desc ?
                ServerInfo.network_desc :
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
              ServerInfo.hub ? "ON" : "OFF",
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
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name, RPL_INFO, sptr->name, "non_redundant_klines",
              ConfigFileEntry.non_redundant_klines ? "YES" : "NO",
              "Check for and Disallow Redundant K-lines");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name, RPL_INFO, sptr->name, "warn_no_nline",
              ConfigFileEntry.warn_no_nline ? "YES" : "NO",
              "Show Notices of Servers Connecting Without an N: line");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name, RPL_INFO, sptr->name, "o_lines_oper_only",
              ConfigFileEntry.o_lines_oper_only ? "YES" : "NO",
              "Only Allow Operators to see STATS o");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name, RPL_INFO, sptr->name, "glines",
              ConfigFileEntry.glines ? "YES" : "NO",
              "Network wide K-lines");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name, RPL_INFO, sptr->name, "gline_time",
              ConfigFileEntry.gline_time,
              "Expire Time for Glines");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name, RPL_INFO, sptr->name, "knock_delay",
              ConfigFileEntry.knock_delay,
              "Delay between KNOCK Attempts");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name, RPL_INFO, sptr->name, "short_motd",
              ConfigFileEntry.short_motd ? "YES" : "NO",
              "Notice Clients They should Read MOTD");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name, RPL_INFO, sptr->name, "client_exit",
              ConfigFileEntry.client_exit ? "YES" : "NO",
              "Prepend 'Client Exit:' to User QUIT Message");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name, RPL_INFO, sptr->name, "hide_server",
              GlobalSetOptions.hide_server ? "YES" : "NO",
              "Hide server info in WHOIS, netsplits, and hide topology");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5s [%-30s]",
              me.name, RPL_INFO, sptr->name, "quiet_on_ban",
              ConfigFileEntry.quiet_on_ban ? "YES" : "NO",
              "Banned users may not send text to a channel");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name, RPL_INFO, sptr->name, "max_targets",
              ConfigFileEntry.max_targets,
              "The maximum number of PRIVMSG/NOTICE targets");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name, RPL_INFO, sptr->name, "maximum_links",
              ConfigFileEntry.maximum_links,
              "Maximum Links for Class default");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name, RPL_INFO, sptr->name, "idletime",
              ConfigFileEntry.idletime,
              "Delay (in minutes) before a client is considered idle");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name, RPL_INFO, sptr->name, "no_oper_flood",
              ConfigFileEntry.no_oper_flood,
              "Disable Flood Control for Operators");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name, RPL_INFO, sptr->name, "pace_wait",
              ConfigFileEntry.pace_wait,
              "Minimum Delay between uses of certain commands");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name, RPL_INFO, sptr->name, "caller_id_wait",
              ConfigFileEntry.caller_id_wait,
              "Minimum Delay between notifying +g users of messages");
  sendto_one(sptr,
              ":%s %d %s :%-30s %-5d [%-30s]",
              me.name, RPL_INFO, sptr->name, "whois_wait",
              ConfigFileEntry.whois_wait,
              "Delay between Remote uses of WHOIS");
  /* Don't send oper_only_umodes...it's a bit mask, we will have to decode it
  ** in order for it to show up properly to opers who issue INFO
  */

  sendto_one(sptr,
	     ":%s %d %s :Compiled on [%s]",
	     me.name, 
	     RPL_INFO,
	     sptr->name,
	     platform); 

  sendto_one(sptr, form_str(RPL_INFO), me.name, sptr->name, "");
}

