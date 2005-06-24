/************************************************************************
 *   IRC - Internet Relay Chat, src/s_serv.c
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

#include <sys/types.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>

#include "config.h"
#include "tools.h"
#include "s_serv.h"
#include "channel.h"
#include "vchannel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "event.h"
#include "fdlist.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "ircd_defs.h"
#include "list.h"
#include "numeric.h"
#include "packet.h"
#include "res.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_stats.h"
#include "s_user.h"
#include "scache.h"
#include "send.h"
#include "client.h"
#include "s_debug.h"
#include "memory.h"

extern char *crypt();

#define MIN_CONN_FREQ 300

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

extern struct irc_inaddr vserv;               /* defined in ircd.c */

int MaxConnectionCount = 1;
int MaxClientCount     = 1;

struct Client *uplink=NULL;

static void        burst_members(struct Client *cptr, dlink_list *list);
static void        burst_ll_members(struct Client *cptr, dlink_list *list);

/*
 * list of recognized server capabilities.  "TS" is not on the list
 * because all servers that we talk to already do TS, and the kludged
 * extra argument to "PASS" takes care of checking that.  -orabidoo
 */
struct Capability captab[] = {
/*  name        cap     */ 
  { "QS",       CAP_QS },
  { "EX",       CAP_EX },
  { "CHW",      CAP_CHW },
  { "LL",       CAP_LL },
  { "IE",       CAP_IE },
  { "VCHAN",    CAP_VCHAN },
  { "EOB",      CAP_EOB },
  { "KLN",      CAP_KLN },
  { "GLN",      CAP_GLN },
  { "HOPS",     CAP_HOPS },
  { "HUB",      CAP_HUB },
  { "AOPS",     CAP_AOPS },
  { "UID",      CAP_UID },
  { 0,   0 }
};

unsigned long nextFreeMask();
static unsigned long freeMask;
static void server_burst(struct Client *cptr);
static void burst_all(struct Client *cptr);
static void cjoin_all(struct Client *cptr);

static CNCB serv_connect_callback;


/*
 * my_name_for_link - return wildcard name of my server name 
 * according to given config entry --Jto
 * XXX - this is only called with me.name as name
 */
const char* my_name_for_link(const char* name, struct ConfItem* aconf)
{
  if(aconf->fakename)
  	return(aconf->fakename);
  else
	return(name);
}

/*
 * hunt_server - Do the basic thing in delivering the message (command)
 *      across the relays to the specific server (server) for
 *      actions.
 *
 *      Note:   The command is a format string and *MUST* be
 *              of prefixed style (e.g. ":%s COMMAND %s ...").
 *              Command can have only max 8 parameters.
 *
 *      server  parv[server] is the parameter identifying the
 *              target server.
 *
 *      *WARNING*
 *              parv[server] is replaced with the pointer to the
 *              real servername from the matched client (I'm lazy
 *              now --msa).
 *
 *      returns: (see #defines)
 */
int hunt_server(struct Client *cptr, struct Client *sptr, char *command,
                int server, int parc, char *parv[])
{
  struct Client *acptr;
  int wilds;

  /*
   * Assume it's me, if no server
   */
  if (parc <= server || BadPtr(parv[server]) ||
      match(me.name, parv[server]) ||
      match(parv[server], me.name))
    return (HUNTED_ISME);
  /*
   * These are to pickup matches that would cause the following
   * message to go in the wrong direction while doing quick fast
   * non-matching lookups.
   */
  if ((acptr = find_client(parv[server], NULL)))
    if (acptr->from == sptr->from && !MyConnect(acptr))
      acptr = NULL;
  if (!acptr && (acptr = find_server(parv[server])))
    if (acptr->from == sptr->from && !MyConnect(acptr))
      acptr = NULL;

  collapse(parv[server]);
  wilds = (strchr(parv[server], '?') || strchr(parv[server], '*'));

  /*
   * Again, if there are no wild cards involved in the server
   * name, use the hash lookup
   */
  if (!acptr)
    {
      if (!wilds)
        {
          if (!(acptr = find_server(parv[server])))
            {
              sendto_one(sptr, form_str(ERR_NOSUCHSERVER), me.name,
                         parv[0], parv[server]);
              return(HUNTED_NOSUCH);
            }
        }
      else
        {
          for (acptr = GlobalClientList;
               (acptr = next_client(acptr, parv[server]));
               acptr = acptr->next)
            {
              if (acptr->from == sptr->from && !MyConnect(acptr))
                continue;
              /*
               * Fix to prevent looping in case the parameter for
               * some reason happens to match someone from the from
               * link --jto
               */
              if (IsRegistered(acptr) && (acptr != cptr))
                break;
            }
        }
    }

  if (acptr)
    {
      if (IsMe(acptr) || MyClient(acptr))
        return HUNTED_ISME;
      if (!match(acptr->name, parv[server]))
        parv[server] = acptr->name;

      /* Deal with lazylinks */
      client_burst_if_needed(acptr, sptr);
      sendto_one(acptr, command, parv[0],
                 parv[1], parv[2], parv[3], parv[4],
                 parv[5], parv[6], parv[7], parv[8]);
      return(HUNTED_PASS);
    } 
  sendto_one(sptr, form_str(ERR_NOSUCHSERVER), me.name,
             parv[0], parv[server]);
  return(HUNTED_NOSUCH);
}

/*
 * try_connections - scan through configuration and try new connections.
 * Returns the calendar time when the next call to this
 * function should be made latest. (No harm done if this
 * is called earlier or later...)
 */
