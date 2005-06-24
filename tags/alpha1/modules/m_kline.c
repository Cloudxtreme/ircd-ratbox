/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_kline.c
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
 *   $Id$
 */
#include "tools.h"
#include "m_kline.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "dline_conf.h"
#include "irc_string.h"
#include "ircd.h"
#include "mtrie_conf.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_misc.h"
#include "send.h"
#include "hash.h"
#include "handlers.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

struct Message kline_msgtab = {
  MSG_KLINE, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, ms_kline, mo_kline}
};

struct Message dline_msgtab = {
  MSG_DLINE, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_error, mo_dline}
};

void
_modinit(void)
{
  mod_add_cmd(&kline_msgtab);
  mod_add_cmd(&dline_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&kline_msgtab);
  mod_del_cmd(&dline_msgtab);
}

extern ConfigFileEntryType ConfigFileEntry; /* defined in ircd.c */

/* Local function prototypes */

time_t  valid_tkline(struct Client *sptr, char *string);
char *cluster(char *);
int find_user_host(struct Client *sptr,
                   char *user_host_or_nick, char *user, char *host);

int valid_comment(struct Client *sptr, char *comment);
int valid_user_host(struct Client *sptr, char *user, char *host);
int valid_wild_card(struct Client *sptr, char *user, char *host);
int already_placed_kline(struct Client *sptr, char *user, char *host,
                         time_t tkline_time, unsigned long ip);

int is_ip_kline(char *host,unsigned long *ip, unsigned long *ip_mask);
void apply_kline(struct Client *sptr, struct ConfItem *aconf,
		 const char *reason, const char *current_date,
		 int ip_kline, unsigned long ip, unsigned long ip_mask);

void apply_tkline(struct Client *sptr, struct ConfItem *aconf,
		  const char *current_date, int temporary_kline_time,
		  int ip_kline, unsigned long ip, unsigned long ip_mask);

char *_version = "20001122";

char buffer[IRCD_BUFSIZE];
char user[USERLEN+2];
char host[HOSTLEN+2];

#define MAX_EXT_REASON 100

/*
 * mo_kline
 *
 * inputs	- pointer to server
 *		- pointer to client
 *		- parameter count
 *		- parameter list
 * output	-
 * side effects - k line is added
 *
 */
int mo_kline(struct Client *cptr,
                struct Client *sptr,
                int parc,
                char **parv)
{
  char *reason = "No Reason";
  const char* current_date;
  const char* target_server=NULL;
  int  ip_kline = NO;
  struct ConfItem *aconf;
  time_t tkline_time=0;
  unsigned long ip;
  unsigned long ip_mask;

  if(!IsSetOperK(sptr))
    {
      sendto_one(sptr,":%s NOTICE %s :You have no K flag",
		 me.name,sptr->name);
      return 0;
    }

  parv++;
  parc--;

  tkline_time = valid_tkline(sptr,*parv);

  if( tkline_time == -1 )
    return 0;
  else if( tkline_time > 0 )
    {
      parv++;
      parc--;
    }

  if(parc == 0)
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
		 me.name, sptr->name, "KLINE");
      return -1;
    }

  if ( find_user_host(sptr,*parv,user,host) == 0 )
    return 0;
  parc--;
  parv++;

  if(0 != parc)
    {
      if(0 == irccmp(*parv,"ON"))
	{
	  parc--;
	  parv++;
	  if(parc == 0)
	    {
	      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
			 me.name, sptr->name, "KLINE");
	      return -1;
	    }
	  target_server = *parv;
	  parc--;
	  parv++;
	}
    }

  if(0 != parc)
    reason = *parv;

  if( valid_user_host(sptr,user,host) == 0 )
    return 0;

  if( valid_wild_card(sptr,user,host) == 0 )
    return 0;

  ip_kline = is_ip_kline(host,&ip,&ip_mask);
  current_date = smalldate((time_t) 0);

  aconf = make_conf();
  aconf->status = CONF_KILL;
  DupString(aconf->host, host);
  DupString(aconf->user, user);

  aconf->port = 0;

  if(target_server != NULL)
    {
      sendto_cap_serv_butone(CAP_KLN, &me,
			   ":%s KLINE %s %s %d %s %s :%s",
			   me.name, sptr->name,
			   target_server,
			   tkline_time, user, host, reason);

      /* If we are sending it somewhere that doesnt include us, we stop
       * else we apply it locally too
       */
      if(!match(target_server,me.name))
	return 0;
    }

  if ( already_placed_kline(sptr, user, host, tkline_time, ip) )
    return 0;

  if (ip_kline)
   {
     aconf->ip = ip;
     aconf->ip_mask = ip_mask;
   }

  if(tkline_time)
    {
      ircsprintf(buffer,
		 "Temporary K-line %d min. - %s (%s)",
		 tkline_time/60,
		 reason,
		 current_date);
      DupString(aconf->passwd, buffer );
      apply_tkline(sptr, aconf, current_date, tkline_time,
		   ip_kline, ip, ip_mask);
    }
  else
    {
      ircsprintf(buffer, "%s (%s)",
		 reason,
		 current_date);
      DupString(aconf->passwd, buffer );
      apply_kline(sptr, aconf, reason, current_date, ip_kline, ip, ip_mask);
    }

  return 0;
} /* mo_kline() */

