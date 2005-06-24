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
#include <errno.h>
#include "../adns/internal.h"
#define ADNS_MAXFD 2

adns_state dns_state;


void delete_adns_queries(struct DNSQuery *q)
{
	if(q != NULL)
	{
		if(q->query != NULL)
		{
			q->query->ads = dns_state;
			adns_cancel(q->query);
		}
	}
	q->query = 0xf00dbeef;
}              	        

void restart_resolver(void)
{
	int err;
	adns_state new_state;
	if(!(err = adns_init(&new_state, adns_if_noautosys, 0))) {
		adns_finish(dns_state);	
		dns_state = new_state;
	} else {
		log(L_NOTICE, "Error rehashing DNS %s...using old settings", strerror(err));		
		sendto_realops_flags(FLAGS_ADMIN, "Error rehashing DNS %s...using old setting", strerror(err));
	}
	dns_select();
}
void init_resolver(void)
{
	adns_init(&dns_state, adns_if_noautosys, 0);	
	eventAdd("timeout_adns", timeout_adns, NULL, 1, 0);
	dns_select();
}

void timeout_adns(void *ptr)
{
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
	adns_query q;
	adns_answer *answer;
	struct DNSQuery *query;
	adns_forallqueries_begin(dns_state);
	while((q = adns_forallqueries_next(dns_state, (void **)&q)) != NULL)
	{
		q->ads = dns_state;
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