void
try_connections(void *unused)
{
  struct ConfItem*   aconf;
  struct Client*     cptr;
  int                connecting = FALSE;
  int                confrq;
  time_t             next = 0;
  struct Class*      cltmp;
  struct ConfItem*   con_conf = NULL;
  int                con_class = 0;

  Debug((DEBUG_NOTICE,"Connection check at: %s", myctime(CurrentTime)));

  for (aconf = ConfigItemList; aconf; aconf = aconf->next )
    {
      /*
       * Also when already connecting! (update holdtimes) --SRB 
       */
      if (!(aconf->status & CONF_SERVER) || aconf->port <= 0 ||
          !(aconf->flags & CONF_FLAGS_ALLOW_AUTO_CONN))
        continue;
      cltmp = ClassPtr(aconf);
      /*
       * Skip this entry if the use of it is still on hold until
       * future. Otherwise handle this entry (and set it on hold
       * until next time). Will reset only hold times, if already
       * made one successfull connection... [this algorithm is
       * a bit fuzzy... -- msa >;) ]
       */
      if (aconf->hold > CurrentTime)
        {
          if (next > aconf->hold || next == 0)
            next = aconf->hold;
          continue;
        }

      if ((confrq = get_con_freq(cltmp)) < MIN_CONN_FREQ )
        confrq = MIN_CONN_FREQ;

      aconf->hold = CurrentTime + confrq;
      /*
       * Found a CONNECT config with port specified, scan clients
       * and see if this server is already connected?
       */
      cptr = find_server(aconf->name);
      
      if (!cptr && (Links(cltmp) < MaxLinks(cltmp)) &&
          (!connecting || (ClassType(cltmp) > con_class)))
        {
          con_class = ClassType(cltmp);
          con_conf = aconf;
          /* We connect only one at time... */
          connecting = TRUE;
        }
      if ((next > aconf->hold) || (next == 0))
        next = aconf->hold;
    }


    if(GlobalSetOptions.autoconn==0)
    {
      /* auto connects disabled, bail */
      goto finish;
    }
     
    if (connecting)
    {
      if (con_conf->next)  /* are we already last? */
      {
        struct ConfItem**  pconf;
        for (pconf = &ConfigItemList; (aconf = *pconf);
          pconf = &(aconf->next))
        /* 
         * put the current one at the end and
         * make sure we try all connections
         */
        if (aconf == con_conf)
          *pconf = aconf->next;
        (*pconf = con_conf)->next = 0;
    }

    if (con_conf->flags & CONF_FLAGS_ALLOW_AUTO_CONN)
    {
      /*
       * We used to only print this if serv_connect() actually
       * suceeded, but since comm_tcp_connect() can call the callback
       * immediately if there is an error, we were getting error messages
       * in the wrong order. SO, we just print out the activated line,
       * and let serv_connect() / serv_connect_callback() print an
       * error afterwards if it fails.
       *   -- adrian
       */
       sendto_realops_flags(FLAGS_ALL,
    		"Connection to %s[%s] activated.",
	      	con_conf->name, con_conf->host);
      serv_connect(con_conf, 0);
    }
  }
  Debug((DEBUG_NOTICE,"Next connection check : %s", myctime(next)));
finish:
  eventAdd("try_connections", try_connections, NULL,
      TRY_CONNECTIONS_TIME, 0);
}

int check_server(const char *name, struct Client* cptr)
{
  struct ConfItem *aconf=NULL;
  struct ConfItem *server_aconf=NULL;
  int error = -1;

  assert(0 != cptr);

  if (!(cptr->localClient->passwd))
    return -2;

  /* loop through looking for all possible connect items that might work */
  for (aconf = ConfigItemList; aconf; aconf = aconf->next)
    {
      if ((aconf->status & CONF_SERVER) == 0)
	continue;

     if (!match(name, aconf->name))
       continue;

     error = -3;

     /* XXX: Fix me for IPv6 */
     /* XXX sockhost is the IPv4 ip as a string */

     if ( match(aconf->host, cptr->host) || 
	  match(aconf->host, cptr->localClient->sockhost) )
       {
	 error = -2;
	   
	 if (IsConfEncrypted(aconf))
	   {
	     if (strcmp(aconf->passwd, 
		   crypt(cptr->localClient->passwd, aconf->passwd)) == 0)
	       {
		 server_aconf = aconf;
	       }
	   }
	 else
	   {
	     if (strcmp(aconf->passwd, cptr->localClient->passwd) == 0)
	       {
		 server_aconf = aconf;
	       }
	   }
       }
    }

  if (server_aconf == NULL)
    return error;

  attach_conf(cptr, server_aconf);

  /* Now find all leaf or hub config items for this server */
  for (aconf = ConfigItemList; aconf; aconf = aconf->next)
    {
      if ((aconf->status & (CONF_HUB|CONF_LEAF)) == 0)
	continue;

      if (!match(name, aconf->name))
	continue;

      attach_conf(cptr, aconf);
    }

  if( !(server_aconf->flags & CONF_FLAGS_LAZY_LINK) )
    ClearCap(cptr,CAP_LL);

  /*
   * Don't unset CAP_HUB here even if the server isn't a hub,
   * it only indicates if the server thinks it's lazylinks are
   * leafs or not.. if you unset it, bad things will happen
   */
  if(aconf != NULL)
  {
#ifdef IPV6
	  if (IN6_IS_ADDR_UNSPECIFIED((struct in6_addr *)
                                      &IN_ADDR(aconf->ipnum)))
#else
	  if (IN_ADDR(aconf->ipnum) == INADDR_NONE)
#endif
	  copy_s_addr(IN_ADDR(aconf->ipnum), IN_ADDR(cptr->localClient->ip)); 
  }
  return 0;
}

/*
 * check_server - check access for a server given its name 
 * (passed in cptr struct). Must check for all C/N lines which have a 
 * name which matches the name given and a host which matches. A host 
 * alias which is the same as the server name is also acceptable in the 
 * host field of a C/N line.
 *  
 *  0 = Access denied
 *  1 = Success
 */