/*
 * ms_kline()
 *
 *
 */
int ms_kline(struct Client *cptr,
                struct Client *sptr,
                int parc,
                char *parv[])
{
  const char *current_date;
  struct Client *rcptr=NULL;
  struct ConfItem *aconf=NULL;
  int    tkline_time;
  int ip_kline = NO;
  unsigned long ip;
  unsigned long ip_mask;

  if(parc < 7)
    return 0;

  /* parv[0] parv[1] parv[2]       parv[3]     parv[4] parv[5] parv[6] */
  /* server  oper    target_server tkline_time user    host    reason */
  sendto_cap_serv_butone (CAP_KLN, cptr, ":%s KLINE %s %s %s %s %s: %s",
			  parv[0], parv[1],
			  parv[2], parv[3],
			  parv[4], parv[5],parv[6]);


  if(!match(parv[2],me.name))
    return 0;

  if ((rcptr = hash_find_client(parv[1],(struct Client *)NULL)))
    {
      if(!IsPerson(rcptr))
	return 0;
    }
  else
    return 0;

  /* These should never happen but... */
  if( rcptr->name == NULL )
    return 0;
  if( rcptr->user == NULL )
    return 0;
  if( rcptr->host == NULL )
    return 0;

  ip_kline = is_ip_kline(parv[5],&ip,&ip_mask);
  tkline_time = atoi(parv[3]);

  if(find_u_conf(sptr->name,rcptr->username,rcptr->host))
    {
      sendto_realops_flags(FLAGS_ALL,
			   "*** Received K-Line for %s@%s, from %s!%s@%s on %s",
			   parv[4],
			   parv[5],
			   rcptr->name,
			   rcptr->username,
			   rcptr->host,
			   sptr->name);

      /* We check if the kline already exists after we've announced its 
       * arrived, to avoid confusing opers - fl
       */
      if ( already_placed_kline(sptr, parv[4], parv[5], (int)parv[3], ip) )
        return 0;

      aconf = make_conf();

      aconf->status = CONF_KILL;
      DupString(aconf->user, parv[4]);
      DupString(aconf->host, parv[5]);
      DupString(aconf->passwd, parv[6]);
      current_date = smalldate((time_t) 0);

      if(ip_kline)
        {
          aconf->ip = ip;
          aconf->ip_mask = ip_mask;
        }

      if(tkline_time)
          apply_tkline(rcptr, aconf, current_date, tkline_time,
                       ip_kline, ip, ip_mask);
      else
	apply_kline(rcptr, aconf, aconf->passwd, current_date,
                       ip_kline, ip, ip_mask);	

      }
  return 0;
} /* ms_kline() */

