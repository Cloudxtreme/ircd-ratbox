/************************************************************************
 *   IRC - Internet Relay Chat, src/ircd_parser.y
 *   Copyright (C) 2000 Diane Bruce <db@db.net>
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

%{

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "tools.h"
#include "s_conf.h"
#include "s_log.h"
#include "client.h"	/* for FLAGS_ALL only */
#include "irc_string.h"
#include "ircdauth.h"
#include "memdebug.h"
#include "modules.h"
extern char *ip_string;

int yyparse();
        
static struct ConfItem *yy_aconf;
static struct ConfItem *yy_cconf;
static struct ConfItem *yy_nconf;
static struct ConfItem *yy_hconf;
static struct ConfItem *yy_lconf;

static struct ConfItem *hub_confs;
static struct ConfItem *leaf_confs;
static struct ConfItem *yy_aconf;
static struct ConfItem *yy_aconf_next;

char* class_name_var;
int   class_ping_time_var;
int   class_number_per_ip_var;
int   class_max_number_var;
int   class_sendq_var;

char* class_redirserv_var;
int   class_redirport_var;

%}

%union {
        int  number;
        char *string;
        struct ip_value ip_entry;
}

%token  ACCEPT
%token  ACCEPT_PASSWORD
%token  ACTION
%token  ADMIN
%token  AUTH
%token  AUTOCONN
%token  CLASS
%token  CONNECT
%token  CONNECTFREQ
%token  DENY
%token  DESCRIPTION
%token  DIE
%token  DOTS_IN_IDENT
%token  EMAIL
%token  EXCEED_LIMIT
%token  FNAME_USERLOG
%token  FNAME_OPERLOG
%token  FNAME_FOPERLOG
%token  GECOS
%token  GLINE
%token  GLINES
%token  GLINE_TIME
%token  GLINE_LOG
%token  GLOBAL_KILL
%token  HAVE_IDENT
%token  HOST
%token  HUB
%token  HUB_MASK
%token  IDLETIME
%token  INCLUDE
%token  IP
%token  IP_TYPE
%token  KILL
%token  KLINE
%token  KLINE_EXEMPT
%token  LAZYLINK
%token  LEAF
%token  LEAF_MASK
%token  LISTEN
%token  LOGGING
%token  T_LOGPATH
%token  LOG_LEVEL
%token  MAX_NUMBER
%token  MAXIMUM_LINKS
%token  MESSAGE_LOCALE
%token  NAME
%token  NETWORK_NAME
%token  NETWORK_DESC
%token  NICK_CHANGES
%token  NO_TILDE
%token  NUMBER
%token  NUMBER_PER_IP
%token  OPERATOR
%token  OPER_LOG
%token  PASSWORD
%token  PING_TIME
%token  PORT
%token  QSTRING
%token  QUARANTINE
%token  REASON
%token  REDIRSERV
%token  REDIRPORT
%token  REHASH
%token  REMOTE
%token  SENDQ
%token  SEND_PASSWORD
%token  SERVERINFO
%token  SHARED
%token  SPOOF
%token  TREJECT
%token  TNO
%token  TYES
%token  T_L_CRIT
%token  T_L_DEBUG
%token  T_L_ERROR
%token  T_L_INFO
%token  T_L_NOTICE
%token  T_L_TRACE
%token  T_L_WARN
%token  UNKLINE
%token  USER
%token  VHOST
%token  WARN
%token  GENERAL
%token  FAILED_OPER_NOTICE
%token  SHOW_FAILED_OPER_ID
%token  ANTI_NICK_FLOOD
%token  MAX_NICK_TIME
%token  MAX_NICK_CHANGES
%token  TS_MAX_DELTA
%token  TS_WARN_DELTA
%token  KLINE_WITH_REASON
%token  KLINE_WITH_CONNECTION_CLOSED
%token  WARN_NO_NLINE
%token  NON_REDUNDANT_KLINES
%token  E_LINES_OPER_ONLY
%token  F_LINES_OPER_ONLY
%token  O_LINES_OPER_ONLY
%token  STATS_NOTICE
%token  WHOIS_WAIT
%token  PACE_WAIT
%token  KNOCK_DELAY
%token  SHORT_MOTD
%token  NO_OPER_FLOOD
%token  IAUTH_SERVER
%token  IAUTH_PORT
%token  STATS_P_NOTICE
%token  INVITE_PLUS_I_ONLY
%token  MODULE
%token  MODULES
%token  HIDESERVER
%token  CLIENT_EXIT
%token  T_BOTS
%token  T_CCONN
%token  T_DEBUG
%token  T_FULL
%token  T_SKILL
%token  T_NCHANGE
%token  T_REJ
%token  T_UNAUTH
%token  T_SPY
%token  T_EXTERNAL
%token  T_OPERWALL
%token  T_SERVNOTICE
%token  T_INVISIBLE
%token  T_CALLERID
%token  T_WALLOP
%token  OPER_ONLY_UMODES
%token  PATH
%token  MAX_TARGETS
%token  LINKS_NOTICE
%token  LINKS_DELAY
%token  VCHANS_OPER_ONLY

%type   <ip_value> IP_TYPE
%type   <string>   QSTRING
%type   <number>   NUMBER

%%
conf:   
        | conf conf_item
        ;