#if 0
int check_server(struct Client* cptr)
{
  dlink_list *lp;
  struct ConfItem* c_conf = 0;
  struct ConfItem* n_conf = 0;

  assert(0 != cptr);

  if (attach_confs(cptr, cptr->name, 
                   CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER ) < 2)
    {
      Debug((DEBUG_DNS,"No C/N lines for %s", cptr->name));
      return 0;
    }
  lp = &cptr->localClient->confs;
  /*
   * This code is from the old world order. It should eventually be
   * duplicated somewhere else later!
   *   -- adrian
   */
#if 0
  if (cptr->dns_reply)
    {
      int             i;
      struct hostent* hp   = cptr->dns_reply->hp;
      char*           name = hp->h_name;
      /*
       * if we are missing a C or N line from above, search for
       * it under all known hostnames we have for this ip#.
       */
      for (i = 0, name = hp->h_name; name; name = hp->h_aliases[i++])
        {
          if (!c_conf)
            c_conf = find_conf_host(lp, name, CONF_CONNECT_SERVER );
          if (!n_conf)
            n_conf = find_conf_host(lp, name, CONF_NOCONNECT_SERVER );
          if (c_conf && n_conf)
            {
              strncpy_irc(cptr->host, name, HOSTLEN);
              break;
            }
        }
      for (i = 0; hp->h_addr_list[i]; ++i)
        {
          if (!c_conf)
            c_conf = find_conf_ip(lp, hp->h_addr_list[i],
                                  cptr->username, CONF_CONNECT_SERVER);
          if (!n_conf)
            n_conf = find_conf_ip(lp, hp->h_addr_list[i],
                                  cptr->username, CONF_NOCONNECT_SERVER);
        }
    }
#endif
  /*
   * Check for C and N lines with the hostname portion the ip number
   * of the host the server runs on. This also checks the case where
   * there is a server connecting from 'localhost'.
   */
  if (!c_conf)
    c_conf = find_conf_host(lp, cptr->host, CONF_CONNECT_SERVER);
  if (!n_conf)
    n_conf = find_conf_host(lp, cptr->host, CONF_NOCONNECT_SERVER);
  /*
   * Attach by IP# only if all other checks have failed.
   * It is quite possible to get here with the strange things that can
   * happen when using DNS in the way the irc server does. -avalon
   */
  if (!c_conf)
    c_conf = find_conf_ip(lp, (char*)& cptr->localClient->ip,
                          cptr->username, CONF_CONNECT_SERVER);
  if (!n_conf)
    n_conf = find_conf_ip(lp, (char*)& cptr->localClient->ip,
                          cptr->username, CONF_NOCONNECT_SERVER);
  /*
   * detach all conf lines that got attached by attach_confs()
   */
  det_confs_butmask(cptr, 0);
  /*
   * if no C or no N lines, then deny access
   */
  if (!c_conf || !n_conf)
    {
      Debug((DEBUG_DNS, "sv_cl: access denied: [%s@%s] c %x n %x",
             cptr->name, cptr->host, c_conf, n_conf));
      return 0;
    }
  /*
   * attach the C and N lines to the client structure for later use.
   */
  attach_conf(cptr, n_conf);
  attach_conf(cptr, c_conf);
  attach_confs(cptr, cptr->name, CONF_HUB | CONF_LEAF);

  
  if( !(n_conf->flags & CONF_FLAGS_LAZY_LINK) )
    ClearCap(cptr,CAP_LL);

  if(ServerInfo.hub)
    {
      if( n_conf->flags & CONF_FLAGS_LAZY_LINK )
        {
          if(find_conf_by_name(n_conf->name, CONF_HUB) 
	     ||
	     IsCapable(cptr,CAP_HUB))
            {
              ClearCap(cptr,CAP_LL);
              sendto_realops_flags(FLAGS_ALL,
		   "*** LazyLinks to a hub from a hub, thats a no-no.");
            }
          else
            {
              cptr->localClient->serverMask = nextFreeMask();

              if(!cptr->localClient->serverMask)
                {
                  sendto_realops_flags(FLAGS_ALL,
				       "serverMask is full!");
                  /* try and negotiate a non LL connect */
                  ClearCap(cptr,CAP_LL);
                }
            }
       }
    }


  /*
   * if the C:line doesn't have an IP address assigned put the one from
   * the client socket there
   */ 
  if (INADDR_NONE == c_conf->ipnum.s_addr)
    c_conf->ipnum.s_addr = cptr->localClient->ip.s_addr;

  Debug((DEBUG_DNS,"sv_cl: access ok: [%s]", cptr->host));
  return 1;
}
#endif

/*
 * send_capabilities
 *
 * inputs	- Client pointer to send to
 *		- int flag of capabilities that this server has
 * output	- NONE
 * side effects	- send the CAPAB line to a server  -orabidoo
 *
 */
void send_capabilities(struct Client* cptr, int lcan_send)
{
  struct Capability* cap;
  char  msgbuf[BUFSIZE];
  char  *t;
  int   tl;

  t = msgbuf;

  for (cap = captab; cap->name; ++cap)
    {
      if (cap->cap & lcan_send)
        {
          tl = ircsprintf(t, "%s ", cap->name);
	  t += tl;
        }
    }
  t--;
  *t = '\0';
  sendto_one(cptr, "CAPAB :%s", msgbuf);
}


/*
 * sendnick_TS
 * 
 * inputs	- client (server) to send nick towards
 * 		- client to send nick for
 * output	- NONE
 * side effects	- NICK message is sent towards given cptr
 */
void sendnick_TS(struct Client *cptr, struct Client *acptr)
{
  static char ubuf[12];

  if (!IsPerson(acptr))
	  return;
  
  send_umode(NULL, acptr, 0, SEND_UMODES, ubuf);
  if (!*ubuf)
  {
	  ubuf[0] = '+';
	  ubuf[1] = '\0';
  }
 
  if (HasID(acptr) && IsCapable(cptr, CAP_UID))
	  sendto_one(cptr, "CLIENT %s %d %lu %s %s %s %s %s :%s",
				 acptr->name, acptr->hopcount + 1, acptr->tsinfo, ubuf,
				 acptr->username, acptr->host,
				 acptr->user->server, acptr->user->id, acptr->info);
  else
	  sendto_one(cptr, "NICK %s %d %lu %s %s %s %s :%s",
				 acptr->name, 
				 acptr->hopcount + 1, acptr->tsinfo, ubuf,
				 acptr->username, acptr->host,
				 acptr->user->server, acptr->info);
}

/*
 * client_burst_if_needed
 * 
 * inputs	- pointer to server
 * 		- pointer to client to add
 * output	- NONE
 * side effects - If this client is not known by this lazyleaf, send it
 */
void client_burst_if_needed(struct Client *cptr, struct Client *acptr)
{
  if (!ServerInfo.hub) return;
  if (!MyConnect(cptr)) return;
  if (!IsCapable(cptr,CAP_LL)) return;

  if((acptr->lazyLinkClientExists & cptr->localClient->serverMask) == 0)
    {
      sendnick_TS( cptr, acptr );
      add_lazylinkclient(cptr,acptr);
    }
}

