/*
 *  ircd-ratbox: A slightly useful ircd.
 *  hostmask.c: Code to efficiently find IP & hostmask based configs.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"
#include "memory.h"
#include "ircd_defs.h"
#include "s_conf.h"
#include "hostmask.h"
#include "numeric.h"
#include "send.h"
#include "irc_string.h"

#ifdef IPV6
static unsigned long hash_ipv6(struct sockaddr_storage *, int);
#endif
static unsigned long hash_ipv4(struct sockaddr_storage *, int);


/* int parse_netmask(const char *, struct sockaddr_storage *, int *);
 * Input: A hostmask, or an IPV4/6 address.
 * Output: An integer describing whether it is an IPV4, IPV6 address or a
 *         hostmask, an address(if it is an IP mask),
 *         a bitlength(if it is IP mask).
 * Side effects: None
 */
int
parse_netmask(const char *text, struct sockaddr_storage *naddr, int *nb)
{
	char *ip = LOCAL_COPY(text);
	char *ptr;
	struct sockaddr_storage *addr, xaddr;
	int *b, xb;
	if(nb == NULL)
		b = &xb;
	else
		b = nb;
	
	if(naddr == NULL)
		addr = &xaddr;
	else
		addr = naddr;
	
#ifdef IPV6
	if(strchr(ip, ':'))
	{	
		if((ptr = strchr(ip, '/')))
		{
			*ptr = '\0';
			ptr++;
			*b = atoi(ptr);
		} else
			*b = 128;
		if(inetpton_sock(ip, addr) > 0)
			return HM_IPV6;
		else
			return HM_HOST;
	} else
#endif
	if(strchr(text, '.'))
	{
		if((ptr = strchr(ip, '/')))
		{
			*ptr = '\0';
			ptr++;
			*b = atoi(ptr);
		} else
			*b = 32;
		if(inetpton_sock(ip, addr) > 0)
			return HM_IPV4;
		else
			return HM_HOST;
	}
	return HM_HOST;
}

/* Hashtable stuff...now external as its used in m_stats.c */
struct AddressRec *atable[ATABLE_SIZE];

void
init_host_hash(void)
{
	memset(&atable, 0, sizeof(atable));
}

/* unsigned long hash_ipv4(struct sockaddr_storage*)
 * Input: An IP address.
 * Output: A hash value of the IP address.
 * Side effects: None
 */
static unsigned long
hash_ipv4(struct sockaddr_storage *saddr, int bits)
{
	struct sockaddr_in *addr = (struct sockaddr_in *) saddr;
	
	if(bits != 0)
	{
		unsigned long av = ntohl(addr->sin_addr.s_addr) & ~((1 << (32 - bits)) - 1);
		return (av ^ (av >> 12) ^ (av >> 24)) & (ATABLE_SIZE - 1);
	}

	return 0;
}

/* unsigned long hash_ipv6(struct sockaddr_storage*)
 * Input: An IP address.
 * Output: A hash value of the IP address.
 * Side effects: None
 */
#ifdef IPV6
static unsigned long
hash_ipv6(struct sockaddr_storage *saddr, int bits)
{
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *) saddr;
	unsigned long v = 0, n;
	for (n = 0; n < 16; n++)
	{
		if(bits >= 8)
		{
			v ^= addr->sin6_addr.s6_addr[n];
			bits -= 8;
		}
		else if(bits)
		{
			v ^= addr->sin6_addr.s6_addr[n] & ~((1 << (8 - bits)) - 1);
			return v & (ATABLE_SIZE - 1);
		}
		else
			return v & (ATABLE_SIZE - 1);
	}
	return v & (ATABLE_SIZE - 1);
}
#endif

/* int hash_text(const char *start)
 * Input: The start of the text to hash.
 * Output: The hash of the string between 1 and (TH_MAX-1)
 * Side-effects: None.
 */
static int
hash_text(const char *start)
{
	const char *p = start;
	unsigned long h = 0;

	while (*p)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*p++));
	}
	return (h & (ATABLE_SIZE - 1));
}

/* unsigned long get_hash_mask(const char *)
 * Input: The text to hash.
 * Output: The hash of the string right of the first '.' past the last
 *         wildcard in the string.
 * Side-effects: None.
 */