/*
 * apply_kline
 *
 * inputs	-
 * output	- NONE
 * side effects	- kline as given, is added to apropriate tree
 *		  and conf file
 */
void apply_kline(struct Client *sptr, struct ConfItem *aconf,
                 const char *reason, const char *current_date,
		 int ip_kline, unsigned long ip, unsigned long ip_mask)
{
  if(ip_kline)
    {
      aconf->ip = ip;
      aconf->ip_mask = ip_mask;
      add_ip_Kline(aconf);
    }
  else
    add_mtrie_conf_entry(aconf,CONF_KILL);

  WriteKlineOrDline( KLINE_TYPE,
		     sptr,
		     aconf->user,
		     aconf->host,
		     reason,
		     current_date);

  /* Now, activate kline against current online clients */
  check_klines();
}

/*
 * apply_tkline
 *
 * inputs	-
 * output	- NONE
 * side effects	- tkline as given is placed
 */
void apply_tkline(struct Client *sptr, struct ConfItem *aconf,
		  const char *current_date, int tkline_time,
		  int ip_kline, unsigned long ip, unsigned long ip_mask)
{
  aconf->hold = CurrentTime + tkline_time;
  add_temp_kline(aconf);
  sendto_realops_flags(FLAGS_ALL,
                       "%s added temporary %d min. K-Line for [%s@%s] [%s]",
                       sptr->name, tkline_time/60, aconf->user, aconf->host,
                       aconf->passwd);
  log(L_TRACE, "%s added temporary %d min. K-Line for [%s@%s] [%s]",
      sptr->name, tkline_time/60, aconf->user, aconf->host, aconf->passwd);
  check_klines();
}

/*
 * valid_tkline()
 * 
 * inputs       - pointer to client requesting kline
 *              - argument count
 *              - pointer to ascii string in
 * output       - -1 not enough parameters
 *              - 0 if not an integer number, else the number
 * side effects - none
 */
time_t valid_tkline(struct Client *sptr, char *p)
{
  time_t result = 0;

  while(*p)
    {
      if(IsDigit(*p))
        {
          result *= 10;
          result += ((*p) & 0xF);
          p++;
        }
      else
        return(0);
    }
  /* in the degenerate case where oper does a /quote kline 0 user@host :reason 
   * i.e. they specifically use 0, I am going to return 1 instead
   * as a return value of non-zero is used to flag it as a temporary kline
   */

  if(result == 0)
    result = 1;

  if(result > (24*60))
    result = (24*60); /* Max it at 24 hours */

  result = (time_t)result * (time_t)60;  /* turn it into seconds */

  return(result);
}

/*
 * cluster()
 *
 * inputs       - pointer to a hostname
 * output       - pointer to a static of the hostname masked
 *                for use in a kline.
 * side effects - NONE
 *
 */
