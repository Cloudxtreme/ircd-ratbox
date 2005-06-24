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
extern void send_queued_slink_write(int fd, void *data);

#define NOCAPS          0               /* no caps */
#define NOFLAGS         0               /* no flags */

#define LL_ICLIENT      0x00000001      /* introduce unknown clients */
#define LL_ICHAN        0x00000002      /* introduce unknown chans */

#ifndef __GNUC__
extern  void sendto_one(struct Client *, const char *, ...);
extern  void sendto_channel_butone(struct Client *one, struct Client *from,
                                   struct Channel *chptr, char *command,
                                   const char *pattern, ...);
extern  void sendto_one_prefix(struct Client *, struct Client *,
			       const char *, ...);
extern  void sendto_common_channels_local(struct Client *, const char *, ...);
extern  void sendto_channel_local(int type,
				  struct Channel *,
				  const char *, ...);
extern void sendto_server(struct Client *one, struct Client *source_p,
                          struct Channel *chptr, unsigned long caps,
                          unsigned long nocaps, unsigned long llflags,
                          char *format, ...);
#else /* !__GNUC__*/
extern  void sendto_one(struct Client *, const char *, ...)
	    __attribute__((format (printf, 2, 3)));

extern  void sendto_channel_butone(struct Client *one, struct Client *from,
                                   struct Channel *chptr, char *command,
                                   const char *pattern, ...)
            __attribute__((format (printf, 5, 6)));
extern  void sendto_one_prefix(struct Client *, struct Client *,
			       const char *, ...)
	    __attribute__((format (printf, 3, 4)));
extern  void sendto_common_channels_local(struct Client *, const char *, ...)
	    __attribute__((format (printf, 2, 3)));
extern  void sendto_channel_local(int type,
				  struct Channel *,
				  const char *, ...)
	    __attribute__((format (printf, 3, 4)));
extern void sendto_server(struct Client *one, struct Client *source_p,
                          struct Channel *chptr, unsigned long caps,
                          unsigned long nocaps, unsigned long llflags,
                          char *format, ...)
            __attribute__((format (printf, 7, 8)));
#endif /* __GNUC__*/

#define ALL_MEMBERS  0
#define NON_CHANOPS  1
#define ONLY_CHANOPS_HALFOPS_VOICED 2
#define ONLY_CHANOPS_HALFOPS 3
#define ONLY_CHANOPS 4

#ifndef __GNUC__
extern  void sendto_channel_remote(struct Channel *, struct Client *client_p, 
				   const char *, ...);
extern  void sendto_channel_remote_prefix(struct Channel *, struct Client *client_p,
										  struct Client *prefix, const char *, ...);

extern  void sendto_ll_channel_remote(struct Channel *, struct Client *client_p, 
				      struct Client *source_p,
				      const char *, ...);
extern  void sendto_match_cap_servs(struct Channel *, struct Client *, 
                                    int, const char *, ...);

extern  void sendto_match_vacap_servs(struct Channel *, struct Client *, ...);

extern  void sendto_match_cap_servs_nocap(struct Channel *, struct Client *,
											int, int, const char *, ...);

extern void sendto_match_nocap_servs(struct Channel *, struct Client *,
									 int, const char *, ...);

extern  void sendto_match_butone(struct Client *, struct Client *, 
                                 char *, int, const char *, ...);

extern  void sendto_realops_flags(int, const char *, ...);

extern  void sendto_wallops_flags(int, struct Client *, const char *, ...);

extern  void sendto_ops_butone(struct Client *, struct Client *, 
                               const char *, ...);
extern  void sendto_wallops_butone(struct Client *, struct Client *, 
                                   const char *, ...);
extern  void ts_warn(const char *, ...);

extern  void sendto_anywhere(struct Client *, struct Client *, 
			     const char *, ...);
#else /* ! __GNUC__ */
extern  void xsendto_channel_remote(struct Channel *, struct Client *client_p, 
				   const char *, ...)
	    __attribute__((format (printf, 3, 4)));
extern  void xsendto_channel_remote_prefix(struct Channel *, struct Client *client_p, 
				   struct Client *prefix, const char *, ...)
	    __attribute__((format (printf, 4, 5)));
				   
extern  void xsendto_ll_channel_remote(struct Channel *, struct Client *client_p, 
				      struct Client *source_p,
				      const char *, ...)
	    __attribute__((format (printf, 4, 5)));
				      
extern  void xsendto_match_cap_servs(struct Channel *, struct Client *, 
                                    int, const char *, ...)
	    __attribute__((format (printf, 4, 5)));

extern  void xsendto_match_vacap_servs(struct Channel *, struct Client *, ...);
		
extern void xsendto_match_cap_servs_nocap(struct Channel *, struct Client *, int, int, const char *, ...)
	__attribute__((format (printf, 5, 6)));

extern void xsendto_match_nocap_servs(struct Channel *, struct Client *, int, const char *, ...)
	__attribute__((format (printf, 4, 5)));
	
extern  void sendto_match_butone(struct Client *, struct Client *, 
                                 char *, int, const char *, ...)
	    __attribute__((format (printf, 5, 6)));

extern  void sendto_realops_flags(int, const char *, ...)
	    __attribute__((format (printf, 2, 3)));

extern  void sendto_wallops_flags(int, struct Client *, const char *, ...)
            __attribute__((format (printf, 3, 4)));

extern  void sendto_ops_butone(struct Client *, struct Client *, 
                               const char *, ...)
	    __attribute__((format (printf, 3, 4)));

extern  void sendto_wallops_butone(struct Client *, struct Client *, 
                                   const char *, ...)
	    __attribute__((format (printf, 3, 4)));

extern  void ts_warn(const char *, ...)
	    __attribute__((format (printf, 1, 2)));

extern  void sendto_anywhere(struct Client *, struct Client *, 
			     const char *, ...)
	    __attribute__((format (printf, 3, 4)));
#endif

extern void
kill_client(struct Client *client_p, struct Client *diedie,
	    const char *pattern, ... );

void
kill_client_ll_serv_butone(struct Client *one, struct Client *source_p,
			   const char *pattern, ...);

/* used when sending to #mask or $mask */

#define MATCH_SERVER  1
#define MATCH_HOST    2

#endif /* INCLUDED_send_h */
