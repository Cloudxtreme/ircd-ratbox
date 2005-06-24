/************************************************************************
 *   IRC - Internet Relay Chat, src/ircd_parser.y
 *   Copyright (C) 2000 Diane Bruce <db@db.net>
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                    University of Oulu, Co Center
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

%{
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include "irc_string.h"
#include "common.h"
#include "s_conf.h"
#include "y.tab.h"

#undef YY_INPUT
#define YY_FATAL_ERROR(msg) conf_yy_fatal_error( msg )
#define YY_INPUT(buf,result,max_size) \
	   if ((result = conf_fbgets( buf, max_size, conf_fbfile_in )) == 0) \
		YY_FATAL_ERROR( "input in flex scanner failed" ); 


int  lineno;
char linebuf[IRCD_BUFSIZE];
%}

ws		[ \t]*
comment		#.*
qstring		\"[^\"\n]*[\"\n]

%%
\n.*		{ 
		  strcpy(linebuf, yytext+1); 
		  lineno++;
                  yyless(1);
                }
{ws}		;
{comment}	;

[0-9]+		{ yylval.number = atoi(yytext); return NUMBER; }
([0-9]{1,3}\.){1,3}[0-9]{1,3}(\/[0-3][0-9])?  {

	if(is_address(yytext,&yylval.ip_entry.ip,&yylval.ip_entry.ip_mask))
	  return( IP_TYPE );
	 }

{qstring}	{ DupString(yylval.string,strdup(yytext+1));
		  if(yylval.string[yyleng-2] != '"')
		    fprintf(stderr,"Unterminated character string\n");
		  else
		    yylval.string[yyleng-2] = '\0'; /* remove close quote */
		  return QSTRING;
		}

accept		{ return ACCEPT; }
accept_password { return ACCEPT_PASSWORD; }
action		{ return ACTION; }
admin		{ return ADMIN; }
administrator	{ return ADMIN; }
allow_bots	{ return ALLOW_BOTS; }
auth		{ return AUTH; }
autoconn	{ return AUTOCONN; }
class		{ return CLASS; }
compressed	{ return COMPRESSED; }
connect		{ return CONNECT; }
deny		{ return DENY; }
description	{ return DESCRIPTION; }
die		{ return DIE; }
email		{ return EMAIL; }
exceed_limit    { return EXCEED_LIMIT; }
gecos		{ return GECOS; }
general		{ return GENERAL; }
gline		{ return GLINE; }
gline_file      { return GLINE_FILE; }
gline_time      { return GLINE_TIME; }
gline_log       { return GLINE_LOG; }
global		{ return GLOBAL; }
global_kill	{ return GLOBAL_KILL; }
have_ident	{ return HAVE_IDENT; }
host		{ return HOST; }
hub		{ return HUB; }
hub_mask	{ return HUB_MASK; }
include		{ return INCLUDE; }
ip		{ return IP; }
kill		{ return KILL; }
kline		{ return KLINE; }
kline_exempt	{ return KLINE_EXEMPT; }
l_crit		{ return T_L_CRIT; }
l_debug		{ return T_L_DEBUG; }
l_error		{ return T_L_ERROR; }
l_info		{ return T_L_INFO; }
l_notice 	{ return T_L_NOTICE; }
l_trace		{ return T_L_TRACE; }
l_warn		{ return T_L_WARN; }
lazylink	{ return LAZYLINK; }
leaf		{ return LEAF; }
leaf_mask	{ return LEAF_MASK; }
listen		{ return LISTEN; }
log_level	{ return LOG_LEVEL; }
logging         { return LOGGING; }
logpath         { return LOGPATH; }
max_number	{ return MAX_NUMBER; }
name		{ return NAME; }
nick_changes	{ return NICK_CHANGES; }
no		{ return TNO; }
no_tilde	{ return NO_TILDE; }
no_oper_flood   { return NO_OPER_FLOOD; }
number_per_ip	{ return NUMBER_PER_IP; }
oper		{ return OPERATOR; }
oper_log        { return OPER_LOG; }
operator	{ return OPERATOR; }
passwd		{ return PASSWORD; }
password	{ return PASSWORD; }
ping_time	{ return PING_TIME; }
port		{ return PORT; }
quarantine	{ return QUARANTINE; }
reason		{ return REASON; }
rehash		{ return REHASH; }
reject		{ return TREJECT; }
remote		{ return REMOTE; }
send_password	{ return SEND_PASSWORD; }
sendq		{ return SENDQ; }
serverinfo	{ return SERVERINFO; }
short_motd      { return SHORT_MOTD; }
spoof		{ return SPOOF; }
unkline		{ return UNKLINE; }
user		{ return USER;} 
vhost		{ return VHOST; }
warn		{ return WARN; }
yes		{ return TYES; }

quiet_on_ban            { return QUIET_ON_BAN; }
failed_oper_notice      { return FAILED_OPER_NOTICE; }
show_failed_oper_passwd { return SHOW_FAILED_OPER_PASSWD; }
show_failed_oper_id     { return SHOW_FAILED_OPER_ID; }
max_nick_changes        { return MAX_NICK_CHANGES; }
max_nick_time           { return MAX_NICK_TIME; }
anti_nick_flood         { return ANTI_NICK_FLOOD; }
ts_max_delta            { return TS_MAX_DELTA; }
ts_warn_delta           { return TS_WARN_DELTA; }
kline_with_connection_closed { return KLINE_WITH_CONNECTION_CLOSED; }
kline_with_reason       { return KLINE_WITH_REASON; }
warn_no_nline           { return WARN_NO_NLINE; }
non_redundant_klines    { return NON_REDUNDANT_KLINES; }
botcheck { return BOTCHECK; }

b_lines_oper_only       { return B_LINES_OPER_ONLY; }
e_lines_oper_only       { return E_LINES_OPER_ONLY; }
f_lines_oper_only       { return F_LINES_OPER_ONLY; }
stats_notice            { return STATS_NOTICE; }
whois_notice            { return WHOIS_NOTICE; }
whois_wait              { return WHOIS_WAIT; }
pace_wait               { return PACE_WAIT; }
knock_delay             { return KNOCK_DELAY; }
wallops_wait            { return WALLOPS_WAIT; }
pace_wallops            { return PACE_WALLOPS; }
iauth_server            { return IAUTH_SERVER; }
iauth_port		{ return IAUTH_PORT; }
stats_p_notice	        { return STATS_P_NOTICE; }
invite_plus_i_only      { return INVITE_PLUS_I_ONLY; }
glines                  { return GLINES; }
topic_uh                { return TOPIC_UH; }
.		        { return yytext[0]; }

%%