static unsigned long
get_mask_hash(const char *text)
{
	const char *hp = "", *p;

	for (p = text + strlen(text) - 1; p >= text; p--)
		if(*p == '*' || *p == '?')
			return hash_text(hp);
		else if(*p == '.')
			hp = p + 1;
	return hash_text(text);
}

/* struct ConfItem* find_conf_by_address(const char*, struct sockaddr_storage*,
 *         int type, int fam, const char *username)
 * Input: The hostname, the address, the type of mask to find, the address
 *        family, the username.
 * Output: The matching value with the highest precedence.
 * Side-effects: None
 * Note: Setting bit 0 of the type means that the username is ignored.
 */
struct ConfItem *
find_conf_by_address(const char *name, struct sockaddr_storage *addr, int type,
		     int fam, const char *username)
{
	unsigned long hprecv = 0;
	struct ConfItem *hprec = NULL;
	struct AddressRec *arec;
	int b;

	if(username == NULL)
		username = "";

	if(addr)
	{
		/* Check for IPV6 matches... */
#ifdef IPV6
		if(fam == AF_INET6)
		{

			for (b = 128; b >= 0; b -= 16)
			{
				for (arec = atable[hash_ipv6(addr, b)]; arec; arec = arec->next)
					if(arec->type == (type & ~0x1) &&
					   arec->masktype == HM_IPV6 &&
					   comp_with_mask_sock(addr, &arec->Mask.ipa.addr,
							       arec->Mask.ipa.bits) && (type & 0x1
											||
											match(arec->
											      username,
											      username))
					   && arec->precedence > hprecv)
					{
						hprecv = arec->precedence;
						hprec = arec->aconf;
					}
			}
		}
		else
#endif
		if(fam == AF_INET)
		{
			for (b = 32; b >= 0; b -= 8)
			{
				for (arec = atable[hash_ipv4(addr, b)]; arec; arec = arec->next)
					if(arec->type == (type & ~0x1) &&
					   arec->masktype == HM_IPV4 &&
					   comp_with_mask_sock(addr, &arec->Mask.ipa.addr,
							       arec->Mask.ipa.bits) && (type & 0x1
											||
											match(arec->
											      username,
											      username))
					   && arec->precedence > hprecv)
					{
						hprecv = arec->precedence;
						hprec = arec->aconf;
					}
			}
		}
	}

	if(name != NULL)
	{
		const char *p;
		/* And yes - we have to check p after strchr and p after increment for
		 * NULL -kre */
		for (p = name; p != NULL;)
		{
			for (arec = atable[hash_text(p)]; arec; arec = arec->next)
				if((arec->type == (type & ~0x1)) &&
				   (arec->masktype == HM_HOST) &&
				   match(arec->Mask.hostname, name) &&
				   (type & 0x1 || match(arec->username, username)) &&
				   (arec->precedence > hprecv))
				{
					hprecv = arec->precedence;
					hprec = arec->aconf;
				}
			p = strchr(p, '.');
			if(p != NULL)
				p++;
			else
				break;
		}
		for (arec = atable[0]; arec; arec = arec->next)
			if(arec->type == (type & ~0x1) &&
			   arec->masktype == HM_HOST &&
			   match(arec->Mask.hostname, name) &&
			   (type & 0x1 || match(arec->username, username)) &&
			   arec->precedence > hprecv)
			{
				hprecv = arec->precedence;
				hprec = arec->aconf;
			}
	}
	return hprec;
}

/* find_kline()
 *
 * input        - pointer to client
 * output       -
 * side effects - matching kline is returned, if found
 */
struct ConfItem *
find_kline(struct Client *client_p)
{
	struct ConfItem *aconf;

	aconf = find_conf_by_address(client_p->host, &client_p->localClient->ip,
				     CONF_KILL, client_p->localClient->ip.ss_family,
				     client_p->username);

	return aconf;
}

/* find_gline()
 *
 * input        - pointer to client
 * output       -
 * side effects - matching dline is returned, if found
 */
struct ConfItem *
find_gline(struct Client *client_p)
{
	struct ConfItem *aconf;

	aconf = find_conf_by_address(client_p->host, &client_p->localClient->ip,
				     CONF_GLINE, client_p->localClient->ip.ss_family,
				     client_p->username);

	return aconf;
}

/* struct ConfItem* find_address_conf(const char*, const char*,
 * 	                               struct sockaddr_storage*, int);
 * Input: The hostname, username, address, address family.
 * Output: The applicable ConfItem.
 * Side-effects: None
 */
