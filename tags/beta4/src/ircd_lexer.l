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
%option noyywrap

%{
/* XXX - No we aren't but lex calls malloc :(. It is even worse that it
   doesn't call free! */
#define WE_ARE_MEMORY_C
#include "tools.h"
#include "irc_string.h"
#include "common.h"
#include "s_conf.h"
#include "y.tab.h"
#include "memory.h"
#include "hostmask.h"
#include "setup.h"
#include "config.h"
#include "s_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#ifdef STRING_WITH_STRINGS
# include <string.h>
# include <strings.h>
#else
# ifdef HAVE_STRING_H
#  include <string.h>
# else
#  ifdef HAVE_STRINGS_H
#   include <strings.h>
#  endif
# endif
#endif
#include <ctype.h>

#undef YY_INPUT
#define YY_FATAL_ERROR(msg) conf_yy_fatal_error(msg)
#define YY_INPUT(buf,result,max_size) \
  if (!(result = conf_fbgets(buf, max_size, conf_fbfile_in))) \
    YY_FATAL_ERROR("input in flex scanner failed"); 
#define MAX_INCLUDE_DEPTH 10

int lineno=1;
char linebuf[IRCD_BUFSIZE];
int include_stack_ptr=0;
YY_BUFFER_STATE include_stack[MAX_INCLUDE_DEPTH];
int lineno_stack[MAX_INCLUDE_DEPTH];
FBFILE* inc_fbfile_in[MAX_INCLUDE_DEPTH];
void ccomment(void);
void cinclude(void);
int ieof(void);
%}

