
/*  A Bison parser, made from ircd_parser.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	ACCEPT_PASSWORD	257
#define	ACTION	258
#define	ADMIN	259
#define	AFTYPE	260
#define	AUTH	261
#define	AUTOCONN	262
#define	CIPHER_PREFERENCE	263
#define	CLASS	264
#define	COMPRESSED	265
#define	COMPRESSION_LEVEL	266
#define	CONNECT	267
#define	CONNECTFREQ	268
#define	DEFAULT_FLOODCOUNT	269
#define	DENY	270
#define	DESCRIPTION	271
#define	DIE	272
#define	DOTS_IN_IDENT	273
#define	EMAIL	274
#define	ENCRYPTED	275
#define	EXCEED_LIMIT	276
#define	EXEMPT	277
#define	FAKENAME	278
#define	FNAME_USERLOG	279
#define	FNAME_OPERLOG	280
#define	FNAME_FOPERLOG	281
#define	GECOS	282
#define	GLINE	283
#define	GLINES	284
#define	GLINE_EXEMPT	285
#define	GLINE_TIME	286
#define	GLINE_LOG	287
#define	GLOBAL_KILL	288
#define	HAVE_IDENT	289
#define	HOST	290
#define	HUB	291
#define	HUB_MASK	292
#define	IDLETIME	293
#define	IP	294
#define	KILL	295
#define	KLINE	296
#define	KLINE_EXEMPT	297
#define	CRYPTLINK	298
#define	LAZYLINK	299
#define	LEAF_MASK	300
#define	LISTEN	301
#define	LOGGING	302
#define	T_LOGPATH	303
#define	LOG_LEVEL	304
#define	MAX_NUMBER	305
#define	MAXIMUM_LINKS	306
#define	MESSAGE_LOCALE	307
#define	NAME	308
#define	NETWORK_NAME	309
#define	NETWORK_DESC	310
#define	NICK_CHANGES	311
#define	NO_HACK_OPS	312
#define	NO_TILDE	313
#define	NUMBER	314
#define	NUMBER_PER_IP	315
#define	OPERATOR	316
#define	OPER_LOG	317
#define	OPER_UMODES	318
#define	PASSWORD	319
#define	PERSISTANT	320
#define	PING_TIME	321
#define	PORT	322
#define	QSTRING	323
#define	QUARANTINE	324
#define	QUIET_ON_BAN	325
#define	REASON	326
#define	REDIRSERV	327
#define	REDIRPORT	328
#define	REHASH	329
#define	REMOTE	330
#define	RESTRICTED	331
#define	RSA_PUBLIC_KEY	332
#define	RSA_PRIVATE_KEY	333
#define	SENDQ	334
#define	SEND_PASSWORD	335
#define	SERVERINFO	336
#define	SERVLINK_PATH	337
#define	SHARED	338
#define	SPOOF	339
#define	SPOOF_NOTICE	340
#define	TREJECT	341
#define	T_IPV4	342
#define	T_IPV6	343
#define	TNO	344
#define	TYES	345
#define	T_L_CRIT	346
#define	T_L_DEBUG	347
#define	T_L_ERROR	348
#define	T_L_INFO	349
#define	T_L_NOTICE	350
#define	T_L_TRACE	351
#define	T_L_WARN	352
#define	UNKLINE	353
#define	USER	354
#define	VHOST	355
#define	WARN	356
#define	SILENT	357
#define	GENERAL	358
#define	FAILED_OPER_NOTICE	359
#define	ANTI_NICK_FLOOD	360
#define	ANTI_SPAM_EXIT_MESSAGE_TIME	361
#define	MAX_ACCEPT	362
#define	MAX_NICK_TIME	363
#define	MAX_NICK_CHANGES	364
#define	TS_MAX_DELTA	365
#define	TS_WARN_DELTA	366
#define	KLINE_WITH_REASON	367
#define	KLINE_WITH_CONNECTION_CLOSED	368
#define	WARN_NO_NLINE	369
#define	NON_REDUNDANT_KLINES	370
#define	O_LINES_OPER_ONLY	371
#define	WHOIS_WAIT	372
#define	PACE_WAIT	373
#define	CALLER_ID_WAIT	374
#define	KNOCK_DELAY	375
#define	SHORT_MOTD	376
#define	NO_OPER_FLOOD	377
#define	IAUTH_SERVER	378
#define	IAUTH_PORT	379
#define	MODULE	380
#define	MODULES	381
#define	HIDESERVER	382
#define	CLIENT_EXIT	383
#define	T_BOTS	384
#define	T_CCONN	385
#define	T_DEBUG	386
#define	T_DRONE	387
#define	T_FULL	388
#define	T_SKILL	389
#define	T_LOCOPS	390
#define	T_NCHANGE	391
#define	T_REJ	392
#define	T_UNAUTH	393
#define	T_SPY	394
#define	T_EXTERNAL	395
#define	T_OPERWALL	396
#define	T_SERVNOTICE	397
#define	T_INVISIBLE	398
#define	T_CALLERID	399
#define	T_WALLOP	400
#define	OPER_ONLY_UMODES	401
#define	PATH	402
#define	PERSISTANT_EXPIRE_TIME	403
#define	MAX_TARGETS	404
#define	T_MAX_CLIENTS	405
#define	LINKS_DELAY	406
#define	VCHANS_OPER_ONLY	407
#define	MIN_NONWILDCARD	408
#define	DISABLE_VCHANS	409
#define	SECONDS	410
#define	MINUTES	411
#define	HOURS	412
#define	DAYS	413
#define	WEEKS	414
#define	MONTHS	415
#define	YEARS	416
#define	DECADES	417
#define	CENTURIES	418
#define	MILLENNIA	419
#define	BYTES	420
#define	KBYTES	421
#define	MBYTES	422
#define	GBYTES	423
#define	TBYTES	424
#define	NEG	425

#line 24 "ircd_parser.y"


#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

/* XXX */
#define  WE_ARE_MEMORY_C

#include "config.h"
#include "ircd.h"
#include "tools.h"
#include "s_conf.h"
#include "s_log.h"
#include "client.h"	/* for FLAGS_ALL only */
#include "irc_string.h"
#include "ircdauth.h"
#include "memory.h"
#include "modules.h"
#include "s_serv.h" /* for CAP_LL / IsCapable */
#include "hostmask.h"
#include "send.h"
#include "listener.h"

#ifdef HAVE_LIBCRYPTO
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#endif

extern char *ip_string;

int yyparse();

static struct ConfItem *yy_achead = NULL;
#if 0
static struct ConfItem *yy_aconf = NULL;
#endif
static struct ConfItem *yy_aprev = NULL;
static int              yy_acount = 0;
static struct ConfItem *yy_hconf;
static struct ConfItem *yy_lconf;

static struct ConfItem *hub_confs;
static struct ConfItem *leaf_confs;
static struct ConfItem *yy_aconf_next;

static dlink_node *node;

char  *class_name_var;
int   class_ping_time_var;
int   class_number_per_ip_var;
int   class_max_number_var;
int   class_sendq_var;

#ifdef HAVE_LIBCRYPTO
int   rsa_keylen = 0;
char* rsa_pub_ascii = NULL;
#endif

static char  *listener_address;

char  *class_redirserv_var;
int   class_redirport_var;


#line 94 "ircd_parser.y"
typedef union {
        int  number;
        char *string;
        struct ip_value ip_entry;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		979
#define	YYFLAG		-32768
#define	YYNTBASE	183

#define YYTRANSLATE(x) ((unsigned)(x) <= 425 ? yytranslate[x] : 384)

static const short yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,   178,
   179,   173,   172,   182,   171,     2,   174,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,   176,     2,
   181,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,   180,     2,   177,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
    27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
    67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
    77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
    87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
    97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
   107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
   117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
   127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
   137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
   147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
   157,   158,   159,   160,   161,   162,   163,   164,   165,   166,
   167,   168,   169,   170,   175
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     6,     8,    10,    12,    14,    16,    18,
    20,    22,    24,    26,    28,    30,    32,    34,    36,    39,
    42,    44,    47,    50,    53,    56,    59,    62,    65,    68,
    71,    74,    77,    79,    82,    85,    88,    91,    94,    96,
   100,   104,   108,   112,   115,   119,   125,   128,   130,   132,
   134,   136,   141,   146,   152,   155,   157,   159,   161,   163,
   165,   167,   169,   171,   173,   175,   177,   182,   187,   192,
   197,   202,   207,   212,   217,   222,   227,   232,   238,   241,
   243,   245,   247,   249,   251,   256,   261,   266,   272,   275,
   277,   279,   281,   283,   285,   287,   292,   297,   302,   307,
   312,   317,   322,   327,   332,   337,   338,   345,   348,   350,
   352,   354,   356,   358,   360,   362,   364,   366,   368,   370,
   372,   374,   376,   378,   383,   388,   393,   398,   403,   408,
   413,   418,   423,   428,   433,   438,   443,   448,   453,   458,
   463,   468,   473,   478,   483,   488,   489,   496,   499,   501,
   503,   505,   507,   509,   511,   513,   515,   520,   525,   530,
   535,   540,   545,   546,   553,   556,   558,   560,   562,   564,
   566,   571,   576,   581,   582,   589,   592,   594,   596,   598,
   600,   602,   604,   606,   608,   610,   612,   614,   616,   618,
   620,   622,   624,   629,   634,   639,   644,   649,   654,   659,
   664,   669,   674,   679,   684,   689,   694,   699,   704,   709,
   714,   719,   724,   729,   734,   735,   742,   745,   747,   749,
   751,   753,   758,   763,   764,   771,   774,   776,   778,   780,
   782,   784,   789,   794,   799,   800,   807,   810,   812,   814,
   816,   818,   820,   822,   824,   826,   828,   830,   832,   834,
   836,   838,   840,   842,   844,   846,   851,   856,   861,   866,
   871,   876,   881,   886,   891,   896,   901,   906,   907,   915,
   918,   920,   922,   924,   929,   934,   939,   944,   949,   954,
   959,   964,   969,   970,   977,   980,   982,   984,   986,   988,
   993,   998,   999,  1006,  1009,  1011,  1013,  1015,  1017,  1022,
  1027,  1028,  1035,  1038,  1040,  1042,  1044,  1049,  1050,  1057,
  1060,  1062,  1064,  1066,  1068,  1070,  1075,  1080,  1085,  1090,
  1095,  1101,  1104,  1106,  1108,  1110,  1112,  1114,  1116,  1118,
  1120,  1122,  1124,  1126,  1128,  1130,  1132,  1134,  1136,  1138,
  1140,  1142,  1144,  1146,  1148,  1150,  1152,  1154,  1156,  1158,
  1160,  1162,  1164,  1166,  1168,  1170,  1172,  1174,  1176,  1178,
  1180,  1182,  1184,  1186,  1188,  1190,  1192,  1194,  1196,  1198,
  1203,  1208,  1213,  1218,  1223,  1228,  1233,  1238,  1243,  1248,
  1253,  1258,  1263,  1268,  1273,  1278,  1283,  1288,  1293,  1298,
  1303,  1308,  1313,  1318,  1323,  1328,  1333,  1338,  1343,  1348,
  1353,  1358,  1363,  1368,  1373,  1378,  1383,  1388,  1393,  1398,
  1403,  1408,  1413,  1418,  1423,  1428,  1433,  1438,  1443,  1448,
  1453,  1454,  1460,  1464,  1466,  1468,  1470,  1472,  1474,  1476,
  1478,  1480,  1482,  1484,  1486,  1488,  1490,  1492,  1494,  1496,
  1498,  1500,  1501,  1507,  1511,  1513,  1515,  1517,  1519,  1521,
  1523,  1525,  1527,  1529,  1531,  1533,  1535,  1537,  1539,  1541,
  1543,  1545,  1547,  1552,  1557,  1562,  1567,  1572,  1577
};

