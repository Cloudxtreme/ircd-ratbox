/* This code is in the public domain.
 * $Nightmare: nightmare/src/main/parser.y,v 1.2.2.1.2.1 2002/07/02 03:42:10 ejb Exp $
 * $Id$
 */

%{
#include <sys/types.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#define WE_ARE_MEMORY_C
#include "stdinc.h"
#include "setup.h"
#include "common.h"
#include "ircd_defs.h"
#include "config.h"
#include "client.h"
#include "modules.h"
#include "newconf.h"

#define YY_NO_UNPUT

typedef
struct	variable_stru
{
	char *	name;
	int 	type;
	union
	{
		char *	strval;
		int	intval;
	} v;
} variable;

	int 		yyparse			();
	int 		yyerror			(char *);
	int 		yylex			();

static 	time_t 		conf_find_time		(char*);
static	void		print_heir		(void);
static	void		free_heir		(void);

static	int	 	set_variable		(char *, int, int, char*);
static	variable*	find_variable		(char *);
static	int		delete_variable		(char *);

static struct {
	char *	name;
	char *	plural;
	time_t 	val;
} times[] = {
	{"second",     "seconds",    1},
	{"minute",     "minutes",    60},
	{"hour",       "hours",      60 * 60},
	{"day",        "days",       60 * 60 * 24},
	{"week",       "weeks",      60 * 60 * 24 * 7},
	{"fortnight",  "fortnights", 60 * 60 * 24 * 14},
	{"month",      "months",     60 * 60 * 24 * 7 * 4},
	{"year",       "years",      60 * 60 * 24 * 365},
	/* ok-- we now do sizes here too. they aren't times, but 
	   it's close enough */
	{"byte",	"bytes",	1},
	{"kb",		NULL,		1024},
	{"kbyte",	"kbytes",	1024},
	{"kilobyte",	"kilebytes",	1024},
	{"mb",		NULL,		1024 * 1024},
	{"mbyte",	"mbytes",	1024 * 1024},
	{"megabyte",	"megabytes",	1024 * 1024},
	{NULL},
};

time_t conf_find_time(char *name)
{
  int i;

  for (i = 0; times[i].name; i++)
    {
      if (strcasecmp(times[i].name, name) == 0 ||
	  (times[i].plural && strcasecmp(times[i].plural, name) == 0))
	return times[i].val;
    }

  return 0;
}

static struct
{
	char *word;
	int yesno;
} yesno[] = {
	{"yes",		1},
	{"no",		0},
	{"true",	1},
	{"false",	0},
	{"on",		1},
	{"off",		0},
	{NULL,		0}
};

static int	conf_get_yesno_value(char *str)
{
	int i;

	for (i = 0; yesno[i].word; i++)
	{
		if (strcasecmp(str, yesno[i].word) == 0)
		{
			return yesno[i].yesno;
		}
	}

	return -1;
}

static void	free_cur_list(conf_parm_t* list)
{
	switch (list->type & CF_MTYPE)
	{
		case CF_STRING:
			MyFree(list->v.string);
			break;
		case CF_LIST:
			free_cur_list(list->v.list);
			break;
		default: break;
	}

	if (list->next)
		free_cur_list(list->next);
}

		
conf_parm_t *	cur_list = NULL;

static void	add_cur_list_cpt(conf_parm_t *new)
{
	if (cur_list == NULL)
	{
		cur_list = MyMalloc(sizeof(conf_parm_t));
		memset(cur_list, 0, sizeof(conf_parm_t));
		cur_list->type |= CF_FLIST;
		cur_list->v.list = new;
	}
	else
	{
		new->next = cur_list->v.list;
		cur_list->v.list = new;
	}
}

static 
void	add_cur_list(int type, char *str, int number)
{
	conf_parm_t *new;

	new = MyMalloc(sizeof(conf_parm_t));
	new->next = NULL;
	new->type = type;

	switch(type)
	{
	case CF_INT:
	case CF_TIME:
	case CF_YESNO:
		new->v.number = number;
		break;
	case CF_STRING:
		DupString(new->v.string, str);
		break;
	}

	add_cur_list_cpt(new);
}

dlink_list	variables;

static
int	set_variable(char *name, int type, int intval, char* strval)
{
	variable *var = MyMalloc(sizeof(variable));

	printf("Setting variable '%s', type %d ", name, type);
	if (find_variable(name) != NULL)
		delete_variable(name);

	DupString(var->name, name);
	var->type = type;
	switch(type)
	{
		case CF_INT:
			printf("integer: %d\n", intval);
			var->v.intval = intval;
			break;
		case CF_STRING:
			printf("string: '%s'\n", strval);
			DupString(var->v.strval, strval);
			break;
	}
	
	dlinkAddAlloc(var, &variables);
	return 0;
}

static
int	delete_variable(char *name)
{
	dlink_node *n;
	variable *var;

	DLINK_FOREACH(n, variables.head)
	{
		var = n->data;

		if (strcasecmp(var->name, name))
		{
			MyFree(var->name);
			if (var->type == CF_STRING)
				MyFree(var->v.strval);
			dlinkFindDelete(&variables, var);
			MyFree(var);
			return 0;
		}
	}

	return -1;
}

static
variable *	find_variable(char *name)
{
	dlink_node *n;
	variable *var;

	printf("(finding variable %s ", name);
	DLINK_FOREACH(n, variables.head)
	{
		var = n->data;
		printf("against %s ", var->name);
		if (strcasecmp(var->name, name) == 0)
		{
			printf("match) ");
			return var;
		}
	}

	printf("no match) ");
	return NULL;
}

static	char *
strrstr	(const char *str, const char *find)
{
	const char *s;

	for (s = str + strlen(str); s >= str; s--)
		if (strncmp(s, find, strlen(find)) == 0)
			return (char*)s;

	return NULL;
}

dlink_list	items; /* list of all heirs */

static	dlink_list	heirs;

static	void
free_heir	(void)
{
	dlink_node *ptr, *ptr_next;
	struct HeirItem *hi;

	DLINK_FOREACH_SAFE(ptr, ptr_next, heirs.head)
	{
		hi = ptr->data;
		MyFree(hi->label);
		MyFree(hi->name);
		MyFree(hi->item);
		MyFree(hi);
		dlinkDelete(ptr, &heirs);
	}
}

static	void
add_item	(void)
{
	dlink_list *new = MyMalloc(sizeof(dlink_list));
	dlink_node *n;
	memset(new, 0, sizeof(*new));

	print_heir();

	DLINK_FOREACH(n, heirs.head)
		dlinkAddAlloc(n->data, new);
	dlinkAddAlloc(new, &items);
}

static	void
push_name	(char *name, const char *label, conf_parm_t *item)
{
	struct HeirItem	*hi = MyMalloc(sizeof(struct HeirItem));
	DupString(hi->name, name);
	if (label)
		DupString(hi->label, label);
	else
		hi->label = NULL;
	hi->item = item;
	dlinkAddAlloc(hi, &heirs);
}

static	void
print_heir()
{
	dlink_node *n;
	struct HeirItem *hi = NULL;

	DLINK_FOREACH_PREV(n, heirs.tail)
	{
		hi = n->data;

		printf("::%s(%s)", hi->name, hi->label ? hi->label : "");
	}
	printf(" = %p\n", hi->item);
}

static	void
pop_name()
{
	dlinkDelete(heirs.head, &heirs);
}

%}

