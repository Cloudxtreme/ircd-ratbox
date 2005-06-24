/************************************************************************
 *   IRC - Internet Relay Chat, src/s_conf.c
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <assert.h>

#include "config.h"
#include "ircd_defs.h"
#include "tools.h"
#include "s_conf.h"
#include "s_stats.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "event.h"
#include "dline_conf.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "listener.h"
#include "hostmask.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_log.h"
#include "send.h"
#include "s_gline.h"
#include "s_debug.h"
#include "fileio.h"
#include "memory.h"


extern int yyparse(); /* defined in yy.tab.c */
extern ConfigFileEntryType ConfigFileEntry; /* defined in ircd.c */
extern int lineno;
extern char linebuf[];

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif


/* internally defined functions */

static void lookup_confhost(struct ConfItem* aconf);
static int  SplitUserHost( struct ConfItem * );

static void     read_conf(FBFILE*);
static void     read_kd_lines(FBFILE*);
static void     clear_out_old_conf(void);
static void     flush_deleted_I_P(void);
static void     expire_tklines(dlink_list *);

FBFILE* conf_fbfile_in;
char    conf_line_in[256];
struct ConfItem* yy_aconf;
extern char yytext[];

/* address of class 0 conf */
static struct   Class* class0;

static  int     attach_iline(struct Client *, struct ConfItem *);

static void clear_special_conf(struct ConfItem **);

/* usually, with hash tables, you use a prime number...
 * but in this case I am dealing with ip addresses, not ascii strings.
 */

#define IP_HASH_SIZE 0x1000

typedef struct ip_entry
{
#ifndef IPV6
  unsigned long ip;
#else
  struct irc_inaddr ip;
#endif
  int        count;
  struct ip_entry *next;
}IP_ENTRY;

static IP_ENTRY *ip_hash_table[IP_HASH_SIZE];

static int hash_ip(struct irc_inaddr *);

static IP_ENTRY *find_or_add_ip(struct Client *);

/* general conf items link list root */
struct ConfItem* ConfigItemList = NULL;

/* conf xline link list root */
struct ConfItem        *x_conf = ((struct ConfItem *)NULL);

/* conf qline link list root */
struct ConfItem        *q_conf = ((struct ConfItem*)NULL);

/* conf uline link list root */
struct ConfItem        *u_conf = ((struct ConfItem *)NULL);

/*
 * conf_dns_callback - called when resolver query finishes
 * if the query resulted in a successful search, hp will contain
 * a non-null pointer, otherwise hp will be null.
 * if successful save hp in the conf item it was called with
 */
static void conf_dns_callback(void* vptr, adns_answer *reply)
{
  struct ConfItem *aconf = (struct ConfItem *) vptr;
  if(reply->status == adns_s_ok)
  {
#ifdef IPV6
	copy_s_addr(IN_ADDR(aconf->ipnum), reply->rrs.addr->addr.inet6.sin6_addr.s6_addr);
#else
	copy_s_addr(IN_ADDR(aconf->ipnum), reply->rrs.addr->addr.inet.sin_addr.s_addr);
#endif
	MyFree(reply);
  } 
  BlockHeapFree(dns_blk, aconf->dns_query);
  aconf->dns_query = NULL;
}

/*
 * conf_dns_lookup - do a nameserver lookup of the conf host
 * if the conf entry is currently doing a ns lookup do nothing, otherwise
 * set the conf_dns_pending flag
 */
void conf_dns_lookup(struct ConfItem* aconf)
{
  if (!aconf->dns_pending)
    {
      aconf->dns_query = BlockHeapAlloc(dns_blk);
      aconf->dns_query->ptr = aconf;
      aconf->dns_query->callback = conf_dns_callback;
      adns_gethost(aconf->host, aconf->aftype, aconf->dns_query);
      aconf->dns_pending = 1;
    }
}

/*
 * make_conf - create a new conf entry
 */
struct ConfItem* make_conf()
{
  struct ConfItem* aconf;

  aconf = (struct ConfItem*) MyMalloc(sizeof(struct ConfItem));
  aconf->status       = CONF_ILLEGAL;
  aconf->aftype	      = AF_INET;
/*  aconf->ipnum.s_addr = INADDR_NONE; */
  return (aconf);
}

/*
 * delist_conf - remove conf item from ConfigItemList
 */
static void delist_conf(struct ConfItem* aconf)
{
  if (aconf == ConfigItemList)
    ConfigItemList = ConfigItemList->next;
  else
    {
      struct ConfItem* bconf;

      for (bconf = ConfigItemList; aconf != bconf->next; bconf = bconf->next)
        ;
      bconf->next = aconf->next;
    }
  aconf->next = NULL;
}

void free_conf(struct ConfItem* aconf)
{
  assert(0 != aconf);
  assert(!(aconf->status & CONF_CLIENT) ||
         strcmp(aconf->host, "NOMATCH") || (aconf->clients == -1));
  delete_adns_queries(aconf->dns_query);
  MyFree(aconf->host);
  if (aconf->passwd)
    memset(aconf->passwd, 0, strlen(aconf->passwd));
  MyFree(aconf->passwd);
  if (aconf->spasswd)
    memset(aconf->spasswd, 0, strlen(aconf->spasswd));
  MyFree(aconf->spasswd);
  MyFree(aconf->name);
  MyFree(aconf->className);
  MyFree(aconf->user);
  MyFree((char*) aconf);
}

/*
 * remove all conf entries from the client except those which match
 * the status field mask.
 */
void det_confs_butmask(struct Client* cptr, int mask)
{
  dlink_node *dlink;
  dlink_node *link_next;
  struct ConfItem *aconf;

  for (dlink = cptr->localClient->confs.head; dlink; dlink = link_next)
    {
      link_next = dlink->next;
      aconf = dlink->data;

      if ((aconf->status & mask) == 0)
        detach_conf(cptr, aconf);
    }
}

static struct LinkReport {
  int conf_type;
  int rpl_stats;
  int conf_char;
} report_array[] = {
  { CONF_SERVER,           RPL_STATSCLINE, 'C'},
  { CONF_LEAF,             RPL_STATSLLINE, 'L'},
  { CONF_OPERATOR,         RPL_STATSOLINE, 'O'},
  { CONF_HUB,              RPL_STATSHLINE, 'H'},
  { 0, 0, '\0' }
};

/*
 * report_configured_links
 *
 * inputs	- pointer to client to report to
 *		- type of line to report
 * output	- NONE
 * side effects	-
 */
void report_configured_links(struct Client* sptr, int mask)
{
  struct ConfItem*   tmp;
  struct LinkReport* p;
  char*              host;
  char*              pass;
  char*              user;
  char*              name;
  char*		     classname;
  int                port;

  for (tmp = ConfigItemList; tmp; tmp = tmp->next) {
    if (tmp->status & mask)
      {
        for (p = &report_array[0]; p->conf_type; p++)
          if (p->conf_type == tmp->status)
            break;
        if(p->conf_type == 0)return;

        get_printable_conf(tmp, &name, &host, &pass, &user, &port,&classname);

        if(mask & CONF_SERVER)
          {
            char c;

            c = p->conf_char;
            if(tmp->flags & CONF_FLAGS_LAZY_LINK)
              c = 'c';

            /* Allow admins to see actual ips */
            if(IsSetOperAdmin(sptr))
              sendto_one(sptr, form_str(p->rpl_stats), me.name,
                         sptr->name, c,
                         host,
                         name,
                         port,
                         classname,
                         oper_flags_as_string((int)tmp->hold));
            else
              sendto_one(sptr, form_str(p->rpl_stats), me.name,
                         sptr->name, c,
                         "*@127.0.0.1",
                         name,
                         port,
                         classname);

          }
        else if(mask & (CONF_OPERATOR))
          {
            /* Don't allow non opers to see oper privs */
            if(IsOper(sptr))
              sendto_one(sptr, form_str(p->rpl_stats), me.name,
                         sptr->name,
                         p->conf_char,
                         user, host, name,
                         oper_privs_as_string((struct Client *)NULL,port),
                         classname,
                         oper_flags_as_string((int)tmp->hold));
            else
              sendto_one(sptr, form_str(p->rpl_stats), me.name,
                         sptr->name, p->conf_char,
                         user, host, name,
                         "0",
                         classname,
                         "");
          }
        else
          sendto_one(sptr, form_str(p->rpl_stats), me.name,
                     sptr->name, p->conf_char,
                     host, name, port,
                     classname);
      }
  }
}

/*
 * report_specials - report special conf entries
 *
 * inputs       - struct Client pointer to client to report to
 *              - int flags type of special struct ConfItem to report
 *              - int numeric for struct ConfItem to report
 * output       - none
 * side effects -
 */
void report_specials(struct Client* sptr, int flags, int numeric)
{
  struct ConfItem* this_conf;
  struct ConfItem* aconf;
  char*            name;
  char*            host;
  char*            pass;
  char*            user;
  char*       classname;
  int              port;

  if (flags & CONF_XLINE)
    this_conf = x_conf;
  else if (flags & CONF_ULINE)
    this_conf = u_conf;
  else return;

  for (aconf = this_conf; aconf; aconf = aconf->next)
    if (aconf->status & flags)
      {
        get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);

        sendto_one(sptr, form_str(numeric),
                   me.name,
                   sptr->name,
                   name,
                   pass);
      }
}