/*
 * show_capabilities - show current server capabilities
 *
 * inputs       - pointer to an struct Client
 * output       - pointer to static string
 * side effects - build up string representing capabilities of server listed
 */

const char* show_capabilities(struct Client* acptr)
{
  static char        msgbuf[BUFSIZE];
  struct Capability* cap;
  char *t;
  int  tl;

  t = msgbuf;
  tl = ircsprintf(msgbuf,"TS ");
  t += tl;

  if (!acptr->localClient->caps)        /* short circuit if no caps */
    {
      msgbuf[2] = '\0';
      return msgbuf;
    }

  for (cap = captab; cap->cap; ++cap)
    {
      if(cap->cap & acptr->localClient->caps)
        {
          tl = ircsprintf(t, "%s ", cap->name);
	  t += tl;
        }
    }

  t--;
  *t = '\0';

  return msgbuf;
}

/*
 * server_estab
 *
 * inputs       - pointer to a struct Client
 * output       -
 * side effects -
 */

int server_estab(struct Client *cptr)
{
  struct Client*    acptr;
  struct ConfItem*  aconf;
  const char*       inpath;
  static char       inpath_ip[HOSTLEN * 2 + USERLEN + 5];
  char 		    serv_desc[HOSTLEN + 15];
  char*             host;
  dlink_node        *m;
  dlink_node        *ptr;

  assert(0 != cptr);
  ClearAccess(cptr);

  strcpy(inpath_ip, get_client_name(cptr, SHOW_IP));
  inpath = get_client_name(cptr, MASK_IP); /* "refresh" inpath with host */
  host = cptr->name;

  if (!(aconf = find_conf_name(&cptr->localClient->confs, host,
                               CONF_SERVER)))
    {
     /* This shouldn't happen, better tell the ops... -A1kmm */
     sendto_realops_flags(FLAGS_ALL, "Warning: Lost connect{} block "
       "for server %s(this shouldn't happen)!", host);
     return exit_client(cptr, cptr, cptr, "Lost connect{} block!");
    }
  /* We shouldn't have to check this, it should already done before
   * server_estab is called. -A1kmm */
#if 0
#ifdef CRYPT_LINK_PASSWORD
  /* use first two chars of the password they send in as salt */

  /* passwd may be NULL. Head it off at the pass... */
  if(*cptr->localClient->passwd && *n_conf->passwd)
    {
      extern  char *crypt();
      encr = crypt(cptr->localClient->passwd, n_conf->passwd);
    }
  else
    encr = "";
#else
  encr = cptr->localClient->passwd;
#endif  /* CRYPT_LINK_PASSWORD */
  if (*n_conf->passwd && 0 != strcmp(n_conf->passwd, encr))
    {
      ServerStats->is_ref++;
      sendto_one(cptr, "ERROR :No Access (passwd mismatch) %s",
                 inpath);
      sendto_realops_flags(FLAGS_ALL,
			   "Access denied (passwd mismatch) %s", inpath);
      log(L_NOTICE, "Access denied (passwd mismatch) %s", inpath_ip);
      return exit_client(cptr, cptr, cptr, "Bad Password");
    }
#endif
  memset((void *)cptr->localClient->passwd, 0,sizeof(cptr->localClient->passwd));

  /* Its got identd , since its a server */
  SetGotId(cptr);

  /* If there is something in the serv_list, it might be this
   * connecting server..
   */
  if(!ServerInfo.hub && serv_list.head)   
    {
      if (cptr != serv_list.head->data || serv_list.head->next)
        {
         ServerStats->is_ref++;
         sendto_one(cptr, "ERROR :I'm a leaf not a hub");
         return exit_client(cptr, cptr, cptr, "I'm a leaf");
        }
    }

  if (IsUnknown(cptr))
    {
      /*
       * jdc -- 1.  Use EmptyString(), not [0] index reference.
       *        2.  Check aconf->spasswd, not aconf->passwd.
       */
      if (!EmptyString(aconf->spasswd))
      {
        sendto_one(cptr,"PASS %s :TS", aconf->spasswd);
      }

      /*
       * Pass my info to the new server
       *
       * If trying to negotiate LazyLinks, pass on CAP_LL
       * If this is a HUB, pass on CAP_HUB
       */

      send_capabilities(cptr,CAP_MASK
			|
			((aconf->flags & CONF_FLAGS_LAZY_LINK) ? CAP_LL : 0)
			|
			(ServerInfo.hub ? CAP_HUB : 0) );

      sendto_one(cptr, "SERVER %s 1 :%s",
                 my_name_for_link(me.name, aconf), 
                 (me.info[0]) ? (me.info) : "IRCers United");
    }

  sendto_one(cptr,"SVINFO %d %d 0 :%lu", TS_CURRENT, TS_MIN, CurrentTime);
  
  det_confs_butmask(cptr, CONF_LEAF|CONF_HUB|CONF_SERVER);
#if 0
  release_client_dns_reply(cptr);
#endif
  /*
  ** *WARNING*
  **    In the following code in place of plain server's
  **    name we send what is returned by get_client_name
  **    which may add the "sockhost" after the name. It's
  **    *very* *important* that there is a SPACE between
  **    the name and sockhost (if present). The receiving
  **    server will start the information field from this
  **    first blank and thus puts the sockhost into info.
  **    ...a bit tricky, but you have been warned, besides
  **    code is more neat this way...  --msa
  */
  SetServer(cptr);
  cptr->servptr = &me;

  /* Some day, all these lists will be consolidated *sigh* */
  add_client_to_llist(&(me.serv->servers), cptr);

  m = dlinkFind(&unknown_list, cptr);
  if(m != NULL)
    {
      dlinkDelete(m, &unknown_list);
      dlinkAdd(cptr, m, &serv_list);
    }

  Count.server++;
  Count.myserver++;

  /*
   * XXX - this should be in s_bsd
   */
  if (!set_sock_buffers(cptr->fd, READBUF_SIZE))
    report_error(SETBUF_ERROR_MSG, get_client_name(cptr, SHOW_IP), errno);

  /* Show the real host/IP to admins */
  sendto_realops_flags(FLAGS_ADMIN,
			"Link with %s established: (%s) link",
			inpath_ip,show_capabilities(cptr));

  /* Now show the masked hostname/IP to opers */
  sendto_realops_flags(FLAGS_NOTADMIN,
			"Link with %s established: (%s) link",
			inpath,show_capabilities(cptr));

  log(L_NOTICE, "Link with %s established: (%s) link",
      inpath_ip, show_capabilities(cptr));

  add_to_client_hash_table(cptr->name, cptr);
  /* doesnt duplicate cptr->serv if allocated this struct already */
  make_server(cptr);
  cptr->serv->up = me.name;
  /* add it to scache */
  find_or_add(cptr->name);
  
  cptr->serv->sconf = aconf;
  cptr->flags2 |= FLAGS2_CBURST;

  ircsprintf(serv_desc, "Server: %s", cptr->name);
  serv_desc[FD_DESC_SZ-1] = '\0';
  fd_note (cptr->fd, serv_desc);

  /*
  ** Old sendto_serv_but_one() call removed because we now
  ** need to send different names to different servers
  ** (domain name matching) Send new server to other servers.
  */
  for(ptr=serv_list.head;ptr;ptr=ptr->next)
    {
      acptr = ptr->data;

      if (acptr == cptr)
        continue;

      if ((aconf = acptr->serv->sconf) &&
          match(my_name_for_link(me.name, aconf), cptr->name))
        continue;
      sendto_one(acptr,":%s SERVER %s 2 :%s", me.name, cptr->name,
                 cptr->info);
    }

  /*
  ** Pass on my client information to the new server
  **
  ** First, pass only servers (idea is that if the link gets
  ** cancelled beacause the server was already there,
  ** there are no NICK's to be cancelled...). Of course,
  ** if cancellation occurs, all this info is sent anyway,
  ** and I guess the link dies when a read is attempted...? --msa
  ** 
  ** Note: Link cancellation to occur at this point means
  ** that at least two servers from my fragment are building
  ** up connection this other fragment at the same time, it's
  ** a race condition, not the normal way of operation...
  **
  ** ALSO NOTE: using the get_client_name for server names--
  **    see previous *WARNING*!!! (Also, original inpath
  **    is destroyed...)
  */

  aconf = cptr->serv->sconf;
  for (acptr = &me; acptr; acptr = acptr->prev)
    {
      /* acptr->from == acptr for acptr == cptr */
      if (acptr->from == cptr)
        continue;
      if (IsServer(acptr))
        {
          if (match(my_name_for_link(me.name, aconf), acptr->name))
            continue;
          sendto_one(cptr, ":%s SERVER %s %d :%s", acptr->serv->up,
                     acptr->name, acptr->hopcount+1, acptr->info);
        }
    }
  
  if(!ServerInfo.hub)
    {
      uplink = cptr;
    }

  server_burst(cptr);

  return 0;
}


