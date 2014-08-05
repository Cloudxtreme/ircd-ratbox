#include <stdlib.h>
#include <string.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
#define YYPREFIX "yy"
#line 7 "ircd_parser.y"

#include "ratbox_lib.h"
#include "stdinc.h"
#include "newconf.h"

#define YY_NO_UNPUT

int yyparse();
int yylex();

static time_t conf_find_time(char*);

static struct {
	const char *	name;
	const char *	plural;
	time_t 	val;
} ircd_times[] = {
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
	{"kilobyte",	"kilobytes",	1024},
	{"mb",		NULL,		1024 * 1024},
	{"mbyte",	"mbytes",	1024 * 1024},
	{"megabyte",	"megabytes",	1024 * 1024},
	{NULL, NULL, 0},
};

time_t 
conf_find_time(char *name)
{
	int i;

	for (i = 0; ircd_times[i].name; i++)
	{
		if (strcasecmp(ircd_times[i].name, name) == 0 || (ircd_times[i].plural && strcasecmp(ircd_times[i].plural, name) == 0))
			return ircd_times[i].val;
	}
	return 0;
}

static struct
{
	const char *word;
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

static int 
conf_get_yesno_value(char *str)
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

static void
free_cur_list(conf_parm_t* list)
{
	switch (list->type & CF_MTYPE)
	{
		case CF_STRING:
		case CF_QSTRING:
			rb_free(list->v.string);
			break;
		case CF_LIST:
			free_cur_list(list->v.list);
			break;
		default: break;
	}

	if (list->next)
		free_cur_list(list->next);
}

		
conf_parm_t *cur_list = NULL;

static void
add_cur_list_cpt(conf_parm_t *new)
{
	
	if (cur_list == NULL)
	{
		cur_list = rb_malloc(sizeof(conf_parm_t));
		cur_list->v.list = new;
	}
	else
	{
		new->next = cur_list->v.list;
		cur_list->v.list = new;
		cur_list->type |= CF_FLIST;
	}
}

static void
add_cur_list(int type, char *str, int number)
{
	conf_parm_t *new;

	new = rb_malloc(sizeof(conf_parm_t));
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
	case CF_QSTRING:
		new->v.string = rb_strdup(str);
		break;
	}

	add_cur_list_cpt(new);
}


#line 154 "ircd_parser.y"
#ifndef YYSTYPE_DEFINED
#define YYSTYPE_DEFINED
typedef union {
	int 		number;
	char 		string[IRCD_BUFSIZE + 1];
	conf_parm_t *	conf_parm;
} YYSTYPE;
#endif /* YYSTYPE_DEFINED */
#line 168 "ircd_parser.c"
#define LOADMODULE 257
#define TWODOTS 258
#define QSTRING 259
#define STRING 260
#define NUMBER 261
#define YYERRCODE 256
const short yylhs[] =
	{                                        -1,
    0,    0,    0,    8,    8,   11,    9,   13,    9,   12,
   12,   14,    7,    7,    6,    6,    5,    5,    5,    5,
   15,   10,    1,    2,    3,    4,    4,    4,
};
const short yylen[] =
	{                                         2,
    0,    2,    1,    1,    1,    0,    6,    0,    7,    2,
    1,    4,    3,    1,    1,    3,    1,    1,    1,    1,
    0,    4,    1,    1,    1,    2,    2,    2,
};
const short yydefred[] =
	{                                      0,
    3,    0,    0,   24,    0,    2,    4,    5,   21,   23,
    8,    0,    0,    0,    0,   22,    0,    0,    0,   11,
    0,    0,    0,   10,    0,   25,   17,   20,    0,    0,
    0,   14,    0,    7,    9,   26,    0,    0,    0,   12,
    0,   16,   13,
};
const short yydgoto[] =
	{                                       2,
   27,   18,   29,   30,   31,   32,   33,    6,    7,    8,
   12,   19,   14,   20,   13,
};
const short yysindex[] =
	{                                   -244,
    0, -246, -243,    0, -237,    0,    0,    0,    0,    0,
    0,  -98,  -33,  -91, -225,    0, -225,  -25, -122,    0,
 -119, -231,  -22,    0,  -21,    0,    0,    0, -225, -222,
 -218,    0,  -36,    0,    0,    0, -225, -222, -231,    0,
 -231,    0,    0,};