static const short yyrhs[] = {    -1,
   183,   184,     0,   205,     0,   211,     0,   218,     0,   235,
     0,   245,     0,   252,     0,   193,     0,   270,     0,   276,
     0,   283,     0,   306,     0,   312,     0,   318,     0,   330,
     0,   323,     0,   188,     0,     1,   176,     0,     1,   177,
     0,   187,     0,   187,   156,     0,   187,   157,     0,   187,
   158,     0,   187,   159,     0,   187,   160,     0,   187,   161,
     0,   187,   162,     0,   187,   163,     0,   187,   164,     0,
   187,   165,     0,   185,   185,     0,   187,     0,   187,   166,
     0,   187,   167,     0,   187,   168,     0,   187,   169,     0,
   187,   170,     0,    60,     0,   187,   172,   187,     0,   187,
   171,   187,     0,   187,   173,   187,     0,   187,   174,   187,
     0,   171,   187,     0,   178,   187,   179,     0,   127,   180,
   189,   177,   176,     0,   189,   190,     0,   190,     0,   191,
     0,   192,     0,     1,     0,   126,   181,    69,   176,     0,
   148,   181,    69,   176,     0,    82,   180,   194,   177,   176,
     0,   194,   195,     0,   195,     0,   198,     0,   202,     0,
   204,     0,   199,     0,   200,     0,   201,     0,   203,     0,
   197,     0,   196,     0,     1,     0,    79,   181,    69,   176,
     0,    58,   181,    91,   176,     0,    58,   181,    90,   176,
     0,    54,   181,    69,   176,     0,    17,   181,    69,   176,
     0,    55,   181,    69,   176,     0,    56,   181,    69,   176,
     0,   101,   181,    69,   176,     0,   151,   181,   187,   176,
     0,    37,   181,    91,   176,     0,    37,   181,    90,   176,
     0,     5,   180,   206,   177,   176,     0,   206,   207,     0,
   207,     0,   208,     0,   210,     0,   209,     0,     1,     0,
    54,   181,    69,   176,     0,    20,   181,    69,   176,     0,
    17,   181,    69,   176,     0,    48,   180,   212,   177,   176,
     0,   212,   213,     0,   213,     0,   214,     0,   215,     0,
   216,     0,   217,     0,     1,     0,    49,   181,    69,   176,
     0,    63,   181,    69,   176,     0,    33,   181,    69,   176,
     0,    50,   181,    92,   176,     0,    50,   181,    94,   176,
     0,    50,   181,    98,   176,     0,    50,   181,    96,   176,
     0,    50,   181,    97,   176,     0,    50,   181,    95,   176,
     0,    50,   181,    93,   176,     0,     0,    62,   219,   180,
   220,   177,   176,     0,   220,   221,     0,   221,     0,   222,
     0,   223,     0,   224,     0,   225,     0,   226,     0,   227,
     0,   228,     0,   229,     0,   230,     0,   231,     0,   232,
     0,   233,     0,   234,     0,     1,     0,    54,   181,    69,
   176,     0,   100,   181,    69,   176,     0,    65,   181,    69,
   176,     0,    10,   181,    69,   176,     0,    34,   181,    91,
   176,     0,    34,   181,    90,   176,     0,    76,   181,    91,
   176,     0,    76,   181,    90,   176,     0,    42,   181,    91,
   176,     0,    42,   181,    90,   176,     0,    99,   181,    91,
   176,     0,    99,   181,    90,   176,     0,    29,   181,    91,
   176,     0,    29,   181,    90,   176,     0,    57,   181,    91,
   176,     0,    57,   181,    90,   176,     0,    18,   181,    91,
   176,     0,    18,   181,    90,   176,     0,    75,   181,    91,
   176,     0,    75,   181,    90,   176,     0,     5,   181,    91,
   176,     0,     5,   181,    90,   176,     0,     0,    10,   236,
   180,   237,   177,   176,     0,   237,   238,     0,   238,     0,
   239,     0,   240,     0,   241,     0,   242,     0,   243,     0,
   244,     0,     1,     0,    54,   181,    69,   176,     0,    67,
   181,   185,   176,     0,    61,   181,   187,   176,     0,    14,
   181,   185,   176,     0,    51,   181,   187,   176,     0,    80,
   181,   186,   176,     0,     0,    47,   246,   180,   247,   177,
   176,     0,   247,   248,     0,   248,     0,   249,     0,   250,
     0,   251,     0,     1,     0,    68,   181,   187,   176,     0,
    40,   181,    69,   176,     0,    36,   181,    69,   176,     0,
     0,     7,   253,   180,   254,   177,   176,     0,   254,   255,
     0,   255,     0,   256,     0,   257,     0,   268,     0,   262,
     0,   263,     0,   261,     0,   260,     0,   264,     0,   265,
     0,   259,     0,   258,     0,   266,     0,   267,     0,   269,
     0,     1,     0,   100,   181,    69,   176,     0,    65,   181,
    69,   176,     0,    86,   181,    90,   176,     0,    86,   181,
    91,   176,     0,    85,   181,    69,   176,     0,    22,   181,
    91,   176,     0,    22,   181,    90,   176,     0,    77,   181,
    91,   176,     0,    77,   181,    90,   176,     0,    43,   181,
    91,   176,     0,    43,   181,    90,   176,     0,    35,   181,
    91,   176,     0,    35,   181,    90,   176,     0,    59,   181,
    91,   176,     0,    59,   181,    90,   176,     0,    31,   181,
    91,   176,     0,    31,   181,    90,   176,     0,    73,   181,
    69,   176,     0,    74,   181,   187,   176,     0,    10,   181,
    69,   176,     0,    66,   181,    91,   176,     0,    66,   181,
    90,   176,     0,     0,    70,   271,   180,   272,   177,   176,
     0,   272,   273,     0,   273,     0,   274,     0,   275,     0,
     1,     0,    54,   181,    69,   176,     0,    72,   181,    69,
   176,     0,     0,    84,   277,   180,   278,   177,   176,     0,
   278,   279,     0,   279,     0,   280,     0,   281,     0,   282,
     0,     1,     0,    54,   181,    69,   176,     0,   100,   181,
    69,   176,     0,    36,   181,    69,   176,     0,     0,    13,
   284,   180,   285,   177,   176,     0,   285,   286,     0,   286,
     0,   287,     0,   288,     0,   289,     0,   290,     0,   291,
     0,   292,     0,   293,     0,   294,     0,   303,     0,   304,
     0,   305,     0,   302,     0,   295,     0,   301,     0,   300,
     0,   296,     0,     1,     0,    54,   181,    69,   176,     0,
    36,   181,    69,   176,     0,    81,   181,    69,   176,     0,
     3,   181,    69,   176,     0,    68,   181,   187,   176,     0,
     6,   181,    88,   176,     0,     6,   181,    89,   176,     0,
    24,   181,    69,   176,     0,    45,   181,    91,   176,     0,
    45,   181,    90,   176,     0,    21,   181,    91,   176,     0,
    21,   181,    90,   176,     0,     0,   297,    78,   181,   180,
   298,   177,   176,     0,   299,   298,     0,   299,     0,     1,
     0,    69,     0,    44,   181,    91,   176,     0,    44,   181,
    90,   176,     0,    11,   181,    90,   176,     0,    11,   181,
    91,   176,     0,     8,   181,    91,   176,     0,     8,   181,
    90,   176,     0,    38,   181,    69,   176,     0,    46,   181,
    69,   176,     0,    10,   181,    69,   176,     0,     0,    41,
   307,   180,   308,   177,   176,     0,   308,   309,     0,   309,
     0,   310,     0,   311,     0,     1,     0,   100,   181,    69,
   176,     0,    72,   181,    69,   176,     0,     0,    16,   313,
   180,   314,   177,   176,     0,   314,   315,     0,   315,     0,
   316,     0,   317,     0,     1,     0,    40,   181,    69,   176,
     0,    72,   181,    69,   176,     0,     0,    23,   319,   180,
   320,   177,   176,     0,   320,   321,     0,   321,     0,   322,
     0,     1,     0,    40,   181,    69,   176,     0,     0,    28,
   324,   180,   325,   177,   176,     0,   325,   326,     0,   326,
     0,   327,     0,   328,     0,   329,     0,     1,     0,    54,
   181,    69,   176,     0,    72,   181,    69,   176,     0,     4,
   181,    87,   176,     0,     4,   181,   102,   176,     0,     4,
   181,   103,   176,     0,   104,   180,   331,   177,   176,     0,
   331,   332,     0,   332,     0,   333,     0,   334,     0,   335,
     0,   336,     0,   337,     0,   338,     0,   339,     0,   340,
     0,   342,     0,   344,     0,   346,     0,   347,     0,   364,
     0,   348,     0,   349,     0,   351,     0,   352,     0,   345,
     0,   353,     0,   354,     0,   355,     0,   356,     0,   360,
     0,   362,     0,   363,     0,   366,     0,   365,     0,   361,
     0,   343,     0,   357,     0,   359,     0,   358,     0,   375,
     0,   367,     0,   341,     0,   371,     0,   379,     0,   380,
     0,   350,     0,   383,     0,   381,     0,   382,     0,   368,
     0,   369,     0,   370,     0,     1,     0,   105,   181,    91,
   176,     0,   105,   181,    90,   176,     0,   106,   181,    91,
   176,     0,   106,   181,    90,   176,     0,   109,   181,   185,
   176,     0,   110,   181,   187,   176,     0,   108,   181,   187,
   176,     0,   107,   181,   185,   176,     0,   112,   181,   185,
   176,     0,   111,   181,   185,   176,     0,   152,   181,   185,
   176,     0,   113,   181,    91,   176,     0,   113,   181,    90,
   176,     0,   129,   181,    91,   176,     0,   129,   181,    90,
   176,     0,   114,   181,    91,   176,     0,   114,   181,    90,
   176,     0,    71,   181,    91,   176,     0,    71,   181,    90,
   176,     0,   115,   181,    91,   176,     0,   115,   181,    90,
   176,     0,   116,   181,    91,   176,     0,   116,   181,    90,
   176,     0,   117,   181,    91,   176,     0,   117,   181,    90,
   176,     0,   119,   181,   185,   176,     0,   120,   181,   185,
   176,     0,   118,   181,   185,   176,     0,   121,   181,   185,
   176,     0,   122,   181,    91,   176,     0,   122,   181,    90,
   176,     0,   123,   181,    91,   176,     0,   123,   181,    90,
   176,     0,   124,   181,    69,   176,     0,   125,   181,   187,
   176,     0,    25,   181,    69,   176,     0,    27,   181,    69,
   176,     0,    26,   181,    69,   176,     0,    30,   181,    91,
   176,     0,    30,   181,    90,   176,     0,    53,   181,    69,
   176,     0,    32,   181,   185,   176,     0,    39,   181,   185,
   176,     0,    19,   181,   187,   176,     0,    52,   181,   187,
   176,     0,   128,   181,    91,   176,     0,   128,   181,    90,
   176,     0,   150,   181,   187,   176,     0,    83,   181,    69,
   176,     0,     9,   181,    69,   176,     0,    12,   181,   187,
   176,     0,     0,    64,   372,   181,   373,   176,     0,   373,
   182,   374,     0,   374,     0,   130,     0,   131,     0,   132,
     0,   134,     0,   135,     0,   137,     0,   138,     0,   139,
     0,   140,     0,   141,     0,   142,     0,   143,     0,   144,
     0,   146,     0,   145,     0,   136,     0,   133,     0,     0,
   147,   376,   181,   377,   176,     0,   377,   182,   378,     0,
   378,     0,   130,     0,   131,     0,   132,     0,   134,     0,
   135,     0,   137,     0,   138,     0,   139,     0,   140,     0,
   141,     0,   142,     0,   143,     0,   144,     0,   146,     0,
   145,     0,   136,     0,   133,     0,   153,   181,    91,   176,
     0,   153,   181,    90,   176,     0,   155,   181,    90,   176,
     0,   155,   181,    91,   176,     0,   149,   181,   185,   176,
     0,   154,   181,   187,   176,     0,    15,   181,   187,   176,
     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   267,   268,   271,   272,   273,   274,   275,   276,   277,   278,
   279,   280,   281,   282,   283,   284,   285,   286,   287,   288,
   292,   296,   300,   304,   308,   312,   316,   321,   326,   330,
   334,   338,   345,   349,   353,   357,   361,   365,   372,   376,
   380,   384,   388,   392,   396,   406,   409,   409,   412,   412,
   412,   415,   433,   445,   448,   448,   451,   451,   451,   452,
   452,   453,   453,   454,   454,   455,   458,   505,   509,   516,
   522,   528,   534,   540,   552,   557,   569,   596,   598,   598,
   601,   601,   601,   602,   604,   610,   616,   626,   628,   628,
   631,   631,   631,   632,   632,   635,   639,   643,   647,   649,
   652,   655,   658,   661,   664,   672,   689,   727,   727,   730,
   730,   730,   730,   731,   731,   731,   732,   732,   732,   732,
   733,   733,   733,   735,   741,   773,   779,   785,   789,   795,
   796,   799,   800,   803,   804,   807,   808,   811,   812,   815,
   816,   819,   820,   823,   824,   831,   840,   851,   851,   854,
   854,   855,   856,   857,   858,   859,   862,   868,   873,   878,
   883,   888,   898,   902,   908,   908,   911,   911,   911,   911,
   913,   918,   924,   934,   955,  1002,  1002,  1005,  1005,  1005,
  1005,  1006,  1006,  1006,  1007,  1007,  1007,  1008,  1008,  1009,
  1009,  1009,  1012,  1046,  1053,  1057,  1063,  1074,  1078,  1084,
  1088,  1094,  1098,  1104,  1108,  1114,  1118,  1124,  1128,  1135,
  1142,  1148,  1156,  1160,  1169,  1179,  1185,  1185,  1188,  1188,
  1188,  1190,  1196,  1206,  1219,  1225,  1225,  1228,  1228,  1228,
  1228,  1230,  1236,  1242,  1252,  1279,  1359,  1359,  1362,  1362,
  1362,  1362,  1363,  1363,  1363,  1364,  1364,  1364,  1365,  1365,
  1365,  1366,  1366,  1366,  1367,  1370,  1382,  1388,  1394,  1400,
  1403,  1408,  1415,  1421,  1425,  1431,  1435,  1441,  1450,  1482,
  1482,  1483,  1485,  1505,  1509,  1518,  1527,  1538,  1542,  1548,
  1568,  1588,  1599,  1609,  1622,  1622,  1625,  1625,  1625,  1628,
  1653,  1663,  1673,  1690,  1690,  1693,  1693,  1693,  1696,  1702,
  1712,  1723,  1736,  1736,  1739,  1739,  1741,  1752,  1762,  1779,
  1779,  1782,  1782,  1782,  1782,  1785,  1790,  1796,  1800,  1805,
  1816,  1819,  1819,  1822,  1822,  1823,  1823,  1824,  1824,  1825,
  1826,  1826,  1827,  1828,  1829,  1830,  1830,  1831,  1832,  1833,
  1834,  1834,  1835,  1835,  1836,  1837,  1838,  1838,  1839,  1840,
  1840,  1841,  1841,  1842,  1842,  1843,  1843,  1844,  1845,  1845,
  1846,  1846,  1847,  1847,  1848,  1848,  1849,  1849,  1850,  1852,
  1856,  1862,  1866,  1872,  1877,  1882,  1887,  1892,  1897,  1902,
  1906,  1910,  1916,  1920,  1926,  1930,  1936,  1940,  1946,  1950,
  1956,  1960,  1966,  1970,  1976,  1981,  1986,  1991,  1996,  2000,
  2006,  2010,  2016,  2023,  2030,  2036,  2042,  2048,  2052,  2058,
  2067,  2072,  2077,  2082,  2087,  2091,  2097,  2102,  2109,  2146,
  2165,  2169,  2171,  2171,  2174,  2178,  2182,  2186,  2190,  2194,
  2198,  2202,  2206,  2210,  2214,  2218,  2222,  2226,  2230,  2234,
  2238,  2243,  2247,  2249,  2249,  2252,  2256,  2260,  2264,  2268,
  2272,  2276,  2280,  2284,  2288,  2292,  2296,  2300,  2304,  2308,
  2312,  2316,  2321,  2325,  2331,  2335,  2341,  2345,  2349
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","ACCEPT_PASSWORD",
"ACTION","ADMIN","AFTYPE","AUTH","AUTOCONN","CIPHER_PREFERENCE","CLASS","COMPRESSED",
"COMPRESSION_LEVEL","CONNECT","CONNECTFREQ","DEFAULT_FLOODCOUNT","DENY","DESCRIPTION",
"DIE","DOTS_IN_IDENT","EMAIL","ENCRYPTED","EXCEED_LIMIT","EXEMPT","FAKENAME",
"FNAME_USERLOG","FNAME_OPERLOG","FNAME_FOPERLOG","GECOS","GLINE","GLINES","GLINE_EXEMPT",
"GLINE_TIME","GLINE_LOG","GLOBAL_KILL","HAVE_IDENT","HOST","HUB","HUB_MASK",
"IDLETIME","IP","KILL","KLINE","KLINE_EXEMPT","CRYPTLINK","LAZYLINK","LEAF_MASK",
"LISTEN","LOGGING","T_LOGPATH","LOG_LEVEL","MAX_NUMBER","MAXIMUM_LINKS","MESSAGE_LOCALE",
"NAME","NETWORK_NAME","NETWORK_DESC","NICK_CHANGES","NO_HACK_OPS","NO_TILDE",
"NUMBER","NUMBER_PER_IP","OPERATOR","OPER_LOG","OPER_UMODES","PASSWORD","PERSISTANT",
"PING_TIME","PORT","QSTRING","QUARANTINE","QUIET_ON_BAN","REASON","REDIRSERV",
"REDIRPORT","REHASH","REMOTE","RESTRICTED","RSA_PUBLIC_KEY","RSA_PRIVATE_KEY",
"SENDQ","SEND_PASSWORD","SERVERINFO","SERVLINK_PATH","SHARED","SPOOF","SPOOF_NOTICE",
"TREJECT","T_IPV4","T_IPV6","TNO","TYES","T_L_CRIT","T_L_DEBUG","T_L_ERROR",
"T_L_INFO","T_L_NOTICE","T_L_TRACE","T_L_WARN","UNKLINE","USER","VHOST","WARN",
"SILENT","GENERAL","FAILED_OPER_NOTICE","ANTI_NICK_FLOOD","ANTI_SPAM_EXIT_MESSAGE_TIME",
"MAX_ACCEPT","MAX_NICK_TIME","MAX_NICK_CHANGES","TS_MAX_DELTA","TS_WARN_DELTA",
"KLINE_WITH_REASON","KLINE_WITH_CONNECTION_CLOSED","WARN_NO_NLINE","NON_REDUNDANT_KLINES",
"O_LINES_OPER_ONLY","WHOIS_WAIT","PACE_WAIT","CALLER_ID_WAIT","KNOCK_DELAY",
"SHORT_MOTD","NO_OPER_FLOOD","IAUTH_SERVER","IAUTH_PORT","MODULE","MODULES",
"HIDESERVER","CLIENT_EXIT","T_BOTS","T_CCONN","T_DEBUG","T_DRONE","T_FULL","T_SKILL",
"T_LOCOPS","T_NCHANGE","T_REJ","T_UNAUTH","T_SPY","T_EXTERNAL","T_OPERWALL",
"T_SERVNOTICE","T_INVISIBLE","T_CALLERID","T_WALLOP","OPER_ONLY_UMODES","PATH",
"PERSISTANT_EXPIRE_TIME","MAX_TARGETS","T_MAX_CLIENTS","LINKS_DELAY","VCHANS_OPER_ONLY",
"MIN_NONWILDCARD","DISABLE_VCHANS","SECONDS","MINUTES","HOURS","DAYS","WEEKS",
"MONTHS","YEARS","DECADES","CENTURIES","MILLENNIA","BYTES","KBYTES","MBYTES",
"GBYTES","TBYTES","'-'","'+'","'*'","'/'","NEG","';'","'}'","'('","')'","'{'",
"'='","','","conf","conf_item","timespec","sizespec","expr","modules_entry",
"modules_items","modules_item","modules_module","modules_path","serverinfo_entry",
"serverinfo_items","serverinfo_item","serverinfo_rsa_private_key","serverinfo_no_hack_ops",
"serverinfo_name","serverinfo_description","serverinfo_network_name","serverinfo_network_desc",
"serverinfo_vhost","serverinfo_max_clients","serverinfo_hub","admin_entry","admin_items",
"admin_item","admin_name","admin_email","admin_description","logging_entry",
"logging_items","logging_item","logging_path","logging_oper_log","logging_gline_log",
"logging_log_level","oper_entry","@1","oper_items","oper_item","oper_name","oper_user",
"oper_password","oper_class","oper_global_kill","oper_remote","oper_kline","oper_unkline",
"oper_gline","oper_nick_changes","oper_die","oper_rehash","oper_admin","class_entry",
"@2","class_items","class_item","class_name","class_ping_time","class_number_per_ip",
"class_connectfreq","class_max_number","class_sendq","listen_entry","@3","listen_items",
"listen_item","listen_port","listen_address","listen_host","auth_entry","@4",
"auth_items","auth_item","auth_user","auth_passwd","auth_spoof_notice","auth_spoof",
"auth_exceed_limit","auth_is_restricted","auth_kline_exempt","auth_have_ident",
"auth_no_tilde","auth_gline_exempt","auth_redir_serv","auth_redir_port","auth_class",
"auth_persistant","quarantine_entry","@5","quarantine_items","quarantine_item",
"quarantine_name","quarantine_reason","shared_entry","@6","shared_items","shared_item",
"shared_name","shared_user","shared_host","connect_entry","@7","connect_items",
"connect_item","connect_name","connect_host","connect_send_password","connect_accept_password",
"connect_port","connect_aftype","connect_fakename","connect_lazylink","connect_encrypted",
"connect_pubkey","@8","connect_pubkey_lines","connect_pubkey_line","connect_cryptlink",
"connect_compressed","connect_auto","connect_hub_mask","connect_leaf_mask","connect_class",
"kill_entry","@9","kill_items","kill_item","kill_user","kill_reason","deny_entry",
"@10","deny_items","deny_item","deny_ip","deny_reason","exempt_entry","@11",
"exempt_items","exempt_item","exempt_ip","gecos_entry","@12","gecos_items","gecos_item",
"gecos_name","gecos_reason","gecos_action","general_entry","general_items","general_item",
"general_failed_oper_notice","general_anti_nick_flood","general_max_nick_time",
"general_max_nick_changes","general_max_accept","general_anti_spam_exit_message_time",
"general_ts_warn_delta","general_ts_max_delta","general_links_delay","general_kline_with_reason",
"general_client_exit","general_kline_with_connection_closed","general_quiet_on_ban",
"general_warn_no_nline","general_non_redundant_klines","general_o_lines_oper_only",
"general_pace_wait","general_caller_id_wait","general_whois_wait","general_knock_delay",
"general_short_motd","general_no_oper_flood","general_iauth_server","general_iauth_port",
"general_fname_userlog","general_fname_foperlog","general_fname_operlog","general_glines",
"general_message_locale","general_gline_time","general_idletime","general_dots_in_ident",
"general_maximum_links","general_hide_server","general_max_targets","general_servlink_path",
"general_cipher_preference","general_compression_level","general_oper_umodes",
"@13","umode_oitems","umode_oitem","general_oper_only_umodes","@14","umode_items",
"umode_item","general_vchans_oper_only","general_disable_vchans","general_persistant_expire_time",
"general_min_nonwildcard","general_default_floodcount", NULL
};
#endif

