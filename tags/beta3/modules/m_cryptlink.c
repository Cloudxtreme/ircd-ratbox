/***********************************************************************
 *   IRC - Internet Relay Chat, modules/m_cryptlink.c
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

/*
 * CRYPTLINK protocol.
 *
 * Please see doc/cryptlink.txt for a description of this protocol.
 *
 */

#include <assert.h>

#ifdef OPENSSL
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#endif

#include "tools.h"
#include "memory.h"
#include "handlers.h"
#include "client.h"      /* client struct */
#include "common.h"      /* TRUE bleah */
#include "event.h"
#include "hash.h"        /* add_to_client_hash_table */
#include "md5.h"
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

#ifndef OPENSSL
#ifndef STATIC_MODULES
/* XXX - print error? */
void _modinit(void) {}
void _moddeinit(void) {}
char *_version = "20010409";
#endif
#else

static int bogus_host(char *host);
static char *parse_cryptserv_args(char *parv[], int parc, char *info,
                                  char *key1, char *key2);

static void mr_cryptserv(struct Client*, struct Client*, int, char **);
static void mr_cryptauth(struct Client*, struct Client*, int, char **);
static void m_cryptkey(struct Client*, struct Client*, int, char **);         

struct Message cryptserv_msgtab = {
  "CRYPTSERV", 0, 3, 0, MFLG_SLOW | MFLG_UNREG, 0,
  {mr_cryptserv, m_registered, m_error, m_registered}
};

struct Message cryptauth_msgtab = {
  "CRYPTAUTH", 0, 1, 0, MFLG_SLOW | MFLG_UNREG, 0,
  {mr_cryptauth, m_registered, m_error, m_registered}
};

struct Message cryptkey_msgtab = {
  "CRYPTKEY", 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_ignore, m_cryptkey, m_ignore}
};

#ifndef STATIC_MODULES
void 
_modinit(void)
{
  mod_add_cmd(&cryptserv_msgtab);
  mod_add_cmd(&cryptauth_msgtab);
  mod_add_cmd(&cryptkey_msgtab);                                               
}

void
_moddeinit(void)
{
  mod_del_cmd(&cryptserv_msgtab);
  mod_del_cmd(&cryptauth_msgtab);
  mod_del_cmd(&cryptkey_msgtab);                                               
}

char *_version = "20010412";
#endif

/*
 * mr_cryptauth - CRYPTAUTH message handler
 *      parv[1] = secret key
 */
static void mr_cryptauth(struct Client *client_p, struct Client *source_p,
                         int parc, char *parv[])
{
  char *key;
  
  if (parc < 2 || !IsWaitAuth(client_p))
    return;

  if (!(unbase64_block(&key, parv[1], strlen(parv[1])) == CIPHERKEYLEN))
  {
    sendto_realops_flags(FLAGS_ADMIN,
          "Unauthorized server connection attempt from %s: malformed CRYPTAUTH "
          "response from server %s", get_client_name(client_p, HIDE_IP),
          client_p->name);
    sendto_realops_flags(FLAGS_NOTADMIN,
          "Unauthorized server connection attempt from %s: malformed CRYPTAUTH "
          "response from server %s", get_client_name(client_p, MASK_IP),
          client_p->name);
    exit_client(client_p, client_p, &me, "Malformed CRYPTAUTH reply");
    return;
  }

  if (memcmp(key, client_p->localClient->auth_secret, CIPHERKEYLEN) != 0)
  {
    sendto_realops_flags(FLAGS_ADMIN,
          "Unauthorized server connection attempt from %s: invalid CRYPTAUTH "
          "response from server %s", get_client_name(client_p, HIDE_IP),
          client_p->name);
    sendto_realops_flags(FLAGS_NOTADMIN,
          "Unauthorized server connection attempt from %s: invalid CRYPTAUTH "
          "response from server %s", get_client_name(client_p, MASK_IP),
          client_p->name);
    exit_client(client_p, client_p, &me, "Invalid CRYPTAUTH reply");
    return;
  }

  SetCryptIn(client_p);
  ClearWaitAuth(client_p);
  server_estab(client_p);

#ifdef 0
  {
    char *in, *out;

    base64_block( &in, client_p->localClient->in_key, CIPHERKEYLEN );
    in[22] = '\0';
    base64_block( &out, client_p->localClient->out_key, CIPHERKEYLEN );
    out[22] = '\0';
    sendto_realops_flags(FLAGS_ALL,
          "Encrypted server link established! Keys are "
          "Encrypt: %s -- Decrypt: %s for %s", out, in,
          client_p->name);
    MyFree(in);
    MyFree(out);
  }
#endif
}

