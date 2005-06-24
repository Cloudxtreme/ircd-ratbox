/*
 * send.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_send_h
#define INCLUDED_send_h
#ifndef INCLUDED_config_h
#include "config.h"       /* HAVE_STDARG_H */
#endif

/*
 * struct decls
 */
struct Client;
struct Channel;
struct dlink_list;

/* The nasty global also used in s_serv.c for server bursts */
unsigned long current_serial;

/* send.c prototypes */

extern void send_queued_write(int fd, void *data);

extern  void sendto_all_local_opers(struct Client *, char *message_type,
				    const char *pattern,...);
extern  void sendto_one(struct Client *, const char *, ...);
extern  void sendto_channel_butone(struct Client *, struct Client *, 
                                   struct Channel *, const char *, ...);
extern  void sendto_serv_butone(struct Client *, const char *, ...);
extern  void sendto_ll_serv_butone(struct Client *, struct Client *sptr, int,
				   const char *, ...);
extern  void sendto_cap_serv_butone(int, struct Client *, const char *, ...);
extern  void sendto_nocap_serv_butone(int, struct Client *, const char *, ...);
extern  void sendto_common_channels_local(struct Client *, const char *, ...);
extern  void sendto_channel_local(int type,
				  struct Channel *,
				  const char *, ...);
#define ALL_MEMBERS  0
#define NON_CHANOPS  1
#define ONLY_CHANOPS_VOICED 2
#define ONLY_CHANOPS 3

extern  void sendto_channel_remote(struct Channel *, struct Client *cptr, 
				   const char *, ...);
extern  void sendto_ll_channel_remote(struct Channel *, struct Client *cptr, 
				      struct Client *sptr,
				      const char *, ...);
extern  void sendto_match_cap_servs(struct Channel *, struct Client *, 
                                    int, const char *, ...);
extern  void sendto_match_butone(struct Client *, struct Client *, 
                                 char *, int, const char *, ...);

extern  void sendto_realops(const char *, ...);
extern  void sendto_realops_flags(int, const char *, ...);

extern  void sendto_ops_butone(struct Client *, struct Client *, 
                               const char *, ...);
extern  void sendto_wallops_butone(struct Client *, struct Client *, 
                                   const char *, ...);
extern  void ts_warn(const char *, ...);

extern  void sendto_anywhere(struct Client *, struct Client *, 
			     const char *, ...);

/* used when sending to #mask or $mask */

#define MATCH_SERVER  1
#define MATCH_HOST    2

#endif /* INCLUDED_send_h */