static const short yyr1[] = {     0,
   183,   183,   184,   184,   184,   184,   184,   184,   184,   184,
   184,   184,   184,   184,   184,   184,   184,   184,   184,   184,
   185,   185,   185,   185,   185,   185,   185,   185,   185,   185,
   185,   185,   186,   186,   186,   186,   186,   186,   187,   187,
   187,   187,   187,   187,   187,   188,   189,   189,   190,   190,
   190,   191,   192,   193,   194,   194,   195,   195,   195,   195,
   195,   195,   195,   195,   195,   195,   196,   197,   197,   198,
   199,   200,   201,   202,   203,   204,   204,   205,   206,   206,
   207,   207,   207,   207,   208,   209,   210,   211,   212,   212,
   213,   213,   213,   213,   213,   214,   215,   216,   217,   217,
   217,   217,   217,   217,   217,   219,   218,   220,   220,   221,
   221,   221,   221,   221,   221,   221,   221,   221,   221,   221,
   221,   221,   221,   222,   223,   224,   225,   226,   226,   227,
   227,   228,   228,   229,   229,   230,   230,   231,   231,   232,
   232,   233,   233,   234,   234,   236,   235,   237,   237,   238,
   238,   238,   238,   238,   238,   238,   239,   240,   241,   242,
   243,   244,   246,   245,   247,   247,   248,   248,   248,   248,
   249,   250,   251,   253,   252,   254,   254,   255,   255,   255,
   255,   255,   255,   255,   255,   255,   255,   255,   255,   255,
   255,   255,   256,   257,   258,   258,   259,   260,   260,   261,
   261,   262,   262,   263,   263,   264,   264,   265,   265,   266,
   267,   268,   269,   269,   271,   270,   272,   272,   273,   273,
   273,   274,   275,   277,   276,   278,   278,   279,   279,   279,
   279,   280,   281,   282,   284,   283,   285,   285,   286,   286,
   286,   286,   286,   286,   286,   286,   286,   286,   286,   286,
   286,   286,   286,   286,   286,   287,   288,   289,   290,   291,
   292,   292,   293,   294,   294,   295,   295,   297,   296,   298,
   298,   298,   299,   300,   300,   301,   301,   302,   302,   303,
   304,   305,   307,   306,   308,   308,   309,   309,   309,   310,
   311,   313,   312,   314,   314,   315,   315,   315,   316,   317,
   319,   318,   320,   320,   321,   321,   322,   324,   323,   325,
   325,   326,   326,   326,   326,   327,   328,   329,   329,   329,
   330,   331,   331,   332,   332,   332,   332,   332,   332,   332,
   332,   332,   332,   332,   332,   332,   332,   332,   332,   332,
   332,   332,   332,   332,   332,   332,   332,   332,   332,   332,
   332,   332,   332,   332,   332,   332,   332,   332,   332,   332,
   332,   332,   332,   332,   332,   332,   332,   332,   332,   333,
   333,   334,   334,   335,   336,   337,   338,   339,   340,   341,
   342,   342,   343,   343,   344,   344,   345,   345,   346,   346,
   347,   347,   348,   348,   349,   350,   351,   352,   353,   353,
   354,   354,   355,   356,   357,   358,   359,   360,   360,   361,
   362,   363,   364,   365,   366,   366,   367,   368,   369,   370,
   372,   371,   373,   373,   374,   374,   374,   374,   374,   374,
   374,   374,   374,   374,   374,   374,   374,   374,   374,   374,
   374,   376,   375,   377,   377,   378,   378,   378,   378,   378,
   378,   378,   378,   378,   378,   378,   378,   378,   378,   378,
   378,   378,   379,   379,   380,   380,   381,   382,   383
};

static const short yyr2[] = {     0,
     0,     2,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
     1,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     1,     2,     2,     2,     2,     2,     1,     3,
     3,     3,     3,     2,     3,     5,     2,     1,     1,     1,
     1,     4,     4,     5,     2,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     5,     2,     1,
     1,     1,     1,     1,     4,     4,     4,     5,     2,     1,
     1,     1,     1,     1,     1,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     0,     6,     2,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     0,     6,     2,     1,     1,
     1,     1,     1,     1,     1,     1,     4,     4,     4,     4,
     4,     4,     0,     6,     2,     1,     1,     1,     1,     1,
     4,     4,     4,     0,     6,     2,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     0,     6,     2,     1,     1,     1,
     1,     4,     4,     0,     6,     2,     1,     1,     1,     1,
     1,     4,     4,     4,     0,     6,     2,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     0,     7,     2,
     1,     1,     1,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     0,     6,     2,     1,     1,     1,     1,     4,
     4,     0,     6,     2,     1,     1,     1,     1,     4,     4,
     0,     6,     2,     1,     1,     1,     4,     0,     6,     2,
     1,     1,     1,     1,     1,     4,     4,     4,     4,     4,
     5,     2,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     0,     5,     3,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     0,     5,     3,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     4,     4,     4,     4,     4,     4,     4
};

static const short yydefact[] = {     1,
     0,     0,     0,   174,   146,   235,   292,   301,   308,   283,
   163,     0,   106,   215,     0,   224,     0,     0,     2,    18,
     9,     3,     4,     5,     6,     7,     8,    10,    11,    12,
    13,    14,    15,    17,    16,    19,    20,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,    84,     0,     0,     0,     0,    80,    81,
    83,    82,     0,     0,     0,     0,     0,     0,     0,     0,
    95,     0,     0,     0,     0,     0,    90,    91,    92,    93,
    94,     0,     0,    66,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,    56,    65,    64,    57,    60,    61,
    62,    58,    63,    59,     0,   369,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,   421,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,   442,     0,     0,     0,     0,     0,
     0,     0,   323,   324,   325,   326,   327,   328,   329,   330,
   331,   358,   332,   352,   333,   341,   334,   335,   337,   338,
   362,   339,   340,   342,   343,   344,   345,   353,   355,   354,
   346,   351,   347,   348,   336,   350,   349,   357,   366,   367,
   368,   359,   356,   360,   361,   364,   365,   363,    51,     0,
     0,     0,    48,    49,    50,     0,     0,     0,     0,    79,
   192,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,   177,   178,   179,   188,
   187,   184,   183,   181,   182,   185,   186,   189,   190,   180,
   191,   156,     0,     0,     0,     0,     0,     0,     0,   149,
   150,   151,   152,   153,   154,   155,   255,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,   238,   239,   240,   241,   242,   243,   244,
   245,   246,   251,   254,     0,   253,   252,   250,   247,   248,
   249,   298,     0,     0,     0,   295,   296,   297,   306,     0,
     0,   304,   305,   315,     0,     0,     0,     0,   311,   312,
   313,   314,   289,     0,     0,     0,   286,   287,   288,   170,
     0,     0,     0,     0,   166,   167,   168,   169,     0,     0,
     0,     0,     0,    89,   123,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,   109,
   110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
   120,   121,   122,   221,     0,     0,     0,   218,   219,   220,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    55,   231,     0,     0,     0,     0,   227,   228,   229,   230,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,   322,     0,     0,     0,
    47,     0,     0,     0,    78,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   176,     0,     0,     0,     0,     0,     0,     0,   148,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,   237,     0,     0,     0,     0,
   294,     0,     0,   303,     0,     0,     0,     0,   310,     0,
     0,     0,   285,     0,     0,     0,     0,   165,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,    88,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,   108,     0,     0,     0,   217,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,    39,     0,
     0,     0,    54,     0,     0,     0,     0,   226,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,    21,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,   321,
     0,     0,    46,    87,    86,    85,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,   175,     0,
     0,     0,     0,     0,     0,    33,   147,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,   236,     0,
     0,     0,   293,     0,   302,     0,     0,     0,     0,     0,
   309,     0,     0,   284,     0,     0,     0,   164,    98,    96,
    99,   105,   100,   104,   102,   103,   101,    97,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   107,     0,     0,   216,    71,    77,    76,    70,    72,    73,
    69,    68,    67,    74,    44,     0,     0,     0,     0,     0,
    75,     0,     0,     0,   225,   419,   420,   469,   413,   405,
   407,   406,   409,   408,   411,    32,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,   412,   414,   410,   425,
   426,   427,   441,   428,   429,   440,   430,   431,   432,   433,
   434,   435,   436,   437,   439,   438,     0,   424,   388,   387,
   418,   371,   370,   373,   372,   377,   376,   374,   375,   379,
   378,   382,   381,   386,   385,   390,   389,   392,   391,   394,
   393,   397,   395,   396,   398,   400,   399,   402,   401,   403,
   404,   416,   415,   384,   383,   446,   447,   448,   462,   449,
   450,   461,   451,   452,   453,   454,   455,   456,   457,   458,
   460,   459,     0,   445,   467,   417,   380,   464,   463,   468,
   465,   466,    52,    53,   212,   199,   198,   209,   208,   205,
   204,   203,   202,   207,   206,   194,   214,   213,   210,   211,
   201,   200,   197,   195,   196,   193,   160,   161,   157,   159,
   158,   162,    34,    35,    36,    37,    38,   259,   261,   262,
   279,   278,   282,   276,   277,   267,   266,   263,   257,   280,
   275,   274,   265,   264,   281,   256,   260,   258,     0,   299,
   300,   307,   318,   319,   320,   316,   317,   291,   290,   173,
   172,   171,   145,   144,   127,   141,   140,   137,   136,   129,
   128,   133,   132,   124,   139,   138,   126,   143,   142,   131,
   130,   135,   134,   125,   222,   223,    45,    41,    40,    42,
    43,   234,   232,   233,   422,     0,   443,     0,   272,   273,
     0,     0,   423,   444,     0,   270,   269,     0,     0
};

