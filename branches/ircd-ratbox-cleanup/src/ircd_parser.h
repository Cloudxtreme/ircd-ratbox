#define LOADMODULE 257
#define TWODOTS 258
#define QSTRING 259
#define STRING 260
#define NUMBER 261
#ifndef YYSTYPE_DEFINED
#define YYSTYPE_DEFINED
typedef union {
	int 		number;
	char 		string[IRCD_BUFSIZE + 1];
	conf_parm_t *	conf_parm;
} YYSTYPE;
#endif /* YYSTYPE_DEFINED */
extern YYSTYPE yylval;