/*
 * mr_cryptserv - CRYPTSERV message handler
 *      parv[1] = servername
 *      parv[2] = encrypted sesion key
 *      parv[3] = serverdescription
 */
static void mr_cryptserv(struct Client *client_p, struct Client *source_p,
                         int parc, char *parv[])
{
  char             info[REALLEN + 1];
  char             *name;
  struct Client    *target_p;
  char             key1[CIPHERKEYLEN];
  char             key2[CIPHERKEYLEN];
  char            *b64_key;
  struct ConfItem  *aconf;

  if ( (name = parse_cryptserv_args(parv, parc, info, key1, key2)) == NULL )
    {
      sendto_one(client_p,"ERROR :Invalid parameters");
      return;
    }

  memcpy( client_p->localClient->in_key, key2, CIPHERKEYLEN );

  /* CRYPTSERV support -> TS support */
  client_p->tsinfo = TS_DOESTS;

  if (bogus_host(name))
  {
    exit_client(client_p, client_p, client_p, "Bogus server name");
    return;
  }

  /* Now we just have to call check_server and everything should be
   * checked for us... -A1kmm. */
  switch (check_server(name, client_p, CHECK_SERVER_CRYPTLINK))
    {
     case -1:
      if (ConfigFileEntry.warn_no_nline)
        {
         sendto_realops_flags(FLAGS_ADMIN,
           "Unauthorized server connection attempt from %s: No entry for "
           "servername %s", get_client_name(client_p, HIDE_IP), name);
         sendto_realops_flags(FLAGS_NOTADMIN,
           "Unauthorized server connection attempt from %s: No entry for "
           "servername %s", get_client_name(client_p, MASK_IP), name);
        }
      exit_client(client_p, client_p, client_p, "Invalid servername.");
      return;
      break;
     case -2:
      sendto_realops_flags(FLAGS_ADMIN,
        "Unauthorized server connection attempt from %s: CRYPTLINK not "
        "enabled for server %s", get_client_name(client_p, HIDE_IP), name);
      sendto_realops_flags(FLAGS_NOTADMIN,
        "Unauthorized server connection attempt from %s: CRYPTLINK not "
        "enabled for server %s", get_client_name(client_p, MASK_IP), name);
      exit_client(client_p, client_p, &me, "CRYPTLINK not enabled.");
      return;
      break;
     case -3:
      sendto_realops_flags(FLAGS_ADMIN,
        "Unauthorized server connection attempt from %s: Invalid host "
        "for server %s", get_client_name(client_p, HIDE_IP), name);
      sendto_realops_flags(FLAGS_NOTADMIN,
        "Unauthorized server connection attempt from %s: Invalid host "
        "for server %s", get_client_name(client_p, MASK_IP), name);

      exit_client(client_p, client_p, client_p, "Invalid host.");
      return;
      break;
    }

  if ((target_p = find_server(name)))
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
         get_client_name(client_p, HIDE_IP));
      sendto_realops_flags(FLAGS_NOTADMIN,
         "Attempt to re-introduce server %s from %s", name,
         get_client_name(client_p, MASK_IP));

      sendto_one(client_p, "ERROR :Server already exists.");
      exit_client(client_p, client_p, client_p, "Server Exists");
      return;
    }

  if(ServerInfo.hub && IsCapable(client_p, CAP_LL))
    {
      if(IsCapable(client_p, CAP_HUB))
        {
          ClearCap(client_p,CAP_LL);
          sendto_realops_flags(FLAGS_ALL,
               "*** LazyLinks to a hub from a hub, thats a no-no.");
        }
      else
        {
          client_p->localClient->serverMask = nextFreeMask();

          if(!client_p->localClient->serverMask)
            {
              sendto_realops_flags(FLAGS_ALL,
                                   "serverMask is full!");
              /* try and negotiate a non LL connect */
              ClearCap(client_p,CAP_LL);
            }
        }
    }
  else if (IsCapable(client_p, CAP_LL))
    {
      if(!IsCapable(client_p, CAP_HUB))
        {
          ClearCap(client_p,CAP_LL);
          sendto_realops_flags(FLAGS_ALL,
               "*** LazyLinks to a leaf from a leaf, thats a no-no.");
        }
    }

  aconf = find_conf_name(&client_p->localClient->confs,
                         name, CONF_SERVER);
  if (!aconf)
  {
    sendto_realops_flags(FLAGS_ALL,
                 "Lost C-Line for %s", get_client_name(client_p, HIDE_IP));
    exit_client(client_p, client_p, &me, "Lost C-line");
    return;
  }

  /*
   * if we are connecting (Handshake), we already have the name from the
   * C:line in client_p->name
   */
  strncpy_irc(client_p->name, name, HOSTLEN);
  strncpy_irc(client_p->info, info[0] ? info : me.name, REALLEN);
  client_p->hopcount = 0;

 
  base64_block(&b64_key, key1, CIPHERKEYLEN);
  
  if (IsUnknown(client_p))
     cryptlink_init(client_p, aconf, -1);

  sendto_one(client_p, "CRYPTAUTH %s", b64_key);
  SetCryptOut(client_p);
  MyFree(b64_key);
}