/*
 * server_burst
 *
 * inputs       - struct Client pointer server
 *              -
 * output       - none
 * side effects - send a server burst
 * bugs		- still too long
 */
static void server_burst(struct Client *cptr)
{
  time_t StartBurst=CurrentTime;
  /*
  ** Send it in the shortened format with the TS, if
  ** it's a TS server; walk the list of channels, sending
  ** all the nicks that haven't been sent yet for each
  ** channel, then send the channel itself -- it's less
  ** obvious than sending all nicks first, but on the
  ** receiving side memory will be allocated more nicely
  ** saving a few seconds in the handling of a split
  ** -orabidoo
  */

  /* On a "lazy link" hubs send nothing.
   * Leafs always have to send nicks plus channels
   */
  if( IsCapable(cptr, CAP_LL) )
    {
      if(!ServerInfo.hub)
	{
	  /* burst all our info */
	  burst_all(cptr);
	  /* Now, ask for channel info on all our current channels */
	  cjoin_all(cptr);
	}
    }
  else
    {
      burst_all(cptr);
    }
  cptr->flags2 &= ~FLAGS2_CBURST;

  /* Always send a PING after connect burst is done */
  sendto_one(cptr, "PING :%s", me.name);

  /* XXX maybe `EOB %d %d` where length of burst and time are sent? */
  if(IsCapable(cptr, CAP_EOB))
    sendto_one(cptr, ":%s EOB %lu", me.name, CurrentTime - StartBurst ); 
}

/*
 * burst_all
 *
 * inputs	- pointer to server to send burst to 
 * output	- NONE
 * side effects - complete burst of channels/nicks is sent to cptr
 */
static void
burst_all(struct Client *cptr)
{
  struct Client*    acptr;
  struct Channel*   chptr;
  struct Channel*   vchan; 
  dlink_node *ptr;

  /* serial counter borrowed from send.c */
  current_serial++;

  for (chptr = GlobalChannelList; chptr; chptr = chptr->nextch)
    {
      /* Don't send vchannels twice; vchannels will be
       * sent along as subchannels of the top channel
       */
      
      if(IsVchan(chptr))
	continue;
	  
      burst_members(cptr,&chptr->chanops);
      burst_members(cptr,&chptr->voiced);
      burst_members(cptr,&chptr->halfops);
      burst_members(cptr,&chptr->peons);
      send_channel_modes(cptr, chptr);

      if(IsVchanTop(chptr))
	{
	  for ( ptr = chptr->vchan_list.head; ptr;
		ptr = ptr->next)
	    {
	      vchan = ptr->data;
	      burst_members(cptr,&vchan->chanops);
	      burst_members(cptr,&vchan->voiced);
	      burst_members(cptr,&vchan->halfops);
	      burst_members(cptr,&vchan->peons);
	      send_channel_modes(cptr, vchan);
	    }
	}
    }

  /*
  ** also send out those that are not on any channel
  */
  for (acptr = &me; acptr; acptr = acptr->prev)
    {
      if (acptr->serial != current_serial)
	{
	  acptr->serial = current_serial;
	  if (acptr->from != cptr)
	    sendnick_TS(cptr, acptr);
	}
    }
}

/*
 * cjoin_all
 *
 * inputs       - server to ask for channel info from
 * output       - NONE
 * side effects	- CJOINS for all the leafs known channels is sent
 */
static void
cjoin_all(struct Client *cptr)
{
  struct Channel *chptr;

  for (chptr = GlobalChannelList; chptr; chptr = chptr->nextch)
    {
      sendto_one(cptr, ":%s CBURST %s",
		 me.name, chptr->chname);
    }
}

