/* copyright (c) 2001 Hybrid Development Team
 *
 * $Id$
 */

#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

int show_info(struct hook_spy_data *);

void
_modinit(void)
{
  hook_add_hook("doing_info", (hookfn *)show_info);
}

void
_moddeinit(void)
{
  hook_del_hook("doing_info", (hookfn *)show_info);
}

char *_version = "1.0";

int show_info(struct hook_spy_data *data)
{
  if (!MyConnect(data->source_p))
    return 0;
	
  sendto_realops_flags(FLAGS_SPY, L_ALL,
                       "info requested by %s (%s@%s) [%s]",
                       data->source_p->name, data->source_p->username,
                       data->source_p->host, data->source_p->user->server);

  return 0;
}
