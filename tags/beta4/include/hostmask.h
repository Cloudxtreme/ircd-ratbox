/***********************************************************************
 *   ircd-hybrid project - Internet Relay Chat
 *  hostmask.h: Hostmask header file.
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

enum
{
 HM_HOST,
 HM_IPV4
#ifdef IPV6
 ,HM_IPV6
#endif
};

struct HostMaskEntry
{
  int type, subtype;
  unsigned long precedence;
  char *hostmask;
  void *data;
  struct HostMaskEntry *next, *nexthash;
};

int parse_netmask(const char*, struct irc_inaddr*, int*);
int match_ipv6(struct irc_inaddr*, struct irc_inaddr*, int);
int match_ipv4(struct irc_inaddr*, struct irc_inaddr*, int);
struct ConfItem* find_conf_by_address(const char*, struct irc_inaddr*,
                                      int, int, const char*);
void add_conf_by_address(const char*, int, const char*, struct ConfItem*);
void delete_one_address_conf(const char*, struct ConfItem*);
void clear_out_address_conf(void);
void init_host_hash(void);
struct ConfItem* find_address_conf(const char*, const char*,
                                   struct irc_inaddr*, int);
struct ConfItem* find_dline(struct irc_inaddr *, int);

void report_dlines(struct Client*);
void report_exemptlines(struct Client*);
void report_Klines(struct Client*, int);
void report_Ilines(struct Client*);
#ifdef IPV6
int match_ipv6(struct irc_inaddr*, struct irc_inaddr*, int);
#endif
int match_ipv4(struct irc_inaddr*, struct irc_inaddr*, int);

/* Hashtable stuff... */
#define ATABLE_SIZE 0x1000

struct AddressRec
{
 /* masktype: HM_HOST, HM_IPV4, HM_IPV6 -A1kmm */
 int masktype;
 union {
  struct {
   /* Pointer into ConfItem... -A1kmm */
   struct irc_inaddr addr;
   int bits;
  } ipa;
   /* Pointer into ConfItem... -A1kmm */
  const char *hostname;
 } Mask;
 /* type: CONF_CLIENT, CONF_DLINE, CONF_KILL etc... -A1kmm */
 int type;
 /* Higher precedences overrule lower ones... */
 unsigned long precedence;
 /* Only checked if !(type & 1)... */
 const char *username;
 struct ConfItem *aconf;
 /* The next record in this hash bucket. */
 struct AddressRec *next;
};