const short yyrindex[] =
	{                                      1,
    0,    0,    0,    0,  -82,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  -42,  -40,
  -35,    0,    0,    0,    0,    0,  -44,  -39,    0,    0,
    0,    0,    0,};
const short yygindex[] =
	{                                      0,
   38,    5,  -20,  -17,    6,    7,    0,    0,    0,    0,
    0,   30,    0,   12,    0,
};
#define YYTABLESIZE 261
const short yytable[] =
	{                                      28,
    1,   19,   23,   18,   27,   25,    5,   41,   15,   37,
    3,    1,   38,    4,   28,    9,   19,   37,   18,   27,
   38,   10,   40,   15,   15,   16,   28,   10,    4,   26,
   24,   17,   24,   36,    4,   22,   34,   35,   26,   39,
    6,   36,   11,   28,   42,   28,   21,   43,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    4,    0,    0,
    4,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   28,    0,   19,   28,   18,   27,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,
    1,
};
const short yycheck[] =
	{                                      44,
    0,   44,  125,   44,   44,  125,    2,   44,   44,   30,
  257,  256,   30,  260,   59,  259,   59,   38,   59,   59,
   38,  259,   59,   59,  123,   59,   22,  259,  260,  261,
   19,  123,   21,   29,  260,   61,   59,   59,  261,  258,
  123,   37,    5,   39,   39,   41,   17,   41,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  260,   -1,   -1,
  260,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  258,   -1,  258,  261,  258,  258,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  257,   -1,   -1,
  260,
};
#define YYFINAL 2
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 261
#if YYDEBUG
const char * const yyname[] =
	{
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,"','",0,0,0,0,0,0,0,0,0,0,0,0,0,0,"';'",0,"'='",0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'{'",0,"'}'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"LOADMODULE",
"TWODOTS","QSTRING","STRING","NUMBER",
};
const char * const yyrule[] =
	{"$accept : conf",
"conf :",
"conf : conf conf_item",
"conf : error",
"conf_item : block",
"conf_item : loadmodule",
"$$1 :",
"block : string $$1 '{' block_items '}' ';'",
"$$2 :",
"block : string qstring $$2 '{' block_items '}' ';'",
"block_items : block_items block_item",
"block_items : block_item",
"block_item : string '=' itemlist ';'",
"itemlist : itemlist ',' single",
"itemlist : single",
"single : oneitem",
"single : oneitem TWODOTS oneitem",
"oneitem : qstring",
"oneitem : timespec",
"oneitem : number",
"oneitem : string",
"$$3 :",
"loadmodule : LOADMODULE QSTRING $$3 ';'",
"qstring : QSTRING",
"string : STRING",
"number : NUMBER",
"timespec : number string",
"timespec : timespec timespec",
"timespec : timespec number",
};
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
/* LINTUSED */
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
unsigned int yystacksize;
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(void)
{
    unsigned int newsize;
    long sslen;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    sslen = yyssp - yyss;
#ifdef SIZE_MAX
#define YY_SIZE_MAX SIZE_MAX
#else
#define YY_SIZE_MAX 0xffffffffU
#endif
    if (newsize && YY_SIZE_MAX / newsize < sizeof *newss)
        goto bail;
    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :
      (short *)malloc(newsize * sizeof *newss); /* overflow check above */
    if (newss == NULL)
        goto bail;
    yyss = newss;
    yyssp = newss + sslen;
    if (newsize && YY_SIZE_MAX / newsize < sizeof *newvs)
        goto bail;
    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :
      (YYSTYPE *)malloc(newsize * sizeof *newvs); /* overflow check above */
    if (newvs == NULL)
        goto bail;
    yyvs = newvs;
    yyvsp = newvs + sslen;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
bail:
    if (yyss)
            free(yyss);
    if (yyvs)
            free(yyvs);
    yyss = yyssp = NULL;
    yyvs = yyvsp = NULL;
    yystacksize = 0;
    return -1;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse(void)
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif /* YYDEBUG */

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(lint) || defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(lint) || defined(__GNUC__)
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym)
        yyval = yyvsp[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
    switch (yyn)
    {
case 6:
#line 181 "ircd_parser.y"
{ 
		conf_start_block(yyvsp[0].string, NULL);
	}
break;
case 7:
#line 186 "ircd_parser.y"
{
		conf_end_block();
	}
break;
case 8:
#line 191 "ircd_parser.y"
{ 
		conf_start_block(yyvsp[-1].string, yyvsp[0].string);
	}
break;
case 9:
#line 195 "ircd_parser.y"
{
		conf_end_block();
	}
break;
case 12:
#line 205 "ircd_parser.y"
{
		conf_call_set(yyvsp[-3].string, cur_list, CF_LIST);
		free_cur_list(cur_list);
		cur_list = NULL;
	}
break;
case 15:
#line 217 "ircd_parser.y"
{
		add_cur_list_cpt(yyvsp[0].conf_parm);
	}
break;
case 16:
#line 221 "ircd_parser.y"
{
		/* "1 .. 5" meaning 1,2,3,4,5 - only valid for integers */
		if ((yyvsp[-2].conf_parm->type & CF_MTYPE) != CF_INT ||
		    (yyvsp[0].conf_parm->type & CF_MTYPE) != CF_INT)
		{
			conf_report_error("Both arguments in '..' notation must be integers.");
			break;
		}
		else
		{
			int i;

			for (i = yyvsp[-2].conf_parm->v.number; i <= yyvsp[0].conf_parm->v.number; i++)
			{
				add_cur_list(CF_INT, 0, i);
			}
		}
	}
break;
case 17:
#line 242 "ircd_parser.y"
{
		yyval.conf_parm = rb_malloc(sizeof(conf_parm_t));
		yyval.conf_parm->type = CF_QSTRING;
		yyval.conf_parm->v.string = rb_strdup(yyvsp[0].string);
	}
break;
case 18:
#line 248 "ircd_parser.y"
{
		yyval.conf_parm = rb_malloc(sizeof(conf_parm_t));
		yyval.conf_parm->type = CF_TIME;
		yyval.conf_parm->v.number = yyvsp[0].number;
	}
break;
case 19:
#line 254 "ircd_parser.y"
{
		yyval.conf_parm = rb_malloc(sizeof(conf_parm_t));
		yyval.conf_parm->type = CF_INT;
		yyval.conf_parm->v.number = yyvsp[0].number;
	}
break;
case 20:
#line 260 "ircd_parser.y"
{
		/* a 'string' could also be a yes/no value .. 
		 so pass it as that, if so */
		 
		int val = conf_get_yesno_value(yyvsp[0].string);

		yyval.conf_parm = rb_malloc(sizeof(conf_parm_t));

		if (val != -1)
		{
			yyval.conf_parm->type = CF_YESNO;
			yyval.conf_parm->v.number = val;
		}
		else
		{
			yyval.conf_parm->type = CF_STRING;
			yyval.conf_parm->v.string = rb_strdup(yyvsp[0].string);
		}
	}
break;
case 21:
#line 283 "ircd_parser.y"
{
#ifndef STATIC_MODULES
/*	load_one_module($2, 0);*/
#endif
	}
break;
case 23:
#line 291 "ircd_parser.y"
{ strcpy(yyval.string, yyvsp[0].string); }
break;
case 24:
#line 292 "ircd_parser.y"
{ strcpy(yyval.string, yyvsp[0].string); }
break;
case 25:
#line 293 "ircd_parser.y"
{ yyval.number = yyvsp[0].number; }
break;
case 26:
#line 297 "ircd_parser.y"
{
		time_t t;

		if ((t = conf_find_time(yyvsp[0].string)) == 0)
		{
			conf_report_error("Unrecognised time type/size '%s'", yyvsp[0].string);
			t = 1;
		}

		yyval.number = yyvsp[-1].number * t;
	}
break;
case 27:
#line 309 "ircd_parser.y"
{
		yyval.number = yyvsp[-1].number + yyvsp[0].number;
	}
break;
case 28:
#line 313 "ircd_parser.y"
{
		yyval.number = yyvsp[-1].number + yyvsp[0].number;
	}
break;
#line 690 "ircd_parser.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    if (yyss)
            free(yyss);
    if (yyvs)
            free(yyvs);
    yyss = yyssp = NULL;
    yyvs = yyvsp = NULL;
    yystacksize = 0;
    return (1);
yyaccept:
    if (yyss)
            free(yyss);
    if (yyvs)
            free(yyvs);
    yyss = yyssp = NULL;
    yyvs = yyvsp = NULL;
    yystacksize = 0;
    return (0);
}
