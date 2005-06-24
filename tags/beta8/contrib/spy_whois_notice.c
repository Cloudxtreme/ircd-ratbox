/* copyright (c) 2000 Edward Brocklesby, Hybrid Development Team
 *
 * $Id$
 */

#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

int
show_notice(struct hook_mfunc_data *);

void
_modinit(void)
{
  hook_add_hook("doing_whois", (hookfn *)show_notice);
}

void
_moddeinit(void)
{
  hook_del_hook("doing_whois", (hookfn *)show_notice);
}

char *_version = "1.0";

/* show a whois notice
   source_p does a /whois on client_p */
int
show_notice(struct hook_mfunc_data *data)
{
  if (MyConnect(data->source_p) && MyConnect(data->client_p) &&
      IsOper(data->client_p) && (data->client_p != data->source_p) 
      && data->client_p->umodes & FLAGS_SPY) 
    {
      sendto_one(data->client_p, ":%s NOTICE %s :*** Notice -- %s (%s@%s) is doing a whois on you",
                 me.name, data->client_p->name, data->source_p->name, data->source_p->username,
                 data->source_p->host);
    }

  return 0;
}