static const short yydefgoto[] = {     1,
    19,   766,   655,   569,    20,   202,   203,   204,   205,    21,
    94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
   104,    22,    58,    59,    60,    61,    62,    23,    76,    77,
    78,    79,    80,    81,    24,    48,   349,   350,   351,   352,
   353,   354,   355,   356,   357,   358,   359,   360,   361,   362,
   363,    25,    40,   249,   250,   251,   252,   253,   254,   255,
   256,    26,    46,   324,   325,   326,   327,   328,    27,    39,
   226,   227,   228,   229,   230,   231,   232,   233,   234,   235,
   236,   237,   238,   239,   240,   241,    28,    49,   367,   368,
   369,   370,    29,    51,   386,   387,   388,   389,   390,    30,
    41,   273,   274,   275,   276,   277,   278,   279,   280,   281,
   282,   283,   284,   285,   971,   972,   286,   287,   288,   289,
   290,   291,    31,    45,   316,   317,   318,   319,    32,    42,
   295,   296,   297,   298,    33,    43,   301,   302,   303,    34,
    44,   308,   309,   310,   311,   312,    35,   152,   153,   154,
   155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
   165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
   175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
   185,   186,   187,   188,   189,   190,   191,   192,   403,   797,
   798,   193,   429,   853,   854,   194,   195,   196,   197,   198
};

static const short yypact[] = {-32768,
   443,   -46,  -171,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,  -161,-32768,-32768,  -159,-32768,  -145,  -141,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   112,  -136,  -134,
  -131,  -121,  -117,   -63,   -44,   -41,   402,   -39,   -11,   257,
    -7,   308,    17,-32768,    -5,    -2,    68,     6,-32768,-32768,
-32768,-32768,   463,   520,   558,    66,     5,    33,    11,   260,
-32768,    70,    75,    79,    82,    52,-32768,-32768,-32768,-32768,
-32768,   478,   196,-32768,    86,    88,   100,   105,   110,   118,
   120,   123,   129,    19,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,     7,-32768,   133,   135,   145,   150,
   169,   204,   215,   221,   225,   253,   261,   266,-32768,   268,
   273,   311,   312,   316,   322,   337,   345,   352,   365,   371,
   381,   386,   399,   408,   424,   426,   428,   450,   457,   471,
   538,   566,   567,   569,-32768,   570,   572,   573,   574,   575,
   576,    39,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   577,
   578,    96,-32768,-32768,-32768,   138,   144,   165,    61,-32768,
-32768,   579,   580,   581,   582,   583,   584,   585,   586,   587,
   588,   589,   590,   591,   592,   180,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   593,   594,   595,   596,   597,   598,    28,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   599,   600,   601,
   602,   603,   604,   605,   606,   607,   608,   609,   610,   611,
   612,   613,   174,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   277,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   614,   615,    56,-32768,-32768,-32768,-32768,   617,
    10,-32768,-32768,-32768,   618,   619,   620,    27,-32768,-32768,
-32768,-32768,-32768,   621,   622,    37,-32768,-32768,-32768,-32768,
   623,   624,   625,    32,-32768,-32768,-32768,-32768,   224,   290,
   616,   297,    71,-32768,-32768,   626,   627,   628,   629,   630,
   631,   632,   633,   634,   635,   636,   637,   638,   230,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   639,   640,    40,-32768,-32768,-32768,
   323,    43,   405,   496,   530,   109,   532,   656,   -55,   313,
-32768,-32768,   641,   642,   643,    26,-32768,-32768,-32768,-32768,
   662,   -55,   -55,   -55,   668,   674,   680,   185,   -55,   -55,
   -55,   683,   644,   187,   728,   207,   212,   -55,   -55,   -55,
   -55,   -55,   -55,   217,   228,   231,   234,   252,   -55,   -55,
   -55,   -55,   254,   262,   757,   -55,   284,   286,   646,   -55,
   -55,   -55,   293,   -55,   314,   445,-32768,   759,   760,   453,
-32768,   483,   487,   654,-32768,   762,   319,   321,   348,   350,
   355,   763,   391,   764,   -55,   396,   765,   418,   766,   660,
-32768,   -55,   -55,   768,   -55,   -55,   -55,   663,-32768,   769,
   200,   420,   771,   433,   440,   772,   773,   774,   448,   454,
   775,   776,   -55,   777,   671,-32768,   667,   780,   781,   675,
-32768,   783,   677,-32768,   -86,   785,   786,   681,-32768,   787,
   789,   684,-32768,   790,   792,   -55,   686,-32768,   687,   688,
   689,   690,   691,   692,   693,   694,   695,   696,-32768,   460,
   804,   482,   485,   523,   525,   805,   560,   806,   563,   571,
   655,   807,   701,-32768,   809,   810,   704,-32768,   705,   706,
   707,   708,   709,   710,   711,   712,   713,   714,-32768,   -55,
   -55,   191,-32768,   822,   823,   824,   718,-32768,   719,   296,
   304,   328,   720,   721,   722,   723,   724,   -57,   484,   -36,
   343,   725,   535,   726,   727,   729,   730,   731,   732,   733,
    30,   384,    65,   412,   114,   161,   734,   735,   736,   737,
   738,   739,   740,   741,   742,   743,   170,   178,   202,   210,
   744,   745,   746,   747,   748,   419,   749,   750,   751,   752,
   552,   211,   451,   219,   753,   754,   461,   755,   756,-32768,
   758,   761,-32768,-32768,-32768,-32768,   767,   770,   778,   779,
   782,   784,   788,   791,   793,   794,   795,   796,   797,   798,
   799,   544,   800,   801,   802,   803,   808,   811,-32768,   222,
   550,   812,   556,   223,   813,   533,-32768,   814,   815,   816,
   817,   818,   819,   820,   821,   825,   826,   827,   828,   829,
   830,   831,   832,   833,   834,   835,   562,   836,-32768,   315,
   837,   838,-32768,   839,-32768,   840,   841,   842,   843,   844,
-32768,   845,   846,-32768,   847,   848,   568,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   849,   850,
   851,   852,   853,   854,   855,   856,   857,   858,   859,   860,
   861,   862,   863,   864,   865,   866,   867,   868,   869,   870,
-32768,   871,   872,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,    53,   -55,   -55,   -55,   -55,
-32768,   873,   874,   875,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   -55,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,  -172,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   -58,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,    24,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   368,   368,-32768,
-32768,-32768,-32768,-32768,-32768,   535,-32768,   552,-32768,-32768,
   876,     1,-32768,-32768,   878,-32768,-32768,   904,-32768
};

static const short yypgoto[] = {-32768,
-32768,   198,-32768,  -379,-32768,-32768,   877,-32768,-32768,-32768,
-32768,   886,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   880,-32768,-32768,-32768,-32768,-32768,   881,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   649,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   698,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   657,-32768,-32768,-32768,-32768,-32768,
-32768,   715,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   685,
-32768,-32768,-32768,-32768,-32768,   547,-32768,-32768,-32768,-32768,
-32768,-32768,   666,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   -37,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   645,-32768,-32768,-32768,-32768,
-32768,   647,-32768,-32768,-32768,-32768,-32768,   648,-32768,-32768,
-32768,-32768,   651,-32768,-32768,-32768,-32768,-32768,   903,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   -30,-32768,-32768,-32768,   -28,-32768,-32768,-32768,-32768,-32768
};


#define	YYLAST		1079


static const short yytable[] = {   552,
   686,   969,   549,   965,   549,   299,    54,   382,    38,   966,
   299,   313,   560,   561,   562,   687,   688,   199,    47,    84,
    50,   571,    55,   549,   969,    56,   382,   304,   242,   582,
   305,   584,   320,   304,    52,    85,   305,   313,    53,   106,
   364,   243,   383,    63,   300,    64,   606,   107,    65,   300,
   108,   613,    71,   109,   617,    86,   292,   110,    66,    57,
   384,   383,    67,   111,   112,   113,   292,   321,   114,   970,
   115,   322,    87,    88,    89,   642,    90,   116,   244,   384,
   306,   245,   314,   651,    72,   653,   306,   656,   246,   549,
   117,   118,   970,   365,   247,   293,   199,    91,   307,   323,
    73,    74,   119,   677,   307,   293,   385,   248,   314,   120,
   315,   366,    54,   550,    75,   550,    68,   967,   765,    92,
   551,   121,   551,   968,   549,   385,   697,   294,    55,    36,
    37,    56,   540,   541,   550,    69,   315,   294,    70,   777,
    82,   551,   200,   122,   123,   124,   125,   126,   127,   128,
   129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
   139,   140,   141,   142,   201,    57,   143,   144,    83,    93,
   745,   746,   105,   549,   257,   206,   258,  -271,   207,   259,
   211,   260,   209,   261,   262,   145,   493,   146,   147,   212,
   148,   149,   150,   151,   263,   380,   364,   264,   545,   546,
   550,   213,   557,   498,   468,   806,   442,   551,   507,   265,
   214,   266,   443,   502,   215,   436,   537,   267,   268,   269,
   549,   200,   216,   747,   748,   749,   750,   270,   333,   549,
   335,   957,   490,   444,   336,   550,   445,   549,   217,   337,
   808,   271,   551,   201,   218,   219,   519,   338,   208,   365,
   329,  -268,   220,   221,   272,   330,   222,    84,   339,   331,
   320,   549,   332,   340,   223,   224,   371,   366,   372,   549,
   549,   341,   440,    85,   566,   567,   574,   575,   549,   225,
   373,   549,   549,   342,   550,   374,   343,   659,   660,   810,
   375,   551,   509,    86,   344,   321,   577,   578,   376,   322,
   377,   579,   580,   378,   345,   346,   587,   588,   106,   379,
    87,    88,    89,   391,    90,   392,   107,   589,   590,   108,
   591,   592,   109,   593,   594,   393,   110,   323,   347,   348,
   394,   550,   111,   112,   113,    91,   811,   114,   551,   115,
   550,   595,   596,   601,   602,   822,   116,   551,   550,   395,
   485,   603,   604,   823,   487,   551,   460,    92,   510,   117,
   118,   747,   748,   749,   750,   518,   751,   958,   959,   960,
   961,   119,   550,   607,   608,   609,   610,   824,   120,   551,
   550,   550,   615,   616,   396,   825,   855,   551,   551,   550,
   121,   539,   550,   550,   857,   397,   551,   887,   891,   551,
   551,   398,    71,   618,   619,   399,   533,    93,   628,   629,
   630,   631,   122,   123,   124,   125,   126,   127,   128,   129,
   130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
   140,   141,   142,   400,    72,   143,   144,   632,   633,   634,
   635,   401,   978,     2,   636,   637,   402,     3,   404,     4,
    73,    74,     5,   405,   145,     6,   146,   147,     7,   148,
   149,   150,   151,   211,    75,     8,   747,   748,   749,   750,
     9,   757,   212,   542,   747,   748,   749,   750,   335,   758,
   639,   640,   336,    10,   213,   643,   644,   337,   553,    11,
    12,   406,   407,   214,   919,   338,   408,   215,   747,   748,
   749,   750,   409,   759,    13,   216,   339,   646,   647,   661,
   662,   340,    14,   747,   748,   749,   750,   410,   778,   341,
   242,   217,   664,   665,    15,   411,    16,   218,   219,   666,
   667,   342,   412,   243,   343,   220,   221,   671,   672,   222,
   749,   750,   344,   673,   674,   413,    17,   223,   224,   709,
   710,   414,   345,   346,   747,   748,   749,   750,   257,   807,
   258,   415,   225,   259,   543,   260,   416,   261,   262,    18,
   244,   712,   713,   245,   714,   715,   347,   348,   263,   417,
   246,   264,   747,   748,   749,   750,   247,   809,   418,   747,
   748,   749,   750,   265,   831,   266,   568,   570,   544,   248,
   547,   267,   268,   269,   419,   581,   420,   583,   421,   585,
   586,   270,   716,   717,   718,   719,   597,   598,   599,   600,
   620,   747,   748,   749,   750,   271,   856,   612,   623,   614,
   422,   747,   748,   749,   750,  -268,   860,   423,   272,   767,
   768,   769,   770,   771,   772,   773,   774,   775,   776,   721,
   722,   424,   724,   725,   747,   748,   749,   750,   624,   650,
   726,   727,   625,   654,   780,   781,   782,   783,   784,   785,
   786,   787,   788,   789,   790,   791,   792,   793,   794,   795,
   796,   836,   837,   838,   839,   840,   841,   842,   843,   844,
   845,   846,   847,   848,   849,   850,   851,   852,   893,   894,
   895,   896,   897,   747,   748,   749,   750,   511,   512,   513,
   514,   515,   516,   517,   747,   748,   749,   750,   425,   880,
   747,   748,   749,   750,   548,   888,   747,   748,   749,   750,
   559,   890,   747,   748,   749,   750,   563,   917,   747,   748,
   749,   750,   564,   932,   728,   729,   426,   427,   565,   428,
   430,   572,   431,   432,   433,   434,   435,   438,   439,   446,
   447,   448,   449,   450,   451,   452,   453,   454,   455,   456,
   457,   458,   459,   462,   463,   464,   465,   466,   467,   470,
   471,   472,   473,   474,   475,   476,   477,   478,   479,   480,
   481,   482,   483,   484,   488,   489,   576,   492,   495,   496,
   497,   500,   501,   504,   505,   506,   520,   521,   522,   523,
   524,   525,   526,   527,   528,   529,   530,   531,   532,   535,
   536,   554,   555,   556,   573,   605,   611,   621,   622,   626,
   627,   638,   641,   645,   648,   649,   652,   658,   657,   663,
   668,   669,   670,   675,   676,   678,   679,   680,   681,   682,
   683,   684,   685,   689,   690,   692,   691,   693,   695,   694,
   696,   698,   699,   700,   701,   702,   703,   704,   705,   706,
   707,   708,   711,   720,   723,   730,   731,   732,   733,   734,
   735,   736,   737,   738,   739,   740,   741,   742,   743,   744,
   752,   753,   754,   755,   756,   760,   761,   762,   763,   764,
   779,   799,   800,   979,   801,   802,   803,   804,   805,   812,
   813,   814,   815,   816,   817,   818,   819,   820,   821,   826,
   827,   828,   829,   830,   832,   833,   834,   835,   858,   859,
   861,   862,   558,   863,   976,   973,   864,   210,   486,   974,
   461,   491,   865,     0,     0,   866,   469,     0,   494,     0,
     0,     0,     0,   867,   868,     0,   334,   869,   499,   870,
   503,     0,     0,   871,     0,     0,   872,     0,   873,   874,
   875,   876,   877,   878,   879,   881,   882,   883,   884,   381,
   508,     0,     0,   885,     0,     0,   886,   889,   892,   898,
   899,   900,   901,   902,   903,   904,   905,   534,     0,     0,
   906,   907,   908,   909,   910,   911,   912,   913,   914,   915,
   916,   918,   920,   921,   922,   923,   924,   925,   926,   927,
   928,   929,   930,   931,   933,   934,   935,   936,   937,   938,
   939,   940,   941,   942,   943,   944,   945,   946,   947,   948,
   949,   950,   951,   952,   953,   954,   955,   956,   962,   963,
   964,   538,   975,   977,   437,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,   441
};