conf_item:        admin_entry
                | logging_entry
                | oper_entry
                | class_entry 
                | listen_entry
                | auth_entry
                | serverinfo_entry
                | quarantine_entry
                | shared_entry
                | connect_entry
                | kill_entry
                | deny_entry
		| general_entry
                | gecos_entry
                | modules_entry
                | error ';'
                | error '}'
        ;

/***************************************************************************
 *  section modules
 ***************************************************************************/

modules_entry:          MODULES
  '{' modules_items '}' ';'

modules_items:   modules_items modules_item |
                    modules_item

modules_item:    modules_module | modules_path |
                 error

modules_module:  MODULE '=' QSTRING ';'
{
  /* I suppose we should just ignore it if it is already loaded(since
   * otherwise we would flood the opers on rehash) -A1kmm. */
  if (!findmodule_byname(yylval.string))
    load_one_module (yylval.string);
};

modules_path: PATH '=' QSTRING ';'
{
	mod_add_path(yylval.string);
};


/***************************************************************************
 *  section serverinfo
 ***************************************************************************/

serverinfo_entry:       SERVERINFO
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_ME;
  }
  '{' serverinfo_items '}' ';'
  {
    if(yy_aconf->host && yy_aconf->user)
      {
        conf_add_me(yy_aconf);
        conf_add_conf(yy_aconf);
      }
    else
      free_conf(yy_aconf);
    yy_aconf = (struct ConfItem *)NULL;
  } ;


serverinfo_items:       serverinfo_items serverinfo_item |
                        serverinfo_item 

serverinfo_item:        serverinfo_name | serverinfo_vhost |
                        serverinfo_hub | serverinfo_description |
                        serverinfo_network_name | serverinfo_network_desc |
			error

serverinfo_name:        NAME '=' QSTRING ';' 
  {
    yy_aconf->host = yylval.string;
  };

serverinfo_description: DESCRIPTION '=' QSTRING ';'
  {
    yy_aconf->user = yylval.string;
  };

serverinfo_network_name: NETWORK_NAME '=' QSTRING ';'
  {
    ConfigFileEntry.network_name = yylval.string;
  };

serverinfo_network_desc: NETWORK_DESC '=' QSTRING ';'
  {
    ConfigFileEntry.network_desc = yylval.string;
  };

serverinfo_vhost:       VHOST '=' IP_TYPE ';'
  {
    yy_aconf->ip = yylval.ip_entry.ip;
  };

serverinfo_hub:         HUB '=' TYES ';' 
  {
    ConfigFileEntry.hub = 1;
  }
                        |
                        HUB '=' TNO ';'
  {
    ConfigFileEntry.hub = 0;
  } ;

/***************************************************************************
 * admin section
 ***************************************************************************/

admin_entry: ADMIN 
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_ADMIN;
  }
 '{' admin_items '}' ';' 
  {
    conf_add_conf(yy_aconf);
    yy_aconf = (struct ConfItem *)NULL;
  }; 

admin_items:    admin_items admin_item |
                admin_item

admin_item:     admin_name | admin_description |
                admin_email | error

admin_name:     NAME '=' QSTRING ';' 
  {
    yy_aconf->passwd = yylval.string;
  };

admin_email:    EMAIL '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->user,yylval.string);
  };

admin_description:      DESCRIPTION '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->host,yylval.string);
  };

/***************************************************************************
 *  section logging
 ***************************************************************************/

logging_entry:          LOGGING  '{' logging_items '}' ';' 

logging_items:          logging_items logging_item |
                        logging_item 

logging_item:           logging_path | logging_oper_log |
                        logging_gline_log | logging_log_level |
			error

logging_path:           T_LOGPATH '=' QSTRING ';' 
                        {
                        };

logging_oper_log:	OPER_LOG '=' QSTRING ';'
                        {
                        };

logging_gline_log:	GLINE_LOG '=' QSTRING ';'
                        {
                        };

logging_log_level:     LOG_LEVEL '=' T_L_CRIT ';'
  { set_log_level(L_CRIT); }
                       |
                       LOG_LEVEL '=' T_L_ERROR ';'
  { set_log_level(L_ERROR); }
                       |
                       LOG_LEVEL '=' T_L_WARN ';'
  { set_log_level(L_WARN); }
                       |
                       LOG_LEVEL '=' T_L_NOTICE ';'
  { set_log_level(L_NOTICE); }
                       |
                       LOG_LEVEL '=' T_L_TRACE ';'
  { set_log_level(L_TRACE); }
                       |
                       LOG_LEVEL '=' T_L_INFO ';'
  { set_log_level(L_INFO); }
                       |
                       LOG_LEVEL '=' T_L_DEBUG ';'
  { set_log_level(L_DEBUG); };

/***************************************************************************
 * oper section
  ***************************************************************************/

oper_entry:     OPERATOR 
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_OPERATOR;
  }
 '{' oper_items '}' ';'
  {
    if(yy_aconf->name && yy_aconf->passwd && yy_aconf->host)
      {
        conf_add_class_to_conf(yy_aconf);
        conf_add_conf(yy_aconf);
      }
    else
      {
        free_conf(yy_aconf);
      }
    yy_aconf = (struct ConfItem *)NULL;
  }; 

oper_items:     oper_items oper_item |
                oper_item