/*
 * m_cryptkey - CRYPTKEY message handler
 *      parv[1] = session key
 */
static void m_cryptkey(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[])
{
  int decoded_len, len;
  char *key;
  char *tmp;

  if (!(parc > 1 && IsCryptOut(client_p) && MyConnect(client_p) &&
        IsCapable(client_p, CAP_CRYPT)))
    return;

  if ( !(decoded_len = unbase64_block(&tmp, parv[1], strlen(parv[1]))) )
  {
    cryptlink_error(client_p, "%s[%s] Key regen failed - couldn't decode key");
    return;
  }

  key = MyMalloc( RSA_size( ServerInfo.rsa_private_key ) );

  len = RSA_private_decrypt( decoded_len, tmp, key,
                             ServerInfo.rsa_private_key,
                             RSA_PKCS1_OAEP_PADDING );

  MyFree(tmp);

  if (len != CIPHERKEYLEN)
  {
    cryptlink_error(client_p, "%s[%s] Key regen failed - couldn't decrypt key");
    MyFree( key );
    return;
  }

  memcpy( client_p->localClient->in_key, key, CIPHERKEYLEN );

  MyFree(key);

#if 0
  {
    char *in, *out;

    base64_block( &in, client_p->localClient->in_key, CIPHERKEYLEN );
    in[22] = '\0';
    base64_block( &out, client_p->localClient->out_key, CIPHERKEYLEN );
    out[22] = '\0';
    sendto_realops_flags(FLAGS_ALL,
          "New decryption key received! Keys are "
          "Encrypt: %s -- Decrypt: %s for %s", out, in,
          client_p->name);
    MyFree(in);
    MyFree(out);
  }
#else
    sendto_realops_flags(FLAGS_ALL,
          "New decryption key received for %s!",
          client_p->name);
#endif
}

/*
 * parse_cryptserv_args
 *
 * inputs	- parv parameters
 * 		- parc count
 *		- info string (to be filled in by this routine)
 *		- key (to be filled in by this routine)
 * output	- NULL if invalid params, server name otherwise
 * side effects	- parv[1] is trimmed to HOSTLEN size if needed.
 */
static char *parse_cryptserv_args(char *parv[], int parc, char *info,
                                  char *key1, char *key2)
{
  char *name;
  char *tmp;
  char *out;
  int len;
  int decoded_len;
  
  info[0] = '\0';

  if (parc < 4 || *parv[3] == '\0')
    return NULL;

  name = parv[1];

  /* parv[2] contains encrypted auth data */
  if ( !(decoded_len = unbase64_block(&tmp, parv[2], strlen(parv[2]))) )
    return NULL;

  if ( (out = MyMalloc( RSA_size( ServerInfo.rsa_private_key ) )) == NULL )
  {
    MyFree(tmp);
    return NULL;
  }

  len = RSA_private_decrypt( decoded_len, tmp, out,
                             ServerInfo.rsa_private_key,
                             RSA_PKCS1_OAEP_PADDING );
 
  MyFree(tmp);

  if ( len < 1 )
  {
    MyFree( out );
    return NULL;
  }

  if ( len != (2 * CIPHERKEYLEN) )
  {
    MyFree( out );
    return NULL;
  }

  memcpy( key1, out, CIPHERKEYLEN );
  memcpy( key2, out + CIPHERKEYLEN, CIPHERKEYLEN );
  MyFree( out );

  strncpy_irc(info, parv[3], REALLEN);
  info[REALLEN] = '\0';

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
static int bogus_host(char *host)
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

#endif
