# MMS/MMK Makefile for OpenVMS
# Copyright (c) 2001 Edward Brocklesby
#
# Usage: 
# $ SET DEF [.IRCD-HYBRID-7.src]
# $ EDIT [-.include]config.h 
#   < change settings in config.h appropriately >
# $ COPY [.include]setup.h_vms [.include]setup.h
# $ MMS IRCD.EXE
#

CC=	CC
CFLAGS=	/INCLUDE_DIRECTORY=[-.INCLUDE]/STANDARD=ISOC94
LDFLAGS=

OBJECTS=	BLALLOC,CHANNEL,CLASS,CLIENT,DLINE_CONF,EVENT,FDLIST,FILEIO,-
		HASH,HOOK,IRCD,IRCDAUTH,IRCD_SIGNAL,IRC_STRING,LINEBUF,-
		LIST,LISTENER,MATCH,MD5,MEMORY,MODULES,MOTD,MTRIE_CONF,M_ERROR,-
		NUMERIC,OLDPARSE,PACKET,PARSE,RESTART,SCACHE,SEND,-
		SPRINTF_IRC,S_AUTH,S_BSD,S_CONF,S_DEBUG,S_GLINE,S_LOG,S_MISC,-
		S_SERV,S_STATS,S_USER,TOOLS,VCHANNEL,WHOWAS,S_BSD_SELECT,RES

IRCD.EXE : IRCD.OLB($(OBJECTS))
	$(LINK)$(LDFLAGS)/EXECUTABLE=IRCD

CLEAN : 
	DELETE *.OBJ;*
	DELETE IRCD.OLD;*