struct ConfItem *
find_address_conf(const char *host, const char *user, struct sockaddr_storage *ip, int aftype)
{
	struct ConfItem *iconf, *kconf;

	/* Find the best I-line... If none, return NULL -A1kmm */
	if(!(iconf = find_conf_by_address(host, ip, CONF_CLIENT, aftype, user)))
		return NULL;

	/* If they are exempt from K-lines, return the best I-line. -A1kmm */
	if(IsConfExemptKline(iconf))
		return iconf;

	/* Find the best K-line... -A1kmm */
	kconf = find_conf_by_address(host, ip, CONF_KILL, aftype, user);

	/* If they are K-lined, return the K-line */
	if(kconf)
		return kconf;

	/* hunt for a gline */
	if(ConfigFileEntry.glines)
	{
		kconf = find_conf_by_address(host, ip, CONF_GLINE, aftype, user);

		if((kconf != NULL) && !IsConfExemptGline(iconf))
			return kconf;
	}

	return iconf;
}

/* struct ConfItem* find_dline(struct sockaddr_storage*, int)
 * Input: An address, an address family.
 * Output: The best matching D-line or exempt line.
 * Side effects: None.
 */
struct ConfItem *
find_dline(struct sockaddr_storage *addr, int aftype)
{
	struct ConfItem *eline;
	eline = find_conf_by_address(NULL, addr, CONF_EXEMPTDLINE | 1, aftype, NULL);
	if(eline)
		return eline;
	return find_conf_by_address(NULL, addr, CONF_DLINE | 1, aftype, NULL);
}

/* void add_conf_by_address(const char*, int, const char *,
 *         struct ConfItem *aconf)
 * Input: 
 * Output: None
 * Side-effects: Adds this entry to the hash table.
 */
void
add_conf_by_address(const char *address, int type, const char *username, struct ConfItem *aconf)
{
	static unsigned long prec_value = 0xFFFFFFFF;
	int masktype, bits;
	unsigned long hv;
	struct AddressRec *arec;

	if(address == NULL)
		address = "/NOMATCH!/";
	arec = MyMalloc(sizeof(struct AddressRec));
	masktype = parse_netmask(address, &arec->Mask.ipa.addr, &bits);
	arec->Mask.ipa.bits = bits;
	arec->masktype = masktype;
#ifdef IPV6
	if(masktype == HM_IPV6)
	{
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		bits -= bits % 16;
		arec->next = atable[(hv = hash_ipv6(&arec->Mask.ipa.addr, bits))];
		atable[hv] = arec;
	}
	else
#endif
	if(masktype == HM_IPV4)
	{
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		bits -= bits % 8;
		arec->next = atable[(hv = hash_ipv4(&arec->Mask.ipa.addr, bits))];
		atable[hv] = arec;
	}
	else
	{
		arec->Mask.hostname = address;
		arec->next = atable[(hv = get_mask_hash(address))];
		atable[hv] = arec;
	}
	arec->username = username;
	arec->aconf = aconf;
	arec->precedence = prec_value--;
	arec->type = type;
}

/* void delete_one_address(const char*, struct ConfItem*)
 * Input: An address string, the associated ConfItem.
 * Output: None
 * Side effects: Deletes an address record. Frees the ConfItem if there
 *               is nothing referencing it, sets it as illegal otherwise.
 */
void
delete_one_address_conf(const char *address, struct ConfItem *aconf)
{
	int masktype, bits;
	unsigned long hv;
	struct AddressRec *arec, *arecl = NULL;
	struct sockaddr_storage addr;
	masktype = parse_netmask(address, &addr, &bits);
#ifdef IPV6
	if(masktype == HM_IPV6)
	{
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		bits -= bits % 16;
		hv = hash_ipv6(&addr, bits);
	}
	else
#endif
	if(masktype == HM_IPV4)
	{
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		bits -= bits % 8;
		hv = hash_ipv4(&addr, bits);
	}
	else
		hv = get_mask_hash(address);
	for (arec = atable[hv]; arec; arec = arec->next)
	{
		if(arec->aconf == aconf)
		{
			if(arecl)
				arecl->next = arec->next;
			else
				atable[hv] = arec->next;
			aconf->status |= CONF_ILLEGAL;
			if(!aconf->clients)
				free_conf(aconf);
			MyFree(arec);
			return;
		}
		arecl = arec;
	}
}