char *cluster(char *hostname)
{
  static char result[HOSTLEN + 1];      /* result to return */
  char        temphost[HOSTLEN + 1];    /* work place */
  char        *ipp;             /* used to find if host is ip # only */
  char        *host_mask;       /* used to find host mask portion to '*' */
  char        *zap_point = NULL; /* used to zap last nnn portion of an ip # */
  char        *tld;             /* Top Level Domain */
  int         is_ip_number;     /* flag if its an ip # */             
  int         number_of_dots;   /* count number of dots for both ip# and
                                   domain klines */
  if (!hostname)
    return (char *) NULL;       /* EEK! */

  /* If a '@' is found in the hostname, this is bogus
   * and must have been introduced by server that doesn't
   * check for bogus domains (dns spoof) very well. *sigh* just return it...
   * I could also legitimately return (char *)NULL as above.
   */

  if(strchr(hostname,'@'))      
    {
      strncpy_irc(result, hostname, HOSTLEN);      
      result[HOSTLEN] = '\0';
      return(result);
    }

  strncpy_irc(temphost, hostname, HOSTLEN);
  temphost[HOSTLEN] = '\0';

  is_ip_number = YES;   /* assume its an IP# */
  ipp = temphost;
  number_of_dots = 0;

  while (*ipp)
    {
      if( *ipp == '.' )
        {
          number_of_dots++;

          if(number_of_dots == 3)
            zap_point = ipp;
          ipp++;
        }
      else if(!IsDigit(*ipp))
        {
          is_ip_number = NO;
          break;
        }
      ipp++;
    }

  if(is_ip_number && (number_of_dots == 3))
    {
      zap_point++;
      *zap_point++ = '*';               /* turn 111.222.333.444 into */
      *zap_point = '\0';                /*      111.222.333.*        */
      strncpy_irc(result, temphost, HOSTLEN);
      result[HOSTLEN] = '\0';
      return(result);
    }
  else
    {
      tld = strrchr(temphost, '.');
      if(tld)
        {
          number_of_dots = 2;
          if(tld[3])                     /* its at least a 3 letter tld
                                            i.e. ".com" tld[3] = 'm' not 
                                            '\0' */
                                         /* 4 letter tld's are coming */
            number_of_dots = 1;

          if(tld != temphost)           /* in these days of dns spoofers ...*/
            host_mask = tld - 1;        /* Look for host portion to '*' */
          else
            host_mask = tld;            /* degenerate case hostname is
                                           '.com' etc. */

          while (host_mask != temphost)
            {
              if(*host_mask == '.')
                number_of_dots--;
              if(number_of_dots == 0)
                {
                  result[0] = '*';
                  strncpy_irc(result + 1, host_mask, HOSTLEN - 1);
                  result[HOSTLEN] = '\0';
                  return(result);
                }
              host_mask--;
            }
          result[0] = '*';                      /* foo.com => *foo.com */
          strncpy_irc(result + 1, temphost, HOSTLEN - 1);
          result[HOSTLEN] = '\0';
        }
      else      /* no tld found oops. just return it as is */
        {
          strncpy_irc(result, temphost, HOSTLEN);
          result[HOSTLEN] = '\0';
          return(result);
        }
    }

  return (result);
}

/*
 * mo_dline
 *
 * inputs	- pointer to server
 *		- pointer to client
 *		- parameter count
 *		- parameter list
 * output	-
 * side effects - D line is added
 *
 */
