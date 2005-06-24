/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_unkline.c: Unklines a user.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"
#include "tools.h"
#include "handlers.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "fileio.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_log.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"
#include "cluster.h"

static void mo_undline(struct Client*, struct Client*, int, char**);

struct Message undline_msgtab = {
  "UNDLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_error, mo_undline}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&undline_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&undline_msgtab);
}
const char *_version = "$Revision$";
#endif

static int flush_write(struct Client *, FBFILE* , char *, char *);
static int remove_temp_dline(char *);

/* mo_undline()
 *
 *      parv[0] = sender nick
 *      parv[1] = dline to remove
 */
static void
mo_undline(struct Client *client_p, struct Client *source_p,
           int parc, char *parv[])
{
  FBFILE* in;
  FBFILE* out;
  char  buf[BUFSIZE], buff[BUFSIZE], temppath[BUFSIZE], *p;
  const char  *filename, *found_cidr;
  char *cidr;
  int pairme = NO, error_on_write = NO;
  mode_t oldumask;

  ircsprintf(temppath, "%s.tmp", ConfigFileEntry.dlinefile);

  if (!IsOperUnkline(source_p))
  {
    sendto_one(source_p, ":%s NOTICE %s :You need unkline = yes;",
               me.name, parv[0]);
    return;
  }

  cidr = parv[1];

  if(remove_temp_dline(cidr))
  {
    sendto_one(source_p,
	       ":%s NOTICE %s :Un-dlined [%s] from temporary D-lines",
	       me.name, parv[0], cidr);
    sendto_realops_flags(UMODE_ALL, L_ALL,
                         "%s has removed the temporary D-Line for: [%s]",
			 get_oper_name(source_p), cidr);
    ilog(L_NOTICE, "%s removed temporary D-Line for [%s]", parv[0], cidr);
    return;
  }

  filename = get_conf_name(DLINE_TYPE);

  if ((in = fbopen(filename, "r")) == 0)
  {
    sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
               me.name, parv[0], filename);
    return;
  }

  oldumask = umask(0);
  if ((out = fbopen(temppath, "w")) == 0)
  {
    sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
               me.name, parv[0], temppath);
    fbclose(in);
    umask(oldumask);
    return;
  }

  umask(oldumask);

  while(fbgets(buf, sizeof(buf), in))
  {
    strlcpy(buff, buf, sizeof(buff));

    if ((p = strchr(buff,'\n')) != NULL)
      *p = '\0';

    if ((*buff == '\0') || (*buff == '#'))
    {
      if(!error_on_write)
        flush_write(source_p, out, buf, temppath);
      continue;
    }

    if ((found_cidr = getfield(buff)) == NULL)
    {
      if(!error_on_write)
        flush_write(source_p, out, buf, temppath);
      continue;
    }

    if (irccmp(found_cidr,cidr) == 0)
    {
      pairme++;
    }
    else
    {
      if(!error_on_write)
        flush_write(source_p, out, buf, temppath);
      continue;
    }
  }

  fbclose(in);
  fbclose(out);

  if (!error_on_write)
  {
    (void) rename(temppath, filename);
    rehash(0);
  }
  else
  {
    sendto_one(source_p,
               ":%s NOTICE %s :Couldn't write D-line file, aborted",
               me.name, parv[0]);
    return;
  }

  if (!pairme)
  {
    sendto_one(source_p, ":%s NOTICE %s :No D-Line for %s", me.name,
               parv[0], cidr);
    return;
  }

  sendto_one(source_p, ":%s NOTICE %s :D-Line for [%s] is removed",
	     me.name, parv[0], cidr);
  sendto_realops_flags(UMODE_ALL, L_ALL, 
                       "%s has removed the D-Line for: [%s]",
		       get_oper_name(source_p), cidr);
  ilog(L_NOTICE, "%s removed D-Line for [%s]", 
       get_oper_name(source_p), cidr);
}

/*
 * flush_write()
 *
 * inputs       - pointer to client structure of oper requesting unkline
 *              - out is the file descriptor
 *              - buf is the buffer to write
 *              - ntowrite is the expected number of character to be written
 *              - temppath is the temporary file name to be written
 * output       - YES for error on write
 *              - NO for success
 * side effects - if successful, the buf is written to output file
 *                if a write failure happesn, and the file pointed to
 *                by temppath, if its non NULL, is removed.
 *
 * The idea here is, to be as robust as possible when writing to the 
 * kline file.
 *
 * -Dianora
 */

static int flush_write(struct Client *source_p, FBFILE* out, char *buf,
		       char *temppath)
{
  int error_on_write = (fbputs(buf, out) < 0) ? YES : NO;

  if (error_on_write)
    {
      sendto_one(source_p,":%s NOTICE %s :Unable to write to %s",
        me.name, source_p->name, temppath );
      fbclose(out);
      if(temppath != NULL)
        (void)unlink(temppath);
    }
  return(error_on_write);
}

static dlink_list *tdline_list[] =
{
  &tdline_hour,
  &tdline_day,
  &tdline_min,
  &tdline_week,
  NULL
};

/* remove_temp_dline()
 *
 * inputs       - hostname to undline
 * outputs      -
 * side effects - tries to undline anything that matches
 */
static int
remove_temp_dline(char *host)
{
  dlink_list *tdlist;
  struct ConfItem *aconf;
  dlink_node *ptr;
  struct irc_inaddr addr, caddr;
  int nm_t, cnm_t, bits, cbits;
  int i;

  nm_t = parse_netmask(host, &addr, &bits);

  for(i = 0; tdline_list[i] != NULL; i++)
  {
    tdlist = tdline_list[i];

    DLINK_FOREACH(ptr, tdlist->head)
    {
      aconf = ptr->data;

      cnm_t = parse_netmask(aconf->host, &caddr, &cbits);

      if (cnm_t != nm_t)
        continue;

      if ((nm_t==HM_HOST && !irccmp(aconf->host, host)) ||
          (nm_t==HM_IPV4 && bits==cbits && comp_with_mask(&IN_ADDR(addr), &IN_ADDR(caddr), bits))
#ifdef IPV6
          || (nm_t==HM_IPV6 && bits==cbits && comp_with_mask(&IN_ADDR(addr), &IN_ADDR(caddr), bits))
#endif
          )
      {
        dlinkDestroy(ptr, tdlist);
        delete_one_address_conf(aconf->host, aconf);
        return YES;
      }
    }
  }

  return NO;
}

