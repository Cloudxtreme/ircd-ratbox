/* modules/m_xline.c
 * Copyright (C) 2002 Hybrid Development Team
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

struct Message xline_msgtab = {
  "XLINE", 0, 0, 3, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_ignore, mo_xline}
};

void
_modinit(void)
{
  mod_add_cmd(&xline_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&xline_msgtab);
}

char *_version = "$Revision$";

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
  struct ConfItem *aconf;
  char *reason;
  int xtype = 1;

  if(!IsOperXline(source_p))
  {
    sendto_one(source_p, ":%s NOTICE %s :You need xline = yes;",
               me.name, source_p->name);
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

  aconf = make_conf();
  aconf->status = CONF_XLINE;
  DupString(aconf->host, parv[1]);
  DupString(aconf->passwd, reason);
  aconf->port = xtype;

  collapse(aconf->host);

  /* conf_add_x_conf must be done last, due to it messing about
   * with aconf --fl
   */
  write_confitem(XLINE_TYPE, source_p, NULL, aconf->host, reason,
                 NULL, NULL, aconf->port);
  conf_add_x_conf(aconf);
}