/*
 * burst_channel
 *
 * inputs	- pointer to server to send sjoins to
 *              - channel pointer
 * output	- none
 * side effects	- All sjoins for channel(s) given by chptr are sent
 *                for all channel members. If channel has vchans, send
 *                them on. ONLY called by hub on behalf of a lazylink
 *		  so cptr is always guaranteed to be a LL leaf.
 */
void
burst_channel(struct Client *cptr, struct Channel *chptr)
{
  dlink_node        *ptr;
  struct Channel*   vchan;

  burst_ll_members(cptr,&chptr->chanops);
  burst_ll_members(cptr,&chptr->voiced);
  burst_ll_members(cptr,&chptr->halfops);
  burst_ll_members(cptr,&chptr->peons);
  send_channel_modes(cptr, chptr);
  add_lazylinkchannel(cptr,chptr);

  if(chptr->topic[0])
    {
      sendto_one(cptr, ":%s TOPIC %s %s %lu :%s",
		 me.name, chptr->chname,
		 chptr->topic_info,chptr->topic_time,
		 chptr->topic);
    }

  if(IsVchanTop(chptr))
    {
      for ( ptr = chptr->vchan_list.head; ptr; ptr = ptr->next)
	{
	  vchan = ptr->data;
	  burst_ll_members(cptr,&vchan->chanops);
	  burst_ll_members(cptr,&vchan->voiced);
	  burst_ll_members(cptr,&vchan->halfops);
	  burst_ll_members(cptr,&vchan->peons);
	  send_channel_modes(cptr, vchan);
	  add_lazylinkchannel(cptr,vchan);

	  if(vchan->topic[0])
	    {
	      sendto_one(cptr, ":%s TOPIC %s %s %lu :%s",
			 me.name, vchan->chname,
			 vchan->topic_info,vchan->topic_time,
			 vchan->topic);
	    }
	}
    }
}

/*
 * add_lazylinkchannel
 *
 * inputs	- pointer to server being introduced to this hub
 *		- pointer to channel structure being introduced
 * output	- NONE
 * side effects	- The channel pointed to by chptr is now known
 *		  to be on lazyleaf server given by cptr.
 *		  mark that in the bit map and add to the list
 *		  of channels to examine after this newly introduced
 *		  server is squit off.
 */
void
add_lazylinkchannel(struct Client *cptr, struct Channel *chptr)
{
  dlink_node *m;

  assert(cptr->localClient != NULL);

  chptr->lazyLinkChannelExists |= cptr->localClient->serverMask;

  m = make_dlink_node();

  dlinkAdd(chptr, m, &lazylink_channels);
}

/*
 * add_lazylinkclient
 *
 * inputs       - pointer to server being introduced to this hub
 *              - pointer to client being introduced
 * output       - NONE
 * side effects - The client pointed to by sptr is now known
 *                to be on lazyleaf server given by cptr.
 *                mark that in the bit map and add to the list
 *                of clients to examine after this newly introduced
 *                server is squit off.
 */
void
add_lazylinkclient(struct Client *cptr, struct Client *sptr)
{
  dlink_node *m;

  assert(cptr->localClient != NULL);

  sptr->lazyLinkClientExists |= cptr->localClient->serverMask;

  m = make_dlink_node();

  dlinkAdd(sptr, m, &lazylink_nicks);
}

/*
 * remove_lazylink_flags
 *
 * inputs	- pointer to server quitting
 * output	- NONE
 * side effects	- All the channels on the lazylink channel list are examined
 *		  If they hold a bit corresponding to the servermask
 *		  attached to cptr, clear that bit. If this bitmask
 *		  goes to 0, then the channel is no longer known to
 *		  be on any lazylink server, and can be removed from the 
 *		  link list.
 *
 *		  Similar is done for lazylink clients
 *
 *		  This function must be run by the HUB on any exiting
 *		  lazylink leaf server, while the pointer is still valid.
 *		  Hence must be run from client.c in exit_one_client()
 *
 *		  The old code scanned all channels, this code only
 *		  scans channels/clients on the lazylink_channels
 *		  lazylink_clients lists.
 */
void
remove_lazylink_flags(unsigned long mask)
{
  dlink_node *ptr;
  dlink_node *next_ptr;
  struct Channel *chptr;
  struct Client *acptr;
  unsigned long clear_mask;

  if(!mask) /* On 0 mask, don't do anything */
    return;

  clear_mask = ~mask;

  freeMask |= mask;

  for (ptr = lazylink_channels.head; ptr; ptr = next_ptr)
    {
      next_ptr = ptr->next;

      chptr = ptr->data;
      chptr->lazyLinkChannelExists &= clear_mask;

      if ( chptr->lazyLinkChannelExists == 0 )
	{
	  dlinkDelete(ptr, &lazylink_channels);
	  free_dlink_node(ptr);
	}
    }

  for (ptr = lazylink_nicks.head; ptr; ptr = next_ptr)
    {
      next_ptr = ptr->next;

      acptr = ptr->data;
      acptr->lazyLinkClientExists &= clear_mask;

      if ( acptr->lazyLinkClientExists == 0 )
        {
          dlinkDelete(ptr, &lazylink_nicks);
          free_dlink_node(ptr);
        }
    }
}

/*
 * burst_members
 *
 * inputs	- pointer to server to send members to
 * 		- dlink_list pointer to membership list to send
 * output	- NONE
 * side effects	-
 */
void burst_members(struct Client *cptr, dlink_list *list)
{
  struct Client *acptr;
  dlink_node *ptr;

  for (ptr = list->head; ptr; ptr = ptr->next)
    {
      acptr = ptr->data;
      if (acptr->serial != current_serial)
	{
	  acptr->serial = current_serial;
	  if (acptr->from != cptr)
	    sendnick_TS(cptr, acptr);
	}
    }
}

/*
 * burst_ll_members
 *
 * inputs	- pointer to server to send members to
 * 		- dlink_list pointer to membership list to send
 * output	- NONE
 * side effects	- This version also has to check the bitmap for lazylink
 */
