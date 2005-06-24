/* debug.c - debugging functions */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "debug.h"
#include "s_log.h"

debug_tab dtab[] = {
  /* the second field here is always 0, unless you
     want this to be enabled by default.. */
  {"send", 0},
  {"error", 0},
  {NULL, 0}
};

debug_tab *dmtab = NULL;
int num_debug_mod = 0;

/*
 * FUNCTION
 *   add_mod_debug
 *
 * ARGUMENTS
 *   char *what - name of debugging thing to add
 *
 * RETURN VALUE
 *   none
 *
 * DESCRIPTION
 *
 *   This function creates a new debugging entity, with the name specified.
 */

void 
add_mod_debug(char *what)
{
  dmtab = realloc(dmtab, sizeof(debug_tab) * (num_debug_mod + 1));
  dmtab[num_debug_mod].name = malloc(strlen(what) + 1);
  strcpy(dmtab[num_debug_mod].name, what);
  dmtab[num_debug_mod].debugging = 0;
  num_debug_mod++;
}

/*
 * FUNCTION 
 *   deprintf
 *
 * ARGUMENTS
 *   what - debugging entity
 *   fmt, ... - printf format string
 * 
 * RETURN VALUE
 *   none
 *
 * DESCRIPTION
 *
 *   If the entity specified is being debugging, the printf-style format string
 *   supplied is written to stdout.  Otherwise no action is taken.
 */

#ifdef DEBUGMODE
void 
deprintf(char *what, char *fmt, ...)
{
  va_list ap;
  char buffer[DEBUG_BUFSIZE];

  va_start(ap, fmt);

  if(!debugging(what)) 
    {
      return;
    }


  vsnprintf(buffer, sizeof(buffer) - 1, fmt, ap);
  log(L_DEBUG, "%s", buffer);
  fflush(stdout);
}
#else
void
deprintf(a,b)
     char *a,*b;
{
  return;
}
#endif

/*
 * FUNCTION
 *   set_debug
 *
 * ARGUMENTS
 *   what - debugging entity
 *   to - what to set it to (0/1)
 *
 * RETURN VALUE
 *   0 - success
 *   1 - "what" does not exist
 *
 * DESCRIPTION
 *
 *   This function searches for the debugging entity "what", and sets its .debugging value
 *   to "to".
 */

int 
set_debug(char *what, int to)
{
  debug_tab *tab;
  
  tab = find_debug_tab(what);
  
  if (tab) 
    {
      tab->debugging = to;
      return 0;
    } 
  else 
    {
      return 1;
    }
}

/*
 * FUNCTION
 *   find_debug_tab
 *
 * ARGUMENTS
 *   what - debugging entity to find
 *
 * RETURN VALUE
 *   Address of debug_tab handling this entity.
 *
 * DESCRIPTION
 *   This function searches for the debugging entity "what", and returns
 *   a pointer to its structure.
 */

debug_tab *
find_debug_tab(char *what)
{
  int i; 
  debug_tab *tab = NULL;

  for (i = 0; dtab[i].name; i++) 
    {
      if (!strcmp(dtab[i].name, what)) 
	{
	  tab = &dtab[i];
	}
    }

  for (i = 0; i < num_debug_mod; i++) 
    {
      if (!strcmp(dmtab[i].name, what)) 
	{
	  tab = &dtab[i];
	}
    }

  return tab;
}

/*
 * FUNCTION
 *   enable_debug
 *
 * ARGUMENTS
 *   what - debugging entity
 *
 * RETURN VALUE
 *   0 - success
 *   1 - failure
 *
 * DESCRIPTION
 * 
 *   Enables debugging for the entity "what", if it exists.
 */

int 
enable_debug(char *what)
{
  debug_tab *tab;

  tab = find_debug_tab(what);

  if (tab) 
    {
      tab->debugging = 1;
      return 0;
    } 
  /* else not found */
  return -1;
}

/*
 * FUNCTION
 *   disable_debug
 *
 * ARGUMENTS
 *   what - debugging entity
 *
 * RETURN VALUE
 *   0 - success
 *   1 - failure
 *
 * DESCRIPTION
 * 
 *   Disbles debugging for the entity "what", if it exists.
 */

int 
disable_debug(char *what)
{
  debug_tab *tab;

  tab = find_debug_tab(what);

  if (tab) 
    {
      tab->debugging = 0;
      return 0;
    } 
    
  /* else not found */
  return -1;
}

/*
 * FUNCTION
 *   debugging
 *
 * ARGUMENTS
 *   what - debugging entity
 *
 * RETURN VALUE
 *   0 - debugging not enabled
 *   1 - debugging enabled
 *
 * DESCRIPTION
 *   This function should be used to check if a certain thing is being
 *   debugged.
 */

int 
debugging(char *what)
{
  debug_tab *tab;

  tab = find_debug_tab(what);
  if (!tab) 
    return 0;

  return tab->debugging;
}