/* void clear_out_address_conf(void)
 * Input: None
 * Output: None
 * Side effects: Clears out all address records in the hash table,
 *               frees them, and frees the ConfItems if nothing references
 *               them, otherwise sets them as illegal.
 */
void
clear_out_address_conf(void)
{
	int i;
	struct AddressRec **store_next;
	struct AddressRec *arec, *arecn;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		store_next = &atable[i];
		for (arec = atable[i]; arec; arec = arecn)
		{
			arecn = arec->next;
			/* We keep the temporary K-lines and destroy the
			 * permanent ones, just to be confusing :) -A1kmm */
			if(arec->aconf->flags & CONF_FLAGS_TEMPORARY)
			{
				*store_next = arec;
				store_next = &arec->next;
			}
			else
			{
				arec->aconf->status |= CONF_ILLEGAL;
				if(!arec->aconf->clients)
					free_conf(arec->aconf);
				MyFree(arec);
			}
		}
		*store_next = NULL;
	}
}


/*
 * show_iline_prefix()
 *
 * inputs       - pointer to struct Client requesting output
 *              - pointer to struct ConfItem 
 *              - name to which iline prefix will be prefixed to
 * output       - pointer to static string with prefixes listed in ascii form
 * side effects - NONE
 */
char *
show_iline_prefix(struct Client *sptr, struct ConfItem *aconf, char *name)
{
	static char prefix_of_host[USERLEN + 15];
	char *prefix_ptr;

	prefix_ptr = prefix_of_host;
	if(IsNoTilde(aconf))
		*prefix_ptr++ = '-';
	if(IsLimitIp(aconf))
		*prefix_ptr++ = '!';
	if(IsNeedIdentd(aconf))
		*prefix_ptr++ = '+';
	if(IsPassIdentd(aconf))
		*prefix_ptr++ = '$';
	if(IsNoMatchIp(aconf))
		*prefix_ptr++ = '%';
	if(IsConfDoSpoofIp(aconf))
		*prefix_ptr++ = '=';
	if(MyOper(sptr) && IsConfExemptKline(aconf))
		*prefix_ptr++ = '^';
	if(MyOper(sptr) && IsConfExemptLimits(aconf))
		*prefix_ptr++ = '>';
	if(MyOper(sptr) && IsConfIdlelined(aconf))
		*prefix_ptr++ = '<';
	*prefix_ptr = '\0';
	strncpy(prefix_ptr, name, USERLEN);
	return (prefix_of_host);
}

/* report_auth()
 *
 * Inputs: pointer to client to report to
 * Output: None
 * Side effects: Reports configured auth{} blocks to client_p
 */
void
report_auth(struct Client *client_p)
{
	char *name, *host, *pass, *user, *classname;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i, port;

	for (i = 0; i < ATABLE_SIZE; i++)
		for (arec = atable[i]; arec; arec = arec->next)
			if(arec->type == CONF_CLIENT)
			{
				aconf = arec->aconf;

				if(!MyOper(client_p) && IsConfDoSpoofIp(aconf))
					continue;

				get_printable_conf(aconf, &name, &host, &pass, &user, &port,
						   &classname);

				sendto_one(client_p, form_str(RPL_STATSILINE), me.name,
					   client_p->name, (IsConfRestricted(aconf)) ? 'i' : 'I',
					   name, show_iline_prefix(client_p, aconf, user),
#ifdef HIDE_SPOOF_IPS
					   IsConfDoSpoofIp(aconf) ? "255.255.255.255" :
#endif
					   host, port, classname);
			}
}

/* report_Klines()
 * 
 * inputs       - Client to report to, mask 
 * outputs      -
 * side effects - Reports configured K-lines to client_p.
 */
void
report_Klines(struct Client *source_p)
{
	char *host, *pass, *user, *oper_reason;
	struct AddressRec *arec;
	struct ConfItem *aconf = NULL;
	int i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_KILL)
			{
				aconf = arec->aconf;

				/* its a tempkline, theyre reported elsewhere */
				if(aconf->flags & CONF_FLAGS_TEMPORARY)
					continue;

				get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);
				sendto_one(source_p, form_str(RPL_STATSKLINE), me.name,
					   source_p->name, 'K', host, user, pass,
					   oper_reason ? "|" : "",
					   oper_reason ? oper_reason : "");
			}
		}
	}
}