%union {
	int 		number;
	char 		string[IRCD_BUFSIZE + 1];
	conf_parm_t *	conf_parm;
}

%token LOADMODULE TWODOTS

%token <string> STRING VARIABLE
%token <number> NUMBER

%type <string> string variable
%type <number> number timespec 
%type <conf_parm> oneitem single itemlist

%start conf

%%

conf:	conf_item 
	| conf_item conf 
	;

conf_item: loadmodule 
	 | variable_assign 
	 | block_items 
	 | blockname '{' 
	 	conf_item
		'}' ';' { pop_name(); }
         ;

blockname: string { 
			       push_name($1, NULL, 0);
		  }
	| string string { 
		push_name($1, $2, 0);
	}
	;

variable_assign:  variable '=' string ';' { set_variable($1, CF_STRING, 0, $3); }
		| variable '=' timespec ';' { set_variable($1, CF_INT, $3, NULL); }
		| variable '=' number ';' { set_variable($1, CF_INT, $3, NULL); } ;

block_items: block_items block_item 
           | block_item
           ;

block_item:	string '=' itemlist ';'
		{
			push_name($1, NULL, $3);
			add_item();
			pop_name();

#if 0
			conf_call_set(conf_cur_block, $1, cur_list, CF_LIST);
			free_cur_list(cur_list);
			cur_list = NULL;
#endif
		}
		;

