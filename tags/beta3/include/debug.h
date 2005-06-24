#ifndef __DEBUG_H_INCLUDED_
#define __DEBUG_H_INCLUDED_

#define DEBUG_BUFSIZE 512

typedef struct 
{
  char *name; /* Name of thing to debug */
  int   debugging; /* Are we debugging this? */
} debug_tab;

int        debugging(char *);
int        enable_debug(char *);
int        disable_debug(char *);
int        set_debug(char *, int);
debug_tab *find_debug_tab(char *);
void       deprintf(char *, char *, ...);
void       add_mod_debug(char *);


#endif