void burst_ll_members(struct Client *cptr, dlink_list *list)
{
  struct Client *acptr;
  dlink_node *ptr;

  for (ptr = list->head; ptr; ptr = ptr->next)
    {
      acptr = ptr->data;
      if ((acptr->lazyLinkClientExists & cptr->localClient->serverMask) == 0)
        {
          if (acptr->from != cptr)
	    {
	      add_lazylinkclient(cptr,acptr);
	      sendnick_TS(cptr, acptr);
	    }
        }
    }
}

/*
 * set_autoconn - set autoconnect mode
 *
 * inputs       - struct Client pointer to oper requesting change
 *              -
 * output       - none
 * side effects -
 */
void set_autoconn(struct Client *sptr,char *parv0,char *name,int newval)
{
  struct ConfItem *aconf;

  if(name && (aconf= find_conf_by_name(name, CONF_SERVER)))
    {
      if (newval)
        aconf->flags |= CONF_FLAGS_ALLOW_AUTO_CONN;
      else
        aconf->flags &= ~CONF_FLAGS_ALLOW_AUTO_CONN;

      sendto_realops_flags(FLAGS_ALL,
			   "%s has changed AUTOCONN for %s to %i",
			   parv0, name, newval);
      sendto_one(sptr,
                 ":%s NOTICE %s :AUTOCONN for %s is now set to %i",
                 me.name, parv0, name, newval);
    }
  else if (name)
    {
      sendto_one(sptr,
                 ":%s NOTICE %s :Can't find %s",
                 me.name, parv0, name);
    }
  else
    {
      sendto_one(sptr,
                 ":%s NOTICE %s :Please specify a server name!",
                 me.name, parv0);
    }
}


/*
 * show_servers - send server list to client
 *
 * inputs        - struct Client pointer to client to show server list to
 *               - name of client
 * output        - NONE
 * side effects  - NONE
 */
void show_servers(struct Client *cptr)
{
  struct Client *cptr2;
  dlink_node *ptr;
  int j=0;                /* used to count servers */

  for(ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      cptr2 = ptr->data;

      ++j;
      sendto_one(cptr, ":%s %d %s :%s (%s!%s@%s) Idle: %lu",
                 me.name, RPL_STATSDEBUG, cptr->name, cptr2->name,
                 (cptr2->serv->by[0] ? cptr2->serv->by : "Remote."), 
                 "*", "*", CurrentTime - cptr2->lasttime);

      /*
       * NOTE: moving the username and host to the client struct
       * makes the names in the server->user struct no longer available
       * IMO this is not a big problem because as soon as the user that
       * started the connection leaves the user info has to go away
       * anyhow. Simply showing the nick should be enough here.
       * --Bleep
       */ 
    }

  sendto_one(cptr, ":%s %d %s :%d Server%s", me.name, RPL_STATSDEBUG,
             cptr->name, j, (j==1) ? "" : "s");
}

void initServerMask(void)
{
  freeMask = 0xFFFFFFFFL;
}

/*
 * nextFreeMask
 *
 * inputs	- NONE
 * output	- unsigned long next unused mask for use in LL
 * side effects	-
 */
unsigned long nextFreeMask()
{
  int i;
  unsigned long mask;

  mask = 1;

  for(i=0;i<32;i++)
    {
      if( mask & freeMask )
        {
          freeMask &= ~mask;
          return(mask);
        }
      mask <<= 1;
    }
  return 0L; /* watch this special case ... */
}

/*
 * New server connection code
 * Based upon the stuff floating about in s_bsd.c
 *   -- adrian
 */

/*
 * serv_connect() - initiate a server connection
 *
 * inputs	- pointer to conf 
 *		- pointer to client doing the connet
 * output	-
 * side effects	-
 *
 * This code initiates a connection to a server. It first checks to make
 * sure the given server exists. If this is the case, it creates a socket,
 * creates a client, saves the socket information in the client, and
 * initiates a connection to the server through comm_connect_tcp(). The
 * completion of this goes through serv_completed_connection().
 *
 * We return 1 if the connection is attempted, since we don't know whether
 * it suceeded or not, and 0 if it fails in here somewhere.
 */