oper_item:      oper_name  | oper_user | oper_password |
                oper_class | oper_global_kill | oper_remote |
                oper_kline | oper_unkline | oper_gline | oper_nick_changes |
                oper_die | oper_rehash | oper_admin | error

oper_name:      NAME '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->name,yylval.string);
  };

oper_user:      USER '='  QSTRING ';'
  {
    char *p;

    if(yylval.string != NULL)
      {
	if((p = strchr(yylval.string,'@')))
	  {
	    *p = '\0';
	    DupString(yy_aconf->user,yylval.string);
	    p++;
	    DupString(yy_aconf->host,p);
	  }
	else
	  {
	    yy_aconf->host = yylval.string;
	    DupString(yy_aconf->user,"*");
	  }
      }
  };

oper_password:  PASSWORD '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->passwd,yylval.string);
  };

oper_class:     CLASS '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->className,yylval.string);
  };

oper_global_kill: GLOBAL_KILL '=' TYES ';'
  {
    yy_aconf->port |= CONF_OPER_GLOBAL_KILL;
  }
                  |
                  GLOBAL_KILL '=' TNO ';'
  {
    yy_aconf->port &= ~CONF_OPER_GLOBAL_KILL;
  };

oper_remote: REMOTE '=' TYES ';' { yy_aconf->port |= CONF_OPER_REMOTE;}
             |
             REMOTE '=' TNO ';' { yy_aconf->port &= ~CONF_OPER_REMOTE; } ;

oper_kline: KLINE '=' TYES ';' { yy_aconf->port |= CONF_OPER_K;}
            |
            KLINE '=' TNO ';' { yy_aconf->port &= ~CONF_OPER_K; } ;

oper_unkline: UNKLINE '=' TYES ';' { yy_aconf->port |= CONF_OPER_UNKLINE;}
              |
              UNKLINE '=' TNO ';' { yy_aconf->port &= ~CONF_OPER_UNKLINE; } ;

oper_gline: GLINE '=' TYES ';' { yy_aconf->port |= CONF_OPER_GLINE;}
            |
            GLINE '=' TNO ';' { yy_aconf->port &= ~CONF_OPER_GLINE; };

oper_nick_changes: NICK_CHANGES '=' TYES ';' { yy_aconf->port |= CONF_OPER_N;}
                   |
                   NICK_CHANGES '=' TNO ';' { yy_aconf->port &= ~CONF_OPER_N;};

oper_die: DIE '=' TYES ';' { yy_aconf->port |= CONF_OPER_DIE; }
          |
          DIE '=' TNO ';' { yy_aconf->port &= ~CONF_OPER_DIE; } ;

oper_rehash: REHASH '=' TYES ';' { yy_aconf->port |= CONF_OPER_REHASH;}
             |
             REHASH '=' TNO ';' { yy_aconf->port &= ~CONF_OPER_REHASH; } ;

oper_admin: ADMIN '=' TYES ';' { yy_aconf->port |= CONF_OPER_ADMIN;}
            |
            ADMIN '=' TNO ';' { yy_aconf->port &= ~CONF_OPER_ADMIN;} ;

/***************************************************************************
 *  section class
 ***************************************************************************/

class_entry:    CLASS 
  {
    if(class_name_var)
      MyFree(class_name_var);

    class_name_var = NULL;
    class_ping_time_var = 0;
    class_number_per_ip_var = 0;
    class_max_number_var = 0;
    class_sendq_var = 0;
  }
  '{' class_items '}' ';'
  {

    add_class(class_name_var,class_ping_time_var,
              class_number_per_ip_var, class_max_number_var,
              class_sendq_var );

    MyFree(class_name_var);
    class_name_var = (char *)NULL;
  };

class_items:    class_items class_item |
                class_item

class_item:     class_name |
                class_ping_time |
                class_number_per_ip |
                class_connectfreq |
                class_max_number |
                class_sendq |
		error

class_name:     NAME '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      DupString(class_name_var,yylval.string);
  };

class_ping_time:        PING_TIME '=' NUMBER ';'
  {
    class_ping_time_var = yylval.number;
  };

class_number_per_ip:    NUMBER_PER_IP '=' NUMBER ';'
  {
    class_number_per_ip_var = yylval.number;
  };

class_connectfreq:     CONNECTFREQ '=' NUMBER ';'
  {
    class_number_per_ip_var = yylval.number;
  };

class_max_number:       MAX_NUMBER '=' NUMBER ';'
  {
    class_max_number_var = yylval.number;
  };

class_sendq:    SENDQ '=' NUMBER ';'
  {
    class_sendq_var = yylval.number;
  };


/***************************************************************************
 *  section listen
 ***************************************************************************/

listen_entry:   LISTEN 
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_LISTEN_PORT;
    DupString(yy_aconf->passwd,"*");
  }
 '{' listen_items '}' ';'
  {
    conf_add_port(yy_aconf);
    free_conf(yy_aconf);
    yy_aconf = (struct ConfItem *)NULL;
  }; 

listen_items:   listen_items listen_item |
                listen_item

listen_item:    listen_name | listen_port | listen_address | error

listen_name:    NAME '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->host,yylval.string);
  };

listen_port:    PORT '=' NUMBER ';'
  {
    yy_aconf->port = yylval.number;
  };

listen_address: IP '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->passwd,yylval.string);
  };