/*
 * check_client
 *
 * inputs	- pointer to client
 * output	- 0 = Success
 * 		  NOT_AUTHORIZED (-1) = Access denied (no I line match)
 * 		  SOCKET_ERROR   (-2) = Bad socket.
 * 		  I_LINE_FULL    (-3) = I-line is full
 *		  TOO_MANY       (-4) = Too many connections from hostname
 * 		  BANNED_CLIENT  (-5) = K-lined
 * side effects - Ordinary client access check.
 *		  Look for conf lines which have the same
 * 		  status as the flags passed.
 */
int check_client(struct Client *cptr, struct Client *sptr, char *username)
{
  static char     sockname[HOSTLEN + 1];
  int             i;
 
  ClearAccess(sptr);

  if ((i = attach_Iline(sptr, username)))
    {
      log(L_INFO, "Access denied: %s[%s]", sptr->name, sockname);
    }

  switch( i )
    {
    case SOCKET_ERROR:
      (void)exit_client(cptr, sptr, &me, "Socket Error");
      break;

    case TOO_MANY:
      sendto_realops_flags(FLAGS_FULL, "%s for %s (%s).",
			   "Too many on IP", get_client_host(sptr),
			   sptr->localClient->sockhost);
      log(L_INFO,"Too many connections on IP from %s.", get_client_host(sptr));
      ServerStats->is_ref++;
      (void)exit_client(cptr, sptr, &me, 
			"No more connections allowed on that IP" );
      break;

    case I_LINE_FULL:
      sendto_realops_flags(FLAGS_FULL, "%s for %s (%s).",
			   "I-line is full", get_client_host(sptr),
			   sptr->localClient->sockhost);
      log(L_INFO,"Too many connections from %s.", get_client_host(sptr));
      ServerStats->is_ref++;
      (void)exit_client(cptr, sptr, &me, 
		"No more connections allowed in your connection class" );
      break;

    case NOT_AUTHORIZED:
    {
      static char ipaddr[HOSTIPLEN];
      ServerStats->is_ref++;
      /* jdc - lists server name & port connections are on */
      /*       a purely cosmetical change */
      inetntop(sptr->localClient->aftype, &IN_ADDR(sptr->localClient->ip), ipaddr, HOSTIPLEN);
      sendto_realops_flags(FLAGS_UNAUTH,
			   "%s from %s [%s] on [%s/%u].",
			   "Unauthorized client connection",
			   get_client_host(sptr),
			   ipaddr,
			   sptr->localClient->listener->name,
			   sptr->localClient->listener->port
			   );
      log(L_INFO,
	  "Unauthorized client connection from %s on [%s/%u].",
	  get_client_host(sptr),
	  sptr->localClient->listener->name,
	  sptr->localClient->listener->port
	  );
	  
      (void)exit_client(cptr, sptr, &me,
			"You are not authorized to use this server");
      break;
    }
    case BANNED_CLIENT:
      (void)exit_client(cptr,cptr, &me, "*** Banned ");
      ServerStats->is_ref++;
      break;

    case 0:
    default:
      break;
    }
  return(i);
}

/*
 * attach_Iline
 *
 * inputs	-
 * output	-
 * side effect	- find the first (best) I line to attach.
 */
int attach_Iline(struct Client* cptr, const char* username)
{
  struct ConfItem* aconf;
  struct ConfItem* gkill_conf;
  struct ConfItem* tkline_conf;
  char       non_ident[USERLEN + 1];

  if (IsGotId(cptr))
    {
      aconf = find_matching_conf(cptr->host,cptr->username,
                                       &cptr->localClient->ip);
      if(aconf && !IsConfElined(aconf))
        {
          if( (tkline_conf = find_tkline(cptr->host,
					 cptr->username,
					 &cptr->localClient->ip)))
            aconf = tkline_conf;
        }
    }
  else
    {
      non_ident[0] = '~';
      strncpy_irc(&non_ident[1],username, USERLEN - 1);
      non_ident[USERLEN] = '\0';
      aconf = find_matching_conf(cptr->host,non_ident,
                                 &cptr->localClient->ip);
      if(aconf && !IsConfElined(aconf))
        {
          if((tkline_conf = find_tkline(cptr->host,
					non_ident,
					&cptr->localClient->ip)))
            aconf = tkline_conf;
        }
    }

  if(aconf != NULL)
    {
      if (aconf->status & CONF_CLIENT)
        {
	  if (aconf->flags & CONF_FLAGS_REDIR)
	    {
	      sendto_one(cptr, form_str(RPL_REDIR),
			 me.name, cptr->name,
			 aconf->name ? aconf->name : "", aconf->port);
	      return(NOT_AUTHORIZED);
	    }


	  if (ConfigFileEntry.glines)
	    {
	      if (!IsConfElined(aconf))
		{
		  if (IsGotId(cptr))
		    gkill_conf = find_gkill(cptr, cptr->username);
		  else
		    gkill_conf = find_gkill(cptr, non_ident);
		  if (gkill_conf)
		    {
		      sendto_one(cptr, ":%s NOTICE %s :*** G-lined", me.name,
				 cptr->name);
		      sendto_one(cptr, ":%s NOTICE %s :*** Banned %s",
				 me.name, cptr->name,
				 gkill_conf->passwd);
		      return(BANNED_CLIENT);
		    }
		}
	    }

	  if(IsConfDoIdentd(aconf))
	    SetNeedId(cptr);

	  if(IsConfRestricted(aconf))
	    SetRestricted(cptr);

	  /* Thanks for spoof idea amm */
	  if(IsConfDoSpoofIp(aconf))
	    {
	      if(IsConfSpoofNotice(aconf))
	      {
	        sendto_realops_flags(FLAGS_ADMIN,
				   "%s spoofing: %s as %s", cptr->name,
				   cptr->host, aconf->name);
	      }
	      strncpy_irc(cptr->host, aconf->name, HOSTLEN);
	      SetIPSpoof(cptr);
	      SetIPHidden(cptr);
	    }

	  return(attach_iline(cptr, aconf));
        }
      else if(aconf->status & CONF_KILL)
        {
	  if(ConfigFileEntry.kline_with_reason)
	    {
	      sendto_one(cptr, ":%s NOTICE %s :*** Banned %s",
			 me.name,cptr->name,aconf->passwd);
	    }
          return(BANNED_CLIENT);
        }
    }

  return(NOT_AUTHORIZED);
}

/*
 * attach_iline
 *
 * inputs	- client pointer
 *		- conf pointer
 * output	-
 * side effects	-
 */
static int attach_iline(struct Client *cptr, struct ConfItem *aconf)
{
  IP_ENTRY *ip_found;

  ip_found = find_or_add_ip(cptr);

  SetIpHash(cptr);
  ip_found->count++;

  /* only check it if its non zero */
  if ( aconf->c_class /* This should never non NULL *grin* */ &&
       ConfConFreq(aconf) && ip_found->count > ConfConFreq(aconf))
    {
      if(!IsConfFlined(aconf))
        return TOO_MANY; /* Already at maximum allowed ip#'s */
      else
        {
          sendto_one(cptr,
       ":%s NOTICE %s :*** :I: line is full, but you have an >I: line!",
                     me.name,cptr->name);
        }
    }

  return (attach_conf(cptr, aconf) );
}

/* link list of free IP_ENTRY's */

static IP_ENTRY *free_ip_entries;

/*
 * clear_ip_hash_table()
 *
 * input                - NONE
 * output               - NONE
 * side effects         - clear the ip hash table
 *
 */

void clear_ip_hash_table()
{
  void *block_IP_ENTRIES;        /* block of IP_ENTRY's */
  IP_ENTRY *new_IP_ENTRY;        /* new IP_ENTRY being made */
  IP_ENTRY *last_IP_ENTRY;        /* last IP_ENTRY in chain */
  int size;
  int n_left_to_allocate = MAXCONNECTIONS;

  size = sizeof(IP_ENTRY) + (sizeof(IP_ENTRY) & (sizeof(void*) - 1) );

  block_IP_ENTRIES = (void *)MyMalloc((size * n_left_to_allocate));  

  free_ip_entries = (IP_ENTRY *)block_IP_ENTRIES;
  last_IP_ENTRY = free_ip_entries;

  /* *shudder* pointer arithmetic */
  while(--n_left_to_allocate)
    {
      block_IP_ENTRIES = (void *)((unsigned long)block_IP_ENTRIES + 
                        (unsigned long) size);
      new_IP_ENTRY = (IP_ENTRY *)block_IP_ENTRIES;
      last_IP_ENTRY->next = new_IP_ENTRY;
      new_IP_ENTRY->next = (IP_ENTRY *)NULL;
      last_IP_ENTRY = new_IP_ENTRY;
    }
  memset((void *)ip_hash_table, 0, sizeof(ip_hash_table));
}