int
serv_connect(struct ConfItem *aconf, struct Client *by)
{
    struct Client *cptr;
    int fd;
    char serv_desc[HOSTLEN + 15];
    char buf[HOSTIPLEN];
    /* Make sure aconf is useful */
    assert(aconf != NULL);

    /* log */
    inetntop(DEF_FAM, &IN_ADDR(aconf->ipnum), buf, HOSTIPLEN);
    log(L_NOTICE, "Connect to %s[%s] @%s", aconf->user, aconf->host,
         buf);

    /*
     * Make sure this server isn't already connected
     * Note: aconf should ALWAYS be a valid C: line
     */
    if ((cptr = find_server(aconf->name)))
      { 
        sendto_realops_flags(FLAGS_ALL,
			     "Server %s already present from %s",
			     aconf->name, get_client_name(cptr, SHOW_IP));
        if (by && IsPerson(by) && !MyClient(by))
	  sendto_one(by, ":%s NOTICE %s :Server %s already present from %s",
		     me.name, by->name, aconf->name,
		     get_client_name(cptr, SHOW_IP));
        return 0;
      }

    /* servernames are always guaranteed under HOSTLEN chars */
    ircsprintf(serv_desc, "Server: %s", aconf->name);
    serv_desc[FD_DESC_SZ-1] = '\0';

    /* create a socket for the server connection */ 
    if ((fd = comm_open(DEF_FAM, SOCK_STREAM, 0, serv_desc)) < 0)
      {
        /* Eek, failure to create the socket */
        report_error("opening stream socket to %s: %s", aconf->name, errno);
        return 0;
      }

    /* Create a local client */
    cptr = make_client(NULL);

    /* Copy in the server, hostname, fd */
    strncpy_irc(cptr->name, aconf->name, HOSTLEN);
    strncpy_irc(cptr->host, aconf->host, HOSTLEN);
    inetntop(DEF_FAM, &IN_ADDR(aconf->ipnum), cptr->localClient->sockhost, HOSTIPLEN);
    cptr->fd = fd;

    /*
     * Set up the initial server evilness, ripped straight from
     * connect_server(), so don't blame me for it being evil.
     *   -- adrian
     */

    if (!set_non_blocking(cptr->fd))
        report_error(NONB_ERROR_MSG, get_client_name(cptr, SHOW_IP), errno);

    if (!set_sock_buffers(cptr->fd, READBUF_SIZE))
        report_error(SETBUF_ERROR_MSG, get_client_name(cptr, SHOW_IP), errno);

    /*
     * NOTE: if we're here we have a valid C:Line and the client should
     * have started the connection and stored the remote address/port and
     * ip address name in itself
     *
     * Attach config entries to client here rather than in
     * serv_connect_callback(). This to avoid null pointer references.
     */
    if (!attach_cn_lines(cptr, aconf->name, aconf->host))
      {
        sendto_realops_flags(FLAGS_ALL,
			   "Host %s is not enabled for connecting:no C/N-line",
			     aconf->name);
        if (by && IsPerson(by) && !MyClient(by))  
            sendto_one(by, ":%s NOTICE %s :Connect to host %s failed.",
              me.name, by->name, cptr->name);
        det_confs_butmask(cptr, 0);
        free_client(cptr);
        return 0;
      }
    /*
     * at this point we have a connection in progress and C/N lines
     * attached to the client, the socket info should be saved in the
     * client and it should either be resolved or have a valid address.
     *
     * The socket has been connected or connect is in progress.
     */
    make_server(cptr);
    if (by && IsPerson(by))
      {
        strcpy(cptr->serv->by, by->name);
        if (cptr->serv->user)
            free_user(cptr->serv->user, NULL);
        cptr->serv->user = by->user;
        by->user->refcnt++;
      }
    else
      {
        strcpy(cptr->serv->by, "AutoConn.");
        if (cptr->serv->user)
            free_user(cptr->serv->user, NULL);
        cptr->serv->user = NULL;
      }
    cptr->serv->up = me.name;
    SetConnecting(cptr);
    add_client_to_list(cptr);
    cptr->localClient->aftype = DEF_FAM;
    /* Now, initiate the connection */
    if(ServerInfo.specific_virtual_host)
      {
	struct irc_sockaddr ipn;
	memset(&ipn, 0, sizeof(struct irc_sockaddr));
	S_FAM(ipn) = DEF_FAM;
	S_PORT(ipn) = 0;
#ifdef IPV6
	copy_s_addr(S_ADDR(ipn), IN_ADDR(vserv));
#else
	copy_s_addr(S_ADDR(ipn), htonl(IN_ADDR(vserv)));
#endif
	comm_connect_tcp(cptr->fd, aconf->host, aconf->port,
			 (struct sockaddr *)&SOCKADDR(ipn), sizeof(struct irc_sockaddr), 
			 serv_connect_callback, cptr, aconf->aftype);
      }
    else
      {
	comm_connect_tcp(cptr->fd, aconf->host, aconf->port, NULL, 0, 
			 serv_connect_callback, cptr, aconf->aftype);
      }

    return 1;
}

/*
 * serv_connect_callback() - complete a server connection.
 * 
 * This routine is called after the server connection attempt has
 * completed. If unsucessful, an error is sent to ops and the client
 * is closed. If sucessful, it goes through the initialisation/check
 * procedures, the capabilities are sent, and the socket is then
 * marked for reading.
 */
static void
serv_connect_callback(int fd, int status, void *data)
{
    struct Client *cptr = data;
    struct ConfItem *aconf;

    /* First, make sure its a real client! */
    assert(cptr != NULL);
    assert(cptr->fd == fd);

    /* Next, for backward purposes, record the ip of the server */
    copy_s_addr(IN_ADDR(cptr->localClient->ip), S_ADDR(fd_table[fd].connect.hostaddr));
    /* Check the status */
    if (status != COMM_OK)
      {
        /* We have an error, so report it and quit */
	/* Admins get to see any IP, mere opers don't *sigh*
	 */
        sendto_realops_flags(FLAGS_ADMIN,
			     "Error connecting to %s[%s]: %s", cptr->name,
			     cptr->host, comm_errstr(status));
	sendto_realops_flags(FLAGS_NOTADMIN,
			     "Error connecting to %s: %s",
			     cptr->name, comm_errstr(status));
	cptr->flags |= FLAGS_DEADSOCKET;
        exit_client(cptr, cptr, &me, comm_errstr(status));
        return;
      }

    /* COMM_OK, so continue the connection procedure */
    /* Get the C/N lines */
    aconf = find_conf_name(&cptr->localClient->confs,
			    cptr->name, CONF_SERVER); 
    if (!aconf)
      { 
        sendto_realops_flags(FLAGS_ALL,
		     "Lost C-Line for %s", get_client_name(cptr, HIDE_IP));
        exit_client(cptr, cptr, &me, "Lost C-line");
        return;
      }
    /* Next, send the initial handshake */
    SetHandshake(cptr);

    /*
     * jdc -- Check and sending spasswd, not passwd.
     */
    if (!EmptyString(aconf->spasswd))
    {
        sendto_one(cptr, "PASS %s :TS", aconf->spasswd);
    }

    /*
     * Pass my info to the new server
     *
     * If trying to negotiate LazyLinks, pass on CAP_LL
     * If this is a HUB, pass on CAP_HUB
     */

    send_capabilities(cptr,CAP_MASK
		      |
		      ((aconf->flags & CONF_FLAGS_LAZY_LINK) ? CAP_LL : 0)
		      |
		      (ServerInfo.hub ? CAP_HUB : 0) );

    sendto_one(cptr, "SERVER %s 1 :%s",
      my_name_for_link(me.name, aconf), me.info);

    /* 
     * If we've been marked dead because a send failed, just exit
     * here now and save everyone the trouble of us ever existing.
     */
    if (IsDead(cptr)) {
        sendto_realops_flags(FLAGS_ADMIN,
			     "%s[%s] went dead during handshake", cptr->name,
			     cptr->host);
        sendto_realops_flags(FLAGS_ADMIN,
			     "%s went dead during handshake", cptr->name);
        exit_client(cptr, cptr, &me, "Went dead during handshake");
        return;
    }

    /* don't move to serv_list yet -- we haven't sent a burst! */

    /* If we get here, we're ok, so lets start reading some data */
    comm_setselect(fd, FDLIST_SERVER, COMM_SELECT_READ, read_packet, cptr, 0);
}