/***************************************************************************
 *  section auth
 ***************************************************************************/

auth_entry:   AUTH
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_CLIENT;
  }
 '{' auth_items '}' ';' 
  {
    if(yy_aconf->name == NULL)
      DupString(yy_aconf->name,"NOMATCH");

    conf_add_class_to_conf(yy_aconf);
    conf_delist_old_conf(yy_aconf);

    if(yy_aconf->user == NULL)
      DupString(yy_aconf->user,"*");
    else
      (void)collapse(yy_aconf->user);

    if(yy_aconf->host == NULL)
      DupString(yy_aconf->host,"*");
    else
      (void)collapse(yy_aconf->host);

    if(yy_aconf->ip && yy_aconf->ip_mask)
      {
        add_ip_Iline(yy_aconf);
      }
    else
      {
        add_mtrie_conf_entry(yy_aconf,CONF_CLIENT);
      }
    yy_aconf = (struct ConfItem *)NULL;
  }; 

auth_items:     auth_items auth_item |
                auth_item

auth_item:      auth_user | auth_passwd | auth_class |
                auth_kline_exempt | auth_have_ident |
                auth_exceed_limit | auth_no_tilde | auth_spoof |
                auth_redir_serv | auth_redir_port | error

auth_user:   USER '=' QSTRING ';'
  {
    char *p;

    if(yylval.string != NULL)
      {
	if((p = strchr(yylval.string,'@')))
	  {
	    *p = '\0';
	    DupString(yy_aconf->user,yylval.string);
	    p++;
	    DupString(yy_aconf->host,p);
	  }
	else
	  {
	    yy_aconf->host = yylval.string;
	    yylval.string = (char *)NULL;
	    DupString(yy_aconf->user,"*");
	  }
      }
  };
             |
        IP '=' IP_TYPE ';'
  {
    char *p;

    yy_aconf->ip = yylval.ip_entry.ip;
    yy_aconf->ip_mask = yylval.ip_entry.ip_mask;
    DupString(yy_aconf->host,ip_string);
    if((p = strchr(yy_aconf->host, ';')))
      *p = '\0';
  };

auth_passwd:  PASSWORD '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->passwd,yylval.string);
  };

auth_spoof:   SPOOF '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->name,yylval.string);
    yy_aconf->flags |= CONF_FLAGS_SPOOF_IP;
  };

auth_exceed_limit:    EXCEED_LIMIT '=' TYES ';'
  {
    yy_aconf->flags |= CONF_FLAGS_F_LINED;
  }
                      |
                      EXCEED_LIMIT '=' TNO ';'
  {
    yy_aconf->flags &= ~CONF_FLAGS_F_LINED;
  };

auth_kline_exempt:    KLINE_EXEMPT '=' TYES ';'
  {
    yy_aconf->flags |= CONF_FLAGS_E_LINED;
  }
                      |
                      KLINE_EXEMPT '=' TNO ';'
  {
    yy_aconf->flags &= ~CONF_FLAGS_E_LINED;
  };

auth_have_ident:      HAVE_IDENT '=' TYES ';'
  {
    yy_aconf->flags |= CONF_FLAGS_NEED_IDENTD;
  }
                      |
                      HAVE_IDENT '=' TNO ';'
  {
    yy_aconf->flags &= ~CONF_FLAGS_NEED_IDENTD;
  };

auth_no_tilde:        NO_TILDE '=' TYES ';' 
  {
    yy_aconf->flags |= CONF_FLAGS_NO_TILDE;
  }
                      |
                      NO_TILDE '=' TNO ';'
  {
    yy_aconf->flags &= ~CONF_FLAGS_NO_TILDE;
  };

auth_redir_serv:    REDIRSERV '=' QSTRING ';'
  {
    yy_aconf->flags |= CONF_FLAGS_REDIR;
    if(yylval.string != NULL)
      DupString(yy_aconf->name,yylval.string);
  };

auth_redir_port:    REDIRPORT '=' NUMBER ';'
  {
    yy_aconf->flags |= CONF_FLAGS_REDIR;
    yy_aconf->port = yylval.number;
  };

auth_class:   CLASS '=' QSTRING ';'
  {
    if((yy_aconf->className == NULL) && (yylval.string != NULL))
      DupString(yy_aconf->className,yylval.string);
  };

/***************************************************************************
 *  section quarantine
 ***************************************************************************/

quarantine_entry:       QUARANTINE
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_QUARANTINED_NICK;
  }
 '{' quarantine_items '}' ';'
  {
    conf_add_q_conf(yy_aconf);
    yy_aconf = (struct ConfItem *)NULL;
  }; 

quarantine_items:       quarantine_items quarantine_item |
			quarantine_item

quarantine_item:        quarantine_name | quarantine_reason | error

quarantine_name:        NAME '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->name,yylval.string);
  };

quarantine_reason:      REASON '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->passwd,yylval.string);
  };

/***************************************************************************
 *  section shared, for sharing remote klines etc.
 ***************************************************************************/

shared_entry:		SHARED
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_ULINE;
    yy_aconf->name = NULL;
    yy_aconf->user = NULL;
    yy_aconf->host = NULL;
  }
  '{' shared_items '}' ';'
  {
    conf_add_u_conf(yy_aconf);
    yy_aconf = (struct ConfItem *)NULL;
  };