static const short yycheck[] = {   379,
    87,     1,    60,   176,    60,     1,     1,     1,   180,   182,
     1,     1,   392,   393,   394,   102,   103,     1,   180,     1,
   180,   401,    17,    60,     1,    20,     1,     1,     1,   409,
     4,   411,     1,     1,   180,    17,     4,     1,   180,     1,
     1,    14,    36,   180,    40,   180,   426,     9,   180,    40,
    12,   431,     1,    15,   434,    37,     1,    19,   180,    54,
    54,    36,   180,    25,    26,    27,     1,    36,    30,    69,
    32,    40,    54,    55,    56,   455,    58,    39,    51,    54,
    54,    54,    72,   463,    33,   465,    54,   467,    61,    60,
    52,    53,    69,    54,    67,    40,     1,    79,    72,    68,
    49,    50,    64,   483,    72,    40,   100,    80,    72,    71,
   100,    72,     1,   171,    63,   171,   180,   176,   176,   101,
   178,    83,   178,   182,    60,   100,   506,    72,    17,   176,
   177,    20,    90,    91,   171,   180,   100,    72,   180,   176,
   180,   178,   126,   105,   106,   107,   108,   109,   110,   111,
   112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
   122,   123,   124,   125,   148,    54,   128,   129,   180,   151,
   550,   551,   180,    60,     1,   181,     3,   177,   181,     6,
     1,     8,   177,    10,    11,   147,   177,   149,   150,    10,
   152,   153,   154,   155,    21,   177,     1,    24,    90,    91,
   171,    22,   177,   177,   177,   176,    69,   178,   177,    36,
    31,    38,    69,   177,    35,   177,   177,    44,    45,    46,
    60,   126,    43,   171,   172,   173,   174,    54,   177,    60,
     1,   179,   177,    69,     5,   171,   176,    60,    59,    10,
   176,    68,   178,   148,    65,    66,   176,    18,   181,    54,
   181,    78,    73,    74,    81,   181,    77,     1,    29,   181,
     1,    60,   181,    34,    85,    86,   181,    72,   181,    60,
    60,    42,   177,    17,    90,    91,    90,    91,    60,   100,
   181,    60,    60,    54,   171,   181,    57,    88,    89,   176,
   181,   178,    69,    37,    65,    36,    90,    91,   181,    40,
   181,    90,    91,   181,    75,    76,    90,    91,     1,   181,
    54,    55,    56,   181,    58,   181,     9,    90,    91,    12,
    90,    91,    15,    90,    91,   181,    19,    68,    99,   100,
   181,   171,    25,    26,    27,    79,   176,    30,   178,    32,
   171,    90,    91,    90,    91,   176,    39,   178,   171,   181,
   177,    90,    91,   176,    78,   178,   177,   101,    69,    52,
    53,   171,   172,   173,   174,    69,   176,   747,   748,   749,
   750,    64,   171,    90,    91,    90,    91,   176,    71,   178,
   171,   171,    90,    91,   181,   176,   176,   178,   178,   171,
    83,    69,   171,   171,   176,   181,   178,   176,   176,   178,
   178,   181,     1,    90,    91,   181,   177,   151,    90,    91,
    90,    91,   105,   106,   107,   108,   109,   110,   111,   112,
   113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
   123,   124,   125,   181,    33,   128,   129,    90,    91,    90,
    91,   181,     0,     1,    90,    91,   181,     5,   181,     7,
    49,    50,    10,   181,   147,    13,   149,   150,    16,   152,
   153,   154,   155,     1,    63,    23,   171,   172,   173,   174,
    28,   176,    10,    69,   171,   172,   173,   174,     1,   176,
    90,    91,     5,    41,    22,    90,    91,    10,   176,    47,
    48,   181,   181,    31,   180,    18,   181,    35,   171,   172,
   173,   174,   181,   176,    62,    43,    29,    90,    91,    90,
    91,    34,    70,   171,   172,   173,   174,   181,   176,    42,
     1,    59,    90,    91,    82,   181,    84,    65,    66,    90,
    91,    54,   181,    14,    57,    73,    74,    90,    91,    77,
   173,   174,    65,    90,    91,   181,   104,    85,    86,    90,
    91,   181,    75,    76,   171,   172,   173,   174,     1,   176,
     3,   181,   100,     6,    69,     8,   181,    10,    11,   127,
    51,    90,    91,    54,    90,    91,    99,   100,    21,   181,
    61,    24,   171,   172,   173,   174,    67,   176,   181,   171,
   172,   173,   174,    36,   176,    38,   399,   400,    69,    80,
    69,    44,    45,    46,   181,   408,   181,   410,   181,   412,
   413,    54,    90,    91,    90,    91,   419,   420,   421,   422,
   176,   171,   172,   173,   174,    68,   176,   430,   176,   432,
   181,   171,   172,   173,   174,    78,   176,   181,    81,   156,
   157,   158,   159,   160,   161,   162,   163,   164,   165,    90,
    91,   181,    90,    91,   171,   172,   173,   174,   176,   462,
    90,    91,   176,   466,   130,   131,   132,   133,   134,   135,
   136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
   146,   130,   131,   132,   133,   134,   135,   136,   137,   138,
   139,   140,   141,   142,   143,   144,   145,   146,   166,   167,
   168,   169,   170,   171,   172,   173,   174,    92,    93,    94,
    95,    96,    97,    98,   171,   172,   173,   174,   181,   176,
   171,   172,   173,   174,    69,   176,   171,   172,   173,   174,
    69,   176,   171,   172,   173,   174,    69,   176,   171,   172,
   173,   174,    69,   176,    90,    91,   181,   181,    69,   181,
   181,    69,   181,   181,   181,   181,   181,   181,   181,   181,
   181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
   181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
   181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
   181,   181,   181,   181,   181,   181,    69,   181,   181,   181,
   181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
   181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
   181,   181,   181,   181,   181,    69,   181,    69,    69,   176,
    69,    69,    69,    69,    69,   176,    69,    69,   176,    69,
    69,    69,    69,    69,    69,    69,   176,   181,    69,    69,
   176,    69,   176,    69,    69,    69,   176,    69,    69,   176,
    69,   176,   176,   176,   176,   176,   176,   176,   176,   176,
   176,   176,    69,    69,    69,    69,   176,    69,    69,   176,
   176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
    69,    69,    69,   176,   176,   176,   176,   176,   176,   176,
   176,   176,   176,     0,   176,   176,   176,   176,   176,   176,
   176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
   176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
   176,   176,   386,   176,   972,   966,   176,    58,   273,   968,
   226,   295,   176,    -1,    -1,   176,   249,    -1,   301,    -1,
    -1,    -1,    -1,   176,   176,    -1,    76,   176,   308,   176,
   316,    -1,    -1,   176,    -1,    -1,   176,    -1,   176,   176,
   176,   176,   176,   176,   176,   176,   176,   176,   176,    94,
   324,    -1,    -1,   176,    -1,    -1,   176,   176,   176,   176,
   176,   176,   176,   176,   176,   176,   176,   349,    -1,    -1,
   176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
   176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
   176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
   176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
   176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
   176,   367,   177,   176,   152,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   202
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/local/share/bison.simple"
/* This file comes from bison-1.28.  */

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for malloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
		 and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC malloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     unsigned int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 217 "/usr/local/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;
  int yyfree_stacks = 0;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  if (yyfree_stacks)
	    {
	      free (yyss);
	      free (yyvs);
#ifdef YYLSP_NEEDED
	      free (yyls);
#endif
	    }
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
		   size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
		   size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
		   size * (unsigned int) sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 21:
#line 293 "ircd_parser.y"
{
			yyval.number = yyvsp[0].number;
		;
    break;}
case 22:
#line 297 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number;
		;
    break;}
case 23:
#line 301 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60;
		;
    break;}
case 24:
#line 305 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60;
		;
    break;}
case 25:
#line 309 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24;
		;
    break;}
case 26:
#line 313 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 7;
		;
    break;}
case 27:
#line 317 "ircd_parser.y"
{
			/* a month has 28 days, or 4 weeks */
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 7 * 4;
		;
    break;}
case 28:
#line 322 "ircd_parser.y"
{
			/* a year has 365 days, *not* 12 months */
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 365;
		;
    break;}
case 29:
#line 327 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 365 * 10;
		;
    break;}
case 30:
#line 331 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 365 * 10 * 10;
		;
    break;}
case 31:
#line 335 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 365 * 10 * 10 * 10;
		;
    break;}
case 32:
#line 339 "ircd_parser.y"
{
			/* 2 years 3 days */
			yyval.number = yyvsp[-1].number + yyvsp[0].number;
		;
    break;}
case 33:
#line 346 "ircd_parser.y"
{
			yyval.number = yyvsp[0].number;
		;
    break;}
case 34:
#line 350 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-1].number;
		;
    break;}
case 35:
#line 354 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 1024;
		;
    break;}
case 36:
#line 358 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 1024 * 1024;
		;
    break;}
case 37:
#line 362 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 1024 * 1024 * 1024;
		;
    break;}
case 38:
#line 366 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 1024 * 1024 * 1024;
		;
    break;}
case 39:
#line 373 "ircd_parser.y"
{ 
			yyval.number = yyvsp[0].number;
		;
    break;}
case 40:
#line 377 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-2].number + yyvsp[0].number;
		;
    break;}
case 41:
#line 381 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-2].number - yyvsp[0].number;
		;
    break;}
case 42:
#line 385 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-2].number * yyvsp[0].number;
		;
    break;}
case 43:
#line 389 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-2].number / yyvsp[0].number;
		;
    break;}
case 44:
#line 393 "ircd_parser.y"
{
			yyval.number = -yyvsp[0].number;
		;
    break;}
case 45:
#line 397 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number;
		;
    break;}
case 52:
#line 416 "ircd_parser.y"
{
#ifndef STATIC_MODULES /* NOOP in the static case */
  char *m_bn;

  m_bn = irc_basename(yylval.string);

  /* I suppose we should just ignore it if it is already loaded(since
   * otherwise we would flood the opers on rehash) -A1kmm. */
  if (findmodule_byname(m_bn) != -1)
    return;

  load_one_module (yylval.string);

  MyFree(m_bn);
#endif
;
    break;}
case 53:
#line 434 "ircd_parser.y"
{
#ifndef STATIC_MODULES
  mod_add_path(yylval.string);
#endif
;
    break;}
case 67:
#line 459 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
  BIO *file;

  file = BIO_new_file( yylval.string, "r" );

  if (!file)
  {
    sendto_realops_flags(FLAGS_ALL,
      "Ignoring config file entry rsa_private_key -- file open failed"
      " (%s)", yylval.string);
    break;
  }

  PEM_read_bio_RSAPrivateKey( file, &ServerInfo.rsa_private_key,
                              NULL, NULL );
  if (!ServerInfo.rsa_private_key)
  {
    sendto_realops_flags(FLAGS_ALL,
      "Ignoring config file entry rsa_private_key -- couldn't extract key");
    break;
  }

  if (!RSA_check_key( ServerInfo.rsa_private_key ))
  {
    sendto_realops_flags(FLAGS_ALL,
      "Ignoring config file entry rsa_private_key -- invalid key");
    break;
  }

  /* require 2048 bit (256 byte) key */
  if (RSA_size( ServerInfo.rsa_private_key ) != 256)
  {
    sendto_realops_flags(FLAGS_ALL,
      "Ignoring config file entry rsa_private_key -- not 2048 bit");
    break;
  }

  BIO_set_close(file, BIO_CLOSE);
  BIO_free(file);
#else
  sendto_realops_flags(FLAGS_ALL,
      "Ignoring config file entry rsa_private_key -- no OpenSSL support");
#endif
  ;
    break;}
case 68:
#line 506 "ircd_parser.y"
{
    ServerInfo.no_hack_ops = 1;
  ;
    break;}
case 69:
#line 511 "ircd_parser.y"
{
    ServerInfo.no_hack_ops = 0;
  ;
    break;}
case 70:
#line 517 "ircd_parser.y"
{
    if(ServerInfo.name == NULL)
      DupString(ServerInfo.name,yylval.string);
  ;
    break;}