int mo_dline(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  char *dlhost, *reason;
  char *p;
  struct Client *acptr;
  char cidr_form_host[HOSTLEN + 1];
  unsigned long ip_host;
  unsigned long ip_mask;
  struct ConfItem *aconf;
  char dlbuffer[1024];
  const char* current_date;

  if(!IsSetOperK(sptr))
    {
      sendto_one(sptr,":%s NOTICE %s :You have no K flag",me.name,parv[0]);
      return 0;
    }

  dlhost = parv[1];
  strncpy_irc(cidr_form_host, dlhost, 32);
  cidr_form_host[32] = '\0';

  if((p = strchr(cidr_form_host,'*')))
    {
      *p++ = '0';
      *p++ = '/';
      *p++ = '2';
      *p++ = '4';
      *p++ = '\0';
      dlhost = cidr_form_host;
    }

  if(!is_address(dlhost,&ip_host,&ip_mask))
    {
      if (!(acptr = find_chasing(sptr, parv[1], NULL)))
        return 0;

      if(!acptr->user)
        return 0;

      if (IsServer(acptr))
        {
          sendto_one(sptr,
                     ":%s NOTICE %s :Can't DLINE a server silly",
                     me.name, parv[0]);
          return 0;
        }
              
      if(!MyConnect(acptr))
        {
          sendto_one(sptr,
                     ":%s NOTICE :%s :Can't DLINE nick on another server",
                     me.name, parv[0]);
          return 0;
        }

      if(IsElined(acptr))
        {
          sendto_one(sptr,
                     ":%s NOTICE %s :%s is E-lined",me.name,parv[0],
                     acptr->name);
          return 0;
        }

      /*
       * XXX - this is always a fixed length output, we can get away
       * with strcpy here
       *
       * strncpy_irc(cidr_form_host, inetntoa((char *)&acptr->ip), 32);
       * cidr_form_host[32] = '\0';
       */
       strcpy(cidr_form_host, inetntoa((char*) &acptr->localClient->ip));
      
      p = strchr(cidr_form_host,'.');
      if(!p)
        return 0;
      /* 192. <- p */

      p++;
      p = strchr(p,'.');
      if(!p)
        return 0;
      /* 192.168. <- p */

      p++;
      p = strchr(p,'.');
      if(!p)
        return 0;
      /* 192.168.0. <- p */

      p++;
      *p++ = '0';
      *p++ = '/';
      *p++ = '2';
      *p++ = '4';
      *p++ = '\0';
      dlhost = cidr_form_host;

      ip_mask = 0xFFFFFF00L;
      ip_host = ntohl(acptr->localClient->ip.s_addr);
    }


  if (parc > 2) /* host :reason */
    {
      if ( valid_comment(sptr,parv[2]) == 0 )
	return 0;

      if(*parv[2])
        reason = parv[2];
      else
        reason = "No reason";
    }
  else
    reason = "No reason";


  if((ip_mask & 0xFFFFFF00) ^ 0xFFFFFF00)
    {
      if(ip_mask != 0xFFFFFFFF)
        {
          sendto_one(sptr, ":%s NOTICE %s :Can't use a mask less than 24 with dline",
                     me.name,
                     parv[0]);
          return 0;
        }
    }

  if( ConfigFileEntry.non_redundant_klines && (aconf = match_Dline(ip_host)) )
     {
       char *creason;
       creason = aconf->passwd ? aconf->passwd : "<No Reason>";
       if(IsConfElined(aconf))
         sendto_one(sptr, ":%s NOTICE %s :[%s] is (E)d-lined by [%s] - %s",
                    me.name,
                    parv[0],
                    dlhost,
                    aconf->host,creason);
         else
           sendto_one(sptr, ":%s NOTICE %s :[%s] already D-lined by [%s] - %s",
                      me.name,
                      parv[0],
                      dlhost,
                      aconf->host,creason);
      return 0;
       
     }

  current_date = smalldate((time_t) 0);

  ircsprintf(dlbuffer, "%s (%s)",reason,current_date);

  aconf = make_conf();
  aconf->status = CONF_DLINE;
  DupString(aconf->host,dlhost);
  DupString(aconf->passwd,dlbuffer);

  aconf->ip = ip_host;
  aconf->ip_mask = ip_mask;

  add_Dline(aconf);

  /*
   * Write dline to configuration file
   */
  WriteKlineOrDline(DLINE_TYPE,
		    sptr,
		    NULL,
		    dlhost,
		    reason,
		    current_date);

  check_klines();
  return 0;
} /* m_dline() */

/*
 * find_user_host
 * inputs	- pointer to client placing kline
 *              - pointer to user_host_or_nick
 *              - pointer to user buffer
 *              - pointer to host buffer
 * output	- 0 if not ok to kline, 1 to kline i.e. if valid user host
 * side effects -
 */