shared_items:		shared_items shared_item |
			shared_item

shared_item:		shared_name | shared_user | shared_host | error

shared_name:		NAME '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString (yy_aconf->name, yylval.string);
  };

shared_user:		USER '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString (yy_aconf->user, yylval.string);
  };

shared_host:		HOST '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString (yy_aconf->host, yylval.string);
  };

/***************************************************************************
 *  section connect
 ***************************************************************************/

connect_entry:  CONNECT   
  {
    hub_confs = (struct ConfItem *)NULL;
    leaf_confs = (struct ConfItem *)NULL;

    if(yy_cconf)
      {
        free_conf(yy_cconf);
        yy_cconf = (struct ConfItem *)NULL;
      }

    if(yy_nconf)
      {
        free_conf(yy_nconf);
        yy_nconf = (struct ConfItem *)NULL;
      }

    if(yy_hconf)
      {
        free_conf(yy_hconf);
        yy_hconf = (struct ConfItem *)NULL;
      }

    if(yy_lconf)
      {
	free_conf(yy_lconf);
	yy_lconf = (struct ConfItem *)NULL;
      }

    yy_cconf=make_conf();
    yy_cconf->status = CONF_CONNECT_SERVER;
    yy_cconf->flags |= CONF_FLAGS_ALLOW_AUTO_CONN;

    yy_nconf=make_conf();
    yy_nconf->status = CONF_NOCONNECT_SERVER;
  }
  '{' connect_items '}' ';'
  {
    if(yy_cconf->host && yy_cconf->passwd && yy_cconf->user)
      {
        ++ccount;
        conf_add_server(yy_cconf,ncount,ccount);
        conf_add_conf(yy_cconf);
      }
    else
      {
        free_conf(yy_cconf);
      }

    if(yy_nconf->host && yy_nconf->passwd && yy_nconf->user)
      {
        ++ncount;
        conf_add_server(yy_nconf,ncount,ccount);
        conf_add_conf(yy_nconf);
      }
    else
      {
        free_conf(yy_nconf);
      }

    for(yy_aconf=hub_confs;yy_aconf;yy_aconf=yy_aconf_next)
      {
	yy_aconf_next = yy_aconf->next;

	if(yy_cconf->host != NULL)
	  {
	    DupString(yy_aconf->user,yy_cconf->name);
 	    conf_add_hub_or_leaf(yy_aconf);
	    conf_add_conf(yy_aconf);
	  }
	else
	  {
	    free_conf(yy_aconf);
 	  }
      }

    for(yy_aconf=leaf_confs;yy_aconf;yy_aconf=yy_aconf_next)
      {
	yy_aconf_next = yy_aconf->next;

	if(yy_cconf->host != NULL)
	  {
	    DupString(yy_aconf->user,yy_cconf->name);
 	    conf_add_hub_or_leaf(yy_aconf);
	    conf_add_conf(yy_aconf);
	  }
	else
	  {
	    free_conf(yy_aconf);
 	  }
      }

    hub_confs = (struct ConfItem*)NULL;
    leaf_confs = (struct ConfItem*)NULL;

    yy_cconf = (struct ConfItem *)NULL;
    yy_nconf = (struct ConfItem *)NULL;
    yy_hconf = (struct ConfItem *)NULL;
  };

connect_items:  connect_items connect_item |
                connect_item

connect_item:   connect_name | connect_host | connect_send_password |
                connect_accept_password | connect_port |
                connect_lazylink | connect_hub_mask |
                connect_leaf_mask | connect_class |
                connect_auto | error

connect_name:   NAME '=' QSTRING ';'
  {
    if(yy_cconf->user)
      {
	sendto_realops_flags(FLAGS_ALL,"*** Multiple connect entry");
      }
    else
      {
        if(yylval.string != NULL)
	  DupString(yy_cconf->user,yylval.string);
      }

    if(yy_nconf->user)
      {
	sendto_realops_flags(FLAGS_ALL,"*** Multiple connect accept entry");
      }
    else
      {
        if(yylval.string != NULL)
	  DupString(yy_nconf->user,yylval.string);
      }

    MyFree(yylval.string);
  };

connect_host:   HOST '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      {
	DupString(yy_cconf->host,yylval.string);
	DupString(yy_nconf->host,yylval.string);
      }
  };
 
connect_send_password:  SEND_PASSWORD '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString(yy_cconf->passwd,yylval.string);
  };

connect_accept_password: ACCEPT_PASSWORD '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      DupString(yy_nconf->passwd,yylval.string);
  };

connect_port:   PORT '=' NUMBER ';' { yy_cconf->port = yylval.number; };

connect_lazylink:       LAZYLINK '=' TYES ';'
  {
    yy_nconf->flags |= CONF_FLAGS_LAZY_LINK;
  }
                        |
                        LAZYLINK '=' TNO ';'
  {
    yy_nconf->flags &= ~CONF_FLAGS_LAZY_LINK;
  };

connect_auto:           AUTOCONN '=' TYES ';'
  {
    yy_cconf->flags |= CONF_FLAGS_ALLOW_AUTO_CONN;
  }
                        |
                        AUTOCONN '=' TNO ';'
  {
    yy_cconf->flags &= ~CONF_FLAGS_ALLOW_AUTO_CONN;
  };

