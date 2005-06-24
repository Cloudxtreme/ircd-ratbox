/*
 * A slightly nicer version of dline_conf.c
 * Written by Aaron Sethman <androsyn@ratbox.org>
 */

#include "config.h"
#include "dline_conf.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_log.h"
#include "send.h"
#include "memory.h"
#include "patricia.h"


patricia_tree_t *dline;
patricia_tree_t *eline;
patricia_tree_t *kline;

void zap_Dlines(void)
{
#ifdef IPV6
	dline = New_Patricia(128);
	eline = New_Patricia(128);	
	kline = New_Patricia(128);
#else
	dline = New_Patricia(32);
	eline = New_Patricia(32);
	kline =  New_Patricia(32);
#endif
}

struct ConfItem *match_Dline(struct irc_inaddr *ip)
{
	patricia_node_t *node;
	node = match_ip(eline, ip);
	if(node != NULL)
		return(NULL); // Well we are an exception..
		
	node = match_ip(dline, ip);
	if(node != NULL)
		return(node->data);
	else
		return(NULL);
}

void clear_Dline_table(void)
{
	if(dline != NULL)
		Destroy_Patricia(dline, NULL);
	if(dline != NULL)
	Destroy_Patricia(eline, NULL);
	if(dline != NULL)
		Destroy_Patricia(kline, NULL);
	zap_Dlines();
}
struct ConfItem *match_ip_Kline(struct irc_inaddr *ip, const char *name)
{
	patricia_node_t *node;
	node = match_ip(kline, ip);

	if(node != NULL)
	{
		if(match(node->data->user, name))
		{
			return(node->data);
		}
	}
	return(NULL);
}
void report_ip_Klines(struct Client *sptr)
{
	patricia_node_t *node;
	char *name, *host, *pass, *user, *classname;
	char conftype = 'K';
	int port;

	PATRICIA_WALK(kline->head, node) 
	{
		get_printable_conf(node->data, &name, &host, &pass, &user, &port, &classname);
		sendto_one(sptr, form_str(RPL_STATSKLINE), me.name, sptr->name, conftype, host, user, pass);	
	} PATRICIA_WALK_END;
}

void add_Dline(struct ConfItem *conf_ptr)
{
        patricia_node_t *node;
	conf_ptr->status = CONF_DLINE;
	node = make_and_lookup(dline, conf_ptr->host);
	node->data = conf_ptr;
}

void add_Eline(struct ConfItem *conf_ptr)
{
	patricia_node_t *node;
	conf_ptr->status = CONF_DLINE;
	conf_ptr->flags |= CONF_FLAGS_E_LINED;
	node = make_and_lookup(eline, conf_ptr->host);
	node->data = conf_ptr;
}

void add_ip_Kline(struct ConfItem *conf_ptr)
{
	patricia_node_t *node;
	node = make_and_lookup(kline, conf_ptr->host);
	node->data = conf_ptr;	
}

void report_dlines(struct Client *sptr)
{
	patricia_node_t *node;
	char *name, *host, *pass, *user, *classname, conftype;
	int port;
	PATRICIA_WALK(dline->head, node) 
	{
		if(IsConfElined(node->data))
			conftype = 'd';
		else
			conftype = 'D';
		get_printable_conf(node->data, &name, &host, &pass, &user, &port, &classname);
		sendto_one(sptr, form_str(RPL_STATSDLINE), me.name, sptr->name, conftype, host, pass);	
	} PATRICIA_WALK_END;
}
