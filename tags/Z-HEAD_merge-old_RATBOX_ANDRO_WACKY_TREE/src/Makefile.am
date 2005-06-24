# $Id$

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = ircd

INCLUDES = -I../adns $(INCLTDL)

version.c: 
	$(SHELL) ./version.c.SH 

version.o: version.c
	$(COMPILE) -c version.c -o version.o
	$(MV) version.c version.c.last
	
AM_YFLAGS=-d

ircd_SOURCES =                     \
  adns.c                        \
  balloc.c                      \
  cache.c                       \
  channel.c                     \
  class.c                       \
  client.c                      \
  commio.c                      \
  event.c                       \
  getopt.c                      \
  hash.c                        \
  hook.c                        \
  hostmask.c                    \
  irc_string.c                  \
  ircd.c                        \
  ircd_signal.c                 \
  kdparse.c                     \
  linebuf.c                     \
  listener.c                    \
  match.c                       \
  memory.c                      \
  modules.c                     \
  monitor.c                     \
  newconf.c                     \
  numeric.c                     \
  packet.c                      \
  parse.c                       \
  patricia.c                    \
  reject.c                      \
  restart.c                     \
  s_auth.c                      \
  s_conf.c                      \
  s_newconf.c                   \
  s_gline.c                     \
  s_log.c                       \
  s_serv.c                      \
  s_stats.c                     \
  s_user.c                      \
  scache.c                      \
  send.c                        \
  snprintf.c                    \
  tools.c                       \
  whowas.c			\
  version.c			\
  ircd_lexer.l			\
  ircd_parser.y			\
  @SELECT_TYPE@.c		

if STATIC_MODULES
ircd_LDADD = ../adns/libadns.a @SSL_LIBS@ ../modules/libmodules.a
else
ircd_LDADD = $(LIBLTDL) ../adns/libadns.a @SSL_LIBS@
endif

ircd_LDFLAGS = -export-dynamic

if STATIC_MODULES
ircd_DEPENDENCIES = ../adns/libadns.a version.o ../modules/libmodules.a
else
ircd_DEPENDENCIES = $(LIBLTDLC) ../adns/libadns.a version.o
endif

