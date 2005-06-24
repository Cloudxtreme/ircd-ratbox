/*
 * motd.h
 *
 * $Id$
 */
#ifndef INCLUDED_motd_h
#define INCLUDED_motd_h
#ifndef INCLUDED_ircd_defs_h
#include "ircd_defs.h"    /* MAX_DATE_STRING */
#endif

#include <limits.h>       /* PATH_MAX */

/* XXX really, should be mallocing this on the fly but... */
#define MESSAGELINELEN 256

typedef enum {
  USER_MOTD,
  USER_LINKS,
  OPER_MOTD,
  HELP_MOTD
} MotdType;

struct MessageFileLine
{
  char                    line[MESSAGELINELEN + 1];
  struct MessageFileLine* next;
};

typedef struct MessageFileLine MessageFileLine;

struct MessageFile
{
  char             fileName[PATH_MAX + 1];
  MotdType         motdType;
  MessageFileLine* contentsOfFile;
  char             lastChangedDate[MAX_DATE_STRING + 1];
};

typedef struct MessageFile MessageFile;

struct Client;

void InitMessageFile(MotdType, char *, struct MessageFile *);
int SendMessageFile(struct Client *, struct MessageFile *);
int ReadMessageFile(MessageFile *);

#endif /* INCLUDED_motd_h */