connect_hub_mask:       HUB_MASK '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      {
	if(!hub_confs)
	  {
	    hub_confs = make_conf();
	    hub_confs->status = CONF_HUB;
	    DupString(hub_confs->host,yylval.string);
	  }
	else
	  {
	    yy_hconf = make_conf();
	    yy_hconf->status = CONF_HUB;
	    DupString(yy_hconf->host,yylval.string);
	    yy_hconf->next = hub_confs;
	    hub_confs = yy_hconf;
	  }
      }
  };

connect_leaf_mask:       LEAF_MASK '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      {
	if(!leaf_confs)
	  {
	    leaf_confs = make_conf();
	    leaf_confs->status = CONF_LEAF;
	    DupString(leaf_confs->host,yylval.string);
	  }
	else
	  {
	    yy_lconf = make_conf();
	    yy_lconf->status = CONF_LEAF;
	    DupString(yy_lconf->host,yylval.string);
	    yy_lconf->next = leaf_confs;
	    leaf_confs = yy_lconf;
	  }
      }
  };
 
connect_class:  CLASS '=' QSTRING ';'
  {
    if(yylval.string != NULL)
      {
	DupString(yy_cconf->className,yylval.string);
	DupString(yy_nconf->className,yylval.string);
      }
  };


/***************************************************************************
 *  section kill
 ***************************************************************************/

kill_entry:     KILL
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_KILL;
  }
 '{' kill_items '}' ';'
  {
    if(yy_aconf->user && yy_aconf->passwd && yy_aconf->host)
      {
        conf_add_k_conf(yy_aconf);
      }
    else
      {
        free_conf(yy_aconf);
      }
    yy_aconf = (struct ConfItem *)NULL;
  }; 

kill_items:     kill_items kill_item |
                kill_item

kill_item:      kill_user | kill_reason | error


kill_user:      USER '=' QSTRING ';'
  {
    char *p;

    if(yylval.string != NULL)
      {
	if((p = strchr(yylval.string,'@')))
	  {
	    *p = '\0';
	    DupString(yy_aconf->user,yylval.string);
	    p++;
	    DupString(yy_aconf->host,p);
	    MyFree(yylval.string);
	  }
	else
	  {
	    yy_aconf->host = yylval.string;
	    yylval.string = (char *)NULL;
	    DupString(yy_aconf->user,"*");
	  }
      }
  };

kill_reason:    REASON '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->passwd,yylval.string); 
  };

/***************************************************************************
 *  section deny
 ***************************************************************************/

deny_entry:     DENY 
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_DLINE;
  }
'{' deny_items '}' ';'
  {
    if(yy_aconf->ip)
      {
	if(yy_aconf->passwd == NULL)
	  {
	    DupString(yy_aconf->passwd,"NO REASON");
	  }
        add_Dline(yy_aconf);
      }
    else
      {
        free_conf(yy_aconf);
      }
    yy_aconf = (struct ConfItem *)NULL;
  }; 

deny_items:     deny_items deny_item |
                deny_item

deny_item:      deny_ip | deny_reason | error


deny_ip:        IP '=' IP_TYPE ';'
  {
    char *p;

    if(yylval.string != NULL)
      {
	yy_aconf->ip = yylval.ip_entry.ip;
	yy_aconf->ip_mask = yylval.ip_entry.ip_mask;
	DupString(yy_aconf->host,ip_string);
	if((p = strchr(yy_aconf->host, ';')))
	  *p = '\0';
      }
  };

deny_reason:    REASON '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->passwd,yylval.string);
  };

/***************************************************************************
 *  section gecos
 ***************************************************************************/

gecos_entry:     GECOS
  {
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_XLINE;
  }
 '{' gecos_items '}' ';'
  {
    if(yy_aconf->host)
      {
	if(yy_aconf->passwd == NULL)
	  {
	    DupString(yy_aconf->passwd,"Something about your name");
	  }
        conf_add_x_conf(yy_aconf);
      }
    else
      {
        free_conf(yy_aconf);
      }
    yy_aconf = (struct ConfItem *)NULL;
  }; 

gecos_items:     gecos_items gecos_item |
                 gecos_item

gecos_item:      gecos_name | gecos_reason | gecos_action | error


gecos_name:    NAME '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->host,yylval.string); 
  };

gecos_reason:    REASON '=' QSTRING ';' 
  {
    if(yylval.string != NULL)
      DupString(yy_aconf->passwd,yylval.string);
  };

gecos_action:    ACTION '=' TREJECT ';'
  {
    yy_aconf->port = 1;
  }
                 |
                 ACTION '=' WARN ';'
  {
    yy_aconf->port = 0;
  };


/***************************************************************************
 *  section general
 ***************************************************************************/

general_entry:      GENERAL
  '{' general_items '}' ';'

general_items:      general_items general_item |
                    general_item

