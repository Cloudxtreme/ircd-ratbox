/* copyright (c) 2000 Edward Brocklesby, Hybrid Development Team */
/*
 * $Id$
 */

#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

int
show_stats(struct hook_stats_data *);

void
_modinit(void)
{
	hook_add_hook("doing_stats", (hookfn *)show_stats);
}

void
_moddeinit(void)
{
	hook_del_hook("doing_stats", (hookfn *)show_stats);
}

char *_version = "1.0";

/* show a stats request */
int
show_stats(struct hook_stats_data *data)
{
  if((data->statchar == 'L') || (data->statchar == 'l'))
    {
      if(data->name != NULL)
	sendto_realops_flags(FLAGS_SPY, L_ALL,
			     "STATS %c requested by %s (%s@%s) [%s] on %s",
			     data->statchar,
			     data->source_p->name,
			     data->source_p->username,
			     data->source_p->host,
			     data->source_p->user->server,
			     data->name);
      else
	sendto_realops_flags(FLAGS_SPY, L_ALL,
			     "STATS %c requested by %s (%s@%s) [%s]",
			     data->statchar,
			     data->source_p->name,
			     data->source_p->username,
			     data->source_p->host,
			     data->source_p->user->server);
    }
  else
    {
      sendto_realops_flags(FLAGS_SPY, L_ALL,
                           "STATS %c requested by %s (%s@%s) [%s]",
			   data->statchar, data->source_p->name, data->source_p->username,
			   data->source_p->host, data->source_p->user->server);
    }

  return 0;
}
