/************************************************************************
 *   IRC - Internet Relay Chat, src/oldparse.c
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
 *  (C) 1988 University of Oulu,Computing Center and Jarkko Oikarinen"
 *
 *  $Id$
 */
#include "s_log.h"
#include "s_conf.h"
#include "client.h"
#include "irc_string.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char *getfield(char *newline);
static  int  oper_privs_from_string(int,char *);
static  int  oper_flags_from_string(char *);
static  char *set_conf_flags(struct ConfItem *,char *);

/*
 * oldParseOneLine
 * Inputs       - pointer to line to parse
 *              - pointer to conf item to add
 * Output       - pointer to aconf if aconf to be added
 *                to link list or NULL if not
 * Side Effects - Parse one old style conf line.
 */

void oldParseOneLine(char* line,struct ConfItem* aconf,
                            int* pccount,int* pncount)
{
  char conf_letter;
  char* tmp;
  char* user_field=(char *)NULL;
  char* pass_field=(char *)NULL;
  char* host_field=(char *)NULL;
  char* port_field=(char *)NULL;
  char* class_field=(char *)NULL;
  int   sendq = 0;

  tmp = getfield(line);
  if (!tmp)
    return;

  conf_letter = *tmp;

  for (;;) /* Fake loop, that I can use break here --msa */
    {
      /* host field */
      if ((host_field = getfield(NULL)) == NULL)
	break;
      
      /* pass field */
      if ((pass_field = getfield(NULL)) == NULL)
	break;

      /* user field */
      if ((user_field = getfield(NULL)) == NULL)
	break;

      /* port field */
      if ((port_field = getfield(NULL)) == NULL)
	break;

      /* class field */
      if ((class_field = getfield(NULL)) == NULL)
	break;
      
      break;
      /* NOTREACHED */
    }

  aconf->flags = 0;

  switch( conf_letter )
    {
    case 'A':case 'a': /* Name, e-mail address of administrator */
      aconf->status = CONF_ADMIN;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_conf(aconf);
      break;

    case 'c':
      aconf->flags |= CONF_FLAGS_ZIP_LINK;
      /* drop into normal C line code */

    case 'C':
      aconf->status = CONF_CONNECT_SERVER;
      ++*pccount;
      aconf->flags |= CONF_FLAGS_ALLOW_AUTO_CONN;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      aconf = conf_add_server(aconf,*pncount,*pccount);
      conf_add_conf(aconf);
      break;

    case 'd':
      aconf->status = CONF_DLINE;
      aconf->flags = CONF_FLAGS_E_LINED;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_d_line(aconf);
      break;

    case 'D': /* Deny lines (immediate refusal) */
      aconf->status = CONF_DLINE;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_d_line(aconf);
      break;

    case 'H': /* Hub server line */
    case 'h':
      aconf->status = CONF_HUB;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_hub_or_leaf(aconf);
      conf_add_conf(aconf);
      break;

    case 'i': /* Just plain normal irc client trying  */
                  /* to connect to me */

      /* drop into normal I line code */

#ifdef LITTLE_I_LINES
      aconf->flags |= CONF_FLAGS_LITTLE_I_LINE;
#endif
    case 'I': /* Just plain normal irc client trying  */
      /* to connect to me */
      aconf->status = CONF_CLIENT;
      
      if(host_field)
	{
	  host_field = set_conf_flags(aconf, host_field);
	  DupString(aconf->host, host_field);
	}
      
      if(user_field)
	{
	  user_field = set_conf_flags(aconf, user_field);
	  DupString(aconf->user, user_field);
	}

      if(class_field)
	DupString(aconf->className, class_field);

      conf_add_i_line(aconf);
      break;
      
    case 'K': /* Kill user line on irc.conf           */
    case 'k':
      aconf->status = CONF_KILL;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_k_line(aconf);
      break;

    case 'L': /* guaranteed leaf server */
    case 'l':
      aconf->status = CONF_LEAF;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_hub_or_leaf(aconf);
      conf_add_conf(aconf);
      break;

      /* Me. Host field is name used for this host */
      /* and port number is the number of the port */
    case 'M':
    case 'm':
      aconf->status = CONF_ME;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_me(aconf);
      ConfigFileEntry.hub = 0;
      if(port_field)
        {
          if(*port_field == '1')
            ConfigFileEntry.hub = 1;
	}
      conf_add_conf(aconf);
      break;

    case 'n': /* connect in case of lp failures     */
      aconf->flags |= CONF_FLAGS_LAZY_LINK;
      /* drop into normal N line code */

    case 'N': /* Server where I should NOT try to     */
      /* but which tries to connect ME        */
      aconf->status = CONF_NOCONNECT_SERVER;
      ++*pncount;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      aconf = conf_add_server(aconf,*pncount,*pccount);
      conf_add_conf(aconf);
      break;

      /* Operator. Line should contain at least */
      /* password and host where connection is  */
    case 'O':
      aconf->status = CONF_OPERATOR;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      /* defaults */
      aconf->port = 
	CONF_OPER_GLOBAL_KILL|CONF_OPER_REMOTE|CONF_OPER_UNKLINE|
	CONF_OPER_K|CONF_OPER_GLINE|CONF_OPER_REHASH;
      if(port_field)
	aconf->port = oper_privs_from_string(aconf->port,port_field);
      if ((tmp = getfield(NULL)) != NULL)
	aconf->hold = oper_flags_from_string(tmp);
      aconf = conf_add_o_line(aconf);
      conf_add_conf(aconf);
      break;

      /* Local Operator, (limited privs --SRB) */
    case 'o':
      aconf->status = CONF_LOCOP;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      aconf->port = CONF_OPER_UNKLINE|CONF_OPER_K;
      if(port_field)
	aconf->port = oper_privs_from_string(aconf->port,port_field);
      if ((tmp = getfield(NULL)) != NULL)
	aconf->hold = oper_flags_from_string(tmp);
      aconf = conf_add_o_line(aconf);
      conf_add_conf(aconf);
      break;

    case 'P': /* listen port line */
    case 'p':
      aconf->status = CONF_LISTEN_PORT;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_port(aconf);
      free_conf(aconf);
      aconf=NULL;
      break;

    case 'Q': /* reserved nicks */
    case 'q': 
      aconf->status = CONF_QUARANTINED_NICK;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_q_line(aconf);
      break;

    case 'U': /* Uphost, ie. host where client reading */
    case 'u': /* this should connect.                  */
      aconf->status = CONF_ULINE;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_u_line(aconf);
      break;

    case 'X': /* rejected gecos */
    case 'x': 
      aconf->status = CONF_XLINE;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      conf_add_x_line(aconf);
      break;

    case 'Y':
    case 'y':
      aconf->status = CONF_CLASS;
      conf_add_fields(aconf,host_field,pass_field,user_field,
		      port_field,class_field);
      if(class_field)
	sendq = atoi(class_field);
      conf_add_class(aconf,sendq);
      free_conf(aconf);
      aconf = NULL;
      break;
      
    default:
      free_conf(aconf);
      log(L_ERROR, "Error in config file: %s", line);
      break;
    }
}

