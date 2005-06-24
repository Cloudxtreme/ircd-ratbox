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

%option case-insensitive
%{
/* XXX - No we aren't but lex calls malloc :(. It is even worse that it
   doesn't call free! */
#define WE_ARE_MEMORY_C
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include "tools.h"
#include "irc_string.h"
#include "common.h"
#include "s_conf.h"
#include "y.tab.h"
#include "memory.h"

char *ip_string;

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
          {
            ip_string = yytext;
	    return( IP_TYPE );
          }
	 }

([0-9][a-f]{0,4}\:){1,7}[0-9][1-f]{0,4}(\/[0-9]+)?  {

	if(is_ipv6_address(yytext,(unsigned char*)&yylval.ip_entry.ip,
	   (unsigned char*)&yylval.ip_entry.ip_mask))
          {
            ip_string = yytext;
	    return( IPV6_TYPE );
          }
	 }

{qstring}	{ yylval.string = yytext+1;
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
aftype		{ return AFTYPE; }
auth		{ return AUTH; }
autoconn	{ return AUTOCONN; }
caller_id_wait	{ return CALLER_ID_WAIT; }
class		{ return CLASS; }
client_exit     { return CLIENT_EXIT; }
connect		{ return CONNECT; }
connectfreq     { return CONNECTFREQ; }
deny		{ return DENY; }
description	{ return DESCRIPTION; }
die		{ return DIE; }
dots_in_ident   { return DOTS_IN_IDENT; }
encrypted       { return ENCRYPTED; }
email		{ return EMAIL; }
exceed_limit    { return EXCEED_LIMIT; }
fakename	{ return FAKENAME; }
fname_userlog   { return FNAME_USERLOG; }
fname_operlog   { return FNAME_OPERLOG; }
fname_foperlog  { return FNAME_FOPERLOG; }
gecos		{ return GECOS; }
general		{ return GENERAL; }
gline		{ return GLINE; }
gline_exempt    { return GLINE_EXEMPT; }
gline_time      { return GLINE_TIME; }
gline_log       { return GLINE_LOG; }
global_kill	{ return GLOBAL_KILL; }
have_ident	{ return HAVE_IDENT; }
hide_server	{ return HIDESERVER; }
host		{ return HOST; }
hub		{ return HUB; }
hub_mask	{ return HUB_MASK; }
idletime        { return IDLETIME; }
include		{ return INCLUDE; }
ip		{ return IP; }
ipv4            { return T_IPV4; }
ipv6            { return T_IPV6; }
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
leaf_mask	{ return LEAF_MASK; }
listen		{ return LISTEN; }
log_level	{ return LOG_LEVEL; }
logging         { return LOGGING; }
logpath         { return T_LOGPATH; }
max_clients     { return T_MAX_CLIENTS; }
max_number	{ return MAX_NUMBER; }
maximum_links   { return MAXIMUM_LINKS; }
message_locale	{ return MESSAGE_LOCALE; }
name		{ return NAME; }
network_name    { return NETWORK_NAME; }
network_desc    { return NETWORK_DESC; }
nick_changes	{ return NICK_CHANGES; }
no		{ return TNO; }
no_tilde	{ return NO_TILDE; }
no_hack_ops     { return NO_HACK_OPS; }
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
quiet_on_ban    { return QUIET_ON_BAN; }
reason		{ return REASON; }
rehash		{ return REHASH; }
redirserv       { return REDIRSERV; }
redirport       { return REDIRPORT; }
reject		{ return TREJECT; }
remote		{ return REMOTE; }
restricted      { return RESTRICTED; }
send_password	{ return SEND_PASSWORD; }
sendq		{ return SENDQ; }
serverinfo	{ return SERVERINFO; }
server_mask     { return SERVER_MASK; }
silent          { return SILENT; }
shared          { return SHARED; }
short_motd      { return SHORT_MOTD; }
spoof		{ return SPOOF; }
spoof_notice	{ return SPOOF_NOTICE; }
unkline		{ return UNKLINE; }
user		{ return USER;} 
vchans_oper_only { return VCHANS_OPER_ONLY; }
vhost		{ return VHOST; }
warn		{ return WARN; }
yes		{ return TYES; }

failed_oper_notice      { return FAILED_OPER_NOTICE; }
show_failed_oper_id     { return SHOW_FAILED_OPER_ID; }
max_nick_changes        { return MAX_NICK_CHANGES; }
max_nick_time           { return MAX_NICK_TIME; }
anti_nick_flood         { return ANTI_NICK_FLOOD; }
ts_max_delta            { return TS_MAX_DELTA; }
ts_warn_delta           { return TS_WARN_DELTA; }
links_delay           { return LINKS_DELAY; }
kline_with_connection_closed { return KLINE_WITH_CONNECTION_CLOSED; }
kline_with_reason       { return KLINE_WITH_REASON; }
warn_no_nline           { return WARN_NO_NLINE; }
non_redundant_klines    { return NON_REDUNDANT_KLINES; }

o_lines_oper_only       { return O_LINES_OPER_ONLY; }
whois_wait              { return WHOIS_WAIT; }
pace_wait               { return PACE_WAIT; }
knock_delay             { return KNOCK_DELAY; }
iauth_server            { return IAUTH_SERVER; }
iauth_port		{ return IAUTH_PORT; }
stats_p_notice	        { return STATS_P_NOTICE; }
glines                  { return GLINES; }
modules                 { return MODULES; }
module                  { return MODULE; }
path                    { return PATH; }
max_targets             { return MAX_TARGETS; }

oper_only_umodes        { return OPER_ONLY_UMODES; }
bots                    { return T_BOTS; }
cconn                   { return T_CCONN; }
debug                   { return T_DEBUG; }
full                    { return T_FULL; }
skill                   { return T_SKILL; }
nchange                 { return T_NCHANGE; }
rej                     { return T_REJ; }
unauth                  { return T_UNAUTH; }
spy                     { return T_SPY; }
external                { return T_EXTERNAL; }
operwall                { return T_OPERWALL; }
servnotice              { return T_SERVNOTICE; }
invisible               { return T_INVISIBLE; }
wallop                  { return T_WALLOP; }
callerid                { return T_CALLERID; }
drone                   { return T_DRONE; }
locops                  { return T_LOCOPS; }
.		        { return yytext[0]; }

%%