/* 
 * find_or_add_ip()
 *
 * inputs       - cptr
 *              - name
 *
 * output       - pointer to an IP_ENTRY element
 * side effects -
 *
 * If the ip # was not found, a new IP_ENTRY is created, and the ip
 * count set to 0.
 * XXX: Broken for IPv6
 */

static IP_ENTRY *
find_or_add_ip(struct Client *cptr)
{
  struct irc_inaddr ip_in;
  
  int hash_index;
  IP_ENTRY *ptr, *newptr;

  
  copy_s_addr(IN_ADDR(ip_in), IN_ADDR(cptr->localClient->ip));

  for(ptr = ip_hash_table[hash_index = hash_ip(&ip_in)]; ptr; ptr = ptr->next )
    {
      if(!memcmp(&ptr->ip, &ip_in, sizeof(ip_in)))
        {
          return(ptr);
        }
    }

  if ( (ptr = ip_hash_table[hash_index]) != (IP_ENTRY *)NULL )
    {
      if( free_ip_entries == (IP_ENTRY *)NULL)
	outofmemory();

      newptr = ip_hash_table[hash_index] = free_ip_entries;
      free_ip_entries = newptr->next;

      memcpy(&newptr->ip, &ip_in, sizeof(ip_in));
      newptr->count = 0;
      newptr->next = ptr;
      return(newptr);
    }

  if( free_ip_entries == (IP_ENTRY *)NULL)
    outofmemory();

  ptr = ip_hash_table[hash_index] = free_ip_entries;
  free_ip_entries = ptr->next;
  memcpy(&ptr->ip, &ip_in, sizeof(ip_in));
  ptr->count = 0;
  ptr->next = (IP_ENTRY *)NULL;
  return (ptr);
}

/* 
 * remove_one_ip
 *
 * inputs        - unsigned long IP address value
 * output        - NONE
 * side effects  - ip address listed, is looked up in ip hash table
 *                 and number of ip#'s for that ip decremented.
 *                 if ip # count reaches 0, the IP_ENTRY is returned
 *                 to the free_ip_enties link list.
 * XXX: Broken for IPV6
 */

void remove_one_ip(struct irc_inaddr *ip_in)
{
  int hash_index;
  IP_ENTRY *last_ptr;
  IP_ENTRY *ptr;
  IP_ENTRY *old_free_ip_entries;

  last_ptr = ptr = ip_hash_table[hash_index = hash_ip(ip_in)];
  while(ptr)
    {
      /* XXX - XXX - XXX - XXX */
#ifndef IPV6
      if(ptr->ip == PIN_ADDR(ip_in))
#else
      if(!memcmp(&IN_ADDR(ptr->ip), &PIN_ADDR(ip_in), sizeof(struct irc_inaddr)))
#endif
        {
          if(ptr->count != 0)
            ptr->count--;

          if(ptr->count == 0)
            {
              if(ip_hash_table[hash_index] == ptr)
                ip_hash_table[hash_index] = ptr->next;
              else
                last_ptr->next = ptr->next;
        
              if(free_ip_entries != (IP_ENTRY *)NULL)
                {
                  old_free_ip_entries = free_ip_entries;
                  free_ip_entries = ptr;
                  ptr->next = old_free_ip_entries;
                }
              else
                {
                  free_ip_entries = ptr;
                  ptr->next = (IP_ENTRY *)NULL;
                }
            }
          return;
        }
      else
        {
          last_ptr = ptr;
          ptr = ptr->next;
        }
    }
  return;
}

/*
 * hash_ip()
 * 
 * input        - pointer to an irc_inaddr
 * output       - integer value used as index into hash table
 * side effects - hopefully, none
 */

static int hash_ip(struct irc_inaddr *addr)
{
#ifndef IPV6
  int hash;
  unsigned long ip;

  ip = ntohl(PIN_ADDR(addr));
  hash = ((ip >> 12) + ip) & (IP_HASH_SIZE-1);
  return(hash);
#else
  unsigned int hash = 0;
  char *ip = (char *) &PIN_ADDR(addr);

  while (*ip)
    { 
      hash = (hash << 4) - (hash + (unsigned char)*ip++);
    }

  return(hash & (IP_HASH_SIZE - 1));
#endif
}

/*
 * count_ip_hash
 *
 * inputs        - pointer to counter of number of ips hashed 
 *               - pointer to memory used for ip hash
 * output        - returned via pointers input
 * side effects  - NONE
 *
 * number of hashed ip #'s is counted up, plus the amount of memory
 * used in the hash.
 */

void count_ip_hash(int *number_ips_stored,u_long *mem_ips_stored)
{
  IP_ENTRY *ip_hash_ptr;
  int i;

  *number_ips_stored = 0;
  *mem_ips_stored = 0;

  for(i = 0; i < IP_HASH_SIZE ;i++)
    {
      ip_hash_ptr = ip_hash_table[i];
      while(ip_hash_ptr)
        {
          *number_ips_stored = *number_ips_stored + 1;
          *mem_ips_stored = *mem_ips_stored +
             sizeof(IP_ENTRY);

          ip_hash_ptr = ip_hash_ptr->next;
        }
    }
}

/*
 * iphash_stats()
 *
 * inputs        - 
 * output        -
 * side effects        -
 */
void iphash_stats(struct Client *cptr, struct Client *sptr,
		  int parc, char *parv[],FBFILE* out)
{
  IP_ENTRY *ip_hash_ptr;
  int i;
  int collision_count;
  char result_buf[256];

  if(out == NULL)
    sendto_one(sptr,":%s NOTICE %s :*** hash stats for iphash",
               me.name,cptr->name);
  else
    {
      (void)sprintf(result_buf,"*** hash stats for iphash\n");
      (void)fbputs(result_buf,out);
    }

  for(i = 0; i < IP_HASH_SIZE ;i++)
    {
      ip_hash_ptr = ip_hash_table[i];

      collision_count = 0;
      while(ip_hash_ptr)
        {
          collision_count++;
          ip_hash_ptr = ip_hash_ptr->next;
        }
      if(collision_count)
        {
          if(out == NULL)
            {
              sendto_one(sptr,":%s NOTICE %s :Entry %d (0x%X) Collisions %d",
                         me.name,cptr->name,i,i,collision_count);
            }
          else
            {
              (void)sprintf(result_buf,"Entry %d (0x%X) Collisions %d\n",
                            i,i,collision_count);
              (void)fbputs(result_buf,out);
            }
        }
    }
}

/*
** detach_conf
**        Disassociate configuration from the client.
**      Also removes a class from the list if marked for deleting.
*/
int detach_conf(struct Client* cptr,struct ConfItem* aconf)
{
  dlink_node *ptr;

  if(aconf == NULL)
    return -1;

  for( ptr = cptr->localClient->confs.head; ptr; ptr = ptr->next )
    {
      if (ptr->data == aconf)
        {
          if ((aconf) && (ClassPtr(aconf)))
            {
              if (aconf->status & CONF_CLIENT_MASK)
                {
                  if (ConfLinks(aconf) > 0)
                    --ConfLinks(aconf);
                }
              if (ConfMaxLinks(aconf) == -1 && ConfLinks(aconf) == 0)
                {
                  free_class(ClassPtr(aconf));
                  ClassPtr(aconf) = NULL;
                }
            }
          if (aconf && !--aconf->clients && IsIllegal(aconf))
            {
              free_conf(aconf);
            }
	  dlinkDelete(ptr, &cptr->localClient->confs);
          free_dlink_node(ptr);
          return 0;
        }
    }
  return -1;
}

static int is_attached(struct ConfItem *aconf,struct Client *cptr)
{
  dlink_node *ptr=NULL;

  for (ptr = cptr->localClient->confs.head; ptr; ptr = ptr->next)
    if (ptr->data == aconf)
      break;
  
  return (ptr) ? 1 : 0;
}

/*
 * attach_conf
 * 
 * inputs	- client pointer
 * 		- conf pointer
 * output	-
 * side effects - Associate a specific configuration entry to a *local*
 *                client (this is the one which used in accepting the
 *                connection). Note, that this automatically changes the
 *                attachment if there was an old one...
 */
int attach_conf(struct Client *cptr,struct ConfItem *aconf)
{
  dlink_node *lp;

  if (is_attached(aconf, cptr))
    {
      return 1;
    }
  if (IsIllegal(aconf))
    {
      return(NOT_AUTHORIZED);
    }

  if ( (aconf->status & CONF_OPERATOR) == 0 )
    {
      if ((aconf->status & CONF_CLIENT) &&
          ConfLinks(aconf) >= ConfMaxLinks(aconf) && ConfMaxLinks(aconf) > 0)
        {
          if (!IsConfFlined(aconf))
            {
              return(I_LINE_FULL); 
            }
          else
            {
              send(cptr->fd,
                   "NOTICE FLINE :I: line is full, but you have an >I: line!\n",
                   56, 0);
              SetFlined(cptr);
            }

        }
    }

  lp = make_dlink_node();

  dlinkAdd(aconf, lp, &cptr->localClient->confs);

  aconf->clients++;
  if (aconf->status & CONF_CLIENT_MASK)
    ConfLinks(aconf)++;
  return 0;
}