/*
 * field breakup for ircd.conf file.
 */
static char *getfield(char *newline)
{
  static char *line = (char *)NULL;
  char  *end, *field;
        
  if (newline)
    line = newline;

  if (line == (char *)NULL)
    return((char *)NULL);

  field = line;
  if ((end = strchr(line,':')) == NULL)
    {
      line = (char *)NULL;
      if ((end = strchr(field,'\n')) == (char *)NULL)
        end = field + strlen(field);
    }
  else
    line = end + 1;
  *end = '\0';
  return(field);
}

/* oper_privs_from_string
 *
 * inputs        - default privs
 *               - privs as string
 * output        - default privs as modified by privs string
 * side effects -
 *
 */

static int oper_privs_from_string(int int_privs,char *privs)
{
  while(*privs)
    {
      if(*privs == 'O')                     /* allow global kill */
        int_privs |= CONF_OPER_GLOBAL_KILL;
      else if(*privs == 'o')                /* disallow global kill */
        int_privs &= ~CONF_OPER_GLOBAL_KILL;
      else if(*privs == 'U')                /* allow unkline */
        int_privs |= CONF_OPER_UNKLINE;
      else if(*privs == 'u')                /* disallow unkline */
        int_privs &= ~CONF_OPER_UNKLINE;
      else if(*privs == 'R')                /* allow remote squit/connect etc.*/
        int_privs |= CONF_OPER_REMOTE;        
      else if(*privs == 'r')                /* disallow remote squit/connect etc.*/
        int_privs &= ~CONF_OPER_REMOTE;
      else if(*privs == 'N')                /* allow +n see nick changes */
        int_privs |= CONF_OPER_N;
      else if(*privs == 'n')                /* disallow +n see nick changes */
        int_privs &= ~CONF_OPER_N;
      else if(*privs == 'K')                /* allow kill and kline privs */
        int_privs |= CONF_OPER_K;
      else if(*privs == 'k')                /* disallow kill and kline privs */
        int_privs &= ~CONF_OPER_K;
      else if(ConfigFileEntry.glines && *privs == 'G')                /* allow gline */
        int_privs |= CONF_OPER_GLINE;
      else if(ConfigFileEntry.glines && *privs == 'g')                /* disallow gline */
        int_privs &= ~CONF_OPER_GLINE;
      else if(*privs == 'H')                /* allow rehash */
        int_privs |= CONF_OPER_REHASH;
      else if(*privs == 'h')                /* disallow rehash */
        int_privs &= ~CONF_OPER_REHASH;
      else if(*privs == 'D')
        int_privs |= CONF_OPER_DIE;         /* allow die */
      else if(*privs == 'd')
        int_privs &= ~CONF_OPER_DIE;        /* disallow die */
      privs++;
    }
  return(int_privs);
}