general_item:       general_failed_oper_notice | general_show_failed_oper_id |
                    general_anti_nick_flood | general_max_nick_time |
                    general_max_nick_changes |
                    general_ts_warn_delta | general_ts_max_delta |
                    general_kline_with_reason |
                    general_kline_with_connection_closed |
                    general_warn_no_nline |
                    general_non_redundant_klines | general_dots_in_ident |
                    general_e_lines_oper_only | general_f_lines_oper_only |
                    general_o_lines_oper_only |
                    general_stats_notice | general_pace_wait |
                    general_whois_wait | 
                    general_knock_delay |
                    general_short_motd | general_no_oper_flood |
                    general_iauth_server |
                    general_iauth_port | general_stats_p_notice |
                    general_glines | general_gline_time |
                    general_idletime |
                    general_hide_server | general_maximum_links |
                    general_message_locale | general_client_exit |
                    general_fname_userlog | general_fname_operlog |
                    general_fname_foperlog | general_oper_only_umodes |
                    general_max_targets | general_links_notice |
                    general_links_delay |
                    general_vchans_oper_only |
                    error

general_failed_oper_notice:   FAILED_OPER_NOTICE '=' TYES ';'
  {
    ConfigFileEntry.failed_oper_notice = 1;
  }
                        |
                        FAILED_OPER_NOTICE '=' TNO ';'
  {
    ConfigFileEntry.failed_oper_notice = 0;
  } ;

general_show_failed_oper_id:   SHOW_FAILED_OPER_ID '=' TYES ';'
  {
    ConfigFileEntry.show_failed_oper_id = 1;
  }
                        |
                        SHOW_FAILED_OPER_ID '=' TNO ';'
  {
    ConfigFileEntry.show_failed_oper_id = 0;
  } ;

general_anti_nick_flood:   ANTI_NICK_FLOOD '=' TYES ';'
  {
    ConfigFileEntry.anti_nick_flood = 1;
  }
                        |
                        ANTI_NICK_FLOOD '=' TNO ';'
  {
    ConfigFileEntry.anti_nick_flood = 0;
  } ;

general_max_nick_time:    MAX_NICK_TIME '=' NUMBER ';'
  {
    ConfigFileEntry.max_nick_time = yylval.number;
  } ;

general_max_nick_changes:  MAX_NICK_CHANGES '=' NUMBER ';'
  {
    ConfigFileEntry.max_nick_changes = yylval.number;
  } ;

general_ts_warn_delta: TS_WARN_DELTA '=' NUMBER ';'
  {
    ConfigFileEntry.ts_warn_delta = yylval.number;
  } ;

general_ts_max_delta: TS_MAX_DELTA '=' NUMBER ';'
  {
    ConfigFileEntry.ts_max_delta = yylval.number;
  } ;

general_links_delay:    LINKS_DELAY '=' NUMBER ';'
  {
    ConfigFileEntry.links_delay = yylval.number;
  } ;
general_kline_with_reason: KLINE_WITH_REASON '=' TYES ';'
  {
    ConfigFileEntry.kline_with_reason = 1;
  }
    |
    KLINE_WITH_REASON '=' TNO ';'
  {
    ConfigFileEntry.kline_with_reason = 0;
  } ;

general_client_exit: CLIENT_EXIT '=' TYES ';'
  {
    ConfigFileEntry.client_exit = 1;
  }
    |
    CLIENT_EXIT '-' TNO ';'
  {
    ConfigFileEntry.client_exit = 0;
  };

general_kline_with_connection_closed: KLINE_WITH_CONNECTION_CLOSED '=' TYES ';'
  {
    ConfigFileEntry.kline_with_connection_closed = 1;
  }
    |
    KLINE_WITH_CONNECTION_CLOSED '=' TNO ';'
  {
    ConfigFileEntry.kline_with_connection_closed = 0;
  } ;

general_warn_no_nline: WARN_NO_NLINE '=' TYES ';'
  {
    ConfigFileEntry.warn_no_nline = 1;
  }
    |
    WARN_NO_NLINE '=' TNO ';'
  {
    ConfigFileEntry.warn_no_nline = 0;
  } ;

general_non_redundant_klines: NON_REDUNDANT_KLINES '=' TYES ';'
  {
    ConfigFileEntry.non_redundant_klines = 1;
  }
    |
    NON_REDUNDANT_KLINES '=' TNO ';'
  {
    ConfigFileEntry.non_redundant_klines = 0;
  } ;

general_e_lines_oper_only: E_LINES_OPER_ONLY '=' TYES ';'
  {
    ConfigFileEntry.e_lines_oper_only = 1;
  }
    |
    E_LINES_OPER_ONLY '=' TNO ';'
  {
    ConfigFileEntry.e_lines_oper_only = 0;
  } ;

general_f_lines_oper_only: F_LINES_OPER_ONLY '=' TYES ';'
  {
    ConfigFileEntry.f_lines_oper_only = 1;
  }
    |
    F_LINES_OPER_ONLY '=' TNO ';'
  {
    ConfigFileEntry.f_lines_oper_only = 0;
  } ;

general_o_lines_oper_only: O_LINES_OPER_ONLY '=' TYES ';'
  {
    ConfigFileEntry.o_lines_oper_only = 1;
  }
    |
    O_LINES_OPER_ONLY '=' TNO ';'
  {
    ConfigFileEntry.o_lines_oper_only = 0;
  } ;

general_stats_notice: STATS_NOTICE '=' TYES ';'
  {
    ConfigFileEntry.stats_notice = 1;
  }
    |
    STATS_NOTICE '=' TNO ';'
  {
    ConfigFileEntry.stats_notice = 0;
  } ;