/*
 * attach_confs - Attach all possible CONF lines to a client
 * if the name passed matches that for the conf file (for non-C/N lines) 
 * or is an exact match (C/N lines only).  The difference in behaviour 
 * is to stop C:*::* and N:*::*.
 * returns count of conf entries attached if successful, 0 if none are found
 *
 * NOTE: this will allow C:::* and N:::* because the match mask is the
 * conf line and not the name
 */
int attach_confs(struct Client* cptr, const char* name, int statmask)
{
  struct ConfItem* tmp;
  int              conf_counter = 0;
  
  for (tmp = ConfigItemList; tmp; tmp = tmp->next)
    {
      if ((tmp->status & statmask) && !IsIllegal(tmp) &&
          tmp->name && match(tmp->name, name))
        {
          if (-1 < attach_conf(cptr, tmp))
            ++conf_counter;
        }
      else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
               tmp->name && !irccmp(tmp->name, name))
        {
          if (-1 < attach_conf(cptr, tmp))
            ++conf_counter;
        }
    }
  return conf_counter;
}

/*
 * attach_cn_lines
 *
 * inputs	- pointer to server to attach c/ns to
 * 		- name of server
 *		- hostname of server
 * output	- true (1) if both are found, otherwise return false (0)
 * side effects -
 * attach_cn_lines - find C/N lines and attach them to connecting client
 * NOTE: this requires an exact match between the name on the C:line and
 * the name on the N:line
 * C/N lines are completely gone now, the name exists only for historical
 * reasons - A1kmm.
 */
int attach_cn_lines(struct Client *cptr, const char* name, const char* host)
{
  struct ConfItem* ptr;

  assert(0 != cptr);
  assert(0 != host);

  for (ptr = ConfigItemList; ptr; ptr = ptr->next)
    {
     if (IsIllegal(ptr))
       continue;
     if (ptr->status != CONF_SERVER)
       continue;
     if (irccmp(name, ptr->name)/* || irccmp(host, ptr->host)*/)
       continue;
     attach_conf(cptr, ptr);
     return -1;
    }
  return 0;
}

/*
 * find a conf entry which matches the hostname and has the same name.
 */
struct ConfItem* find_conf_exact(const char* name, const char* user, 
                           const char* host, int statmask)
{
  struct ConfItem *tmp;

  for (tmp = ConfigItemList; tmp; tmp = tmp->next)
    {
      if (!(tmp->status & statmask) || !tmp->name || !tmp->host ||
          irccmp(tmp->name, name))
        continue;
      /*
      ** Accept if the *real* hostname (usually sockethost)
      ** socket host) matches *either* host or name field
      ** of the configuration.
      */
      if (!match(tmp->host, host) || !match(tmp->user,user)
          || irccmp(tmp->name, name) )
        continue;
      if (tmp->status & CONF_OPERATOR)
        {
          if (tmp->clients < ConfMaxLinks(tmp))
            return tmp;
          else
            continue;
        }
      else
        return tmp;
    }
  return NULL;
}

struct ConfItem* find_conf_name(dlink_list *list, const char* name, 
                                int statmask)
{
  dlink_node *ptr;
  struct ConfItem* aconf;
  
  for (ptr = list->head; ptr; ptr = ptr->next)
    {
      aconf = ptr->data;
      if ((aconf->status & statmask) && aconf->name && 
          (!irccmp(aconf->name, name) || match(aconf->name, name)))
        return aconf;
    }
  return NULL;
}

/* 
 * NOTES
 *
 * C:192.168.0.240:password:irc.server.name:...
 * C:irc.server.name:password:irc.server.name
 * C:host:passwd:name:port:class
 * N:host:passwd:name:hostmask number:class
 */
/*
 * Added for new access check    meLazy <- no youShithead, your code sucks
 */
struct ConfItem* find_conf_host(dlink_list *list, const char* host, 
                                int statmask)
{
  dlink_node *ptr;
  struct ConfItem *aconf;
  
  for (ptr = list->head; ptr; ptr = ptr->next)
    {
      aconf = ptr->data;
      if (aconf->status & statmask && aconf->host && match(aconf->host, host))
        return aconf;
    }
  return NULL;
}

/*
 * find_conf_ip
 *
 * Find a conf line using the IP# stored in it to search upon.
 * Added 1/8/92 by Avalon.
 */
struct ConfItem *find_conf_ip(dlink_list *list, char *ip, char *user, 
                              int statmask)
{
  dlink_node *ptr;
  struct ConfItem *aconf;
  
  for (ptr = list->head; ptr; ptr = ptr->next)
    {
      aconf = ptr->data;

      if (!(aconf->status & statmask))
        continue;

      if (!match(aconf->user, user))
        {
          continue;
        }
/* XXX: broken for IPv6 */
      if (!memcmp((void *)&IN_ADDR(aconf->ipnum), (void *)ip, sizeof(struct in_addr)))
        return aconf;
    }
  return ((struct ConfItem *)NULL);
}

/*
 * find_conf_by_name - return a conf item that matches name and type
 */
struct ConfItem* find_conf_by_name(const char* name, int status)
{
  struct ConfItem* conf;
  assert(0 != name);
 
  for (conf = ConfigItemList; conf; conf = conf->next)
    {
      if (conf->status == status && conf->name &&
          match(name, conf->name))
#if 0
          (match(name, conf->name) || match(conf->name, name)))
#endif
        return conf;
    }
  return NULL;
}

/*
 * find_conf_by_name - return a conf item that matches host and type
 */
struct ConfItem* find_conf_by_host(const char* host, int status)
{
  struct ConfItem* conf;
  assert(0 != host);
 
  for (conf = ConfigItemList; conf; conf = conf->next)
    {
      if (conf->status == status && conf->host &&
          match(host, conf->host))
#if 0
          (match(host, conf->host) || match(conf->host, host)))
#endif
        return conf;
    }
  return NULL;
}

/*
 * find_conf_entry
 *
 * - looks for a match on all given fields.
 */
struct ConfItem *find_conf_entry(struct ConfItem *aconf, int mask)
{
  struct ConfItem *bconf;

  for (bconf = ConfigItemList, mask &= ~CONF_ILLEGAL; bconf; 
       bconf = bconf->next)
    {
      if (!(bconf->status & mask) || (bconf->port != aconf->port))
        continue;
      
      if ((BadPtr(bconf->host) && !BadPtr(aconf->host)) ||
          (BadPtr(aconf->host) && !BadPtr(bconf->host)))
        continue;

      if (!BadPtr(bconf->host) && irccmp(bconf->host, aconf->host))
        continue;

      if ((BadPtr(bconf->passwd) && !BadPtr(aconf->passwd)) ||
          (BadPtr(aconf->passwd) && !BadPtr(bconf->passwd)))
        continue;

      if (!BadPtr(bconf->passwd) &&
          irccmp(bconf->passwd, aconf->passwd))
      continue;

      if ((BadPtr(bconf->name) && !BadPtr(aconf->name)) ||
          (BadPtr(aconf->name) && !BadPtr(bconf->name)))
        continue;

      if (!BadPtr(bconf->name) && irccmp(bconf->name, aconf->name))
        continue;
      break;
    }
  return bconf;
}

/*
 * find_x_conf
 *
 * inputs       - pointer to char string to find
 * output       - NULL or pointer to found struct ConfItem
 * side effects - looks for a match on name field
 */
struct ConfItem *find_x_conf(char *to_find)
{
  struct ConfItem *aconf;

  for (aconf = x_conf; aconf; aconf = aconf->next)
    {
      if (BadPtr(aconf->name))
          continue;

      if(match(aconf->name,to_find))
        return(aconf);

    }
  return(NULL);
}

/*
 * find_u_conf
 *
 * inputs       - pointer to servername
 *		- pointer to user of oper
 *		- pointer to host of oper
 * output       - NULL or pointer to found struct ConfItem
 * side effects - looks for a matches on all fields
 */
int find_u_conf(char *server,char *user,char *host)
{
  struct ConfItem *aconf;

  for (aconf = u_conf; aconf; aconf = aconf->next)
    {
      if (BadPtr(aconf->name))
          continue;

      if(match(aconf->name,server))
	{
	  if (BadPtr(aconf->user) || BadPtr(aconf->host))
	    return YES;
	  if(match(aconf->user,user) && match(aconf->host,host))
	    return YES;

	}
    }
  return NO;
}

/*
 * find_q_conf
 *
 * inputs       - nick to find
 *              - user to match
 *              - host to mask
 * output       - YES if found, NO if not found
 * side effects - looks for matches on Q lined nick
 */
int find_q_conf(char *nickToFind,char *user,char *host)
{
  struct ConfItem *aconf;

  for (aconf = q_conf; aconf; aconf = aconf->next)
    {
      if (BadPtr(aconf->name))
          continue;

      if(match(aconf->name,nickToFind))
        {
          return YES;
        }
    }
  return NO;
}

/*
 * report_qlines
 *
 * inputs       - pointer to client to report to
 * output       - none
 * side effects - all Q lines are listed to client 
 */