/* oper_flags_from_string
 *
 * inputs        - flags as string
 * output        - flags as bit mask
 * side effects -
 *
 * -Dianora
 */

static int oper_flags_from_string(char *flags)
{
  int int_flags=0;

  while(*flags)
    {
      if(*flags == 'i')                        /* invisible */
        int_flags |= FLAGS_INVISIBLE;
      else if(*flags == 'w')                /* see wallops */
        int_flags |= FLAGS_WALLOP;
      else if(*flags == 's')
        int_flags |= FLAGS_SERVNOTICE;
      else if(*flags == 'c')
        int_flags |= FLAGS_CCONN;
      else if(*flags == 'r')
        int_flags |= FLAGS_REJ;
      else if(*flags == 'k')
        int_flags |= FLAGS_SKILL;
      else if(*flags == 'f')
        int_flags |= FLAGS_FULL;
      else if(*flags == 'y')
        int_flags |= FLAGS_SPY;
      else if(*flags == 'd')
        int_flags |= FLAGS_DEBUG;
      else if(*flags == 'n')
        int_flags |= FLAGS_NCHANGE;
      flags++;
    }

  return(int_flags);
}

/*
** from comstud
*/

static char *set_conf_flags(struct ConfItem *aconf,char *tmp)
{
  for(;*tmp;tmp++)
    {
      switch(*tmp)
        {
        case '=':
          aconf->flags |= CONF_FLAGS_SPOOF_IP;
          break;
        case '!':
          aconf->flags |= CONF_FLAGS_LIMIT_IP;
          break;
        case '-':
          aconf->flags |= CONF_FLAGS_NO_TILDE;
          break;
        case '+':
          aconf->flags |= CONF_FLAGS_NEED_IDENTD;
          break;
        case '$':
          aconf->flags |= CONF_FLAGS_PASS_IDENTD;
          break;
        case '%':
          aconf->flags |= CONF_FLAGS_NOMATCH_IP;
          break;
        case '^':        /* is exempt from k/g lines */
          aconf->flags |= CONF_FLAGS_E_LINED;
          break;
        case '&':        /* can run a bot */
          aconf->flags |= CONF_FLAGS_B_LINED;
          break;
        case '>':        /* can exceed max connects */
          aconf->flags |= CONF_FLAGS_F_LINED;
          break;
#ifdef IDLE_CHECK
        case '<':        /* can idle */
          aconf->flags |= CONF_FLAGS_IDLE_LINED;
          break;
#endif
        default:
          return tmp;
        }
    }
  return tmp;
}
