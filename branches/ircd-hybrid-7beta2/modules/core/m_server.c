/***********************************************************************
 *   IRC - Internet Relay Chat, modules/m_server.c
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
#include "handlers.h"  /* m_server prototype */
#include "client.h"      /* client struct */
#include "common.h"      /* TRUE bleah */
#include "event.h"
#include "hash.h"        /* add_to_client_hash_table */
#include "irc_string.h"  /* strncpy_irc */
#include "ircd.h"        /* me */
#include "list.h"        /* make_server */
#include "numeric.h"     /* ERR_xxx */
#include "s_conf.h"      /* struct ConfItem */
#include "s_log.h"       /* log level defines */
#include "s_serv.h"      /* server_estab, check_server, my_name_for_link */
#include "s_stats.h"     /* ServerStats */
#include "scache.h"      /* find_or_add */
#include "send.h"        /* sendto_one */
#include "motd.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <string.h>
#include <stdlib.h>

static void mr_server(struct Client*, struct Client*, int, char **);
static void ms_server(struct Client*, struct Client*, int, char **);
struct Message server_msgtab = {
  "SERVER", 0, 3, 0, MFLG_SLOW | MFLG_UNREG, 0,
  {mr_server, m_registered, ms_server, m_registered}
};

void 
_modinit(void)
{
  mod_add_cmd(&server_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&server_msgtab);
}

char *parse_server_args(char *parv[], int parc, char *info, int *hop);
int bogus_host(char *host);
void write_links_file(void*);

char *_version = "20001122";

static int       refresh_user_links=0;

/*
 * mr_server - SERVER message handler
 *      parv[0] = sender prefix
 *      parv[1] = servername
 *      parv[2] = serverinfo/hopcount
 *      parv[3] = serverinfo
 */