void report_qlines(struct Client *sptr)
{
  struct ConfItem *aconf;
  char *host;
  char *user;
  char *pass;
  char *name;
  char *classname;
  int  port;

  for (aconf = q_conf; aconf; aconf = aconf->next)
    {
      get_printable_conf(aconf, &name, &host, &pass, &user, &port,&classname);
          
      sendto_one(sptr, form_str(RPL_STATSQLINE),
		 me.name, sptr->name, name, pass, user, host);
    }
}

/*
 * clear_special_conf
 * 
 * inputs       - pointer to pointer of root of special conf link list
 * output       - none
 * side effects - clears given special conf lines
 */
static void clear_special_conf(struct ConfItem **this_conf)
{
  struct ConfItem *aconf;
  struct ConfItem *next_aconf;

  for (aconf = *this_conf; aconf; aconf = next_aconf)
    {
      next_aconf = aconf->next;
      free_conf(aconf);
    }
  *this_conf = (struct ConfItem *)NULL;
  return;
}

/*
 * rehash
 *
 * Actual REHASH service routine. Called with sig == 0 if it has been called
 * as a result of an operator issuing this command, else assume it has been
 * called as a result of the server receiving a HUP signal.
 */
int rehash(struct Client *cptr,struct Client *sptr, int sig)
{
  if (sig)
  {
    sendto_realops_flags(FLAGS_ALL,"Got signal SIGHUP, reloading ircd conf. file");
  }

  close_listeners();
  restart_resolver();
  read_conf_files(NO);

  if (ServerInfo.description != NULL)
  {
    strncpy_irc(me.info, ServerInfo.description, REALLEN);
  }

  flush_deleted_I_P();
  check_klines();
  return 0;
}

/*
** read_conf() 
**    Read configuration file.
**
*
* Inputs        - file descriptor pointing to config file to use
*
**    returns -1, if file cannot be opened
**             0, if file opened
*/

#define MAXCONFLINKS 150

/* bleh. unfortunately, these have to become globals as well */

int              scount = 0;

static void read_conf(FBFILE* file)
{
  scount = lineno = 0;

  class0 = find_class("default");       /* which one is the default class ? */
  yyparse(); /* wheee! */

  check_class();

  if(ConfigFileEntry.ts_warn_delta < TS_WARN_DELTA_MIN)
    ConfigFileEntry.ts_warn_delta = TS_WARN_DELTA_DEFAULT;

  if(ConfigFileEntry.ts_max_delta < TS_MAX_DELTA_MIN)
    ConfigFileEntry.ts_max_delta = TS_MAX_DELTA_DEFAULT;

  if(ServerInfo.network_name == NULL)
    DupString(ServerInfo.network_name,NETWORK_NAME_DEFAULT);

  if(ServerInfo.network_desc == NULL)
    DupString(ServerInfo.network_desc,NETWORK_DESC_DEFAULT);

  if (!ConfigFileEntry.maximum_links)
    ConfigFileEntry.maximum_links = MAXIMUM_LINKS_DEFAULT;

  if (!ConfigFileEntry.max_targets)
    ConfigFileEntry.max_targets = MAX_TARGETS_DEFAULT;

  if (!ConfigFileEntry.knock_delay)
    ConfigFileEntry.knock_delay = 300;

  if (!ConfigFileEntry.caller_id_wait)
    ConfigFileEntry.caller_id_wait = 60;
  
  GlobalSetOptions.idletime = (ConfigFileEntry.idletime * 60);

  if (!ConfigFileEntry.links_delay)
        ConfigFileEntry.links_delay = LINKS_DELAY_DEFAULT;
  GlobalSetOptions.hide_server = ConfigFileEntry.hide_server;
}

static void read_kd_lines(FBFILE* file)
{
  char             line[BUFSIZE];
  char*            p;

  scount = 0;

  while (fbgets(line, sizeof(line), file))
    {
      if ((p = strchr(line, '\n')))
        *p = '\0';

      if (!*line || line[0] == '#')
        continue;

      if (line[1] == ':')
        oldParseOneLine(line);
    }

}

/*
 * conf_add_conf
 * Inputs	- ConfItem
 * Output	- none
 * Side effects	- add given conf to link list
 */

void conf_add_conf(struct ConfItem *aconf)
{
  (void)collapse(aconf->host);
  (void)collapse(aconf->user);
  Debug((DEBUG_NOTICE,
	 "Read Init: (%d) (%s) (%s) (%s) (%d) (%d)",
	 aconf->status, 
	 aconf->host ? aconf->host : "<NULL>",
	 aconf->passwd ? aconf->passwd : "<NULL>",
	 aconf->user ? aconf->user : "<NULL>",
	 aconf->port,
	 aconf->c_class ? ConfClassType(aconf): 0 ));

  aconf->next = ConfigItemList;
  ConfigItemList = aconf;
}

/*
 * SplitUserHost
 *
 * inputs	- struct ConfItem pointer
 * output	- return 1/0 true false or -1 for error
 * side effects - splits user@host found in a name field of conf given
 *		  stuff the user into ->user and the host into ->host
 */
static int SplitUserHost(struct ConfItem *aconf)
{
  char *p;
  char *new_user;
  char *new_host;

  if ( (p = strchr(aconf->host, '@')) )
    {
      *p = '\0';
      DupString(new_user, aconf->host);
      MyFree(aconf->user);
      aconf->user = new_user;
      p++;
      DupString(new_host,p);
      MyFree(aconf->host);
      aconf->host = new_host;
    }
  else
    {
      DupString(aconf->user, "*");
    }
  return(1);
}

/*
 * lookup_confhost - start DNS lookups of all hostnames in the conf
 * line and convert an IP addresses in a.b.c.d number for to IP#s.
 *
 */
static void lookup_confhost(struct ConfItem* aconf)
{
  if (BadPtr(aconf->host) || BadPtr(aconf->name))
    {
      log(L_ERROR, "Host/server name error: (%s) (%s)",
          aconf->host, aconf->name);
      return;
    }

  if (strchr(aconf->host, '*') || strchr(aconf->host, '?'))
    return;
  /*
  ** Do name lookup now on hostnames given and store the
  ** ip numbers in conf structure.
  */
  if (inetpton(DEF_FAM, aconf->host, &IN_ADDR(aconf->ipnum)) <= 0)
    {
      conf_dns_lookup(aconf);
    }
}

/*
 * conf_connect_allowed (untested)
 */
int conf_connect_allowed(struct irc_inaddr *addr)
{
  struct ConfItem *aconf = match_Dline(addr);

  if (aconf && !IsConfElined(aconf))
    return 0;
  return 1;
}

/*
 * find_kill
 *
 * inputs	- pointer to client structure
 * output	- pointer to struct ConfItem if found
 * side effects	- See if this user is klined already,
 *		  and if so, return struct ConfItem pointer
 */
struct ConfItem *find_kill(struct Client* cptr)
{
  assert(0 != cptr);

  /* If client is e-lined, then its not k-linable */
  /* opers get that flag automatically, normal users do not */
  return (IsElined(cptr)) ? 0 : find_is_klined(cptr->host, 
                                               cptr->username, 
                                               &cptr->localClient->ip);
}

/*
 * find_tkline
 *
 * inputs        - hostname
 *               - username
 * output        - matching struct ConfItem or NULL
 * side effects        -
 *
 * WARNING, no sanity checking on length of name,host etc.
 * thats expected to be done by caller.... 
 *
 * XXX: Broken for IPV6
 */

struct ConfItem* find_tkline(const char* host, const char* user, struct irc_inaddr *ipn)
{
  dlink_node *kill_node;
  struct ConfItem *kill_ptr;
  struct irc_inaddr ip;

  if ( ipn != NULL )
    copy_s_addr(IN_ADDR(ip), PIN_ADDR(ipn));  

  for (kill_node = temporary_klines.head; kill_node; kill_node = kill_node->next)
    {
      kill_ptr = kill_node->data;
      if ((kill_ptr->user && (!user || match(kill_ptr->user, user)))
          && (kill_ptr->host && (!host || match(kill_ptr->host, host))))
        {
          return(kill_ptr);
        }
    }

  if ( ipn == NULL )
    return NULL;

  for (kill_node = temporary_ip_klines.head; 
       kill_node; kill_node = kill_node->next)
    {
      kill_ptr = kill_node->data;

      if ((kill_ptr->user && (!user || match(kill_ptr->user, user)))
	  && (kill_ptr->ip && (((unsigned long)IN_ADDR(&ip) &
				kill_ptr->ip_mask) ==
			       (unsigned long)IN_ADDR(&ip))))
	{
	  return(kill_ptr);
	}
    }

  return NULL;
}

/*
 * find_is_klined()
 *
 * inputs        - hostname
 *               - username
 *               - ip of possible "victim"
 * output        - matching struct ConfItem or NULL
 * side effects        -
 *
 * WARNING, no sanity checking on length of name,host etc.
 * thats expected to be done by caller.... 
 */
struct ConfItem *find_is_klined(const char* host, const char* name,
				struct irc_inaddr *ip)
{
  struct ConfItem *found_aconf;

  if( (found_aconf = find_tkline(host, name, ip)) )
    return(found_aconf);

  /* find_matching_mtrie_conf() can return either CONF_KILL,
   * CONF_CLIENT or NULL, i.e. no I line at all.
   */