int find_user_host(struct Client *sptr,
		   char *user_host_or_nick, char *luser, char *lhost)
{
  struct Client *acptr;
  char *hostp;

  if ( (hostp = strchr(user_host_or_nick, '@')) || *user_host_or_nick == '*' )
    {
      /* Explicit user@host mask given */

      if(hostp)                                    /* I'm a little user@host */
        {
          *(hostp++) = '\0';                       /* short and squat */
          strncpy(luser,user_host_or_nick,USERLEN); /* here is my user */
          strncpy(lhost,hostp,HOSTLEN);             /* here is my host */
        }
      else
        {
          luser[0] = '*';             /* no @ found, assume its *@somehost */
          luser[1] = '\0';	  
          strncpy(lhost,user_host_or_nick,HOSTLEN);
        }

      return 1;
    }
  else
    {
      /* Try to find user@host mask from nick */

      if (!(acptr = find_chasing(sptr, user_host_or_nick, NULL)))
        return 0;

      if(!acptr->user)
        return 0;

      if (IsServer(acptr))
        {
	  sendto_one(sptr,
	     ":%s NOTICE %s :Can't KLINE a server, use @'s where appropriate",
		     me.name, sptr->name);
          return 0;
        }

      if(IsElined(acptr))
        {
          if(!IsServer(sptr))
            sendto_one(sptr,
                       ":%s NOTICE %s :%s is E-lined",me.name,sptr->name,
                       acptr->name);
          return 0;
        }

      /* turn the "user" bit into "*user", blow away '~'
       * if found in original user name (non-idented)
       */

      strncpy_irc(luser, acptr->username, USERLEN);
      if (*acptr->username == '~')
        luser[0] = '*';

      strncpy_irc(lhost,cluster(acptr->host),HOSTLEN);
    }

  return 1;
}

/*
 * valid_user_host
 * inputs	- pointer to client placing kline
 *              - pointer to user buffer
 *              - pointer to host buffer
 * output	- 0 if not valid user or host, 1 if valid
 * side effects -
 */
int valid_user_host( struct Client *sptr, char *luser, char *lhost)
{
  /*
   * Check for # in user@host
   */

  if(strchr(lhost, '#'))
    {
      if(!IsServer(sptr))
        sendto_one(sptr, ":%s NOTICE %s :Invalid character '#' in hostname",
                   me.name, sptr->name);
      return 0;
    }
  if(strchr(luser, '#'))
    { 
      if(!IsServer(sptr))
        sendto_one(sptr, ":%s NOTICE %s :Invalid character '#' in username",
                   me.name, sptr->name);
      return 0;
    }   

  return 1;
}

/*
 * valid_wild_card
 * input        - pointer to client
 *              - pointer to user to check
 *              - pointer to host to check
 * output       - 0 if not valid, 1 if valid
 * side effects -
 */
int valid_wild_card(struct Client *sptr, char *luser, char *lhost)
{
  char *p;
  char tmpch;
  int nonwild;

  /*
   * Now we must check the user and host to make sure there
   * are at least NONWILDCHARS non-wildcard characters in
   * them, otherwise assume they are attempting to kline
   * *@* or some variant of that. This code will also catch
   * people attempting to kline *@*.tld, as long as NONWILDCHARS
   * is greater than 3. In that case, there are only 3 non-wild
   * characters (tld), so if NONWILDCHARS is 4, the kline will
   * be disallowed.
   * -wnder
   */

  nonwild = 0;
  p = luser;
  while ((tmpch = *p++))
  {
    if (!IsKWildChar(tmpch))
    {
      /*
       * If we find enough non-wild characters, we can
       * break - no point in searching further.
       */
      if (++nonwild >= NONWILDCHARS)
        break;
    }
  }

  if (nonwild < NONWILDCHARS)
  {
    /*
     * The user portion did not contain enough non-wild
     * characters, try the host.
     */
    p = host;
    while ((tmpch = *p++))
    {
      if (!IsKWildChar(tmpch))
        if (++nonwild >= NONWILDCHARS)
          break;
    }
  }

  if (nonwild < NONWILDCHARS)
  {
    /*
     * Not enough non-wild characters were found, assume
     * they are trying to kline *@*.
     */
    if (!IsServer(sptr))
      sendto_one(sptr,
        ":%s NOTICE %s :Please include at least %d non-wildcard characters with the user@host",
        me.name,
        sptr->name,
        NONWILDCHARS);

    return 0;
  }

  return 1;
}

