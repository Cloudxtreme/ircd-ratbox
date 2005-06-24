/***********************************************************************
 *   ircd-hybrid project - Internet Relay Chat
 *  hostmask.c: Find hosts for klines and auth{} etc...
 *  All parts of this program are Copyright(C) 2001(or later).
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * $Id$ 
 */
#include <unistd.h>
#include <string.h>
#include "irc_string.h"
#include "memory.h"
#include "s_conf.h"
#include "hostmask.h"
#include "sprintf_irc.h"
#include "send.h"
#include "numeric.h"
#include "dline_conf.h"
#include "ircd.h"
#include <assert.h>

#define TH_MAX 0x1000

static struct HostMaskEntry *first_miscmask = NULL, *first_mask = NULL;
static struct HostMaskEntry *hmhash[TH_MAX-1];
static unsigned long precedence = 0xFFFFFFF;

/* int hash_text(const char *start)
 * Input: The start of the text to hash.
 * Output: The hash of the string between 1 and (TH_MAX-1)
 * Side-effects: None.
 */
static int
hash_text(const char* start)
{
  register const char *p = start;
  register unsigned long h = 0;
  while (*p)
    {
      h = (h << 4) - (h + (unsigned char)ToLower(*p++));
    }
  return(h & (TH_MAX - 1));
}

/* int get_uhosthash(const char *uhost)
 * Input: A host-name, optionally with wildcards.
 * Output: The hash of the string from the start of the first . or @
 *         delimited token which contains no wildcards in or to the right
 *         of.
 * Side-effects: None.
 * Note: A return value of -1 means the host cannot be hashed and must
 *       be placed on the misc list.
 */
static int
get_uhosthash(const char *uhost)
{
  const char *end, *lastdot = NULL;
  int sig = 1;
  char c = 0;

  if (!*uhost)
    return 0;

  end = uhost + strlen(uhost) - 1;
  while (sig && end >= uhost && (c = *end--))
    switch (c)
      {
      case '*':
      case '?':
       sig--;
       break;
      case '!':
      case '@':
      case '.':
       lastdot = end+2;
     }

  if ((c == '?' || c == '*') && (!lastdot || *lastdot == 0))
    return -1;
  
  if (end < uhost)
    return hash_text(uhost);

  return hash_text(lastdot ? lastdot : uhost);
}

/* void add_hostmask(const char *mask, int type, void *data)
 * Input: The mask, the type, a data value applicable to the type.
 * Output: -
 * Side-effects: Creates a HostMaskEntry for the mask and adds it to the
 *               hashtable.
 */
void
add_hostmask(const char *mask, int type, void *data)
{
  struct HostMaskEntry *hme;
  int hash = get_uhosthash(mask);
  hme = MyMalloc(sizeof(*hme));

  hme->data = data;
  /* Just an ugly kludge so first entry in the conf file matches. 
   * Also so K-lines overrule I lines...
   */

  if (type == HOST_CONFITEM &&
      (((struct ConfItem*)data)->status & CONF_KILL))
    hme->precedence = 0xFFFFFFFF;
  else
    hme->precedence = precedence--;

  DupString(hme->hostmask, mask);
  hme->type = type;
  hme->next = first_mask;
  first_mask = hme;

  if (hash != -1)
    {
      hme->nexthash = hmhash[hash];
      hmhash[hash] = hme;
    }
  else
    {
      hme->nexthash = first_miscmask;
      first_miscmask = hme;
    }
}

/* const char * strcchr(const char *a, const char *b)
 *
 * Inputs:	a, the string to search in
 *		b, the characters to search for.
 * Output:	A pointer to character past the first occurrence of any
 *         	character from b in a, or NULL.
 * Side-effects: - none
 */
static const char*
strcchr(const char *a, const char *b)
{
  const char *p = a, *q;
  char c, d;
  while ((c = *p++))
    {
      q = b;
      while ((d = *q++))
	if (c == d)
	  return p;
    }
  return NULL;
}

/* struct HostMaskEntry *match_hostmask(const char *uhost, int type)
 *
 * Inputs:	a uhost to match
 *		and a type.
 * Output: 	a HostMaskEntry if one found, or NULL.
 * Side-effects: -
 */
