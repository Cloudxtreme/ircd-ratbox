/*
 * $Id$
 * adns.c  functions to enter libadns 
 *
 * Written by Aaron Sethman <androsyn@ratbox.org>
 */
#include "fileio.h"
#include "res.h"
#include "send.h"
#include "s_bsd.h"
#include "s_log.h"
#include "event.h"
#include "client.h"
#include "ircd_defs.h"
#include "numeric.h"
#include "blalloc.h"
#include <errno.h>
#include "../adns/internal.h"
#define ADNS_MAXFD 2

adns_state dns_state;
BlockHeap *dns_blk;

void report_adns_servers(struct Client *sptr)
{
	int x;
	char buf[16]; /* XXX: adns only deals with ipv4 dns servers so this is okay */
	for(x = 0; x < dns_state->nservers; x++)	
	{
		inetntop(AF_INET, &dns_state->servers[x].addr.s_addr, buf, 16);
 		sendto_one(sptr, form_str(RPL_STATSALINE), me.name, sptr->name, buf); 
	}
}

void delete_adns_queries(struct DNSQuery *q)
{
	if(q != NULL)
	{
		if(q->query != NULL)
		{
			adns_cancel(q->query);
		}
	}
}              	        

void restart_resolver(void)
{
	adns__rereadconfig(dns_state);
}
void init_resolver(void)
{

	dns_blk = BlockHeapCreate(sizeof(struct DNSQuery), DNS_BLOCK_SIZE);
	adns_init(&dns_state, adns_if_noautosys, 0);	
	eventAdd("timeout_adns", timeout_adns, NULL, 1, 0);
	dns_select();
}

void timeout_adns(void *ptr)
{
	struct timeval now;
	gettimeofday(&now, 0);
	adns_processtimeouts(dns_state, &now); 
	eventAdd("timeout_adns", timeout_adns, NULL, 1, 0);
}



void dns_writeable(int fd, void *ptr)
{
	struct timeval now;
	gettimeofday(&now, 0);
	adns_processwriteable(dns_state, fd, &now);	
	dns_select();
}

void dns_do_callbacks(void)
{
	adns_query q, r;
	adns_answer *answer;
	struct DNSQuery *query;
	adns_forallqueries_begin(dns_state);
	while((q = adns_forallqueries_next(dns_state, (void **)&r)) != NULL)
	{
		switch(adns_check(dns_state, &q, &answer, (void **)&query))
		{
			case 0:
				/* Looks like we got a winner */			
				query->query = NULL;
				query->callback(query->ptr, answer);
				break;
			case EAGAIN:
				/* Go into the queue again */
				break;;
			default:
				/* Awww we failed, what a shame */
				query->query = NULL;
				query->callback(query->ptr, NULL);		
				break;
		} 
	}
}

void dns_readable(int fd, void *ptr)
{
	struct timeval now;
	gettimeofday(&now, 0);
	adns_processreadable(dns_state, fd, &now);	
	dns_do_callbacks();
	dns_select();
}

void dns_select(void)
{
	struct adns_pollfd pollfds[MAXFD_POLL];
	int npollfds, i, fd;
	adns__consistency(dns_state,0,cc_entex);
	npollfds = adns__pollfds(dns_state, pollfds);
	for(i = 0; i < npollfds; i++)
	{
		fd = pollfds[i].fd;
		if(pollfds[i].events & ADNS_POLLIN) 
			comm_setselect(fd, FDLIST_SERVER, COMM_SELECT_READ, dns_readable, NULL, 0);
		if(pollfds[i].events & ADNS_POLLOUT)
			comm_setselect(fd, FDLIST_SERVICE, COMM_SELECT_WRITE, dns_writeable, NULL, 0);
	}		 
	/*  Give the dns_callbacks some time 
	 *  Here is better than never, 
	 *  as we are likely to have returned something
	 */
	dns_do_callbacks();
}


void adns_gethost(const char *name, int aftype, struct DNSQuery *req)
{
#ifdef IPV6	
	if(aftype == AF_INET6)
		adns_submit(dns_state, name, adns_r_addr6, adns_qf_owner, req, &req->query);
	else
		adns_submit(dns_state, name, adns_r_addr, adns_qf_owner, req, &req->query);
#else
		adns_submit(dns_state, name, adns_r_addr, adns_qf_owner, req, &req->query);
#endif

}


void adns_getaddr(struct irc_inaddr *addr, int aftype, struct DNSQuery *req)
{
	struct irc_sockaddr ipn;
#ifdef IPV6
	if(aftype == AF_INET6)
	{
		ipn.sins.sin6.sin6_family = AF_INET6;
		ipn.sins.sin6.sin6_port = 0;
		memcpy(&ipn.sins.sin6.sin6_addr.s6_addr, &addr->sins.sin6.s6_addr, sizeof(struct in6_addr));
		adns_submit_reverse(dns_state, (struct sockaddr *)&ipn.sins.sin6,
			adns_r_ptr_ip6, adns_qf_owner, req, &req->query);

	} else
	{
		ipn.sins.sin.sin_family = AF_INET;
		ipn.sins.sin.sin_port = 0;
		ipn.sins.sin.sin_addr.s_addr = addr->sins.sin.s_addr;	
		adns_submit_reverse(dns_state, (struct sockaddr *)&ipn.sins.sin,
			adns_r_ptr, adns_qf_owner, req, &req->query);

	}
#else
	ipn.sins.sin.sin_family = AF_INET;
	ipn.sins.sin.sin_port = 0;
	ipn.sins.sin.sin_addr.s_addr = addr->sins.sin.s_addr;	
	adns_submit_reverse(dns_state, (struct sockaddr *)&ipn.sins.sin,
		adns_r_ptr, adns_qf_owner, req, &req->query);
#endif	
}