case 71:
#line 523 "ircd_parser.y"
{
    MyFree(ServerInfo.description);
    DupString(ServerInfo.description,yylval.string);
  ;
    break;}
case 72:
#line 529 "ircd_parser.y"
{
    MyFree(ServerInfo.network_name);
    DupString(ServerInfo.network_name,yylval.string);
  ;
    break;}
case 73:
#line 535 "ircd_parser.y"
{
    MyFree(ServerInfo.network_desc);
    DupString(ServerInfo.network_desc,yylval.string);
  ;
    break;}
case 74:
#line 541 "ircd_parser.y"
{
#ifndef IPV6
/* XXX: Broken for IPv6 */
    if (parse_netmask(yylval.string, &ServerInfo.ip, NULL) == HM_HOST)
    {
     log(L_ERROR, "Invalid netmask for server vhost(%s)", yylval.string);
    }
    ServerInfo.specific_virtual_host = 1;
#endif
  ;
    break;}
case 75:
#line 553 "ircd_parser.y"
{
    ServerInfo.max_clients = yyvsp[-1].number;
  ;
    break;}
case 76:
#line 558 "ircd_parser.y"
{
    /* Don't become a hub if we have a lazylink active. */
    if (!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
    {
      sendto_realops_flags(FLAGS_ALL,
        "Ignoring config file line hub = yes; due to active LazyLink (%s)",
        uplink->name);
    }
    else
      ServerInfo.hub = 1;
  ;
    break;}
case 77:
#line 571 "ircd_parser.y"
{
    /* Don't become a leaf if we have a lazylink active. */
    if (ServerInfo.hub)
    {
      ServerInfo.hub = 0;
      for(node = serv_list.head; node; node = node->next)
      {
        if(MyConnect((struct Client *)node->data) &&
           IsCapable((struct Client *)node->data,CAP_LL))
        {
          sendto_realops_flags(FLAGS_ALL,
            "Ignoring config file line hub = no; due to active LazyLink (%s)",
            ((struct Client *)node->data)->name);
          ServerInfo.hub = 1;
        }
      }
    }
    else
      ServerInfo.hub = 0;
  ;
    break;}
case 85:
#line 605 "ircd_parser.y"
{
    MyFree(AdminInfo.name);
    DupString(AdminInfo.name, yylval.string);
  ;
    break;}
case 86:
#line 611 "ircd_parser.y"
{
    MyFree(AdminInfo.email);
    DupString(AdminInfo.email, yylval.string);
  ;
    break;}
case 87:
#line 617 "ircd_parser.y"
{
    MyFree(AdminInfo.description);
    DupString(AdminInfo.description, yylval.string);
  ;
    break;}
case 96:
#line 636 "ircd_parser.y"
{
                        ;
    break;}
case 97:
#line 640 "ircd_parser.y"
{
                        ;
    break;}
case 98:
#line 644 "ircd_parser.y"
{
                        ;
    break;}
case 99:
#line 648 "ircd_parser.y"
{ set_log_level(L_CRIT); ;
    break;}
case 100:
#line 651 "ircd_parser.y"
{ set_log_level(L_ERROR); ;
    break;}
case 101:
#line 654 "ircd_parser.y"
{ set_log_level(L_WARN); ;
    break;}
case 102:
#line 657 "ircd_parser.y"
{ set_log_level(L_NOTICE); ;
    break;}
case 103:
#line 660 "ircd_parser.y"
{ set_log_level(L_TRACE); ;
    break;}
case 104:
#line 663 "ircd_parser.y"
{ set_log_level(L_INFO); ;
    break;}
case 105:
#line 666 "ircd_parser.y"
{ set_log_level(L_DEBUG); ;
    break;}
case 106:
#line 673 "ircd_parser.y"
{
    struct ConfItem *yy_tmp;

    yy_tmp = yy_achead;
    while(yy_tmp)
      {
        yy_aconf = yy_tmp;
        yy_tmp = yy_tmp->next;
        yy_aconf->next = NULL;
        free_conf(yy_aconf);
      }
    yy_acount = 0;

    yy_achead = yy_aconf = make_conf();
    yy_aconf->status = CONF_OPERATOR;
  ;
    break;}
case 107:
#line 690 "ircd_parser.y"
{
    struct ConfItem *yy_tmp;
    struct ConfItem *yy_next;

    /* copy over settings from first struct */
    for( yy_tmp = yy_achead->next; yy_tmp; yy_tmp = yy_tmp->next )
    {
      if (yy_achead->className)
        DupString(yy_tmp->className, yy_achead->className);
      if (yy_achead->name)
        DupString(yy_tmp->name, yy_achead->name);
      if (yy_achead->passwd)
        DupString(yy_tmp->passwd, yy_achead->passwd);
      yy_tmp->port = yy_achead->port;
    }

    for( yy_tmp = yy_achead; yy_tmp; yy_tmp = yy_next )
      {
        yy_next = yy_tmp->next;
        yy_tmp->next = NULL;

        if(yy_tmp->name && yy_tmp->passwd && yy_tmp->host)
          {
            conf_add_class_to_conf(yy_tmp);
            conf_add_conf(yy_tmp);
          }
        else
          {
            free_conf(yy_tmp);
          }
      }
    yy_achead = NULL;
    yy_aconf = NULL;
    yy_aprev = NULL;
    yy_acount = 0;
  ;
    break;}
case 124:
#line 736 "ircd_parser.y"
{
    MyFree(yy_achead->name);
    DupString(yy_achead->name, yylval.string);
  ;
    break;}
case 125:
#line 742 "ircd_parser.y"
{
    char *p;
    char *new_user;
    char *new_host;

    /* The first user= line doesn't allocate a new struct */
    if ( yy_acount++ )
    {
      yy_aconf = (yy_aconf->next = make_conf());
      yy_aconf->status = CONF_OPERATOR;
    }

    if((p = strchr(yylval.string,'@')))
      {
	*p = '\0';
	DupString(new_user,yylval.string);
	MyFree(yy_aconf->user);
	yy_aconf->user = new_user;
	p++;
	DupString(new_host,p);
	MyFree(yy_aconf->host);
	yy_aconf->host = new_host;
      }
    else
      {
	MyFree(yy_aconf->host);
   	DupString(yy_aconf->host, yylval.string);
   	DupString(yy_aconf->user,"*");
      }
  ;
    break;}
case 126:
#line 774 "ircd_parser.y"
{
    MyFree(yy_achead->passwd);
    DupString(yy_achead->passwd, yylval.string);
  ;
    break;}
case 127:
#line 780 "ircd_parser.y"
{
    MyFree(yy_achead->className);
    DupString(yy_achead->className, yylval.string);
  ;
    break;}
case 128:
#line 786 "ircd_parser.y"
{
    yy_achead->port |= CONF_OPER_GLOBAL_KILL;
  ;
    break;}
case 129:
#line 791 "ircd_parser.y"
{
    yy_achead->port &= ~CONF_OPER_GLOBAL_KILL;
  ;
    break;}
case 130:
#line 795 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_REMOTE;;
    break;}
case 131:
#line 797 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_REMOTE; ;
    break;}
case 132:
#line 799 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_K;;
    break;}
case 133:
#line 801 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_K; ;
    break;}
case 134:
#line 803 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_UNKLINE;;
    break;}
case 135:
#line 805 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_UNKLINE; ;
    break;}
case 136:
#line 807 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_GLINE;;
    break;}
case 137:
#line 809 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_GLINE; ;
    break;}
case 138:
#line 811 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_N;;
    break;}
case 139:
#line 813 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_N;;
    break;}
case 140:
#line 815 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_DIE; ;
    break;}
case 141:
#line 817 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_DIE; ;
    break;}
case 142:
#line 819 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_REHASH;;
    break;}
case 143:
#line 821 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_REHASH; ;
    break;}
case 144:
#line 823 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_ADMIN;;
    break;}
case 145:
#line 825 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_ADMIN;;
    break;}
case 146:
#line 832 "ircd_parser.y"
{
    MyFree(class_name_var);
    class_name_var = NULL;
    class_ping_time_var = 0;
    class_number_per_ip_var = 0;
    class_max_number_var = 0;
    class_sendq_var = 0;
  ;
    break;}
case 147:
#line 841 "ircd_parser.y"
{

    add_class(class_name_var,class_ping_time_var,
              class_number_per_ip_var, class_max_number_var,
              class_sendq_var );

    MyFree(class_name_var);
    class_name_var = NULL;
  ;
    break;}
case 157:
#line 863 "ircd_parser.y"
{
    MyFree(class_name_var);
    DupString(class_name_var, yylval.string);
  ;
    break;}
case 158:
#line 869 "ircd_parser.y"
{
    class_ping_time_var = yyvsp[-1].number;
  ;
    break;}
case 159:
#line 874 "ircd_parser.y"
{
    class_number_per_ip_var = yyvsp[-1].number;
  ;
    break;}
case 160:
#line 879 "ircd_parser.y"
{
    class_number_per_ip_var = yyvsp[-1].number;
  ;
    break;}
case 161:
#line 884 "ircd_parser.y"
{
    class_max_number_var = yyvsp[-1].number;
  ;
    break;}
case 162:
#line 889 "ircd_parser.y"
{
    class_sendq_var = yyvsp[-1].number;
  ;
    break;}
case 163:
#line 899 "ircd_parser.y"
{
    listener_address = NULL;
  ;
    break;}
case 164:
#line 903 "ircd_parser.y"
{
    MyFree(listener_address);
    listener_address = NULL;
  ;
    break;}
case 171:
#line 914 "ircd_parser.y"
{
    add_listener(yyvsp[-1].number, listener_address);
  ;
    break;}
case 172:
#line 919 "ircd_parser.y"
{
    MyFree(listener_address);
    DupString(listener_address, yylval.string);
  ;
    break;}
case 173:
#line 925 "ircd_parser.y"
{
    MyFree(listener_address);
    DupString(listener_address, yylval.string);
  ;
    break;}
case 174:
#line 935 "ircd_parser.y"
{
    struct ConfItem *yy_tmp;

    yy_tmp = yy_achead;
    while(yy_tmp)
      {
        yy_aconf = yy_tmp;
        yy_tmp = yy_tmp->next;
        yy_aconf->next = NULL;
        free_conf(yy_aconf);
      }
    yy_achead = NULL;
    yy_aconf = NULL;
    yy_aprev = NULL;

    yy_achead = yy_aprev = yy_aconf = make_conf();
    yy_aconf->status = CONF_CLIENT;
    yy_achead->className = NULL;
    yy_acount = 0;
  ;
    break;}
case 175:
#line 956 "ircd_parser.y"
{
    struct ConfItem *yy_tmp;
    struct ConfItem *yy_next;

    /* copy over settings from first struct */
    for( yy_tmp = yy_achead->next; yy_tmp; yy_tmp = yy_tmp->next )
    {
      if(yy_achead->passwd)
        DupString(yy_tmp->passwd, yy_achead->passwd);
      if(yy_achead->name)
        DupString(yy_tmp->name, yy_achead->name);
      if(yy_achead->className)
        DupString(yy_tmp->className, yy_achead->className);

      yy_tmp->flags = yy_achead->flags;
      yy_tmp->port  = yy_achead->port;
    }

    for( yy_tmp = yy_achead; yy_tmp; yy_tmp = yy_next )
    {
      yy_next = yy_tmp->next; /* yy_tmp->next is used by conf_add_conf */
      yy_tmp->next = NULL;

      if (yy_tmp->name == NULL)
       DupString(yy_tmp->name,"NOMATCH");

      conf_add_class_to_conf(yy_tmp);

      if (yy_tmp->user == NULL)
       DupString(yy_tmp->user,"*");
      else
       (void)collapse(yy_tmp->user);

      if (yy_tmp->host == NULL)
       continue;
      else
        (void)collapse(yy_tmp->host);

      add_conf_by_address(yy_tmp->host, CONF_CLIENT, yy_tmp->user, yy_tmp);
    }
    yy_achead = NULL;
    yy_aconf = NULL;
    yy_aprev = NULL;
    yy_acount = 0;
  ;
    break;}
case 193:
#line 1013 "ircd_parser.y"
{
    char *p;
    char *new_user;
    char *new_host;

    /* The first user= line doesn't allocate a new struct */
    if ( yy_acount++ )
    {
      yy_aprev = yy_aconf;
      yy_aconf = (yy_aconf->next = make_conf());
      yy_aconf->status = CONF_CLIENT;
    }

    if((p = strchr(yylval.string,'@')))
      {
	*p = '\0';
	DupString(new_user, yylval.string);
	MyFree(yy_aconf->user);
	yy_aconf->user = new_user;
	p++;
	MyFree(yy_aconf->host);
	DupString(new_host,p);
	yy_aconf->host = new_host;
      }
    else
      {
	MyFree(yy_aconf->host);
	DupString(yy_aconf->host, yylval.string);
	DupString(yy_aconf->user,"*");
      }
  ;
    break;}
case 194:
#line 1047 "ircd_parser.y"
{
    MyFree(yy_achead->passwd);
    DupString(yy_achead->passwd,yylval.string);
  ;
    break;}
case 195:
#line 1054 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_SPOOF_NOTICE;
  ;
    break;}
case 196:
#line 1059 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_SPOOF_NOTICE;
  ;
    break;}
case 197:
#line 1064 "ircd_parser.y"
{
    MyFree(yy_achead->name);
    if(strlen(yylval.string) < HOSTLEN)
    {    
	DupString(yy_achead->name, yylval.string);
    	yy_achead->flags |= CONF_FLAGS_SPOOF_IP;
    } else
	log(L_ERROR, "Spoofs must be less than %d..ignoring it", HOSTLEN);
  ;
    break;}
case 198:
#line 1075 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_NOLIMIT;
  ;
    break;}
case 199:
#line 1080 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_NOLIMIT;
  ;
    break;}
case 200:
#line 1085 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_RESTRICTED;
  ;
    break;}
case 201:
#line 1090 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_RESTRICTED;
  ;
    break;}
case 202:
#line 1095 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_EXEMPTKLINE;
  ;
    break;}
case 203:
#line 1100 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_EXEMPTKLINE;
  ;
    break;}
case 204:
#line 1105 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_NEED_IDENTD;
  ;
    break;}
case 205:
#line 1110 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_NEED_IDENTD;
  ;
    break;}
case 206:
#line 1115 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_NO_TILDE;
  ;
    break;}
case 207:
#line 1120 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_NO_TILDE;
  ;
    break;}
case 208:
#line 1125 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_EXEMPTGLINE;
  ;
    break;}