  found_aconf = find_matching_conf(host, name, ip);
  if(found_aconf && (found_aconf->status & (CONF_ELINE|CONF_DLINE|CONF_KILL)))
    return(found_aconf);

  return NULL;
}

/* add_temp_kline
 *
 * inputs        - pointer to struct ConfItem
 * output        - none
 * Side effects  - links in given struct ConfItem into 
 *                 temporary kline link list
 */

void add_temp_kline(struct ConfItem *aconf)
{
  dlink_node *kill_node;

  kill_node = make_dlink_node();

  if (aconf->ip == 0)
    dlinkAdd(aconf, kill_node, &temporary_klines);
  else 
    dlinkAdd(aconf, kill_node, &temporary_ip_klines);
}

/* report_temp_klines
 *
 * inputs        - struct Client pointer, client to report to
 * output        - NONE
 * side effects  - NONE
 *                  
 */
void report_temp_klines(struct Client *sptr)
{
  show_temp_klines(sptr, &temporary_klines);
  show_temp_klines(sptr, &temporary_ip_klines);
}

/* show_temp_klines
 *
 * inputs         - struct Client pointer, client to report to
 *                - dlink_list pointer, the tkline list to show
 * outputs        - NONE
 * side effects   - NONE
 */
void
show_temp_klines(struct Client *sptr, dlink_list *tklist)
{
  dlink_node *kill_node;
  struct ConfItem *kill_list_ptr;
  char *host;
  char *user;
  char *reason;

  for (kill_node = tklist->head; kill_node; kill_node = kill_node->next)
    {
      kill_list_ptr = kill_node->data;

      if (kill_list_ptr->host)
        host = kill_list_ptr->host;
      else
        host = "*";

      if (kill_list_ptr->user)
        user = kill_list_ptr->user;
      else
        user = "*";

      if (kill_list_ptr->passwd)
        reason = kill_list_ptr->passwd;
      else
        reason = "No Reason";

      sendto_one(sptr, form_str(RPL_STATSKLINE), me.name,
                 sptr->name, 'k', host, user, reason);
    }
}

/*
 * cleanup_tklines
 *
 * inputs       - NONE
 * output       - NONE
 * side effects - call function to expire tklines
 *                This is an event started off in ircd.c
 */
void
cleanup_tklines(void *notused)
{
  expire_tklines(&temporary_klines);
  expire_tklines(&temporary_ip_klines);

  eventAdd("cleanup_tklines", cleanup_tklines, NULL,
           CLEANUP_TKLINES_TIME, 0);
}

/*
 * expire_tklines
 *
 * inputs       - tkline list pointer
 * output       - NONE
 * side effects - expire tklines
 */
static void
expire_tklines(dlink_list *tklist)
{
  dlink_node *kill_node;
  dlink_node *next_node;
  struct ConfItem *kill_ptr;

  for (kill_node = tklist->head; kill_node; kill_node = next_node)
    {
      kill_ptr = kill_node->data;
      next_node = kill_node->next;

      if (kill_ptr->hold <= CurrentTime)
        {
          free_conf(kill_ptr);
          dlinkDelete(kill_node, tklist);
          free_dlink_node(kill_node);
        }
    }
}

/*
 * oper_privs_as_string
 *
 * inputs        - pointer to cptr or NULL
 * output        - pointer to static string showing oper privs
 * side effects  -
 * return as string, the oper privs as derived from port
 * also, set the oper privs if given cptr non NULL
 */

char *oper_privs_as_string(struct Client *cptr,int port)
{
  static char privs_out[16];
  char *privs_ptr;

  privs_ptr = privs_out;
  *privs_ptr = '\0';

  if(port & CONF_OPER_GLINE)
    {
      if(cptr)
        SetOperGline(cptr);
      *privs_ptr++ = 'G';
    }
  else
    *privs_ptr++ = 'g';

  if(port & CONF_OPER_K)
    {
      if(cptr)
        SetOperK(cptr);
      *privs_ptr++ = 'K';
    }
  else
    *privs_ptr++ = 'k';

  if(port & CONF_OPER_N)
    {
      if(cptr)
        SetOperN(cptr);
      *privs_ptr++ = 'N';
    }

  if(port & CONF_OPER_GLOBAL_KILL)
    {
      if(cptr)
        SetOperGlobalKill(cptr);
      *privs_ptr++ = 'O';
    }
  else
    *privs_ptr++ = 'o';

  if(port & CONF_OPER_REMOTE)
    {
      if(cptr)
        SetOperRemote(cptr);
      *privs_ptr++ = 'R';
    }
  else
    *privs_ptr++ = 'r';
  
  if(port & CONF_OPER_UNKLINE)
    {
      if(cptr)
        SetOperUnkline(cptr);
      *privs_ptr++ = 'U';
    }
  else
    *privs_ptr++ = 'u';

  if(port & CONF_OPER_REHASH)
    {
      if(cptr)
        SetOperRehash(cptr);
      *privs_ptr++ = 'H';
    }
  else
    *privs_ptr++ = 'h';

  if(port & CONF_OPER_DIE)
    {
      if(cptr)
        SetOperDie(cptr);
      *privs_ptr++ = 'D';
    }
  else
    *privs_ptr++ = 'd';

  if (port & CONF_OPER_ADMIN)
    {
      if (cptr)
	SetOperAdmin(cptr);
      *privs_ptr++ = 'A';
    }
  else
    *privs_ptr++ = 'a';
  
  *privs_ptr = '\0';

  return(privs_out);
}


/* oper_flags_as_string
 *
 * inputs        - oper flags as bit mask
 * output        - oper flags as as string
 * side effects -
 *
 */

char *oper_flags_as_string(int flags)
{
  /* This MUST be extended if we add any more modes... -Hwy */
  static char flags_out[18];
  char *flags_ptr;

  flags_ptr = flags_out;
  *flags_ptr = '\0';

  if(flags & FLAGS_INVISIBLE)
    *flags_ptr++ = 'i';
  if(flags & FLAGS_WALLOP)
    *flags_ptr++ = 'w';
  if(flags & FLAGS_SERVNOTICE)
    *flags_ptr++ = 's';
  if(flags & FLAGS_CCONN)
    *flags_ptr++ = 'c';
  if(flags & FLAGS_REJ)
    *flags_ptr++ = 'r';
  if(flags & FLAGS_SKILL)
    *flags_ptr++ = 'k';
  if(flags & FLAGS_FULL)
    *flags_ptr++ = 'f';
  if(flags & FLAGS_SPY)
    *flags_ptr++ = 'y';
  if(flags & FLAGS_DEBUG)
    *flags_ptr++ = 'd';
  if(flags & FLAGS_NCHANGE)
    *flags_ptr++ = 'n';
  if(flags & FLAGS_ADMIN)
    *flags_ptr++ = 'a';
  if(flags & FLAGS_EXTERNAL)
    *flags_ptr++ = 'x';
  if(flags & FLAGS_UNAUTH)
    *flags_ptr++ = 'u';
  if(flags & FLAGS_BOTS)
    *flags_ptr++ = 'b';
  if(flags & FLAGS_DRONE)
    *flags_ptr++ = 'e';
  if(flags & FLAGS_LOCOPS)
    *flags_ptr++ = 'l';
  if(flags & FLAGS_CALLERID)
    *flags_ptr++ = 'g';
  *flags_ptr = '\0';

  return(flags_out);
}

/* table used for is_address */
unsigned long cidr_to_bitmask[]=
{
  /* 00 */ 0x00000000,
  /* 01 */ 0x80000000,
  /* 02 */ 0xC0000000,
  /* 03 */ 0xE0000000,
  /* 04 */ 0xF0000000,
  /* 05 */ 0xF8000000,
  /* 06 */ 0xFC000000,
  /* 07 */ 0xFE000000,
  /* 08 */ 0xFF000000,
  /* 09 */ 0xFF800000,
  /* 10 */ 0xFFC00000,
  /* 11 */ 0xFFE00000,
  /* 12 */ 0xFFF00000,
  /* 13 */ 0xFFF80000,
  /* 14 */ 0xFFFC0000,
  /* 15 */ 0xFFFE0000,
  /* 16 */ 0xFFFF0000,
  /* 17 */ 0xFFFF8000,
  /* 18 */ 0xFFFFC000,
  /* 19 */ 0xFFFFE000,
  /* 20 */ 0xFFFFF000,
  /* 21 */ 0xFFFFF800,
  /* 22 */ 0xFFFFFC00,
  /* 23 */ 0xFFFFFE00,
  /* 24 */ 0xFFFFFF00,
  /* 25 */ 0xFFFFFF80,
  /* 26 */ 0xFFFFFFC0,
  /* 27 */ 0xFFFFFFE0,
  /* 28 */ 0xFFFFFFF0,
  /* 29 */ 0xFFFFFFF8,
  /* 30 */ 0xFFFFFFFC,
  /* 31 */ 0xFFFFFFFE,
  /* 32 */ 0xFFFFFFFF
};

/*
 * is_address
 *
 * inputs        - hostname
 *               - pointer to ip result
 *               - pointer to ip_mask result
 * output        - YES if hostname is ip# only NO if its not
 * side effects        - NONE
 * 
 * Thanks Soleil
 *
 * BUGS
 */

