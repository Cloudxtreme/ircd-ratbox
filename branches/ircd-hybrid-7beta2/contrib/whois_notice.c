/* copyright (c) 2000 Edward Brocklesby, Hybrid Development Team */

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
   sptr does a /whois on cptr */
int
show_notice(struct hook_mfunc_data *data)
{
	if (MyConnect(data->sptr) && MyConnect(data->cptr) &&
		IsOper(data->cptr) && (data->cptr != data->sptr) 
		&& data->cptr->umodes & FLAGS_SPY) 
	{
		sendto_one(data->cptr, ":%s NOTICE %s :*** Notice -- %s (%s@%s) is doing a /whois on you",
				   me.name, data->cptr->name, data->sptr->name, data->sptr->username, data->sptr->host);
	}
	return 0;
}