static void mr_server(struct Client *cptr, struct Client *sptr,
                      int parc, char *parv[])
{
  char             info[REALLEN + 1];
  char             *name;
  struct Client    *acptr;
  int              hop;

  if ( (name = parse_server_args(parv, parc, info, &hop)) == NULL )
    {
      sendto_one(cptr,"ERROR :No servername");
      return;
    }
  /* 
   * Reject a direct nonTS server connection if we're TS_ONLY -orabidoo
   */
  if (!DoesTS(cptr))
    {
      sendto_realops_flags(FLAGS_ADMIN,"Link %s dropped, non-TS server",
			   get_client_name(cptr, HIDE_IP));
      sendto_realops_flags(FLAGS_NOTADMIN,"Link %s dropped, non-TS server",
			   get_client_name(cptr, MASK_IP));
      exit_client(cptr, cptr, cptr, "Non-TS server");
      return;
    }

  if (bogus_host(name))
  {
    exit_client(cptr, cptr, cptr, "Bogus server name");
    return;
  }

  /* Now we just have to call check_server and everything should be
   * check for us... -A1kmm. */
  switch (check_server(name, cptr))
    {
     case -1:
      if (ConfigFileEntry.warn_no_nline)
        {
         sendto_realops_flags(FLAGS_ADMIN,
           "Unauthorized server connection attempt from %s: No entry for "
           "servername %s", get_client_name(cptr, HIDE_IP), name);

         sendto_realops_flags(FLAGS_NOTADMIN,
           "Unauthorized server connection attempt from %s: No entry for "
           "servername %s", get_client_name(cptr, MASK_IP), name);
        }
      exit_client(cptr, cptr, cptr, "Invalid servername.");
      return;
      break;
     case -2:
      sendto_realops_flags(FLAGS_ADMIN,
        "Unauthorized server connection attempt from %s: Bad password "
        "for server %s", get_client_name(cptr, HIDE_IP), name);

      sendto_realops_flags(FLAGS_NOTADMIN,
        "Unauthorized server connection attempt from %s: Bad password "
        "for server %s", get_client_name(cptr, MASK_IP), name);

      exit_client(cptr, cptr, cptr, "Invalid password.");
      return;
      break;
     case -3:
      sendto_realops_flags(FLAGS_ADMIN,
        "Unauthorized server connection attempt from %s: Invalid host "
        "for server %s", get_client_name(cptr, HIDE_IP), name);

      sendto_realops_flags(FLAGS_NOTADMIN,
        "Unauthorized server connection attempt from %s: Invalid host "
        "for server %s", get_client_name(cptr, MASK_IP), name);

      exit_client(cptr, cptr, cptr, "Invalid host.");
      return;
      break;
    }
    
  if ((acptr = find_server(name)))
    {
      /*
       * This link is trying feed me a server that I already have
       * access through another path -- multiple paths not accepted
       * currently, kill this link immediately!!
       *
       * Rather than KILL the link which introduced it, KILL the
       * youngest of the two links. -avalon
       *
       * Definitely don't do that here. This is from an unregistered
       * connect - A1kmm.
       */
      sendto_realops_flags(FLAGS_ADMIN,
         "Attempt to re-introduce server %s from %s", name,
         get_client_name(cptr, HIDE_IP));

      sendto_realops_flags(FLAGS_NOTADMIN,
         "Attempt to re-introduce server %s from %s", name,
         get_client_name(cptr, MASK_IP));

      sendto_one(cptr, "ERROR :Server already exists.");
      exit_client(cptr, cptr, cptr, "Server Exists");
      return;
    }

  if(ServerInfo.hub && IsCapable(cptr, CAP_LL))
    {
      if(IsCapable(cptr, CAP_HUB))
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
  else if (IsCapable(cptr, CAP_LL))
    {
      if(!IsCapable(cptr, CAP_HUB))
        {
          ClearCap(cptr,CAP_LL);
          sendto_realops_flags(FLAGS_ALL,
               "*** LazyLinks to a leaf from a leaf, thats a no-no.");
        }
    }

  /*
   * if we are connecting (Handshake), we already have the name from the
   * C:line in cptr->name
   */
  strncpy_irc(cptr->name, name, HOSTLEN);
  strncpy_irc(cptr->info, info[0] ? info : me.name, REALLEN);
  cptr->hopcount = hop;

  server_estab(cptr);
}

/*
 * ms_server - SERVER message handler
 *      parv[0] = sender prefix
 *      parv[1] = servername
 *      parv[2] = serverinfo/hopcount
 *      parv[3] = serverinfo
 */
static void ms_server(struct Client *cptr, struct Client *sptr,
                      int parc, char *parv[])
{
  char             info[REALLEN + 1];
                   /* same size as in s_misc.c */
  char*            name;
  struct Client*   acptr;
  struct Client*   bcptr;
  struct ConfItem* aconf;
  int              hop;
  int              hlined = 0;
  int              llined = 0;
  dlink_node	   *ptr;

  if ( (name = parse_server_args(parv, parc, info, &hop)) == NULL )
    {
      sendto_one(cptr,"ERROR :No servername");
      return;
    }

  if ((acptr = find_server(name)))
    {
      /*
       * This link is trying feed me a server that I already have
       * access through another path -- multiple paths not accepted
       * currently, kill this link immediately!!
       *
       * Rather than KILL the link which introduced it, KILL the
       * youngest of the two links. -avalon
       *
       * I think that we should exit the link itself, not the introducer,
       * and we should always exit the most recently received(i.e. the
       * one we are receiving this SERVER for. -A1kmm
       */
      /* It is behind a host-masked server. Completely ignore the
       * server message(don't propagate or we will delink from whoever
       * we propagate to). -A1kmm */
      if (irccmp(acptr->name, name) && acptr->from==cptr)
        return;
      
      sendto_realops_flags(FLAGS_ALL,
                           "Server %s(via %s) introduced an existing server %s.",
                           sptr->name, cptr->name, name);
      exit_client(NULL, sptr, &me, "Server Exists");
      return;
    }

  /* 
   * User nicks never have '.' in them and server names
   * must always have '.' in them.
   */
  if ( strchr(name,'.') == NULL )
    {
      /*
       * Server trying to use the same name as a person. Would
       * cause a fair bit of confusion. Enough to make it hellish
       * for a while and servers to send stuff to the wrong place.
       */
      sendto_one(cptr,"ERROR :Nickname %s already exists!", name);
      sendto_realops_flags(FLAGS_ALL,
			   "Link %s cancelled: Server/nick collision on %s",
		/* inpath */ get_client_name(cptr, HIDE_IP),
				name);
      exit_client(cptr, cptr, cptr, "Nick as Server");
      return;
    }

  /*
   * Server is informing about a new server behind
   * this link. Create REMOTE server structure,
   * add it to list and propagate word to my other
   * server links...
   */
  if (parc == 1 || info[0] == '\0')
    {
      sendto_one(cptr, "ERROR :No server info specified for %s", name);
      return;
    }

  /*
   * See if the newly found server is behind a guaranteed
   * leaf. If so, close the link.
   *
   */

  for (aconf = ConfigItemList; aconf; aconf=aconf->next)
    {
     if (!(aconf->status == CONF_LEAF || aconf->status == CONF_HUB))
       continue;

     if (match(aconf->name, cptr->name))
       {
        if (aconf->status == CONF_HUB)
	  {
	    if(match(aconf->host, name))
	      hlined++;
	  }
        else if (aconf->status == CONF_LEAF)
	  {
	    if(match(aconf->host, name))
	      llined++;
	  }
       }
    }

  /* Ok, this way this works is
   *
   * A server can have a CONF_HUB allowing it to introduce servers
   * behind it.
   *
   * connect {
   *            name = "irc.bighub.net";
   *            host_mask="*";
   *            ...
   * 
   * That would allow "irc.bighub.net" to introduce anything it wanted..
   *
   * However
   *
   * connect {
   *            name = "irc.somehub.fi";
   *		host_mask="*";
   *		leaf_mask="*.edu";
   *...
   * Would allow this server in finland to hub anything but
   * .edu's
   */

  /* Ok, check cptr can hub the new server, and make sure it's not a LL */
  if (!hlined || (IsCapable(cptr, CAP_LL) && !IsCapable(cptr, CAP_HUB)))
    {
      /* OOOPs nope can't HUB */
      sendto_realops_flags(FLAGS_ALL,"Non-Hub link %s introduced %s.",
                get_client_name(cptr, HIDE_IP), name);
      /* If it is new, we are probably misconfigured, so split the
       * non-hub server introducing this. Otherwise, split the new
       * server. -A1kmm. */
      if ((CurrentTime - sptr->firsttime) < 20)
        {
          exit_client(NULL, sptr, &me, "No H-line.");
          return;
        }
      else
        {
          sendto_one(sptr, ":%s SQUIT %s :Sorry, no H-line.",
                     me.name, name);
          return;
        }
    }

  /* Check for the new server being leafed behind this HUB */
  if (llined)
    {
      /* OOOPs nope can't HUB this leaf */
      sendto_realops_flags(FLAGS_ALL,"link %s introduced leafed %s.",
                get_client_name(cptr, HIDE_IP), name);
      /* If it is new, we are probably misconfigured, so split the
       * non-hub server introducing this. Otherwise, split the new
       * server. -A1kmm. */
      if ((CurrentTime - sptr->firsttime) < 20)
        {
          exit_client(NULL, sptr, &me, "Leafed Server.");
          return;
        }
      else
        {
          sendto_one(sptr, ":%s SQUIT %s :Sorry, Leafed server.",
                     me.name, name);
          return;
        }
    }

  acptr = make_client(cptr);
  make_server(acptr);
  acptr->hopcount = hop;
  strncpy_irc(acptr->name, name, HOSTLEN);
  strncpy_irc(acptr->info, info, REALLEN);
  acptr->serv->up = find_or_add(parv[0]);
  acptr->servptr = sptr;

  SetServer(acptr);

  Count.server++;

  add_client_to_list(acptr);
  add_to_client_hash_table(acptr->name, acptr);
  add_client_to_llist(&(acptr->servptr->serv->servers), acptr);


  /*
   * Old sendto_serv_but_one() call removed because we now
   * need to send different names to different servers
   * (domain name matching)
   */
  for (ptr = serv_list.head; ptr; ptr = ptr->next)
    {
      bcptr = ptr->data;

      if (bcptr == cptr)
	continue;
      if (!(aconf = bcptr->serv->sconf))
	{
	  sendto_realops_flags(FLAGS_ALL,"Lost N-line for %s on %s. Closing",
			       get_client_name(cptr, HIDE_IP), name);
	  exit_client(cptr, cptr, cptr, "Lost N line");
          return;
	}
      if (match(my_name_for_link(me.name, aconf), acptr->name))
	continue;

      sendto_one(bcptr, ":%s SERVER %s %d :%s",
		 parv[0], acptr->name, hop + 1, acptr->info);
                         
    }
      
  sendto_realops_flags(FLAGS_EXTERNAL, "Server %s being introduced by %s",
		       acptr->name, sptr->name);

  if (!refresh_user_links)
    {
      refresh_user_links = 1;
      eventAdd("write_links_file", write_links_file, NULL,
	ConfigFileEntry.links_delay, 0);
    }
}

/*
 * write_links_file
 *
 * 
 */
void write_links_file(void* notused)
{
  MessageFileLine *next_mptr = 0;
  MessageFileLine *mptr = 0;
  MessageFileLine *currentMessageLine = 0;
  MessageFileLine *newMessageLine = 0;
  MessageFile *MessageFileptr;
  struct Client *acptr;
  char *p;
  FBFILE* file;
  char buff[512];

  refresh_user_links = 0;

  MessageFileptr = &ConfigFileEntry.linksfile;

  if ((file = fbopen(MessageFileptr->fileName, "w")) == 0)
    return;

  for( mptr = MessageFileptr->contentsOfFile; mptr; mptr = next_mptr)
    {
      next_mptr = mptr->next;
      MyFree(mptr);
    }
  MessageFileptr->contentsOfFile = NULL;
  currentMessageLine = NULL;

  for (acptr = GlobalClientList; acptr; acptr = acptr->next) 
    {
      if(IsServer(acptr))
	{
          if(acptr->info[0])
            {
              if( (p = strchr(acptr->info,']')) )
                p += 2; /* skip the nasty [IP] part */
              else
                p = acptr->info;
            }
          else
            p = "(Unknown Location)";

	  newMessageLine = (MessageFileLine*) MyMalloc(sizeof(MessageFileLine));

/* Attempt to format the file in such a way it follows the usual links output
 * ie  "servername uplink :hops info"
 * Mostly for aesthetic reasons - makes it look pretty in mIRC ;)
 * - madmax
*/
	  ircsprintf(newMessageLine->line,"%s %s :1 %s",
		     acptr->name,me.name,p);
	  newMessageLine->next = (MessageFileLine *)NULL;

	  if (MessageFileptr->contentsOfFile)
	    {
	      if (currentMessageLine)
		currentMessageLine->next = newMessageLine;
	      currentMessageLine = newMessageLine;
	    }
	  else
	    {
	      MessageFileptr->contentsOfFile = newMessageLine;
	      currentMessageLine = newMessageLine;
	    }
	  ircsprintf(buff,"%s %s :1 %s\n",
		     acptr->name,me.name,p);
	  fbputs(buff,file);
	}
    }
  fbclose(file);
}

/*
 * parse_server_args
 *
 * inputs	- parv parameters
 * 		- parc count
 *		- info string (to be filled in by this routine)
 *		- hop count (to be filled in by this routine)
 * output	- NULL if invalid params, server name otherwise
 * side effects	- parv[1] is trimmed to HOSTLEN size if needed.
 */

char *parse_server_args(char *parv[], int parc, char *info, int *hop)
{
  int i;
  char *name;

  info[0] = '\0';

  if (parc < 2 || *parv[1] == '\0')
    return NULL;

  *hop = 0;

  name = parv[1];

  if (parc > 3 && atoi(parv[2]))
    {
      *hop = atoi(parv[2]);
      strncpy_irc(info, parv[3], REALLEN);
      info[REALLEN] = '\0';
    }
  else if (parc > 2)
    {
      /*
       * XXX - hmmmm
       */
      strncpy_irc(info, parv[2], REALLEN);
      info[REALLEN] = '\0';
      if ((parc > 3) && ((i = strlen(info)) < (REALLEN - 2)))
        {
          strcat(info, " ");
          strncat(info, parv[3], REALLEN - i - 2);
          info[REALLEN] = '\0';
        }
    }

  if (strlen(name) > HOSTLEN)
    name[HOSTLEN] = '\0';

  return(name);
}

/*
 * bogus_host
 *
 * inputs	- hostname
 * output	- 1 if a bogus hostname input, 0 if its valid
 * side effects	- none
 */
int bogus_host(char *host)
{
  int bogus_server = 0;
  char *s;
  int dots = 0;

  for( s = host; *s; s++ )
    {
      if (!IsServChar(*s))
	{
	  bogus_server = 1;
	  break;
	}
      if ('.' == *s)
	++dots;
    }

  if (!dots || bogus_server )
    return 1;

  return 0;
}
