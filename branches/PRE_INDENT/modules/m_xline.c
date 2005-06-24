/* modules/m_xline.c
 *  Copyright (C) 2002 Hybrid Development Team
 *  Copyright (C) 2002 ircd-ratbox development team
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
#include "tools.h"
#include "handlers.h"
#include "send.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "class.h"
#include "ircd.h"
#include "numeric.h"
#include "memory.h"
#include "s_log.h"
#include "s_serv.h"
#include "whowas.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"

static void mo_xline(struct Client *client_p, struct Client *source_p,
                     int parc, char *parv[]);
static void mo_unxline(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[]);

struct Message xline_msgtab = {
  "XLINE", 0, 0, 3, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, mo_xline}
};

struct Message unxline_msgtab = {
  "UNXLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, mo_unxline}
};

void
_modinit(void)
{
  mod_add_cmd(&xline_msgtab);
  mod_add_cmd(&unxline_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&xline_msgtab);
  mod_del_cmd(&unxline_msgtab);
}

const char *_version = "$Revision$";

/* m_xline()
 *
 * parv[1] - thing to xline
 * parv[2] - optional type/reason
 * parv[3] - reason
 */
void 
mo_xline(struct Client *client_p, struct Client *source_p,
         int parc, char *parv[])
{
  char buffer[BUFSIZE*2];
  FBFILE *out;
  struct xline *xconf;
  const char *filename;
  const char *reason;
  int xtype = 1;

  if(!IsOperXline(source_p))
  {
    sendto_one(source_p, ":%s NOTICE %s :You need xline = yes;",
               me.name, source_p->name);
    return;
  }

  xconf = find_xline(parv[1]);
  if(xconf != NULL)
  {
    sendto_one(source_p, ":%s NOTICE %s :[%s] already X-lined by [%s] - %s",
               me.name, source_p->name, parv[1], xconf->gecos, xconf->reason);
    return;
  }

  if(parc > 3)
  {
    xtype = atoi(parv[2]);
    reason = parv[3];
  }
  else
  {
    if(IsDigit(*parv[2]))
    {
      xtype = atoi(parv[2]);
      reason = "No Reason";
    }
    else
      reason = parv[2];
  }

  xconf = make_xline(parv[1], reason, xtype);
  collapse(xconf->gecos);

  filename = ConfigFileEntry.xlinefile;

  if ((out = fbopen(filename, "a")) == NULL)
  {
    sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem opening %s ", 
                         filename);
    return;
  }

  ircsprintf(buffer, "\"%s\",\"%d\",\"%s\",\"%s\",%lu\n",
             xconf->gecos, xconf->type, xconf->reason,
             get_oper_name(source_p), CurrentTime);

  if (fbputs(buffer, out) == -1)
  {
    sendto_realops_flags(UMODE_ALL, L_ALL, "*** Problem writing to %s",
                         filename);
    fbclose(out);
    return;
  }

  fbclose(out);

  sendto_realops_flags(UMODE_ALL, L_ALL,  "%s added X-line for [%s] [%s]",
                       get_oper_name(source_p), xconf->gecos, xconf->reason);
  sendto_one(source_p, ":%s NOTICE %s :Added X-line for [%s] [%s]",
             me.name, source_p->name, xconf->gecos, xconf->reason);
  ilog(L_TRACE, "%s added X-line for [%s] [%s]",
       get_oper_name(source_p), xconf->gecos, xconf->reason);

  dlinkAddAlloc(xconf, &xline_list);
}

/* mo_unxline()
 *
 * parv[1] - thing to unxline
 */
static void
mo_unxline(struct Client *client_p, struct Client *source_p,
           int parc, char *parv[])
{
  FBFILE *in, *out;
  char buf[BUFSIZE];
  char buff[BUFSIZE];
  char temppath[BUFSIZE];
  const char *filename;
  const char *gecos;
  mode_t oldumask;
  char *p;
  int error_on_write = 0;
  int found_xline = 0;
  
  if(!IsOperXline(source_p))
  {
    sendto_one(source_p, ":%s NOTICE %s :You need xline = yes;",
               me.name, source_p->name);
    return;
  }

  if(BadPtr(parv[1]))
  {
    sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
               me.name, source_p->name, "UNXLINE");
    return;
  }

  filename = ConfigFileEntry.xlinefile;
  ircsprintf(temppath, "%s.tmp", ConfigFileEntry.xlinefile);

  if((in = fbopen(filename, "r")) == NULL)
  {
    sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
               me.name, source_p->name, filename);
    return;
  }

  oldumask = umask(0);

  if((out = fbopen(temppath, "w")) == NULL)
  {
    sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
               me.name, source_p->name, temppath);
    fbclose(in);
    umask(oldumask);
    return;
  }

  umask(oldumask);

  while(fbgets(buf, sizeof(buf), in))
  {
    if(error_on_write)
    {
      if(temppath != NULL)
        (void) unlink(temppath);

      break;
    }

    strlcpy(buff, buf, sizeof(buff));

    if((p = strchr(buff, '\n')) != NULL)
      *p = '\0';

    if((*buff == '\0') || (*buff == '#'))
    {
      error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
      continue;
    }

    if((gecos = getfield(buff)) == NULL)
    {
      error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
      continue;
    }

    /* matching.. */
    if(irccmp(gecos, parv[1]) == 0)
      found_xline++;
    else
      error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
  }

  fbclose(in);
  fbclose(out);

  if(error_on_write)
  {
    sendto_one(source_p,
               ":%s NOTICE %s :Couldn't write temp xline file, aborted",
               me.name, source_p->name);
    return;
  }
  else
  {
    (void) rename(temppath, filename);
    rehash(0);
  }

  if(found_xline == 0)
  {
    sendto_one(source_p, ":%s NOTICE %s :No XLINE for %s",
               me.name, source_p->name, parv[1]);
    return;
  }

  sendto_one(source_p, ":%s NOTICE %s :XLINE for [%s] is removed",
             me.name, source_p->name, parv[1]);
  sendto_realops_flags(UMODE_ALL, L_ALL,
                       "%s has removed the XLINE for: [%s]",
                       get_oper_name(source_p), parv[1]);
  ilog(L_NOTICE, "%s has removed the XLINE for [%s]",
       get_oper_name(source_p), parv[1]);
}
  