int        is_address(char *host,
                   unsigned long *ip_ptr,
                   unsigned long *ip_mask_ptr)
{
  unsigned long current_ip=0L;
  unsigned int octet=0;
  int found_mask=0;
  int dot_count=0;
  char c;

  while( (c = *host) )
    {
      if(IsDigit(c))
        {
          octet *= 10;
          octet += (*host & 0xF);
        }
      else if(c == '.')
        {
          current_ip <<= 8;
          current_ip += octet;
          if( octet > 255 )
            return( 0 );
          octet = 0;
          dot_count++;
        }
      else if(c == '/')
        {
          if( octet > 255 )
            return( 0 );
          found_mask = 1;
          current_ip <<= 8;
          current_ip += octet;
          octet = 0;
          *ip_ptr = current_ip;
          current_ip = 0L;
        }
      else if(c == '*')
        {
          if( (dot_count == 3) && (*(host+1) == '\0') && (*(host-1) == '.'))
            {
              current_ip <<= 8;
              *ip_ptr = current_ip;
              *ip_mask_ptr = 0xFFFFFF00L;
              return( 1 );
            }
          else
            return( 0 );
        }
      else
        return( 0 );
      host++;
    }

  if(octet > 255)
    return( 0 );
  current_ip <<= 8;
  current_ip += octet;

  if(found_mask)
    {
      if(current_ip>32)
        return( 0 );
      *ip_mask_ptr = cidr_to_bitmask[current_ip];
    }
  else
    {
      *ip_ptr = current_ip;
      *ip_mask_ptr = 0xFFFFFFFFL;
    }

  return( 1 );
}

/*
 * is_ipv6_address
 *
 * inputs        - hostname
 *               - pointer to ip result
 *               - pointer to ip_mask result
 * output        - YES if hostname is ip# only NO if its not
 * side effects  - NONE
 * 
 */

int        is_ipv6_address(char *host,
			   unsigned char *ip_ptr,
			   unsigned char *ip_mask_ptr)
{
  char *p;
  int mask_value;

  if((p = strchr(host,'/')))
    {
      *p = '\0';
      mask_value = atoi(p+1);
    }

  /* XXX finish later, lie for now ... */
  return 1;
}

/*
 * get_printable_conf
 *
 * inputs        - struct ConfItem
 *
 * output         - name 
 *                - host
 *                - pass
 *                - user
 *                - port
 *
 * side effects        -
 * Examine the struct struct ConfItem, setting the values
 * of name, host, pass, user to values either
 * in aconf, or "<NULL>" port is set to aconf->port in all cases.
 */

void get_printable_conf(struct ConfItem *aconf, char **name, char **host,
                           char **pass, char **user,int *port,char **classname)
{
  static  char        null[] = "<NULL>";
  static  char        zero[] = "default";

  *name = BadPtr(aconf->name) ? null : aconf->name;
  *host = BadPtr(aconf->host) ? null : aconf->host;
  *pass = BadPtr(aconf->passwd) ? null : aconf->passwd;
  *user = BadPtr(aconf->user) ? null : aconf->user;
  *classname = BadPtr(aconf->className) ? zero : aconf->className;
  *port = (int)aconf->port;
}

/*
 * read_conf_files
 *
 * inputs       - cold start YES or NO
 * output       - none
 * side effects - read all conf files needed, ircd.conf kline.conf etc.
 */
void read_conf_files(int cold)
{
  FBFILE *file;
  const char *filename, *kfilename, *dfilename; /* kline or conf filename */

  conf_fbfile_in = NULL;

  filename = get_conf_name(CONF_TYPE);

  if ((conf_fbfile_in = fbopen(filename,"r")) == NULL)
    {
      if(cold)
        {
          log(L_CRIT, "Failed in reading configuration file %s", filename);
          exit(-1);
        }
      else
        {
          sendto_realops_flags(FLAGS_ALL,
			       "Can't open %s file aborting rehash!",
			       filename );
          return;
        }
    }

  if (!cold)
    clear_out_old_conf();

  read_conf(conf_fbfile_in);
  fbclose(conf_fbfile_in);

  kfilename = get_conf_name(KLINE_TYPE);
  if (irccmp(filename, kfilename))
    {
      if((file = fbopen(kfilename,"r")) == NULL)
        {
	  if (cold)
	    log(L_ERROR, "Failed reading kline file %s", filename);
	  else
	    sendto_realops_flags(FLAGS_ALL,
				 "Can't open %s file klines could be missing!",
				 kfilename);
	}
      else
	{
	  read_kd_lines(file);
	  fbclose(file);
	}
    }

  dfilename = get_conf_name(DLINE_TYPE);
  if (irccmp(filename, dfilename) && irccmp(kfilename, dfilename))
    {
      if ((file = fbopen(dfilename,"r")) == NULL)
	{
	  if(cold)
	    log(L_ERROR, "Failed reading dline file %s", dfilename);
	  else
	    sendto_realops_flags(FLAGS_ALL,
				 "Can't open %s file dlines could be missing!",
				 dfilename);
	}
      else
	{
	  read_kd_lines(file);
	  fbclose(file);
	}
    }
}

/*
 * clear_out_old_conf
 *
 * inputs       - none
 * output       - none
 * side effects - Clear out the old configuration
 */
static void clear_out_old_conf(void)
{
  struct ConfItem **tmp = &ConfigItemList;
  struct ConfItem *tmp2;
  struct Class    *cltmp;

    while ((tmp2 = *tmp))
      {
        if (tmp2->clients)
          {
            /*
            ** Configuration entry is still in use by some
            ** local clients, cannot delete it--mark it so
            ** that it will be deleted when the last client
            ** exits...
            */
            if (!(tmp2->status & CONF_CLIENT))
              {
                *tmp = tmp2->next;
                tmp2->next = NULL;
              }
            else
              tmp = &tmp2->next;
            tmp2->status |= CONF_ILLEGAL;
          }
        else
          {
            *tmp = tmp2->next;
            free_conf(tmp2);
          }
      }

    /*
     * We don't delete the class table, rather mark all entries
     * for deletion. The table is cleaned up by check_class. - avalon
     */
    assert(0 != ClassList);
    for (cltmp = ClassList->next; cltmp; cltmp = cltmp->next)
      MaxLinks(cltmp) = -1;

    clear_conf();
    clear_Dline_table();
    clear_special_conf(&x_conf);
    clear_special_conf(&u_conf);
    clear_special_conf(&q_conf);
}

/*
 * flush_deleted_I_P
 *
 * inputs       - none
 * output       - none
 * side effects - This function removes I/P conf items
 */

static void flush_deleted_I_P(void)
{
  struct ConfItem **tmp = &ConfigItemList;
  struct ConfItem *tmp2;

  /*
   * flush out deleted I and P lines although still in use.
   */
  for (tmp = &ConfigItemList; (tmp2 = *tmp); )
    {
      if (!(tmp2->status & CONF_ILLEGAL))
        tmp = &tmp2->next;
      else
        {
          *tmp = tmp2->next;
          tmp2->next = NULL;
          if (!tmp2->clients)
            free_conf(tmp2);
        }
    }
}

/*
 * WriteKlineOrDline
 *
 * inputs       - kline or dline type flag
 *              - client pointer to report to
 *              - user name of target
 *              - host name of target
 *              - reason for target
 *              - current tiny date string
 * output       - NONE
 * side effects - This function takes care of
 *                finding right kline or dline conf file, writing
 *                the right lines to this file, 
 *                notifying the oper that their kline/dline is in place
 *                notifying the opers on the server about the k/d line
 *                forwarding the kline onto the next U lined server
 *                
 */