itemlist: itemlist ',' single 
	| single 
	;

single: oneitem
	{
		add_cur_list_cpt($1);
	}
	| oneitem TWODOTS oneitem
	{
		/* "1 .. 5" meaning 1,2,3,4,5 - only valid for integers */
		if (($1->type & CF_MTYPE) != CF_INT ||
		    ($3->type & CF_MTYPE) != CF_INT)
		{
			conf_report_error("Both arguments in '..' notation must be integers.");
			break;
		}
		else
		{
			int i;

			for (i = $1->v.number; i <= $3->v.number; i++)
			{
				add_cur_list(CF_INT, 0, i);
			}
		}
	}
	;

oneitem: timespec
            {
		$$ = MyMalloc(sizeof(conf_parm_t));
		memset($$, 0, sizeof(conf_parm_t));
		$$->type = CF_TIME;
		$$->v.number = $1;
	    }
          | number
            {
		$$ = MyMalloc(sizeof(conf_parm_t));
		memset($$, 0, sizeof(conf_parm_t));
		$$->type = CF_INT;
		$$->v.number = $1;
	    }
          | string
            {
		/* a 'string' could also be a yes/no value .. 
		   so pass it as that, if so */
		int val = conf_get_yesno_value($1);

		$$ = MyMalloc(sizeof(conf_parm_t));
		memset($$, 0, sizeof(conf_parm_t));

		if (val != -1)
		{
			$$->type = CF_YESNO;
			$$->v.number = val;
		}
		else
		{
			$$->type = CF_STRING;
			DupString($$->v.string, $1);
		}
            }
	  | variable
	    {
		    /* extract the value of the variable.. */
		    variable *var = find_variable($1);

		    if (!var)
		    {
			    /* unknown variable */
			    /* ignore this option; the setting them defaults
			       to its previous default, which is what users
			       would expect I think. --larne */
			    conf_report_error("Undefined variable $%s.", $1);
			    break;
		    }
		    
		    $$ = MyMalloc(sizeof(conf_parm_t));
		    memset($$, 0, sizeof(conf_parm_t));
		    $$->type = var->type;

		    switch (var->type)
		    {
			    case CF_INT:
		    		$$->v.number = var->v.intval;
				break;

			    case CF_STRING:
				DupString($$->v.string, var->v.strval);
				break;
		    }
	    }
          ;

loadmodule:
	  LOADMODULE string
            {
	      load_one_module($2);
	    }
	  ';'
          ;

string: STRING { strcpy($$, $1); } ;
number: NUMBER { $$ = $1; } ;
variable: VARIABLE { strcpy($$, $1 + 1); /* strip the $ */} ;

timespec:	number string
         	{
			time_t t;

			if ((t = conf_find_time($2)) == 0)
			{
				conf_report_error("Unrecognised time type/size '%s'", $2);
				t = 1;
			}
	    
			$$ = $1 * t;
		}
		| timespec timespec
		{
			$$ = $1 + $2;
		}
		| timespec number
		{
			$$ = $1 + $2;
		}
		;