case 209:
#line 1130 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_EXEMPTGLINE;
  ;
    break;}
case 210:
#line 1136 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_REDIR;
    MyFree(yy_achead->name);
    DupString(yy_achead->name, yylval.string);
  ;
    break;}
case 211:
#line 1143 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_REDIR;
    yy_achead->port = yyvsp[-1].number;
  ;
    break;}
case 212:
#line 1149 "ircd_parser.y"
{
    if (yy_achead->className == NULL)
      {
	DupString(yy_achead->className, yylval.string);
      }
  ;
    break;}
case 213:
#line 1157 "ircd_parser.y"
{
   yy_achead->flags |= CONF_FLAGS_PERSISTANT;
  ;
    break;}
case 214:
#line 1161 "ircd_parser.y"
{
   yy_achead->flags &= CONF_FLAGS_PERSISTANT;
  ;
    break;}
case 215:
#line 1170 "ircd_parser.y"
{
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_QUARANTINED_NICK;
  ;
    break;}
case 216:
#line 1180 "ircd_parser.y"
{
    conf_add_q_conf(yy_aconf);
    yy_aconf = (struct ConfItem *)NULL;
  ;
    break;}
case 222:
#line 1191 "ircd_parser.y"
{
    MyFree(yy_aconf->name);
    DupString(yy_aconf->name, yylval.string);
  ;
    break;}
case 223:
#line 1197 "ircd_parser.y"
{
    MyFree(yy_aconf->passwd);
    DupString(yy_aconf->passwd, yylval.string);
  ;
    break;}
case 224:
#line 1207 "ircd_parser.y"
{
    if(yy_aconf != NULL)
      {
        free_conf(yy_aconf);
        yy_aconf = NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_ULINE;
    yy_aconf->name = NULL;
    yy_aconf->user = NULL;
    yy_aconf->host = NULL;
  ;
    break;}
case 225:
#line 1220 "ircd_parser.y"
{
    conf_add_u_conf(yy_aconf);
    yy_aconf = (struct ConfItem *)NULL;
  ;
    break;}
case 232:
#line 1231 "ircd_parser.y"
{
    MyFree(yy_aconf->name);
    DupString(yy_aconf->name, yylval.string);
  ;
    break;}
case 233:
#line 1237 "ircd_parser.y"
{
    MyFree(yy_aconf->user);
    DupString(yy_aconf->user, yylval.string);
  ;
    break;}
case 234:
#line 1243 "ircd_parser.y"
{
    MyFree(yy_aconf->host);
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 235:
#line 1253 "ircd_parser.y"
{
    hub_confs = (struct ConfItem *)NULL;

    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }

    if(yy_hconf)
      {
        free_conf(yy_hconf);
        yy_hconf = (struct ConfItem *)NULL;
      }

    if(yy_lconf)
      {
	free_conf(yy_lconf);
	yy_lconf = (struct ConfItem *)NULL;
      }

    yy_aconf=make_conf();
    yy_aconf->passwd = NULL;
    /* Finally we can do this -A1kmm. */
    yy_aconf->status = CONF_SERVER;
  ;
    break;}
case 236:
#line 1280 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
    if(yy_aconf->host &&
       ((yy_aconf->passwd && yy_aconf->spasswd) ||
        (yy_aconf->rsa_public_key && IsConfCryptLink(yy_aconf))))
#else /* !HAVE_LIBCRYPTO */
    if(yy_aconf->host && !IsConfCryptLink(yy_aconf) && 
       yy_aconf->passwd && yy_aconf->spasswd)
#endif /* !HAVE_LIBCRYPTO */
      {
        if( conf_add_server(yy_aconf, scount) >= 0 )
	  {
	    conf_add_conf(yy_aconf);
	    ++scount;
	  }
	else
	  {
	    free_conf(yy_aconf);
	    yy_aconf = NULL;
	  }
      }
    else
      {
#ifndef HAVE_LIBCRYPTO
        if (IsConfCryptLink(yy_aconf) && yy_aconf->name)
          sendto_realops_flags(FLAGS_ALL,
            "Ignoring connect block for %s -- no OpenSSL support",
            yy_aconf->name);
#endif        
        free_conf(yy_aconf);
        yy_aconf = NULL;
      }

    /*
     * yy_aconf is still pointing at the server that is having
     * a connect block built for it. This means, y_aconf->name 
     * points to the actual irc name this server will be known as.
     * Now this new server has a set or even just one hub_mask (or leaf_mask)
     * given in the link list at yy_hconf. Fill in the HUB confs
     * from this link list now.
     */        
    for (yy_hconf = hub_confs; yy_hconf; yy_hconf = yy_aconf_next)
      {
	yy_aconf_next = yy_hconf->next;
	MyFree(yy_hconf->name);
	yy_hconf->name = NULL;

	/* yy_aconf == NULL is a fatal error for this connect block! */
	if (yy_aconf != NULL)
	  {
	    DupString(yy_hconf->name, yy_aconf->name);
	    conf_add_conf(yy_hconf);
	  }
	else
	  free_conf(yy_hconf);
      }

    /* Ditto for the LEAF confs */

    for (yy_lconf = leaf_confs; yy_lconf; yy_lconf = yy_aconf_next)
      {
	yy_aconf_next = yy_lconf->next;
	if (yy_aconf != NULL)
	  {
	    DupString(yy_lconf->name, yy_aconf->name);
	    conf_add_conf(yy_lconf);
	  }
	else
	  free_conf(yy_lconf);
      }

    hub_confs = NULL;
    leaf_confs = NULL;

    yy_aconf = NULL;
    yy_hconf = NULL;
    yy_lconf = NULL;
  ;
    break;}
case 256:
#line 1371 "ircd_parser.y"
{
    if(yy_aconf->name != NULL)
      {
	sendto_realops_flags(FLAGS_ALL,"*** Multiple connect name entry");
	log(L_WARN, "Multiple connect name entry %s", yy_aconf->name);
      }

    MyFree(yy_aconf->name);
    DupString(yy_aconf->name, yylval.string);
  ;
    break;}
case 257:
#line 1383 "ircd_parser.y"
{
    MyFree(yy_aconf->host);
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 258:
#line 1389 "ircd_parser.y"
{
    MyFree(yy_aconf->spasswd);
    DupString(yy_aconf->spasswd, yylval.string);
  ;
    break;}
case 259:
#line 1395 "ircd_parser.y"
{
    MyFree(yy_aconf->passwd);
    DupString(yy_aconf->passwd, yylval.string);
  ;
    break;}
case 260:
#line 1400 "ircd_parser.y"
{ yy_aconf->port = yyvsp[-1].number; ;
    break;}
case 261:
#line 1404 "ircd_parser.y"
{
    yy_aconf->aftype = AF_INET;
#ifdef IPV6
  ;
    break;}
case 262:
#line 1410 "ircd_parser.y"
{
    yy_aconf->aftype = AF_INET6;
#endif
  ;
    break;}
case 263:
#line 1416 "ircd_parser.y"
{
    MyFree(yy_aconf->fakename);
    DupString(yy_aconf->fakename, yylval.string);
 ;
    break;}
case 264:
#line 1422 "ircd_parser.y"
{
    yy_aconf->flags |= CONF_FLAGS_LAZY_LINK;
  ;
    break;}
case 265:
#line 1427 "ircd_parser.y"
{
    yy_aconf->flags &= ~CONF_FLAGS_LAZY_LINK;
  ;
    break;}
case 266:
#line 1432 "ircd_parser.y"
{
    yy_aconf->flags |= CONF_FLAGS_ENCRYPTED;
  ;
    break;}
case 267:
#line 1437 "ircd_parser.y"
{
    yy_aconf->flags &= ~CONF_FLAGS_ENCRYPTED;
  ;
    break;}
case 268:
#line 1442 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
    rsa_keylen = 0;
    if (rsa_pub_ascii)
      MyFree(rsa_pub_ascii);
    rsa_pub_ascii = 0;
#endif
  ;
    break;}
case 269:
#line 1451 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
    BIO *mem;

    mem = BIO_new_mem_buf( rsa_pub_ascii, rsa_keylen +5 );

    if (!mem)
    {
      sendto_realops_flags(FLAGS_ALL,
        "Ignoring config file entry rsa_public_key -- BIO open failed");
      break;
    }

    yy_aconf->rsa_public_key = PEM_read_bio_RSA_PUBKEY( mem, NULL, NULL, NULL );

    if (!yy_aconf->rsa_public_key)
    {
      sendto_realops_flags(FLAGS_ALL,
        "Ignoring config file entry rsa_public_key -- couldn't extract key");
      break;
    }

    BIO_set_close(mem, BIO_CLOSE);
    BIO_free(mem);

    rsa_keylen = 0;
    MyFree(rsa_pub_ascii);
    rsa_pub_ascii = 0;
#endif /* HAVE_LIBCRYPTO */
  ;
    break;}
case 273:
#line 1486 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
    if (rsa_keylen == 0)
    {
      rsa_keylen += strlen(yylval.string) + 2; /* '\0' */
      rsa_pub_ascii = MyMalloc(rsa_keylen);
      strcpy(rsa_pub_ascii, yylval.string);
      strcat(rsa_pub_ascii, "\n");                                    
    }
    else
    {
      rsa_keylen += strlen(yylval.string) + 1;
      rsa_pub_ascii = MyRealloc(rsa_pub_ascii, rsa_keylen);
      strcat(rsa_pub_ascii, yylval.string);
      strcat(rsa_pub_ascii, "\n");
    }
#endif
  ;
    break;}
case 274:
#line 1506 "ircd_parser.y"
{
    yy_aconf->flags |= CONF_FLAGS_CRYPTLINK;
  ;
    break;}
case 275:
#line 1511 "ircd_parser.y"
{
    yy_aconf->flags &= ~CONF_FLAGS_CRYPTLINK;
  ;
    break;}
case 276:
#line 1519 "ircd_parser.y"
{
#ifndef HAVE_LIBZ
    sendto_realops_flags(FLAGS_ALL,
      "Ignoring compressed = no; -- no zlib support");
#else
    yy_aconf->flags |= CONF_FLAGS_NOCOMPRESSED;
#endif
  ;
    break;}
case 277:
#line 1529 "ircd_parser.y"
{
#ifndef HAVE_LIBZ
    sendto_realops_flags(FLAGS_ALL,
      "Ignoring compressed = yes; -- no zlib support");
#else
    yy_aconf->flags &= ~CONF_FLAGS_NOCOMPRESSED;
#endif
  ;
    break;}
case 278:
#line 1539 "ircd_parser.y"
{
    yy_aconf->flags |= CONF_FLAGS_ALLOW_AUTO_CONN;
  ;
    break;}
case 279:
#line 1544 "ircd_parser.y"
{
    yy_aconf->flags &= ~CONF_FLAGS_ALLOW_AUTO_CONN;
  ;
    break;}
case 280:
#line 1549 "ircd_parser.y"
{
    if(hub_confs == NULL)
      {
	hub_confs = make_conf();
	hub_confs->status = CONF_HUB;
	DupString(hub_confs->host, yylval.string);
	DupString(hub_confs->user, "*");
      }
    else
      {
	yy_hconf = make_conf();
	yy_hconf->status = CONF_HUB;
	DupString(yy_hconf->host, yylval.string);
	DupString(yy_hconf->user, "*");
	yy_hconf->next = hub_confs;
	hub_confs = yy_hconf;
      }
  ;
    break;}
case 281:
#line 1569 "ircd_parser.y"
{
    if(leaf_confs == NULL)
      {
	leaf_confs = make_conf();
	leaf_confs->status = CONF_LEAF;
	DupString(leaf_confs->host, yylval.string);
	DupString(leaf_confs->user, "*");
      }
    else
      {
	yy_lconf = make_conf();
	yy_lconf->status = CONF_LEAF;
	DupString(yy_lconf->host, yylval.string);
	DupString(yy_lconf->user, "*");
	yy_lconf->next = leaf_confs;
	leaf_confs = yy_lconf;
      }
  ;
    break;}
case 282:
#line 1589 "ircd_parser.y"
{
    MyFree(yy_aconf->className);
    DupString(yy_aconf->className, yylval.string);
  ;
    break;}
case 283:
#line 1600 "ircd_parser.y"
{
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_KILL;
  ;
    break;}
case 284:
#line 1610 "ircd_parser.y"
{
    if(yy_aconf->user && yy_aconf->passwd && yy_aconf->host)
      {
        conf_add_k_conf(yy_aconf);
      }
    else
      {
        free_conf(yy_aconf);
      }
    yy_aconf = NULL;
  ;
    break;}
case 290:
#line 1629 "ircd_parser.y"
{
    char *p;
    char *new_user;
    char *new_host;

    if((p = strchr(yylval.string,'@')))
      {
	*p = '\0';
	DupString(new_user,yylval.string);
	yy_aconf->user = new_user;
	p++;
	DupString(new_host,p);
	MyFree(yy_aconf->host);
	yy_aconf->host = new_host;
      }
    else
      {
	MyFree(yy_aconf->host);
	DupString(yy_aconf->host, yylval.string);
	MyFree(yy_aconf->user);
	DupString(yy_aconf->user,"*");
      }
  ;
    break;}
case 291:
#line 1654 "ircd_parser.y"
{
    MyFree(yy_aconf->passwd);
    DupString(yy_aconf->passwd, yylval.string);
  ;
    break;}
case 292:
#line 1664 "ircd_parser.y"
{
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_DLINE;
  ;
    break;}
case 293:
#line 1674 "ircd_parser.y"
{
   if (yy_aconf->host &&
      parse_netmask(yy_aconf->host, NULL, NULL) != HM_HOST)
   {
    if (yy_aconf->passwd == NULL)
	{
	 DupString(yy_aconf->passwd,"NO REASON");
	}
    add_conf_by_address(yy_aconf->host, CONF_DLINE, NULL, yy_aconf);
   } else
   {
    free_conf(yy_aconf);
   }
   yy_aconf = (struct ConfItem *)NULL;
  ;
    break;}
