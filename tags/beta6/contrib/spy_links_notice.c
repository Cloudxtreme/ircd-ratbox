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
show_links(struct hook_links_data *);

void
_modinit(void)
{
  hook_add_hook("doing_links", (hookfn *)show_links);
}

void
_moddeinit(void)
{
  hook_del_hook("doing_links", (hookfn *)show_links);
}

char *_version = "1.0";

/* show a stats request */
int
show_links(struct hook_links_data *data)
{
  if (!MyConnect(data->source_p))
    return 0;
	
  sendto_realops_flags(FLAGS_SPY,
                       "LINKS '%s' requested by %s (%s@%s) [%s]",
                       data->mask, data->source_p->name, data->source_p->username,
                       data->source_p->host, data->source_p->user->server);

  return 0;
}