ws        [ \t]*
digit     [0-9]
comment   #.*
qstring   \"[^\"\n]*[\"\n]
include   \.include{ws}(\<.*\>|\".*\")

%%
{include}       { cinclude(); }
"/*"	        { ccomment(); }

\n.*	        { strcpy(linebuf, yytext+1); lineno++; yyless(1); }

{ws}            ;
{comment}       ;

{digit}+        { yylval.number = atoi(yytext); return NUMBER; }

{qstring}	{ yylval.string = yytext+1;
		  if(yylval.string[yyleng-2] != '"')
		    log(L_ERROR, "Unterminated character string");
		  else
		    yylval.string[yyleng-2] = '\0'; /* remove close quote */
		  return QSTRING;
		}

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
compressed	{ return COMPRESSED; }
connect		{ return CONNECT; }
connectfreq     { return CONNECTFREQ; }
default_floodcount  { return DEFAULT_FLOODCOUNT; }
deny		{ return DENY; }
description	{ return DESCRIPTION; }
die		{ return DIE; }
dots_in_ident   { return DOTS_IN_IDENT; }
encrypted       { return ENCRYPTED; }
email		{ return EMAIL; }
exceed_limit    { return EXCEED_LIMIT; }
exempt		{ return EXEMPT; }
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
persistant     { return PERSISTANT; }
have_ident	{ return HAVE_IDENT; }
hide_server	{ return HIDESERVER; }
host		{ return HOST; }
hub		{ return HUB; }
hub_mask	{ return HUB_MASK; }
persistant_expire_time { return PERSISTANT_EXPIRE_TIME; }
idletime        { return IDLETIME; }
ip		{ return IP; }
ipv4            { return T_IPV4; }
ipv6            { return T_IPV6; }
kill		{ return KILL; }
kline		{ return KLINE; }
kline_exempt	{ return KLINE_EXEMPT; }
cryptlink	{ return CRYPTLINK; }
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
min_nonwildcard { return MIN_NONWILDCARD; }
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
rsa_public_key	{ return RSA_PUBLIC_KEY; }
rsa_private_key { return RSA_PRIVATE_KEY; }
send_password	{ return SEND_PASSWORD; }
sendq		{ return SENDQ; }
serverinfo	{ return SERVERINFO; }
silent          { return SILENT; }
shared          { return SHARED; }
short_motd      { return SHORT_MOTD; }
spoof		{ return SPOOF; }
spoof_notice	{ return SPOOF_NOTICE; }
unkline		{ return UNKLINE; }
user		{ return USER;} 
vchans_oper_only { return VCHANS_OPER_ONLY; }
disable_vchans  { return DISABLE_VCHANS; }
vhost		{ return VHOST; }
warn		{ return WARN; }
yes		{ return TYES; }

failed_oper_notice      { return FAILED_OPER_NOTICE; }
max_accept              { return MAX_ACCEPT; }
max_nick_changes        { return MAX_NICK_CHANGES; }
max_nick_time           { return MAX_NICK_TIME; }
anti_nick_flood         { return ANTI_NICK_FLOOD; }
anti_spam_exit_message_time { return ANTI_SPAM_EXIT_MESSAGE_TIME; }
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
glines                  { return GLINES; }
modules                 { return MODULES; }
module                  { return MODULE; }
path                    { return PATH; }
max_targets             { return MAX_TARGETS; }

oper_only_umodes        { return OPER_ONLY_UMODES; }
oper_umodes             { return OPER_UMODES; }
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

millennia		{ return MILLENNIA; }
millennium		{ return MILLENNIA; }
centuries		{ return CENTURIES; }
century			{ return CENTURIES; }
decades			{ return DECADES; }
decade			{ return DECADES; }
years			{ return YEARS; }
year			{ return YEARS; }
months			{ return MONTHS; }
month			{ return MONTHS; }
weeks			{ return WEEKS; }
week			{ return WEEKS; }
days			{ return DAYS; }
day			{ return DAYS; }
hours			{ return HOURS; }
hour			{ return HOURS; }
minutes			{ return MINUTES; }
minute			{ return MINUTES; }
seconds			{ return SECONDS; }
second			{ return SECONDS; }

bytes			{ return BYTES; }
byte			{ return BYTES; }
kilobytes		{ return KBYTES; }
kilobyte		{ return KBYTES; }
kbytes			{ return KBYTES; }
kbyte			{ return KBYTES; }
kb			{ return KBYTES; }
megabytes		{ return MBYTES; }
megabyte		{ return MBYTES; }
mbytes			{ return MBYTES; }
mbyte			{ return MBYTES; }
mb			{ return MBYTES; }
gigabytes		{ return GBYTES; }
gigabyte		{ return GBYTES; }
gbytes			{ return GBYTES; }
gbyte			{ return GBYTES; }
gb			{ return GBYTES; }
terabytes		{ return TBYTES; }
terabyte		{ return TBYTES; }
tbytes			{ return TBYTES; }
tbyte			{ return TBYTES; }
tb			{ return TBYTES; }

.		        { return yytext[0]; }
<<EOF>>     { if (ieof()) yyterminate(); }

%%

/* C-comment ignoring routine -kre*/
void ccomment(void)
{
  int c;

  /* log(L_NOTICE, "got comment"); */
  while (1)
  {
    while ((c = input()) != '*' && c != EOF)
      if (c == '\n') ++lineno;
    if (c == '*')
    {
      while ((c = input()) == '*');
      if (c == '/') break;
    }
    if (c == EOF)
    {
      YY_FATAL_ERROR("EOF in comment");
      break;
    }
  }
}

/* C-style .includes. This function will properly swap input conf buffers,
 * and lineno -kre */
void cinclude(void)
{
  char *c;
  if ((c = index(yytext, '<')) == NULL)
    *index(c = index(yytext, '"') + 1, '"') = 0;
  else
    *index(++c, '>') = 0;

  /* log(L_NOTICE, "got include %s!", c); */

  /* do stacking and co. */ 
  if (include_stack_ptr >= MAX_INCLUDE_DEPTH)
    log(L_ERROR, "Includes nested too deep in %s", c);
  else
  {
    FBFILE *tmp_fbfile_in;
    char filenamebuf[IRCD_BUFSIZE];
    
    snprintf(filenamebuf, IRCD_BUFSIZE, "%s/%s", ETCPATH, c);
    tmp_fbfile_in = fbopen(filenamebuf, "r");
    
    if (tmp_fbfile_in == NULL)
    {
      log(L_ERROR, "Cannot include %s", filenamebuf);
      return;
    }
    lineno_stack[include_stack_ptr] = lineno;
    lineno = 1;
    inc_fbfile_in[include_stack_ptr] = conf_fbfile_in;
    include_stack[include_stack_ptr++] = YY_CURRENT_BUFFER;
    conf_fbfile_in = tmp_fbfile_in;
    yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
  }
}

/* This is function that will be called on EOF in conf file. It will
 * apropriately close conf if it not main conf and swap input buffers -kre
 * */
int ieof(void)
{
  /* log(L_NOTICE, "return from include stack!"); */
  if (include_stack_ptr)
    fbclose(conf_fbfile_in);
  if (--include_stack_ptr < 0)
  {
    /* log(L_NOTICE, "terminating lexer"); */
    /* We will now exit the lexer - restore init values if we get /rehash
     * later and reenter lexer -kre */
    include_stack_ptr = 0;
    lineno = 1;
    return 1;
  }
  /* switch buffer */
  /* log(L_NOTICE, "deleting include_stack_ptr=%d", include_stack_ptr); */
  yy_delete_buffer(YY_CURRENT_BUFFER);
  lineno = lineno_stack[include_stack_ptr];
  conf_fbfile_in = inc_fbfile_in[include_stack_ptr];
  yy_switch_to_buffer(include_stack[include_stack_ptr]);
  return 0;
}