void WriteKlineOrDline( KlineType type,
			struct Client *sptr,
			char *user,
			char *host,
			const char *reason,
			const char *current_date)
{
  char buffer[1024];
  FBFILE *out;
  const char *filename;         /* filename to use for kline */

  filename = get_conf_name(type);

  if(type == DLINE_TYPE)
    {
      sendto_realops_flags(FLAGS_ALL,
			   "%s added D-Line for [%s] [%s]",
			   sptr->name, host, reason);
      sendto_one(sptr, ":%s NOTICE %s :Added D-Line [%s] to %s",
		 me.name, sptr->name, host, filename);

      log(L_TRACE, "%s added D-Line for [%s] [%s]", 
	  sptr->name, host, reason);
    }
  else
    {
      sendto_realops_flags(FLAGS_ALL,
			   "%s added K-Line for [%s@%s] [%s]",
			   sptr->name, user, host, reason);
      sendto_one(sptr, ":%s NOTICE %s :Added K-Line [%s@%s]",
		 me.name, sptr->name, user, host);
      log(L_TRACE, "%s added K-Line for [%s] [%s@%s]", 
	  sptr->name, user, host, reason);
    }

  if ( (out = fbopen(filename, "a")) == NULL )
    {
      sendto_realops_flags(FLAGS_ALL,
			   "*** Problem opening %s ", filename);
      return;
    }

  if(type==KLINE_TYPE)
    {
      if (MyClient(sptr))
	{
	  ircsprintf(buffer, "#%s!%s@%s K'd: %s@%s:%s\n",
		     sptr->name, sptr->username, sptr->host,
		     user, host, reason);
	}
      else
	{
	  ircsprintf(buffer, "#%s!%s@%s on %s K'd: %s@%s:%s\n",
		     sptr->name, sptr->username, sptr->host,
		     sptr->servptr?sptr->servptr->name:"<Unknown>",
		     user, host, reason);
	}
    }
  else
    ircsprintf(buffer, "#%s!%s@%s D'd: %s:%s\n",
	       sptr->name, sptr->username, sptr->host,
	       host, reason);
  
  if (fbputs(buffer,out) == -1)
    {
      sendto_realops_flags(FLAGS_ALL,"*** Problem writing to %s",filename);
      fbclose(out);
      return;
    }

  if(type==KLINE_TYPE)
    ircsprintf(buffer, "K:%s:%s (%s):%s\n",
               host,
               reason,
               current_date,
               user);
  else
    ircsprintf(buffer, "D:%s:%s (%s)\n",
               host,
               reason,
               current_date);


  if (fbputs(buffer,out) == -1)
    {
      sendto_realops_flags(FLAGS_ALL,"*** Problem writing to %s",filename);
      fbclose(out);
      return;
    }
      
  fbclose(out);

  if(type==KLINE_TYPE)
    log(L_TRACE, "%s added K-Line for [%s@%s] [%s]",
        sptr->name, user, host, reason);
  else
    log(L_TRACE, "%s added D-Line for [%s] [%s]",
           sptr->name, host, reason);
}

/* get_conf_name
 *
 * inputs       - type of conf file to return name of file for
 * output       - pointer to filename for type of conf
 * side effects - none
 */
const char *
get_conf_name(KlineType type)
{
  if(type == CONF_TYPE)
    {
      return(ConfigFileEntry.configfile);
    }
  else if(type == KLINE_TYPE)
    {
      return(ConfigFileEntry.klinefile);
    }

  return(ConfigFileEntry.dlinefile);
}

/*
 * conf_add_class
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a class
 *
 */

void conf_add_class(struct ConfItem *aconf,int sendq)
{
  /*
  ** If conf line is a class definition, create a class entry
  */
    /*
    ** associate each conf line with a class by using a pointer
    ** to the correct class record. -avalon
    */
  if (aconf->host)
    {
      add_class(aconf->host, atoi(aconf->passwd),
		atoi(aconf->user), aconf->port,
		sendq);
    }
}

/*
 * conf_add_class_to_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a class pointer to a conf 
 */

void conf_add_class_to_conf(struct ConfItem *aconf)
{
  if(aconf->className == NULL)
    {
      DupString(aconf->className,"default");
      ClassPtr(aconf) = class0;
      return;
    }

  ClassPtr(aconf) = find_class(aconf->className);

  if(ClassPtr(aconf) == class0)
    {
      sendto_realops_flags(FLAGS_ALL,
	   "Warning *** Defaulting to default class for missing class \"%s\"",
			   aconf->className);
      MyFree(aconf->className);
      DupString(aconf->className,"default");
      return;
    }

  if (ConfMaxLinks(aconf) < 0)
    {
      ClassPtr(aconf) = find_class(0);
      MyFree(aconf->className);
      DupString(aconf->className,"default");
      return;
    }
}

/*
 * conf_delist_old_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - delist old conf (if present)
 */

void conf_delist_old_conf(struct ConfItem *aconf)
{
  struct ConfItem *bconf;

  if ((bconf = find_conf_entry(aconf, aconf->status)))
    {
      delist_conf(bconf);
      bconf->status &= ~CONF_ILLEGAL;
      if (aconf->status == CONF_CLIENT)
	{
	  ConfLinks(bconf) -= bconf->clients;
	  ClassPtr(bconf) = ClassPtr(aconf);
	  ConfLinks(bconf) += bconf->clients;
	  bconf->flags = aconf->flags;
	  if(bconf->flags & CONF_OPERATOR)
	    bconf->port = aconf->port;
	}
      free_conf(aconf);
      aconf = bconf;
    }
}

/*
 * conf_add_server
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a connect block
 */
int conf_add_server(struct ConfItem *aconf, int lcount)
{
  conf_add_class_to_conf(aconf);

  if (lcount > MAXCONFLINKS || !aconf->host || !aconf->name)
    {
      sendto_realops_flags(FLAGS_ALL,"Bad connect block");
      log(L_WARN, "Bad connect block");
      return -1;
    }

  if (BadPtr(aconf->passwd))
    {
      sendto_realops_flags(FLAGS_ALL,"Bad connect block, name %s",
			   aconf->name);
      log(L_WARN, "Bad connect block, host %s",aconf->name);
      return -1;
    }
          
  if( SplitUserHost(aconf) < 0 )
    {
      sendto_realops_flags(FLAGS_ALL,"Bad connect block, name %s",
			   aconf->name);
      log(L_WARN, "Bad connect block, name %s",aconf->name);
      return -1;
    }
  lookup_confhost(aconf);
  return 0;
}

/*
 * conf_add_k_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a K line
 */

void conf_add_k_conf(struct ConfItem *aconf)
{
  unsigned long    ip;
  unsigned long    ip_mask;

  if (aconf->host)
    {
      if(is_address(aconf->host,&ip,&ip_mask))
	{
	  ip &= ip_mask;
	  aconf->ip = ip;
	  aconf->ip_mask = ip_mask;
	  if(add_ip_Kline(aconf) < 0)
	    {
	      log(L_ERROR,"Invalid IP K line %s ignored",aconf->host);
	      free_conf(aconf);
	    }
	}
      else
	{
	  (void)collapse(aconf->host);
	  (void)collapse(aconf->user);
	  add_conf(aconf);
	}
    }
}

/*
 * conf_add_d_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a d/D line
 */

void conf_add_d_conf(struct ConfItem *aconf)
{
  unsigned long    ip;
  unsigned long    ip_mask;

  if (aconf->host)
    {
      DupString(aconf->user,aconf->host);
      (void)is_address(aconf->host,&ip,&ip_mask);
      ip &= ip_mask;
      aconf->ip = ip;
      aconf->ip_mask = ip_mask;

      if(aconf->flags & CONF_FLAGS_E_LINED)
	{
	  if(add_Eline(aconf) < 0)
	    {
	      log(L_WARN,"Invalid Eline %s ignored",aconf->host);
	      free_conf(aconf);
	    }
	}
      else
	{
	  if(add_Dline(aconf) < 0)
	    {
	      log(L_WARN,"Invalid Dline %s ignored",aconf->host);
	      free_conf(aconf);
	    }
	}
    }
}

/*
 * conf_add_x_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a X line
 */

void conf_add_x_conf(struct ConfItem *aconf)
{
  MyFree(aconf->user);
  aconf->user = NULL;
  aconf->name = aconf->host;
  aconf->host = (char *)NULL;
  aconf->next = x_conf;
  x_conf = aconf;
}

/*
 * conf_add_x_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add an U line
 */

void conf_add_u_conf(struct ConfItem *aconf)
{
  aconf->next = u_conf;
  u_conf = aconf;
}

/*
 * conf_add_q_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a Q line
 */

void conf_add_q_conf(struct ConfItem *aconf)
{
  if(aconf->user == NULL)
    DupString(aconf->user, "-");

  aconf->next = q_conf;
  q_conf = aconf;
}

/*
 * conf_add_fields
 * inputs       - pointer to config item
 *              - pointer to host_field
 *		- pointer to pass_field
 *              - pointer to user_field
 *              - pointer to port_field
 *		- pointer to class_field
 * output       - NONE
 * side effects - update host/pass/user/port fields of given aconf
 */

void conf_add_fields(struct ConfItem *aconf,
                               char *host_field,
                               char *pass_field,
                               char *user_field,
                               char *port_field,
		     	       char *class_field)
{
  if(host_field)
    DupString(aconf->host, host_field);
  if(pass_field)
    DupString(aconf->passwd, pass_field);
  if(user_field)
    DupString(aconf->user, user_field);
  if(port_field)
    aconf->port = atoi(port_field);
  if(class_field)
    DupString(aconf->className, class_field);
}

/*
 * yyerror
 *
 * inputs	- message from parser
 * output	- none
 * side effects	- message to opers and log file entry is made
 */
void yyerror(char *msg)
{
  char newlinebuf[BUFSIZE];

  strip_tabs(newlinebuf, (const unsigned char *)linebuf, strlen(linebuf));

  sendto_realops_flags(FLAGS_ALL,"%d: %s on line: %s",
		       lineno + 1, msg, newlinebuf);

  log(L_WARN, "%d: %s on line: %s",
      lineno + 1, msg, newlinebuf);
}

int conf_fbgets(char *lbuf,int max_size, FBFILE *fb)
{
  char* buff;

  buff = fbgets(lbuf,max_size,fb);

  if(!buff)
    return 0;

  return(strlen(lbuf));
}

int conf_yy_fatal_error(char *msg)
{
#if 0
  sendto_realops_flags(FLAGS_ALL,
		       "lexer barfed. lets leave it at that for now");
#endif
  return 0;
}

