#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ircd_getopt.h"
#include "debug.h"
#include "config.h"

#ifndef VMS
# define OPTCHAR '-'
#else
# define OPTCHAR '/'
#endif

void
parseargs(int *argc, char ***argv, struct lgetopt *opts)
{
  int i;
  char *progname = (*argv)[0];

  /* loop through each argument */
  for (;;)
    {
      int found = 0;

      (*argc)--;
      (*argv)++;
      
      if (*argc < 1)
	{
	  return;
	}
      
      /* check if it *is* an arg.. */
      if ((*argv)[0][0] != OPTCHAR)
	{
	  return;
	}
      
      (*argv)[0]++;

      /* search through our argument list, and see if it matches */
      for (i = 0; opts[i].opt; i++) 
	{
	  if (!strcmp(opts[i].opt, (*argv)[0]))
	    {
	      /* found our argument */
	      found = 1;

	      switch (opts[i].argtype)
		{
		case YESNO:
		  *((int *)opts[i].argloc) = 1;
		  break;
		case INTEGER:
		  if (*argc < 2)
		    {
		      fprintf(stderr, "Error: option '%c%s' requires an argument\n",
			      OPTCHAR, opts[i].opt);
		      usage((*argv)[0]);
		    }
		  
		  *((int *)opts[i].argloc) = atoi((*argv)[1]);
		  break;
		case STRING:
		  if (*argc < 2)
		    {
		      fprintf(stderr, "error: option '%c%s' requires an argument\n",
			      OPTCHAR, opts[i].opt);
		      usage(progname);
		    }
		  
		  *((char**)opts[i].argloc) = malloc(strlen((*argv)[1]) + 1);
		  strcpy(*((char**)opts[i].argloc), (*argv)[1]);
		  break;

		case USAGE:
		  usage(progname);
		  /*NOTREACHED*/

#ifdef DEBUGMODE
		case ENDEBUG:
		  if (*argc < 2)
		    {
		      fprintf(stderr, "error: option '%c%s' requires an argument\n",
			      OPTCHAR, opts[i].opt);
		      usage(progname);
		    }

		  if (enable_debug((*argv)[1]) == -1)
		    {
		      fprintf(stderr, "error: '%s' unknown for debugging\n",
			      (*argv)[1]);
		      fprintf(stderr, "ircd: exiting on error.\n");
		      exit(EXIT_FAILURE);
		    }
		  break;
#endif

		default:
		  fprintf(stderr, "Error: internal error in parseargs() at %s:%d\n",
			  __FILE__, __LINE__);
		  exit(EXIT_FAILURE);
		}
	    }
	}
	if (!found)
	  {
	    fprintf(stderr, "error: unknown argument '%c%s'\n", OPTCHAR, (*argv)[0]);
	    usage(progname);
	  }
    }
}

void 
usage(char *name)
{
  int i = 0;
  
  fprintf(stderr, "Usage: %s [options]\n", name);
  fprintf(stderr, "Where valid options are:\n");
  
  for (i = 0; myopts[i].opt; i++)
    {
      fprintf(stderr, "\t%c%-10s %-20s%s\n", OPTCHAR, myopts[i].opt, 
	      (myopts[i].argtype == YESNO || myopts[i].argtype == USAGE) ? "" : 
	      myopts[i].argtype == INTEGER ? "<number>" : "<string>",
	      myopts[i].desc);
    }
  
  exit(EXIT_FAILURE);
}