/*
 * valid_comment
 * inputs	- pointer to client
 *              - pointer to comment
 * output       - 0 if no valid comment, 1 if valid
 * side effects - NONE
 */
int valid_comment(struct Client *sptr, char *comment)
{
  if(strchr(comment, ':'))
    {
      if(!IsServer(sptr))
	sendto_one(sptr,
		   ":%s NOTICE %s :Invalid character ':' in comment",
		   me.name, sptr->name);
      return 0;
    }

  if(strchr(comment, '#'))
    {
      if(!IsServer(sptr))
	sendto_one(sptr,
		   ":%s NOTICE %s :Invalid character '#' in comment",
		   me.name, sptr->name);
      return 0;
    }
  return 1;
}

/*
 * already_placed_kline
 * inputs	- pointer to client placing kline
 *		- user
 *		- host
 *              - tkline_time
 *		- ip 
 * output	- 1 if already placed, 0 if not
 * side effects - NONE
 */
int already_placed_kline(struct Client *sptr, char *luser, char *lhost,
                         time_t tkline_time, unsigned long ip)
{
  char *reason;
  struct ConfItem *aconf;

  if(ConfigFileEntry.non_redundant_klines) 
    {
      if ((aconf = find_matching_mtrie_conf(lhost,luser,ip)) && 
         (aconf->status & CONF_KILL))
        {
          reason = aconf->passwd ? aconf->passwd : "<No Reason>";

          /* Remote servers can set klines, so if its a dupe we warn all 
           * local opers and leave it at that
           */
          if(IsServer(sptr))
            sendto_realops_flags(FLAGS_ALL, 
                     "*** Remote K-Line [%s@%s] already K-Lined by [%s@%s] - %s",
                     luser, lhost, aconf->user, aconf->host, reason);
          else
             sendto_one(sptr,
                     ":%s NOTICE %s :[%s@%s] already K-Lined by [%s@%s] - %s",
                     me.name, sptr->name, luser, lhost, aconf->user,
                     aconf->host, reason);
          return 1;
        }

      if (tkline_time && (aconf = find_tkline(lhost,luser,(unsigned long)ip)))
        {
          reason = aconf->passwd ? aconf->passwd : "<No Reason>";
          if(IsServer(sptr))
            sendto_realops_flags(FLAGS_ALL,
                    "*** Remote K-Line [%s@%s] already temp K-Lined by [%s@%s] - %s",
                    luser, lhost, aconf->user, aconf->host, reason);
          else
            sendto_one(sptr,
                    ":%s NOTICE %s :[%s@%s] already temp K-Lined by [%s@%s] - %s",
                     me.name, sptr->name, luser, lhost, aconf->user,
                     aconf->host, reason);
          return 1;
        }
    }

  return 0;
}

/*
 * is_ip_kline
 * inputs	- hostname (ip)
 * 		- pointer to where to put ip
 * 		- pointer to where to put ip_mask
 * output	- YES if valid ip_kline NO if not
 * side effects	- NONE
 */
int is_ip_kline(char *lhost,unsigned long *ip, unsigned long *ip_mask)
{
  char *p;

  /* 
  ** At this point, I know the user and the host to place the k-line on
  ** I also know whether its supposed to be a temporary kline or not
  ** I also know the reason field is clean
  ** Now what I want to do, is find out if its a kline of the form
  **
  ** /quote kline *@192.168.0.*
  **
  */

  if((is_address(lhost, ip, ip_mask)))
     {
       if( (p = strchr(lhost,'*')) )
         {
           *p++ = '0';
           *p++ = '/';
           *p++ = '2';
           *p++ = '4';
           *p++ = '\0';
         }
       return(YES);
    }

  return NO;
}
