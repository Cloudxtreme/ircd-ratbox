/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * cluster.c: The code for handling kline clusters
 *
 * Copyright (C) 2002 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#include "stdinc.h"
#include "ircd_defs.h"
#include "cluster.h"
#include "tools.h"
#include "client.h"
#include "memory.h"
#include "s_serv.h"
#include "send.h"

dlink_list cluster_list;

struct cluster *
make_cluster(void)
{
  struct cluster *clptr;

  clptr = (struct cluster *) MyMalloc(sizeof(struct cluster));
  memset(clptr, 0, sizeof(struct cluster));

  return clptr;
}

void
free_cluster(struct cluster *clptr)
{
  assert(clptr != NULL);
  if(clptr == NULL)
    return;

  MyFree(clptr->name);
  MyFree((char *) clptr);
}

void
clear_clusters(void)
{
  dlink_node *ptr;
  dlink_node *next_ptr;

  DLINK_FOREACH_SAFE(ptr, next_ptr, cluster_list.head)
  {
    free_cluster(ptr->data);
    dlinkDestroy(ptr, &cluster_list);
  }
}

int
find_cluster(const char *name, int type)
{
  struct cluster *clptr;
  dlink_node *ptr;

  DLINK_FOREACH(ptr, cluster_list.head)
  {
    clptr = ptr->data;

    if(match(clptr->name, name) && clptr->type & type)
      return 1;
  }

  return 0;
}

void
cluster_kline(struct Client *source_p, int tkline_time, const char *user, 
              const char *host, const char *reason)
{
  struct cluster *clptr;
  dlink_node *ptr;

  DLINK_FOREACH(ptr, cluster_list.head)
  {
    clptr = ptr->data;

    if(clptr->type & CLUSTER_KLINE)
      sendto_match_servs(source_p, clptr->name, CAP_KLN,
                         "KLINE %s %d %s %s :%s",
                         clptr->name, tkline_time, user, host, reason);
  }
}

void
cluster_unkline(struct Client *source_p, const char *user, const char *host)
{
  struct cluster *clptr;
  dlink_node *ptr;

  DLINK_FOREACH(ptr, cluster_list.head)
  {
    clptr = ptr->data;

    if(clptr->type & CLUSTER_UNKLINE)
      sendto_match_servs(source_p, clptr->name, CAP_UNKLN,
                         "UNKLINE %s %s %s",
                         clptr->name, user, host);
  }
}

void
cluster_locops(struct Client *source_p, const char *message)
{
  struct cluster *clptr;
  dlink_node *ptr;

  DLINK_FOREACH(ptr, cluster_list.head)
  {
    clptr = ptr->data;

    if(clptr->type & CLUSTER_LOCOPS)
      sendto_match_servs(source_p, clptr->name, CAP_CLUSTER,
                         "LOCOPS %s :%s",
                         clptr->name, message);
  }
}