case 299:
#line 1697 "ircd_parser.y"
{
    char *p;
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 300:
#line 1703 "ircd_parser.y"
{
    MyFree(yy_aconf->passwd);
    DupString(yy_aconf->passwd, yylval.string);
  ;
    break;}
case 301:
#line 1713 "ircd_parser.y"
{
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    DupString(yy_aconf->passwd, "*");
    yy_aconf->status = CONF_EXEMPTDLINE;
  ;
    break;}
case 302:
#line 1724 "ircd_parser.y"
{
   if (yy_aconf->host &&
      parse_netmask(yy_aconf->host, NULL, NULL) != HM_HOST)
   {
    add_conf_by_address(yy_aconf->host, CONF_EXEMPTDLINE, NULL, yy_aconf);
   } else
   {
    free_conf(yy_aconf);
   }
   yy_aconf = (struct ConfItem *)NULL;
  ;
    break;}
case 307:
#line 1742 "ircd_parser.y"
{
    char *p;
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 308:
#line 1753 "ircd_parser.y"
{
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_XLINE;
  ;
    break;}
case 309:
#line 1763 "ircd_parser.y"
{
    if(yy_aconf->host)
      {
	if(yy_aconf->passwd == NULL)
	  {
	    DupString(yy_aconf->passwd,"Something about your name");
	  }
        conf_add_x_conf(yy_aconf);
      }
    else
      {
        free_conf(yy_aconf);
      }
    yy_aconf = (struct ConfItem *)NULL;
  ;
    break;}
case 316:
#line 1786 "ircd_parser.y"
{
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 317:
#line 1791 "ircd_parser.y"
{
    MyFree(yy_aconf->passwd);
    DupString(yy_aconf->passwd, yylval.string);
  ;
    break;}
case 318:
#line 1797 "ircd_parser.y"
{
    yy_aconf->port = 1;
  ;
    break;}
case 319:
#line 1802 "ircd_parser.y"
{
    yy_aconf->port = 0;
  ;
    break;}
case 320:
#line 1807 "ircd_parser.y"
{
    yy_aconf->port = 2;
  ;
    break;}
case 370:
#line 1853 "ircd_parser.y"
{
    ConfigFileEntry.failed_oper_notice = 1;
  ;
    break;}
case 371:
#line 1858 "ircd_parser.y"
{
    ConfigFileEntry.failed_oper_notice = 0;
  ;
    break;}
case 372:
#line 1863 "ircd_parser.y"
{
    ConfigFileEntry.anti_nick_flood = 1;
  ;
    break;}
case 373:
#line 1868 "ircd_parser.y"
{
    ConfigFileEntry.anti_nick_flood = 0;
  ;
    break;}
case 374:
#line 1873 "ircd_parser.y"
{
    ConfigFileEntry.max_nick_time = yyvsp[-1].number; 
  ;
    break;}
case 375:
#line 1878 "ircd_parser.y"
{
    ConfigFileEntry.max_nick_changes = yyvsp[-1].number;
  ;
    break;}
case 376:
#line 1883 "ircd_parser.y"
{
    ConfigFileEntry.max_accept = yyvsp[-1].number;
  ;
    break;}
case 377:
#line 1888 "ircd_parser.y"
{
    ConfigFileEntry.anti_spam_exit_message_time = yyvsp[-1].number;
  ;
    break;}
case 378:
#line 1893 "ircd_parser.y"
{
    ConfigFileEntry.ts_warn_delta = yyvsp[-1].number;
  ;
    break;}
case 379:
#line 1898 "ircd_parser.y"
{
    ConfigFileEntry.ts_max_delta = yyvsp[-1].number;
  ;
    break;}
case 380:
#line 1903 "ircd_parser.y"
{
    ConfigFileEntry.links_delay = yyvsp[-1].number;
  ;
    break;}
case 381:
#line 1907 "ircd_parser.y"
{
    ConfigFileEntry.kline_with_reason = 1;
  ;
    break;}
case 382:
#line 1912 "ircd_parser.y"
{
    ConfigFileEntry.kline_with_reason = 0;
  ;
    break;}
case 383:
#line 1917 "ircd_parser.y"
{
    ConfigFileEntry.client_exit = 1;
  ;
    break;}
case 384:
#line 1922 "ircd_parser.y"
{
    ConfigFileEntry.client_exit = 0;
  ;
    break;}
case 385:
#line 1927 "ircd_parser.y"
{
    ConfigFileEntry.kline_with_connection_closed = 1;
  ;
    break;}
case 386:
#line 1932 "ircd_parser.y"
{
    ConfigFileEntry.kline_with_connection_closed = 0;
  ;
    break;}
case 387:
#line 1937 "ircd_parser.y"
{
    ConfigFileEntry.quiet_on_ban = 1;
  ;
    break;}
case 388:
#line 1942 "ircd_parser.y"
{
    ConfigFileEntry.quiet_on_ban = 0;
  ;
    break;}
case 389:
#line 1947 "ircd_parser.y"
{
    ConfigFileEntry.warn_no_nline = 1;
  ;
    break;}
case 390:
#line 1952 "ircd_parser.y"
{
    ConfigFileEntry.warn_no_nline = 0;
  ;
    break;}
case 391:
#line 1957 "ircd_parser.y"
{
    ConfigFileEntry.non_redundant_klines = 1;
  ;
    break;}
case 392:
#line 1962 "ircd_parser.y"
{
    ConfigFileEntry.non_redundant_klines = 0;
  ;
    break;}
case 393:
#line 1967 "ircd_parser.y"
{
    ConfigFileEntry.o_lines_oper_only = 1;
  ;
    break;}
case 394:
#line 1972 "ircd_parser.y"
{
    ConfigFileEntry.o_lines_oper_only = 0;
  ;
    break;}
case 395:
#line 1977 "ircd_parser.y"
{
    ConfigFileEntry.pace_wait = yyvsp[-1].number;
  ;
    break;}
case 396:
#line 1982 "ircd_parser.y"
{
    ConfigFileEntry.caller_id_wait = yyvsp[-1].number;
  ;
    break;}
case 397:
#line 1987 "ircd_parser.y"
{
    ConfigFileEntry.whois_wait = yyvsp[-1].number;
  ;
    break;}
case 398:
#line 1992 "ircd_parser.y"
{
    ConfigFileEntry.knock_delay = yyvsp[-1].number;
  ;
    break;}
case 399:
#line 1997 "ircd_parser.y"
{
    ConfigFileEntry.short_motd = 1;
  ;
    break;}
case 400:
#line 2002 "ircd_parser.y"
{
    ConfigFileEntry.short_motd = 0;
  ;
    break;}
case 401:
#line 2007 "ircd_parser.y"
{
    ConfigFileEntry.no_oper_flood = 1;
  ;
    break;}
case 402:
#line 2012 "ircd_parser.y"
{
    ConfigFileEntry.no_oper_flood = 0;
  ;
    break;}
case 403:
#line 2017 "ircd_parser.y"
{
#if 0
    strncpy(iAuth.hostname, yylval.string, HOSTLEN)[HOSTLEN] = 0;
#endif
;
    break;}
case 404:
#line 2024 "ircd_parser.y"
{
#if 0
    iAuth.port = yyvsp[-1].number;
#endif
;
    break;}
case 405:
#line 2031 "ircd_parser.y"
{
  strncpy_irc(ConfigFileEntry.fname_userlog, yylval.string,
	      MAXPATHLEN-1)[MAXPATHLEN-1] = 0;
;
    break;}
case 406:
#line 2037 "ircd_parser.y"
{
  strncpy_irc(ConfigFileEntry.fname_foperlog, yylval.string,
	      MAXPATHLEN-1)[MAXPATHLEN-1] = 0;
;
    break;}
case 407:
#line 2043 "ircd_parser.y"
{
  strncpy_irc(ConfigFileEntry.fname_operlog, yylval.string,
	      MAXPATHLEN-1)[MAXPATHLEN-1] = 0;
;
    break;}
case 408:
#line 2049 "ircd_parser.y"
{
    ConfigFileEntry.glines = 1;
  ;
    break;}
case 409:
#line 2054 "ircd_parser.y"
{
    ConfigFileEntry.glines = 0;
  ;
    break;}
case 410:
#line 2059 "ircd_parser.y"
{
    char langenv[BUFSIZE];
    if (strlen(yylval.string) > BUFSIZE-10)
      yylval.string[BUFSIZE-9] = 0;
    ircsprintf(langenv, "LANGUAGE=%s", yylval.string);
    putenv(langenv);
  ;
    break;}
case 411:
#line 2068 "ircd_parser.y"
{
    ConfigFileEntry.gline_time = yyvsp[-1].number;
  ;
    break;}
case 412:
#line 2073 "ircd_parser.y"
{
    ConfigFileEntry.idletime = yyvsp[-1].number;
  ;
    break;}
case 413:
#line 2078 "ircd_parser.y"
{
    ConfigFileEntry.dots_in_ident = yyvsp[-1].number;
  ;
    break;}
case 414:
#line 2083 "ircd_parser.y"
{
    ConfigFileEntry.maximum_links = yyvsp[-1].number;
  ;
    break;}
case 415:
#line 2088 "ircd_parser.y"
{
    ConfigFileEntry.hide_server = 1;
  ;
    break;}
case 416:
#line 2093 "ircd_parser.y"
{
    ConfigFileEntry.hide_server = 0;
  ;
    break;}
case 417:
#line 2098 "ircd_parser.y"
{
    ConfigFileEntry.max_targets = yyvsp[-1].number;
  ;
    break;}
case 418:
#line 2103 "ircd_parser.y"
{
    if (ConfigFileEntry.servlink_path)
      MyFree(ConfigFileEntry.servlink_path);
    DupString(ConfigFileEntry.servlink_path, yylval.string);
  ;
    break;}
case 419:
#line 2110 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
    struct EncCapability *ecap;
    char *s, *p;
    int cipher_count = 0;
    int found;

    for(ecap = enccaptab; ecap->name; ecap++)
    {
	    ecap->priority = 0;
    }

    for (s = strtoken(&p, yylval.string, ", "); s; s = strtoken(&p, NULL, ", "))
    {
      found = 0;
      for(ecap = enccaptab; ecap->name; ecap++)
      {
        if (!strcmp(s, ecap->name))
        {
          ecap->priority = ++cipher_count;
          found = 1;
        }
      }
      if (!found)
      {
        sendto_realops_flags(FLAGS_ALL,
			     "Invalid cipher '%s' ignored",
			     s);
      }
    }
#else
    sendto_realops_flags(FLAGS_ALL,
                  "Ignoring 'cipher_preference' line -- no OpenSSL support");
#endif
  ;
    break;}
case 420:
#line 2147 "ircd_parser.y"
{
    ConfigFileEntry.compression_level = yyvsp[-1].number;                                     
#ifndef HAVE_LIBZ
    sendto_realops_flags(FLAGS_ALL,
      "Ignoring compression_level = %d; -- no zlib support",
       ConfigFileEntry.compression_level);
#else
    if ((ConfigFileEntry.compression_level < 1) ||
        (ConfigFileEntry.compression_level > 9))
    {
      sendto_realops_flags(FLAGS_ALL,
        "Ignoring invalid compression level '%d', using default",
        ConfigFileEntry.compression_level);
      ConfigFileEntry.compression_level = 0;
    }
#endif
  ;
    break;}
case 421:
#line 2166 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes = 0;
  ;
    break;}
case 425:
#line 2175 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_BOTS;
  ;
    break;}
case 426:
#line 2179 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_CCONN;
  ;
    break;}
case 427:
#line 2183 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_DEBUG;
  ;
    break;}
case 428:
#line 2187 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_FULL;
  ;
    break;}
case 429:
#line 2191 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_SKILL;
  ;
    break;}
case 430:
#line 2195 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_NCHANGE;
  ;
    break;}
case 431:
#line 2199 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_REJ;
  ;
    break;}
case 432:
#line 2203 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_UNAUTH;
  ;
    break;}
case 433:
#line 2207 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_SPY;
  ;
    break;}
case 434:
#line 2211 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_EXTERNAL;
  ;
    break;}
case 435:
#line 2215 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_OPERWALL;
  ;
    break;}
case 436:
#line 2219 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_SERVNOTICE;
  ;
    break;}
case 437:
#line 2223 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_INVISIBLE;
  ;
    break;}
case 438:
#line 2227 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_WALLOP;
  ;
    break;}
case 439:
#line 2231 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_CALLERID;
  ;
    break;}
case 440:
#line 2235 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_LOCOPS;
  ;
    break;}
case 441:
#line 2239 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_DRONE;
  ;
    break;}
case 442:
#line 2244 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes = 0;
  ;
    break;}
case 446:
#line 2253 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_BOTS;
  ;
    break;}
case 447:
#line 2257 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_CCONN;
  ;
    break;}
case 448:
#line 2261 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_DEBUG;
  ;
    break;}
case 449:
#line 2265 "ircd_parser.y"
{ 
    ConfigFileEntry.oper_only_umodes |= FLAGS_FULL;
  ;
    break;}
case 450:
#line 2269 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_SKILL;
  ;
    break;}
case 451:
#line 2273 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_NCHANGE;
  ;
    break;}
case 452:
#line 2277 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_REJ;
  ;
    break;}
case 453:
#line 2281 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_UNAUTH;
  ;
    break;}
case 454:
#line 2285 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_SPY;
  ;
    break;}
case 455:
#line 2289 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_EXTERNAL;
  ;
    break;}
case 456:
#line 2293 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_OPERWALL;
  ;
    break;}
case 457:
#line 2297 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_SERVNOTICE;
  ;
    break;}
case 458:
#line 2301 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_INVISIBLE;
  ;
    break;}
case 459:
#line 2305 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_WALLOP;
  ;
    break;}
case 460:
#line 2309 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_CALLERID;
  ;
    break;}
case 461:
#line 2313 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_LOCOPS;
  ;
    break;}
case 462:
#line 2317 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_DRONE;
  ;
    break;}
case 463:
#line 2322 "ircd_parser.y"
{
    ConfigFileEntry.vchans_oper_only = 1;
  ;
    break;}
case 464:
#line 2327 "ircd_parser.y"
{
    ConfigFileEntry.vchans_oper_only = 0;
  ;
    break;}
case 465:
#line 2332 "ircd_parser.y"
{
    ConfigFileEntry.disable_vchans = 0;
  ;
    break;}
case 466:
#line 2337 "ircd_parser.y"
{
    ConfigFileEntry.disable_vchans = 1;
  ;
    break;}
case 467:
#line 2342 "ircd_parser.y"
{
    ConfigFileEntry.persist_expire = yyvsp[-1].number;  
  ;
    break;}
case 468:
#line 2346 "ircd_parser.y"
{
    ConfigFileEntry.min_nonwildcard = yyvsp[-1].number;
  ;
    break;}
case 469:
#line 2350 "ircd_parser.y"
{
    ConfigFileEntry.default_floodcount = yyvsp[-1].number;
  ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/usr/local/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 2354 "ircd_parser.y"