struct HostMaskEntry*
match_hostmask(const char *uhost, int type)
{
  struct HostMaskEntry *misc_hme = NULL;
  struct HostMaskEntry *hme = NULL;
  struct HostMaskEntry *hmk = NULL;
  struct HostMaskEntry *hmc = NULL;
  unsigned long prec = 0;
  unsigned int hash;
  const char *pos;

  /* Look for a match amongst the misc. link list
   * keep a pointer to the best match if found
   */
  for (hme = first_miscmask; hme; hme = hme->nexthash)
    {
      if (hme->type == type && match(hme->hostmask, uhost) &&
	  hme->precedence > prec)
	{
	  prec = hme->precedence;
	  misc_hme = hme;
	}
    }

  /* Now try the hash tree... */

  for (pos = uhost; pos; pos = strcchr(pos, "@!."))
    {
      hash = hash_text(pos);
      for (hme = hmhash[hash]; hme; hme=hme->nexthash)
	if (hme->type == type && match(hme->hostmask, uhost) &&
	    hme->precedence > prec)
	  {
	    if((hme->type == HOST_CONFITEM) &&
	       ((struct ConfItem*)hme->data)->status & CONF_KILL)
	      hmk = hme;	
	    else
	      hmc = hme;
	    prec = hme->precedence;
	  }
    }

  /* If the misc list returns an exemption to kline, return it */

  if (misc_hme && IsConfElined((struct ConfItem *)misc_hme->data))
    {
      return(misc_hme);
    }

  /* if nothing found in the hash tree,
   * return whatever was found in the misc list
   */

  if ( (hmk == NULL) && (hmc == NULL) )
    {
      return(misc_hme);
    }

  if (hmc && IsConfElined((struct ConfItem *)hmc->data))
    {
      return hmc;
    }

  if (hmk != NULL)
    {
      return hmk;
    }

  return hmc;
}

/* struct ConfItem *find_matching_conf(const char *host, const char *user,
 *                                     struct irc_inaddr *ip)
 * Input: a hostname/username and/or ip address to match.
 * Output: The highest precedence matching I-line or K-line.
 * Side-effects: -
 * Note: precedence is not currently correctly implemented for IP Ilines
 *       and Klines. If no hostmask I/K-line is found, then and only then
 *       are IP lines searched for.
 */
struct ConfItem *
find_matching_conf(const char *host, const char *user,
                   struct irc_inaddr *ip)
{
  struct HostMaskEntry *hm;
  struct ConfItem *aconf = NULL, *aconf_k = NULL;
  char buffer[HOSTLEN+USERLEN+1];

  if (!host || !user)
    return NULL;

  ircsprintf(buffer, "%s@%s", user, host);

  if ((hm = match_hostmask(buffer, HOST_CONFITEM)))
   aconf = (struct ConfItem*)hm->data;

  if (aconf && aconf->status == CONF_KILL)
    {
      aconf_k = aconf;
      aconf = NULL;
    }

  if (!aconf)
    {
      aconf = match_ip_Iline(ip, user);
    }

  if (!aconf_k && ip)
    {
      aconf_k = match_ip_Kline(ip, user);
    }

  return (aconf_k && (!aconf || !IsConfElined(aconf))) ? aconf_k : aconf;
}

/* void add_conf(strcut ConfItem *aconf)
 * Inputs:	A hostmask I/K-line ConfItem to add.
 * Output:	NONE
 * Side-effects: The hostmask is added to the hashtable and the ConfItem
 *               is associated with it. The precedence is set with
 *               decreasing precedence on each call.
 */
void add_conf(struct ConfItem *aconf)
{
  char buffer[HOSTLEN+USERLEN+1];

  ircsprintf(buffer, "%s@%s",
	     aconf->user ? aconf->user : "",
	     aconf->host ? aconf->host : "");
  add_hostmask(buffer, HOST_CONFITEM, aconf);
}

/* void clear_conf(void)
 * Inputs:	NONE
 * Output: 	NONE
 * Side-effects: All ConfItems on the linked list with no attached
 *               clients are freed. The remainder are moved onto a
 *               list of deferred masks to be deleted on future calls to
 *               this function.
 */