general_pace_wait: PACE_WAIT '=' NUMBER ';'
  {
    ConfigFileEntry.pace_wait = yylval.number;
  } ;

general_whois_wait: WHOIS_WAIT '=' NUMBER ';'
  {
    ConfigFileEntry.whois_wait = yylval.number;
  } ;

general_knock_delay: KNOCK_DELAY '=' NUMBER ';'
  {
    ConfigFileEntry.knock_delay = yylval.number;
  } ;

general_short_motd: SHORT_MOTD '=' TYES ';'
  {
    ConfigFileEntry.short_motd = 1;
  }
    |
    SHORT_MOTD '=' TNO ';'
  {
    ConfigFileEntry.short_motd = 0;
  } ;
  
general_no_oper_flood: NO_OPER_FLOOD '=' TYES ';'
  {
    ConfigFileEntry.no_oper_flood = 1;
  }
    | 
    NO_OPER_FLOOD '=' TNO ';'
  {
    ConfigFileEntry.no_oper_flood = 0;
  };

general_iauth_server: IAUTH_SERVER '=' QSTRING ';'
{
	strcpy(iAuth.hostname, yylval.string);
} ;

general_iauth_port: IAUTH_PORT '=' NUMBER ';'
{
	iAuth.port = yylval.number;
} ;

general_fname_userlog: FNAME_USERLOG '=' QSTRING ';'
{
	strcpy(ConfigFileEntry.fname_userlog, yylval.string);
} ;

general_fname_foperlog: FNAME_FOPERLOG '=' QSTRING ';'
{
	strcpy(ConfigFileEntry.fname_foperlog, yylval.string);
};

general_fname_operlog: FNAME_OPERLOG '=' QSTRING ';'
{
	strcpy(ConfigFileEntry.fname_operlog, yylval.string);
};

general_stats_p_notice: STATS_P_NOTICE '=' TYES ';'
  {
    ConfigFileEntry.stats_p_notice = 1;
  } 
    | 
    STATS_P_NOTICE '=' TNO ';'
  {
    ConfigFileEntry.stats_p_notice = 0;
  } ;

general_glines: GLINES '=' TYES ';'
  {
    ConfigFileEntry.glines = 1;
  } 
    |
    GLINES '=' TNO ';'
  {
    ConfigFileEntry.glines = 0;
  } ;

general_message_locale: MESSAGE_LOCALE '=' QSTRING ';'
  {
    char langenv[BUFSIZE];
    ircsprintf(langenv, "LANGUAGE=%s", yylval.string);
    putenv(langenv);
  } ;

general_gline_time: GLINE_TIME '=' NUMBER ';'
  {
    ConfigFileEntry.gline_time = yylval.number;
  } ;

general_idletime: IDLETIME '=' NUMBER ';'
  {
    ConfigFileEntry.idletime = yylval.number;
  } ;

general_dots_in_ident: DOTS_IN_IDENT '=' NUMBER ';'
  {
    ConfigFileEntry.dots_in_ident = yylval.number;
  } ;

general_maximum_links: MAXIMUM_LINKS '=' NUMBER ';'
  {
    ConfigFileEntry.maximum_links = yylval.number;
  } ;

general_hide_server: HIDESERVER '=' TYES ';'
  {
    ConfigFileEntry.hide_server = 1;
  }
    |
    HIDESERVER '=' TNO ';'
  {
    ConfigFileEntry.hide_server = 0;
  } ;

general_max_targets: MAX_TARGETS '=' NUMBER ';'
  {
    ConfigFileEntry.max_targets = yylval.number;
  } ;

general_oper_only_umodes: OPER_ONLY_UMODES 
  {
    ConfigFileEntry.oper_only_umodes = 0;
  }
  '='  umode_items ';' ;

umode_items:	umode_items ',' umode_item |
  umode_item 

umode_item:	T_BOTS 
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_BOTS;
  } 
              | T_CCONN
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_CCONN;
  }
              | T_DEBUG
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_DEBUG;
  }
              | T_FULL
  { 
    ConfigFileEntry.oper_only_umodes |= FLAGS_FULL;
  }
              | T_SKILL
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_SKILL;
  }
              | T_NCHANGE
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_NCHANGE;
  }
              | T_REJ
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_REJ;
  }
              | T_UNAUTH
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_UNAUTH;
  }
              | T_SPY
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_SPY;
  }
              | T_EXTERNAL
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_EXTERNAL;
  }
              | T_OPERWALL
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_OPERWALL;
  } 
              | T_SERVNOTICE
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_SERVNOTICE;
  }
              | T_INVISIBLE
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_INVISIBLE;
  }
              | T_WALLOP
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_WALLOP;
  }
              | T_CALLERID
  {
    ConfigFileEntry.oper_only_umodes |= FLAGS_CALLERID;
  } ;

general_links_notice: LINKS_NOTICE '=' TYES ';'
  {
    ConfigFileEntry.links_notice = 1;
  }
    |
    LINKS_NOTICE '=' TNO ';'
  {
    ConfigFileEntry.links_notice = 0;
  };

general_vchans_oper_only: VCHANS_OPER_ONLY '=' TYES ';'
  {
    ConfigFileEntry.vchans_oper_only = 1;
  }
    |
    VCHANS_OPER_ONLY '=' TNO ';'
  {
    ConfigFileEntry.vchans_oper_only = 0;
  };
