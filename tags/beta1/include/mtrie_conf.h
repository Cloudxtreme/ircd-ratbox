/* $Id$ */
#ifndef INCLUDED_mtrie_conf_h
#define INCLUDED_mtrie_conf_h
#include "ircd_defs.h"
#ifndef FOREVER
#define FOREVER for(;;)
#endif

struct ConfItem;
struct Client;

extern void   add_mtrie_conf_entry(struct ConfItem *,int);
extern void   add_ip_Iline( struct ConfItem * );
extern struct ConfItem* find_matching_mtrie_conf(const char* host,
                                           const char* user, 
                                           struct irc_inaddr *ip);
extern void report_mtrie_conf_links(struct Client *,int);
extern void clear_mtrie_conf_links(void);


/* As ircd only allow 63 characters in a hostname, 100 is more than enough */
#define MAX_TLD_STACK 100

#define MAX_PIECE_LIST 32       /* number of chars to use in domain piece
                                 * "lookat" table. It has to be a multiple
                                 * of 2, 32 works well for 26 chars in
                                 * alphabet.
                                 */
/*
 * What this is, is a modified trie
 * It will help if you read Knuth, Sorting and Searching, third volume
 * of "The Art Of Computer Programming" pg.
 *
 * Each node has more than two branches
 * the tree consists of "domain_levels" which have a piece of the domain name
 * being looked for. If a match is found on that piece, it will point
 * to the next "domain_level" node to scan from. The stack is popped
 * and search continues.
 * If a domain_level is ever filled up, an overflow domain_level is created.
 * simple.
 *
 *
 *
 * [domain_level "com" "ca" "org"]
 *                  \   |
 *                   \  _>[domain_level "passport" "group"
 *                    \
 *                     \-->[domain_level "varner" "voo"]->...
 *                
 */

struct domain_level;

typedef struct domain_piece
{
  char *host_piece;
  struct ConfItem *conf_ptr;
  struct ConfItem *wild_conf_ptr;
  int flags;            /* E_type I_type K_type */
  struct domain_piece *next_piece;
  struct domain_level *next_level;
} DOMAIN_PIECE;

typedef struct domain_level
{
  struct domain_piece *piece_list[MAX_PIECE_LIST];
} DOMAIN_LEVEL;

#define MAXPREFIX (HOSTLEN + USERLEN + 15)

extern char *show_iline_prefix(struct Client *,struct ConfItem *,char *);



#endif /* INCLUDED_mtrie_conf_h */