void clear_conf(void)
{
  struct ConfItem *conf=NULL;
  static struct HostMaskEntry *deferred_masks;
  struct HostMaskEntry *hme=NULL, *hmen, *hmel = NULL;

  for (hme = deferred_masks; hme; hme=hmen)
    {
      hmen = hme->next;
      conf = ((struct ConfItem*)hme->data);
      if (conf->clients == 0)
	{
	  if(hmel)
	    hmel->next = hmen;
	  else
	    deferred_masks = hmen;
	  conf->clients--;
	  free_conf(conf);
	  MyFree(hme->hostmask);
	  MyFree(hme);
	}
      else
	hmel = hme;
    }

  for (hme = first_mask; hme; hme = hmen)
    {
      hmen = hme->next;

      /* We don't use types as of yet, but lets just check... -A1kmm. */
      assert(hme->type == HOST_CONFITEM);
      conf = (struct ConfItem*)hme->data;
      if (conf->clients)
	{
	  conf->status |= CONF_ILLEGAL;
	  hme->next = deferred_masks;
	  deferred_masks = hme;
	}
      else
	{
	  conf->clients--;
	  free_conf(conf);
	  MyFree(hme->hostmask);
	  MyFree(hme);
	}
    }
  first_miscmask = NULL;
  first_mask = NULL;
  memset((void *)hmhash,0,sizeof(hmhash));
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
char *show_iline_prefix(struct Client *sptr,struct ConfItem *aconf,char *name)
{
  static char prefix_of_host[MAXPREFIX];
  char *prefix_ptr;

  prefix_ptr = prefix_of_host;
 
  if (IsNoTilde(aconf))
    *prefix_ptr++ = '-';
  if (IsLimitIp(aconf))
    *prefix_ptr++ = '!';
  if (IsNeedIdentd(aconf))
    *prefix_ptr++ = '+';
  if (IsPassIdentd(aconf))
    *prefix_ptr++ = '$';
  if (IsNoMatchIp(aconf))
    *prefix_ptr++ = '%';
  if (IsConfDoSpoofIp(aconf))
    *prefix_ptr++ = '=';

  if(IsOper(sptr))
    if (IsConfElined(aconf))
      *prefix_ptr++ = '^';

  if(IsOper(sptr))
    if (IsConfFlined(aconf))
      *prefix_ptr++ = '>';

  if(IsOper(sptr)) 
    if (IsConfIdlelined(aconf))
      *prefix_ptr++ = '<';

  *prefix_ptr = '\0';

  strncat(prefix_of_host,name,MAXPREFIX);
  return(prefix_of_host);
}

/* void report_hostmask_conf_links(struct Client *sptr, int flags)
 * Input: Client to send report to, flags on whether to report I/K lines.
 * Output: -
 * Side-effects: The client is sent a list of all ConfItems they are
 *               entitled to see of the type specified.
 */
void
report_hostmask_conf_links(struct Client *sptr, int flags)
{
  struct HostMaskEntry *mask;
  struct ConfItem *aconf;
  char *name, *host, *pass, *user, *classname;
  int  port;

  if (flags & CONF_CLIENT) /* Show I-lines... */
    {
      for (mask = first_mask; mask; mask = mask->next)
	{
	  if (mask->type != HOST_CONFITEM)
	    continue;
	  aconf = (struct ConfItem*)mask->data;
	  if (!(aconf->status & CONF_CLIENT))
	    continue;
	  if (!(MyConnect(sptr) && IsOper(sptr)) &&
	      IsConfDoSpoofIp(aconf))
	    continue;
	  get_printable_conf(aconf, &name, &host, &pass, &user, &port,
			     &classname);
	  sendto_one(sptr, form_str(RPL_STATSILINE), me.name, sptr->name,
		     'I', name,
		     show_iline_prefix(sptr,aconf,user),
		     host, port, classname
		     );
	}
      /* I-lines next... */
      report_ip_Ilines(sptr);
    }
  else /* Show K-lines... */
    {
      /* IP K-lines first... */
      report_ip_Klines(sptr);
      for (mask = first_mask; mask; mask = mask->next)
	{
	  if (mask->type != HOST_CONFITEM)
	    continue;
	  aconf = (struct ConfItem*)mask->data;
	  if (!(aconf->status & CONF_KILL))
	    continue;
	  get_printable_conf(aconf, &name, &host, &pass, &user, &port,
			     &classname);
	  sendto_one(sptr, form_str(RPL_STATSKLINE), me.name, sptr->name,
		     'K', host, user, pass
		     );
	}
    }
}
