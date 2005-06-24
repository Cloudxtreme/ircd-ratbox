/* dline_conf.h  -- lets muse over dlines, shall we?
 *
 * $Id$ 
 */

#ifndef INCLUDED_dline_conf_h
#define INCLUDED_dline_conf_h
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ircd_defs.h"

struct Client;
struct ConfItem;

extern void clear_Dline_table();
extern void zap_Dlines();
extern void add_Dline(struct ConfItem *conf_ptr);
extern void add_ip_Kline(struct ConfItem *conf_ptr);

extern void add_Eline(struct ConfItem *conf_ptr);

extern struct ConfItem *match_Dline(struct irc_inaddr *ip);
extern struct ConfItem* match_ip_Kline(struct irc_inaddr *ip, const char* name);

extern void report_dlines(struct Client *sptr);
extern void report_ip_Klines(struct Client *sptr);

#endif /* INCLUDED_dline_conf_h */


