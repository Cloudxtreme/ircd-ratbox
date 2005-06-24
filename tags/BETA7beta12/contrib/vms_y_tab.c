
/*  A Bison parser, made from ircd_parser.y
    by bison-1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	ACCEPT_PASSWORD	257
#define	ACTION	258
#define	ADMIN	259
#define	AFTYPE	260
#define	ANTI_NICK_FLOOD	261
#define	ANTI_SPAM_EXIT_MESSAGE_TIME	262
#define	AUTH	263
#define	AUTOCONN	264
#define	BYTES	265
#define	KBYTES	266
#define	MBYTES	267
#define	GBYTES	268
#define	TBYTES	269
#define	CALLER_ID_WAIT	270
#define	CHANNEL	271
#define	CIPHER_PREFERENCE	272
#define	CLASS	273
#define	CLIENT_EXIT	274
#define	COMPRESSED	275
#define	COMPRESSION_LEVEL	276
#define	CONNECT	277
#define	CONNECTFREQ	278
#define	CRYPTLINK	279
#define	DEFAULT_CIPHER_PREFERENCE	280
#define	DEFAULT_FLOODCOUNT	281
#define	DENY	282
#define	DESCRIPTION	283
#define	DIE	284
#define	USE_VCHANS	285
#define	DOTS_IN_IDENT	286
#define	EGDPOOL_PATH	287
#define	EMAIL	288
#define	ENCRYPTED	289
#define	EXCEED_LIMIT	290
#define	EXEMPT	291
#define	FAILED_OPER_NOTICE	292
#define	FAKENAME	293
#define	FNAME_FOPERLOG	294
#define	FNAME_OPERLOG	295
#define	FNAME_USERLOG	296
#define	GECOS	297
#define	GENERAL	298
#define	GLINE	299
#define	GLINES	300
#define	GLINE_EXEMPT	301
#define	GLINE_LOG	302
#define	GLINE_TIME	303
#define	GLOBAL_KILL	304
#define	HAVE_IDENT	305
#define	HAVENT_READ_CONF	306
#define	HIDESERVER	307
#define	HOST	308
#define	HUB	309
#define	HUB_MASK	310
#define	IAUTH_PORT	311
#define	IAUTH_SERVER	312
#define	IDLETIME	313
#define	IP	314
#define	KILL	315
#define	KLINE	316
#define	KLINE_EXEMPT	317
#define	KLINE_WITH_CONNECTION_CLOSED	318
#define	KLINE_WITH_REASON	319
#define	KNOCK_DELAY	320
#define	LAZYLINK	321
#define	LEAF_MASK	322
#define	LINKS_DELAY	323
#define	LISTEN	324
#define	LOGGING	325
#define	LOG_LEVEL	326
#define	MAXBANS	327
#define	MAXIMUM_LINKS	328
#define	MAX_ACCEPT	329
#define	MAX_CHANS_PER_USER	330
#define	MAX_NICK_CHANGES	331
#define	MAX_NICK_TIME	332
#define	MAX_NUMBER	333
#define	MAX_TARGETS	334
#define	MESSAGE_LOCALE	335
#define	MIN_NONWILDCARD	336
#define	MODULE	337
#define	MODULES	338
#define	NAME	339
#define	NETWORK_DESC	340
#define	NETWORK_NAME	341
#define	NICK	342
#define	NICK_CHANGES	343
#define	NON_REDUNDANT_KLINES	344
#define	NO_HACK_OPS	345
#define	NO_OPER_FLOOD	346
#define	NO_TILDE	347
#define	NUMBER	348
#define	NUMBER_PER_IP	349
#define	OPERATOR	350
#define	OPER_LOG	351
#define	OPER_ONLY_UMODES	352
#define	OPER_UMODES	353
#define	PACE_WAIT	354
#define	PASSWORD	355
#define	PATH	356
#define	PERSIST_TIME	357
#define	PING_TIME	358
#define	PORT	359
#define	QSTRING	360
#define	QUIET_ON_BAN	361
#define	REASON	362
#define	REDIRPORT	363
#define	REDIRSERV	364
#define	REHASH	365
#define	REMOTE	366
#define	RESTRICTED	367
#define	RSA_PRIVATE_KEY_FILE	368
#define	RSA_PUBLIC_KEY_FILE	369
#define	RESV	370
#define	SECONDS	371
#define	MINUTES	372
#define	HOURS	373
#define	DAYS	374
#define	WEEKS	375
#define	MONTHS	376
#define	YEARS	377
#define	DECADES	378
#define	CENTURIES	379
#define	MILLENNIA	380
#define	SENDQ	381
#define	SEND_PASSWORD	382
#define	SERVERINFO	383
#define	SERVLINK_PATH	384
#define	SHARED	385
#define	SHORT_MOTD	386
#define	SILENT	387
#define	SPOOF	388
#define	SPOOF_NOTICE	389
#define	STATS_I_OPER_ONLY	390
#define	STATS_K_OPER_ONLY	391
#define	STATS_O_OPER_ONLY	392
#define	TMASKED	393
#define	TNO	394
#define	TREJECT	395
#define	TS_MAX_DELTA	396
#define	TS_WARN_DELTA	397
#define	TWODOTS	398
#define	TYES	399
#define	T_BOTS	400
#define	T_CALLERID	401
#define	T_CCONN	402
#define	T_CLIENT_FLOOD	403
#define	T_DEBUG	404
#define	T_DRONE	405
#define	T_EXTERNAL	406
#define	T_FULL	407
#define	T_INVISIBLE	408
#define	T_IPV4	409
#define	T_IPV6	410
#define	T_LOCOPS	411
#define	T_LOGPATH	412
#define	T_L_CRIT	413
#define	T_L_DEBUG	414
#define	T_L_ERROR	415
#define	T_L_INFO	416
#define	T_L_NOTICE	417
#define	T_L_TRACE	418
#define	T_L_WARN	419
#define	T_MAX_BUFFER	420
#define	T_MAX_CLIENTS	421
#define	T_NCHANGE	422
#define	T_OPERWALL	423
#define	T_REJ	424
#define	T_SERVNOTICE	425
#define	T_SKILL	426
#define	T_SPY	427
#define	T_UNAUTH	428
#define	T_WALLOP	429
#define	THROTTLE_TIME	430
#define	UNKLINE	431
#define	USER	432
#define	USE_EGD	433
#define	USE_EXCEPT	434
#define	USE_INVEX	435
#define	USE_KNOCK	436
#define	VCHANS_OPER_ONLY	437
#define	VHOST	438
#define	VHOST6	439
#define	WARN	440
#define	WARN_NO_NLINE	441
#define	WHOIS_WAIT	442
#define	NEG	443

#line 24 "ircd_parser.y"


#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

/* XXX */
#define  WE_ARE_MEMORY_C

#define YY_NO_UNPUT
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
#include "resv.h"

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

static char  *listener_address;

char *resv_reason;

char  *class_redirserv_var;
int   class_redirport_var;


#line 93 "ircd_parser.y"
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



#define	YYFINAL		1076
#define	YYFLAG		-32768
#define	YYNTBASE	201

#define YYTRANSLATE(x) ((unsigned)(x) <= 443 ? yytranslate[x] : 419)

static const short yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,   196,
   197,   191,   190,   200,   189,     2,   192,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,   194,     2,
   199,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,   198,     2,   195,     2,     2,     2,     2,     2,
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
   167,   168,   169,   170,   171,   172,   173,   174,   175,   176,
   177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
   187,   188,   193
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     6,     8,    10,    12,    14,    16,    18,
    20,    22,    24,    26,    28,    30,    32,    34,    36,    38,
    41,    44,    46,    49,    52,    55,    58,    61,    64,    67,
    70,    73,    76,    79,    81,    84,    87,    90,    93,    96,
    98,   102,   106,   110,   114,   117,   121,   127,   130,   132,
   134,   136,   138,   143,   148,   154,   157,   159,   161,   163,
   165,   167,   169,   171,   173,   175,   177,   179,   181,   183,
   188,   193,   198,   203,   208,   213,   218,   223,   228,   233,
   238,   243,   248,   254,   257,   259,   261,   263,   265,   267,
   272,   277,   282,   288,   291,   293,   295,   297,   299,   301,
   303,   308,   313,   318,   323,   328,   333,   338,   343,   348,
   353,   354,   361,   364,   366,   368,   370,   372,   374,   376,
   378,   380,   382,   384,   386,   388,   390,   392,   394,   399,
   404,   409,   414,   419,   424,   429,   434,   439,   444,   449,
   454,   459,   464,   469,   474,   479,   484,   489,   494,   499,
   504,   505,   512,   515,   517,   519,   521,   523,   525,   527,
   529,   531,   536,   541,   546,   551,   556,   561,   562,   569,
   572,   574,   576,   578,   580,   582,   587,   591,   593,   595,
   599,   604,   609,   610,   617,   620,   622,   624,   626,   628,
   630,   632,   634,   636,   638,   640,   642,   644,   646,   648,
   650,   655,   660,   665,   670,   675,   680,   685,   690,   695,
   700,   705,   710,   715,   720,   725,   730,   735,   740,   745,
   750,   751,   758,   761,   763,   765,   767,   769,   771,   776,
   781,   786,   787,   794,   797,   799,   801,   803,   805,   807,
   812,   817,   822,   823,   830,   833,   835,   837,   839,   841,
   843,   845,   847,   849,   851,   853,   855,   857,   859,   861,
   863,   865,   867,   869,   871,   876,   881,   886,   891,   896,
   901,   906,   911,   916,   921,   926,   931,   936,   941,   946,
   951,   956,   961,   966,   971,   976,   981,   986,   987,   994,
   997,   999,  1001,  1003,  1005,  1010,  1015,  1016,  1023,  1026,
  1028,  1030,  1032,  1034,  1039,  1044,  1045,  1052,  1055,  1057,
  1059,  1061,  1066,  1067,  1074,  1077,  1079,  1081,  1083,  1085,
  1087,  1092,  1097,  1102,  1107,  1112,  1118,  1121,  1123,  1125,
  1127,  1129,  1131,  1133,  1135,  1137,  1139,  1141,  1143,  1145,
  1147,  1149,  1151,  1153,  1155,  1157,  1159,  1161,  1163,  1165,
  1167,  1169,  1171,  1173,  1175,  1177,  1179,  1181,  1183,  1185,
  1187,  1189,  1191,  1193,  1195,  1197,  1199,  1201,  1203,  1205,
  1207,  1209,  1211,  1213,  1215,  1217,  1219,  1224,  1229,  1234,
  1239,  1244,  1249,  1254,  1259,  1264,  1269,  1274,  1279,  1284,
  1289,  1294,  1299,  1304,  1309,  1314,  1319,  1324,  1329,  1334,
  1339,  1344,  1349,  1354,  1359,  1364,  1369,  1374,  1379,  1384,
  1389,  1394,  1399,  1404,  1409,  1414,  1419,  1424,  1429,  1434,
  1439,  1444,  1449,  1454,  1459,  1464,  1469,  1474,  1479,  1484,
  1489,  1494,  1499,  1504,  1509,  1514,  1515,  1521,  1525,  1527,
  1529,  1531,  1533,  1535,  1537,  1539,  1541,  1543,  1545,  1547,
  1549,  1551,  1553,  1555,  1557,  1559,  1561,  1562,  1568,  1572,
  1574,  1576,  1578,  1580,  1582,  1584,  1586,  1588,  1590,  1592,
  1594,  1596,  1598,  1600,  1602,  1604,  1606,  1608,  1613,  1618,
  1623,  1629,  1632,  1634,  1636,  1638,  1640,  1642,  1644,  1646,
  1648,  1650,  1652,  1654,  1656,  1661,  1666,  1671,  1676,  1681,
  1686,  1691,  1696,  1701,  1706,  1711,  1716,  1721,  1726,  1731
};

static const short yyrhs[] = {    -1,
   201,   202,     0,   225,     0,   231,     0,   238,     0,   406,
     0,   255,     0,   265,     0,   274,     0,   211,     0,   291,
     0,   298,     0,   305,     0,   326,     0,   332,     0,   338,
     0,   350,     0,   343,     0,   206,     0,     1,   194,     0,
     1,   195,     0,   205,     0,   205,   117,     0,   205,   118,
     0,   205,   119,     0,   205,   120,     0,   205,   121,     0,
   205,   122,     0,   205,   123,     0,   205,   124,     0,   205,
   125,     0,   205,   126,     0,   203,   203,     0,   205,     0,
   205,    11,     0,   205,    12,     0,   205,    13,     0,   205,
    14,     0,   205,    15,     0,    94,     0,   205,   190,   205,
     0,   205,   189,   205,     0,   205,   191,   205,     0,   205,
   192,   205,     0,   189,   205,     0,   196,   205,   197,     0,
    84,   198,   207,   195,   194,     0,   207,   208,     0,   208,
     0,   209,     0,   210,     0,     1,     0,    83,   199,   106,
   194,     0,   102,   199,   106,   194,     0,   129,   198,   212,
   195,   194,     0,   212,   213,     0,   213,     0,   216,     0,
   220,     0,   224,     0,   217,     0,   218,     0,   219,     0,
   222,     0,   215,     0,   214,     0,   221,     0,   223,     0,
     1,     0,   114,   199,   106,   194,     0,    91,   199,   145,
   194,     0,    91,   199,   140,   194,     0,    85,   199,   106,
   194,     0,    29,   199,   106,   194,     0,    87,   199,   106,
   194,     0,    86,   199,   106,   194,     0,   184,   199,   106,
   194,     0,   185,   199,   106,   194,     0,   167,   199,   205,
   194,     0,   166,   199,   205,   194,     0,    55,   199,   145,
   194,     0,    55,   199,   140,   194,     0,     5,   198,   226,
   195,   194,     0,   226,   227,     0,   227,     0,   228,     0,
   230,     0,   229,     0,     1,     0,    85,   199,   106,   194,
     0,    34,   199,   106,   194,     0,    29,   199,   106,   194,
     0,    71,   198,   232,   195,   194,     0,   232,   233,     0,
   233,     0,   234,     0,   235,     0,   236,     0,   237,     0,
     1,     0,   158,   199,   106,   194,     0,    97,   199,   106,
   194,     0,    48,   199,   106,   194,     0,    72,   199,   159,
   194,     0,    72,   199,   161,   194,     0,    72,   199,   165,
   194,     0,    72,   199,   163,   194,     0,    72,   199,   164,
   194,     0,    72,   199,   162,   194,     0,    72,   199,   160,
   194,     0,     0,    96,   239,   198,   240,   195,   194,     0,
   240,   241,     0,   241,     0,   242,     0,   243,     0,   244,
     0,   245,     0,   246,     0,   247,     0,   248,     0,   249,
     0,   250,     0,   251,     0,   252,     0,   253,     0,   254,
     0,     1,     0,    85,   199,   106,   194,     0,   178,   199,
   106,   194,     0,   101,   199,   106,   194,     0,    19,   199,
   106,   194,     0,    50,   199,   145,   194,     0,    50,   199,
   140,   194,     0,   112,   199,   145,   194,     0,   112,   199,
   140,   194,     0,    62,   199,   145,   194,     0,    62,   199,
   140,   194,     0,   177,   199,   145,   194,     0,   177,   199,
   140,   194,     0,    45,   199,   145,   194,     0,    45,   199,
   140,   194,     0,    89,   199,   145,   194,     0,    89,   199,
   140,   194,     0,    30,   199,   145,   194,     0,    30,   199,
   140,   194,     0,   111,   199,   145,   194,     0,   111,   199,
   140,   194,     0,     5,   199,   145,   194,     0,     5,   199,
   140,   194,     0,     0,    19,   256,   198,   257,   195,   194,
     0,   257,   258,     0,   258,     0,   259,     0,   260,     0,
   261,     0,   262,     0,   263,     0,   264,     0,     1,     0,
    85,   199,   106,   194,     0,   104,   199,   203,   194,     0,
    95,   199,   205,   194,     0,    24,   199,   203,   194,     0,
    79,   199,   205,   194,     0,   127,   199,   204,   194,     0,
     0,    70,   266,   198,   267,   195,   194,     0,   267,   268,
     0,   268,     0,   269,     0,   272,     0,   273,     0,     1,
     0,   105,   199,   270,   194,     0,   270,   200,   271,     0,
   271,     0,    94,     0,    94,   144,    94,     0,    60,   199,
   106,   194,     0,    54,   199,   106,   194,     0,     0,     9,
   275,   198,   276,   195,   194,     0,   276,   277,     0,   277,
     0,   278,     0,   279,     0,   290,     0,   284,     0,   285,
     0,   283,     0,   282,     0,   286,     0,   287,     0,   281,
     0,   280,     0,   288,     0,   289,     0,     1,     0,   178,
   199,   106,   194,     0,   101,   199,   106,   194,     0,   135,
   199,   140,   194,     0,   135,   199,   145,   194,     0,   134,
   199,   106,   194,     0,    36,   199,   145,   194,     0,    36,
   199,   140,   194,     0,   113,   199,   145,   194,     0,   113,
   199,   140,   194,     0,    63,   199,   145,   194,     0,    63,
   199,   140,   194,     0,    51,   199,   145,   194,     0,    51,
   199,   140,   194,     0,    93,   199,   145,   194,     0,    93,
   199,   140,   194,     0,    47,   199,   145,   194,     0,    47,
   199,   140,   194,     0,   110,   199,   106,   194,     0,   109,
   199,   205,   194,     0,    19,   199,   106,   194,     0,     0,
   116,   292,   198,   293,   195,   194,     0,   293,   294,     0,
   294,     0,   295,     0,   296,     0,   297,     0,     1,     0,
   108,   199,   106,   194,     0,    17,   199,   106,   194,     0,
    88,   199,   106,   194,     0,     0,   131,   299,   198,   300,
   195,   194,     0,   300,   301,     0,   301,     0,   302,     0,
   303,     0,   304,     0,     1,     0,    85,   199,   106,   194,
     0,   178,   199,   106,   194,     0,    54,   199,   106,   194,
     0,     0,    23,   306,   198,   307,   195,   194,     0,   307,
   308,     0,   308,     0,   309,     0,   310,     0,   311,     0,
   312,     0,   313,     0,   314,     0,   315,     0,   316,     0,
   322,     0,   323,     0,   324,     0,   321,     0,   317,     0,
   320,     0,   319,     0,   318,     0,   325,     0,     1,     0,
    85,   199,   106,   194,     0,    54,   199,   106,   194,     0,
   128,   199,   106,   194,     0,     3,   199,   106,   194,     0,
   105,   199,   205,   194,     0,     6,   199,   155,   194,     0,
     6,   199,   156,   194,     0,    39,   199,   106,   194,     0,
    67,   199,   145,   194,     0,    67,   199,   140,   194,     0,
    35,   199,   145,   194,     0,    35,   199,   140,   194,     0,
   115,   199,   106,   194,     0,    25,   199,   145,   194,     0,
    25,   199,   140,   194,     0,    21,   199,   145,   194,     0,
    21,   199,   140,   194,     0,    10,   199,   145,   194,     0,
    10,   199,   140,   194,     0,    56,   199,   106,   194,     0,
    68,   199,   106,   194,     0,    19,   199,   106,   194,     0,
    18,   199,   106,   194,     0,     0,    61,   327,   198,   328,
   195,   194,     0,   328,   329,     0,   329,     0,   330,     0,
   331,     0,     1,     0,   178,   199,   106,   194,     0,   108,
   199,   106,   194,     0,     0,    28,   333,   198,   334,   195,
   194,     0,   334,   335,     0,   335,     0,   336,     0,   337,
     0,     1,     0,    60,   199,   106,   194,     0,   108,   199,
   106,   194,     0,     0,    37,   339,   198,   340,   195,   194,
     0,   340,   341,     0,   341,     0,   342,     0,     1,     0,
    60,   199,   106,   194,     0,     0,    43,   344,   198,   345,
   195,   194,     0,   345,   346,     0,   346,     0,   347,     0,
   348,     0,   349,     0,     1,     0,    85,   199,   106,   194,
     0,   108,   199,   106,   194,     0,     4,   199,   186,   194,
     0,     4,   199,   141,   194,     0,     4,   199,   133,   194,
     0,    44,   198,   351,   195,   194,     0,   351,   352,     0,
   352,     0,   353,     0,   354,     0,   355,     0,   356,     0,
   357,     0,   358,     0,   359,     0,   360,     0,   363,     0,
   365,     0,   366,     0,   367,     0,   385,     0,   368,     0,
   369,     0,   371,     0,   370,     0,   373,     0,   374,     0,
   375,     0,   376,     0,   377,     0,   381,     0,   383,     0,
   384,     0,   387,     0,   386,     0,   382,     0,   364,     0,
   378,     0,   380,     0,   379,     0,   399,     0,   388,     0,
   392,     0,   393,     0,   361,     0,   395,     0,   372,     0,
   404,     0,   403,     0,   389,     0,   390,     0,   391,     0,
   405,     0,   394,     0,   362,     0,     1,     0,    38,   199,
   145,   194,     0,    38,   199,   140,   194,     0,     7,   199,
   145,   194,     0,     7,   199,   140,   194,     0,    78,   199,
   203,   194,     0,    77,   199,   205,   194,     0,    75,   199,
   205,   194,     0,     8,   199,   203,   194,     0,   143,   199,
   203,   194,     0,   142,   199,   203,   194,     0,    69,   199,
   203,   194,     0,    52,   199,   205,   194,     0,    65,   199,
   145,   194,     0,    65,   199,   140,   194,     0,    20,   199,
   145,   194,     0,    20,   199,   140,   194,     0,    64,   199,
   145,   194,     0,    64,   199,   140,   194,     0,   187,   199,
   145,   194,     0,   187,   199,   140,   194,     0,    90,   199,
   145,   194,     0,    90,   199,   140,   194,     0,   138,   199,
   145,   194,     0,   138,   199,   140,   194,     0,   137,   199,
   145,   194,     0,   137,   199,   139,   194,     0,   137,   199,
   140,   194,     0,   136,   199,   145,   194,     0,   136,   199,
   139,   194,     0,   136,   199,   140,   194,     0,   100,   199,
   203,   194,     0,    16,   199,   203,   194,     0,   188,   199,
   203,   194,     0,   132,   199,   145,   194,     0,   132,   199,
   140,   194,     0,    92,   199,   145,   194,     0,    92,   199,
   140,   194,     0,    58,   199,   106,   194,     0,    57,   199,
   205,   194,     0,    42,   199,   106,   194,     0,    40,   199,
   106,   194,     0,    41,   199,   106,   194,     0,    46,   199,
   145,   194,     0,    46,   199,   140,   194,     0,    81,   199,
   106,   194,     0,    49,   199,   203,   194,     0,    59,   199,
   203,   194,     0,    32,   199,   205,   194,     0,    74,   199,
   205,   194,     0,    53,   199,   145,   194,     0,    53,   199,
   140,   194,     0,    80,   199,   205,   194,     0,   130,   199,
   106,   194,     0,    26,   199,   106,   194,     0,    22,   199,
   205,   194,     0,   179,   199,   145,   194,     0,   179,   199,
   140,   194,     0,    33,   199,   106,   194,     0,   176,   199,
   203,   194,     0,     0,    99,   396,   199,   397,   194,     0,
   397,   200,   398,     0,   398,     0,   146,     0,   148,     0,
   150,     0,   153,     0,   172,     0,   168,     0,   170,     0,
   174,     0,   173,     0,   152,     0,   169,     0,   171,     0,
   154,     0,   175,     0,   147,     0,   157,     0,   151,     0,
     0,    98,   400,   199,   401,   194,     0,   401,   200,   402,
     0,   402,     0,   146,     0,   148,     0,   150,     0,   153,
     0,   172,     0,   168,     0,   170,     0,   174,     0,   173,
     0,   152,     0,   169,     0,   171,     0,   154,     0,   175,
     0,   147,     0,   157,     0,   151,     0,    82,   199,   205,
   194,     0,    27,   199,   205,   194,     0,   149,   199,   205,
   194,     0,    17,   198,   407,   195,   194,     0,   407,   408,
     0,   408,     0,   409,     0,   410,     0,   411,     0,   412,
     0,   413,     0,   417,     0,   414,     0,   415,     0,   416,
     0,   418,     0,     1,     0,   181,   199,   145,   194,     0,
   181,   199,   140,   194,     0,   180,   199,   145,   194,     0,
   180,   199,   140,   194,     0,   182,   199,   145,   194,     0,
   182,   199,   140,   194,     0,   183,   199,   145,   194,     0,
   183,   199,   140,   194,     0,    31,   199,   140,   194,     0,
    31,   199,   145,   194,     0,    66,   199,   203,   194,     0,
    76,   199,   205,   194,     0,   107,   199,   145,   194,     0,
   107,   199,   140,   194,     0,    73,   199,   205,   194,     0,
   103,   199,   203,   194,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   284,   285,   288,   289,   290,   291,   292,   293,   294,   295,
   296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
   306,   310,   314,   318,   322,   326,   330,   334,   339,   344,
   348,   352,   356,   363,   367,   371,   375,   379,   383,   390,
   394,   398,   402,   406,   411,   415,   425,   428,   428,   431,
   431,   431,   434,   453,   465,   468,   468,   471,   471,   471,
   472,   472,   473,   473,   474,   474,   475,   475,   476,   479,
   537,   541,   548,   555,   561,   567,   573,   582,   593,   598,
   603,   618,   645,   647,   647,   650,   650,   650,   651,   653,
   659,   665,   675,   677,   677,   680,   680,   680,   681,   681,
   684,   688,   692,   696,   698,   701,   704,   707,   710,   713,
   721,   738,   776,   776,   779,   779,   779,   779,   780,   780,
   780,   781,   781,   781,   781,   782,   782,   782,   784,   793,
   825,   833,   839,   843,   849,   850,   853,   854,   857,   858,
   861,   862,   865,   866,   869,   870,   873,   874,   877,   878,
   885,   894,   905,   905,   908,   908,   909,   910,   911,   912,
   913,   916,   922,   927,   932,   937,   942,   952,   956,   962,
   962,   965,   965,   965,   965,   967,   969,   969,   971,   974,
   983,   989,   999,  1020,  1067,  1067,  1070,  1070,  1070,  1070,
  1071,  1071,  1071,  1072,  1072,  1072,  1073,  1073,  1074,  1074,
  1077,  1111,  1121,  1125,  1131,  1142,  1146,  1152,  1156,  1162,
  1166,  1172,  1176,  1182,  1186,  1192,  1196,  1203,  1210,  1216,
  1227,  1231,  1237,  1237,  1240,  1240,  1240,  1240,  1242,  1248,
  1261,  1279,  1292,  1298,  1298,  1301,  1301,  1301,  1301,  1303,
  1309,  1315,  1325,  1356,  1436,  1436,  1439,  1439,  1439,  1439,
  1440,  1440,  1440,  1441,  1441,  1441,  1442,  1442,  1442,  1443,
  1443,  1443,  1444,  1444,  1447,  1459,  1465,  1473,  1481,  1484,
  1488,  1496,  1502,  1506,  1512,  1516,  1522,  1565,  1569,  1575,
  1584,  1590,  1594,  1600,  1620,  1640,  1646,  1687,  1697,  1710,
  1710,  1713,  1713,  1713,  1716,  1741,  1751,  1763,  1777,  1777,
  1780,  1780,  1780,  1783,  1789,  1799,  1810,  1823,  1823,  1826,
  1826,  1828,  1839,  1851,  1860,  1860,  1863,  1863,  1863,  1863,
  1866,  1872,  1878,  1882,  1887,  1898,  1901,  1901,  1904,  1904,
  1905,  1905,  1906,  1906,  1907,  1908,  1908,  1909,  1910,  1911,
  1912,  1912,  1913,  1913,  1914,  1914,  1915,  1916,  1916,  1917,
  1918,  1919,  1919,  1920,  1921,  1921,  1922,  1922,  1923,  1923,
  1924,  1924,  1925,  1926,  1926,  1927,  1927,  1928,  1928,  1929,
  1930,  1931,  1932,  1932,  1933,  1933,  1936,  1940,  1946,  1950,
  1956,  1961,  1966,  1971,  1976,  1981,  1986,  1991,  1999,  2003,
  2009,  2013,  2019,  2023,  2029,  2033,  2039,  2043,  2049,  2053,
  2059,  2063,  2068,  2074,  2078,  2083,  2089,  2094,  2099,  2104,
  2108,  2114,  2118,  2124,  2131,  2138,  2144,  2150,  2156,  2160,
  2166,  2175,  2180,  2185,  2190,  2195,  2199,  2205,  2210,  2216,
  2251,  2270,  2274,  2280,  2286,  2291,  2295,  2297,  2297,  2300,
  2304,  2308,  2312,  2316,  2320,  2324,  2328,  2332,  2336,  2340,
  2344,  2348,  2352,  2356,  2360,  2364,  2369,  2373,  2375,  2375,
  2378,  2382,  2386,  2390,  2394,  2398,  2402,  2406,  2410,  2414,
  2418,  2422,  2426,  2430,  2434,  2438,  2442,  2447,  2451,  2456,
  2465,  2468,  2468,  2471,  2471,  2472,  2473,  2474,  2475,  2476,
  2477,  2478,  2479,  2480,  2483,  2487,  2493,  2497,  2503,  2507,
  2513,  2517,  2523,  2527,  2533,  2538,  2543,  2547,  2553,  2558
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","ACCEPT_PASSWORD",
"ACTION","ADMIN","AFTYPE","ANTI_NICK_FLOOD","ANTI_SPAM_EXIT_MESSAGE_TIME","AUTH",
"AUTOCONN","BYTES","KBYTES","MBYTES","GBYTES","TBYTES","CALLER_ID_WAIT","CHANNEL",
"CIPHER_PREFERENCE","CLASS","CLIENT_EXIT","COMPRESSED","COMPRESSION_LEVEL","CONNECT",
"CONNECTFREQ","CRYPTLINK","DEFAULT_CIPHER_PREFERENCE","DEFAULT_FLOODCOUNT","DENY",
"DESCRIPTION","DIE","USE_VCHANS","DOTS_IN_IDENT","EGDPOOL_PATH","EMAIL",
"ENCRYPTED","EXCEED_LIMIT","EXEMPT","FAILED_OPER_NOTICE","FAKENAME","FNAME_FOPERLOG",
"FNAME_OPERLOG","FNAME_USERLOG","GECOS","GENERAL","GLINE","GLINES","GLINE_EXEMPT",
"GLINE_LOG","GLINE_TIME","GLOBAL_KILL","HAVE_IDENT","HAVENT_READ_CONF","HIDESERVER",
"HOST","HUB","HUB_MASK","IAUTH_PORT","IAUTH_SERVER","IDLETIME","IP","KILL","KLINE",
"KLINE_EXEMPT","KLINE_WITH_CONNECTION_CLOSED","KLINE_WITH_REASON","KNOCK_DELAY",
"LAZYLINK","LEAF_MASK","LINKS_DELAY","LISTEN","LOGGING","LOG_LEVEL","MAXBANS",
"MAXIMUM_LINKS","MAX_ACCEPT","MAX_CHANS_PER_USER","MAX_NICK_CHANGES","MAX_NICK_TIME",
"MAX_NUMBER","MAX_TARGETS","MESSAGE_LOCALE","MIN_NONWILDCARD","MODULE","MODULES",
"NAME","NETWORK_DESC","NETWORK_NAME","NICK","NICK_CHANGES","NON_REDUNDANT_KLINES",
"NO_HACK_OPS","NO_OPER_FLOOD","NO_TILDE","NUMBER","NUMBER_PER_IP","OPERATOR",
"OPER_LOG","OPER_ONLY_UMODES","OPER_UMODES","PACE_WAIT","PASSWORD","PATH","PERSIST_TIME",
"PING_TIME","PORT","QSTRING","QUIET_ON_BAN","REASON","REDIRPORT","REDIRSERV",
"REHASH","REMOTE","RESTRICTED","RSA_PRIVATE_KEY_FILE","RSA_PUBLIC_KEY_FILE",
"RESV","SECONDS","MINUTES","HOURS","DAYS","WEEKS","MONTHS","YEARS","DECADES",
"CENTURIES","MILLENNIA","SENDQ","SEND_PASSWORD","SERVERINFO","SERVLINK_PATH",
"SHARED","SHORT_MOTD","SILENT","SPOOF","SPOOF_NOTICE","STATS_I_OPER_ONLY","STATS_K_OPER_ONLY",
"STATS_O_OPER_ONLY","TMASKED","TNO","TREJECT","TS_MAX_DELTA","TS_WARN_DELTA",
"TWODOTS","TYES","T_BOTS","T_CALLERID","T_CCONN","T_CLIENT_FLOOD","T_DEBUG",
"T_DRONE","T_EXTERNAL","T_FULL","T_INVISIBLE","T_IPV4","T_IPV6","T_LOCOPS","T_LOGPATH",
"T_L_CRIT","T_L_DEBUG","T_L_ERROR","T_L_INFO","T_L_NOTICE","T_L_TRACE","T_L_WARN",
"T_MAX_BUFFER","T_MAX_CLIENTS","T_NCHANGE","T_OPERWALL","T_REJ","T_SERVNOTICE",
"T_SKILL","T_SPY","T_UNAUTH","T_WALLOP","THROTTLE_TIME","UNKLINE","USER","USE_EGD",
"USE_EXCEPT","USE_INVEX","USE_KNOCK","VCHANS_OPER_ONLY","VHOST","VHOST6","WARN",
"WARN_NO_NLINE","WHOIS_WAIT","'-'","'+'","'*'","'/'","NEG","';'","'}'","'('",
"')'","'{'","'='","','","conf","conf_item","timespec","sizespec","expr","modules_entry",
"modules_items","modules_item","modules_module","modules_path","serverinfo_entry",
"serverinfo_items","serverinfo_item","serverinfo_rsa_private_key_file","serverinfo_no_hack_ops",
"serverinfo_name","serverinfo_description","serverinfo_network_name","serverinfo_network_desc",
"serverinfo_vhost","serverinfo_vhost6","serverinfo_max_clients","serverinfo_max_buffer",
"serverinfo_hub","admin_entry","admin_items","admin_item","admin_name","admin_email",
"admin_description","logging_entry","logging_items","logging_item","logging_path",
"logging_oper_log","logging_gline_log","logging_log_level","oper_entry","@1",
"oper_items","oper_item","oper_name","oper_user","oper_password","oper_class",
"oper_global_kill","oper_remote","oper_kline","oper_unkline","oper_gline","oper_nick_changes",
"oper_die","oper_rehash","oper_admin","class_entry","@2","class_items","class_item",
"class_name","class_ping_time","class_number_per_ip","class_connectfreq","class_max_number",
"class_sendq","listen_entry","@3","listen_items","listen_item","listen_port",
"port_items","port_item","listen_address","listen_host","auth_entry","@4","auth_items",
"auth_item","auth_user","auth_passwd","auth_spoof_notice","auth_spoof","auth_exceed_limit",
"auth_is_restricted","auth_kline_exempt","auth_have_ident","auth_no_tilde","auth_gline_exempt",
"auth_redir_serv","auth_redir_port","auth_class","resv_entry","@5","resv_items",
"resv_item","resv_creason","resv_channel","resv_nick","shared_entry","@6","shared_items",
"shared_item","shared_name","shared_user","shared_host","connect_entry","@7",
"connect_items","connect_item","connect_name","connect_host","connect_send_password",
"connect_accept_password","connect_port","connect_aftype","connect_fakename",
"connect_lazylink","connect_encrypted","connect_rsa_public_key_file","connect_cryptlink",
"connect_compressed","connect_auto","connect_hub_mask","connect_leaf_mask","connect_class",
"connect_cipher_preference","kill_entry","@8","kill_items","kill_item","kill_user",
"kill_reason","deny_entry","@9","deny_items","deny_item","deny_ip","deny_reason",
"exempt_entry","@10","exempt_items","exempt_item","exempt_ip","gecos_entry",
"@11","gecos_items","gecos_item","gecos_name","gecos_reason","gecos_action",
"general_entry","general_items","general_item","general_failed_oper_notice",
"general_anti_nick_flood","general_max_nick_time","general_max_nick_changes",
"general_max_accept","general_anti_spam_exit_message_time","general_ts_warn_delta",
"general_ts_max_delta","general_links_delay","general_havent_read_conf","general_kline_with_reason",
"general_client_exit","general_kline_with_connection_closed","general_warn_no_nline",
"general_non_redundant_klines","general_stats_o_oper_only","general_stats_k_oper_only",
"general_stats_i_oper_only","general_pace_wait","general_caller_id_wait","general_whois_wait",
"general_short_motd","general_no_oper_flood","general_iauth_server","general_iauth_port",
"general_fname_userlog","general_fname_foperlog","general_fname_operlog","general_glines",
"general_message_locale","general_gline_time","general_idletime","general_dots_in_ident",
"general_maximum_links","general_hide_server","general_max_targets","general_servlink_path",
"general_default_cipher_preference","general_compression_level","general_use_egd",
"general_egdpool_path","general_throttle_time","general_oper_umodes","@12","umode_oitems",
"umode_oitem","general_oper_only_umodes","@13","umode_items","umode_item","general_min_nonwildcard",
"general_default_floodcount","general_client_flood","channel_entry","channel_items",
"channel_item","channel_use_invex","channel_use_except","channel_use_knock",
"channel_vchans_oper_only","channel_use_vchans","channel_knock_delay","channel_max_chans_per_user",
"channel_quiet_on_ban","channel_maxbans","channel_persist_time", NULL
};
#endif

static const short yyr1[] = {     0,
   201,   201,   202,   202,   202,   202,   202,   202,   202,   202,
   202,   202,   202,   202,   202,   202,   202,   202,   202,   202,
   202,   203,   203,   203,   203,   203,   203,   203,   203,   203,
   203,   203,   203,   204,   204,   204,   204,   204,   204,   205,
   205,   205,   205,   205,   205,   205,   206,   207,   207,   208,
   208,   208,   209,   210,   211,   212,   212,   213,   213,   213,
   213,   213,   213,   213,   213,   213,   213,   213,   213,   214,
   215,   215,   216,   217,   218,   219,   220,   221,   222,   223,
   224,   224,   225,   226,   226,   227,   227,   227,   227,   228,
   229,   230,   231,   232,   232,   233,   233,   233,   233,   233,
   234,   235,   236,   237,   237,   237,   237,   237,   237,   237,
   239,   238,   240,   240,   241,   241,   241,   241,   241,   241,
   241,   241,   241,   241,   241,   241,   241,   241,   242,   243,
   244,   245,   246,   246,   247,   247,   248,   248,   249,   249,
   250,   250,   251,   251,   252,   252,   253,   253,   254,   254,
   256,   255,   257,   257,   258,   258,   258,   258,   258,   258,
   258,   259,   260,   261,   262,   263,   264,   266,   265,   267,
   267,   268,   268,   268,   268,   269,   270,   270,   271,   271,
   272,   273,   275,   274,   276,   276,   277,   277,   277,   277,
   277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
   278,   279,   280,   280,   281,   282,   282,   283,   283,   284,
   284,   285,   285,   286,   286,   287,   287,   288,   289,   290,
   292,   291,   293,   293,   294,   294,   294,   294,   295,   296,
   297,   299,   298,   300,   300,   301,   301,   301,   301,   302,
   303,   304,   306,   305,   307,   307,   308,   308,   308,   308,
   308,   308,   308,   308,   308,   308,   308,   308,   308,   308,
   308,   308,   308,   308,   309,   310,   311,   312,   313,   314,
   314,   315,   316,   316,   317,   317,   318,   319,   319,   320,
   320,   321,   321,   322,   323,   324,   325,   327,   326,   328,
   328,   329,   329,   329,   330,   331,   333,   332,   334,   334,
   335,   335,   335,   336,   337,   339,   338,   340,   340,   341,
   341,   342,   344,   343,   345,   345,   346,   346,   346,   346,
   347,   348,   349,   349,   349,   350,   351,   351,   352,   352,
   352,   352,   352,   352,   352,   352,   352,   352,   352,   352,
   352,   352,   352,   352,   352,   352,   352,   352,   352,   352,
   352,   352,   352,   352,   352,   352,   352,   352,   352,   352,
   352,   352,   352,   352,   352,   352,   352,   352,   352,   352,
   352,   352,   352,   352,   352,   352,   353,   353,   354,   354,
   355,   356,   357,   358,   359,   360,   361,   362,   363,   363,
   364,   364,   365,   365,   366,   366,   367,   367,   368,   368,
   369,   369,   369,   370,   370,   370,   371,   372,   373,   374,
   374,   375,   375,   376,   377,   378,   379,   380,   381,   381,
   382,   383,   384,   385,   386,   387,   387,   388,   389,   390,
   391,   392,   392,   393,   394,   396,   395,   397,   397,   398,
   398,   398,   398,   398,   398,   398,   398,   398,   398,   398,
   398,   398,   398,   398,   398,   398,   400,   399,   401,   401,
   402,   402,   402,   402,   402,   402,   402,   402,   402,   402,
   402,   402,   402,   402,   402,   402,   402,   403,   404,   405,
   406,   407,   407,   408,   408,   408,   408,   408,   408,   408,
   408,   408,   408,   408,   409,   409,   410,   410,   411,   411,
   412,   412,   413,   413,   414,   415,   416,   416,   417,   418
};

static const short yyr2[] = {     0,
     0,     2,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
     2,     1,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     1,     2,     2,     2,     2,     2,     1,
     3,     3,     3,     3,     2,     3,     5,     2,     1,     1,
     1,     1,     4,     4,     5,     2,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     5,     2,     1,     1,     1,     1,     1,     4,
     4,     4,     5,     2,     1,     1,     1,     1,     1,     1,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     0,     6,     2,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     0,     6,     2,     1,     1,     1,     1,     1,     1,     1,
     1,     4,     4,     4,     4,     4,     4,     0,     6,     2,
     1,     1,     1,     1,     1,     4,     3,     1,     1,     3,
     4,     4,     0,     6,     2,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     0,     6,     2,     1,     1,     1,     1,     1,     4,     4,
     4,     0,     6,     2,     1,     1,     1,     1,     1,     4,
     4,     4,     0,     6,     2,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     0,     6,     2,
     1,     1,     1,     1,     4,     4,     0,     6,     2,     1,
     1,     1,     1,     4,     4,     0,     6,     2,     1,     1,
     1,     4,     0,     6,     2,     1,     1,     1,     1,     1,
     4,     4,     4,     4,     4,     5,     2,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     0,     5,     3,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     0,     5,     3,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     4,     4,     4,
     5,     2,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     4,     4,     4,     4,     4,     4,
     4,     4,     4,     4,     4,     4,     4,     4,     4,     4
};

static const short yydefact[] = {     1,
     0,     0,     0,   183,     0,   151,   243,   297,   306,   313,
     0,   288,   168,     0,     0,   111,   221,     0,   232,     2,
    19,    10,     3,     4,     5,     7,     8,     9,    11,    12,
    13,    14,    15,    16,    18,    17,     6,    20,    21,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,    89,     0,     0,     0,
     0,    85,    86,    88,    87,     0,   494,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,   483,   484,
   485,   486,   487,   488,   490,   491,   492,   489,   493,     0,
     0,     0,     0,     0,   376,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,   457,   436,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,   328,   329,   330,   331,   332,   333,   334,
   335,   336,   365,   375,   337,   357,   338,   339,   340,   342,
   343,   345,   344,   367,   346,   347,   348,   349,   350,   358,
   360,   359,   351,   356,   352,   353,   341,   355,   354,   362,
   370,   371,   372,   363,   364,   374,   366,   361,   369,   368,
   373,     0,     0,   100,     0,     0,     0,     0,     0,    95,
    96,    97,    98,    99,    52,     0,     0,     0,    49,    50,
    51,     0,     0,    69,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,    57,    66,    65,    58,
    61,    62,    63,    59,    67,    64,    68,    60,     0,     0,
     0,     0,     0,    84,   200,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,   186,
   187,   188,   197,   196,   193,   192,   190,   191,   194,   195,
   198,   199,   189,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,   482,   161,     0,     0,     0,     0,
     0,     0,     0,   154,   155,   156,   157,   158,   159,   160,
   264,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,   246,
   247,   248,   249,   250,   251,   252,   253,   254,   259,   262,
   261,   260,   258,   255,   256,   257,   263,   303,     0,     0,
     0,   300,   301,   302,   311,     0,     0,   309,   310,   320,
     0,     0,     0,     0,   316,   317,   318,   319,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,   327,   294,     0,     0,
     0,   291,   292,   293,   175,     0,     0,     0,     0,   171,
   172,   173,   174,     0,     0,     0,     0,     0,    94,     0,
     0,     0,    48,   128,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,   114,   115,
   116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
   126,   127,   228,     0,     0,     0,     0,   224,   225,   226,
   227,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,    56,   239,     0,     0,     0,     0,   235,
   236,   237,   238,     0,     0,     0,    83,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,   185,     0,     0,    40,     0,     0,     0,    22,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   481,     0,     0,     0,     0,     0,     0,     0,
   153,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,   245,
     0,     0,     0,   299,     0,     0,   308,     0,     0,     0,
     0,   315,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,   326,     0,     0,     0,
   290,     0,     0,     0,     0,   170,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,    93,     0,     0,    47,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,   113,     0,     0,     0,     0,   223,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,    55,     0,     0,     0,     0,   234,    92,
    91,    90,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   184,   503,   504,    45,     0,   505,    33,    23,
    24,    25,    26,    27,    28,    29,    30,    31,    32,     0,
     0,     0,     0,   509,   506,   510,   508,   507,   498,   497,
   496,   495,   500,   499,   502,   501,     0,     0,     0,     0,
     0,     0,    34,   152,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,   244,     0,     0,
   298,     0,   307,     0,     0,     0,     0,     0,   314,   380,
   379,   384,   408,   392,   391,   431,   430,   479,   424,   434,
   378,   377,   417,   418,   416,   420,   419,   422,   388,   427,
   426,   415,   414,   423,   394,   393,   390,   389,   387,   425,
   383,   382,   381,   428,   421,   478,   398,   397,   413,   412,
   461,   475,   462,   463,   477,   470,   464,   473,   476,   466,
   471,   467,   472,   465,   469,   468,   474,     0,   460,   440,
   454,   441,   442,   456,   449,   443,   452,   455,   445,   450,
   446,   451,   444,   448,   447,   453,     0,   439,   407,   429,
   411,   410,   405,   406,   404,   402,   403,   401,   400,   399,
   386,   385,   480,   435,   433,   432,   396,   395,   409,     0,
     0,   289,     0,     0,   179,     0,   178,   169,   103,   104,
   110,   105,   109,   107,   108,   106,   102,   101,    53,    54,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   112,     0,     0,     0,   222,    74,    82,    81,
    73,    76,    75,    72,    71,    70,    80,    79,    77,    78,
     0,     0,     0,   233,   220,   207,   206,   217,   216,   213,
   212,   211,   210,   215,   214,   202,   219,   218,   209,   208,
   205,   203,   204,   201,    46,    42,    41,    43,    44,   165,
   166,   162,   164,   163,   167,    35,    36,    37,    38,    39,
   268,   270,   271,   283,   282,   287,   286,   281,   280,   279,
   278,   276,   275,   272,   266,   284,   274,   273,   285,   265,
   269,   277,   267,   304,   305,   312,   325,   324,   323,   321,
   322,   458,     0,   437,     0,   296,   295,   182,   181,     0,
   176,     0,   150,   149,   132,   146,   145,   142,   141,   134,
   133,   138,   137,   129,   144,   143,   131,   148,   147,   136,
   135,   140,   139,   130,   230,   231,   229,   242,   240,   241,
   459,   438,   180,   177,     0,     0
};

static const short yydefgoto[] = {     1,
    20,   729,   762,   519,    21,   208,   209,   210,   211,    22,
   226,   227,   228,   229,   230,   231,   232,   233,   234,   235,
   236,   237,   238,    23,    61,    62,    63,    64,    65,    24,
   199,   200,   201,   202,   203,   204,    25,    53,   448,   449,
   450,   451,   452,   453,   454,   455,   456,   457,   458,   459,
   460,   461,   462,    26,    43,   293,   294,   295,   296,   297,
   298,   299,   300,    27,    50,   419,   420,   421,   906,   907,
   422,   423,    28,    41,   259,   260,   261,   262,   263,   264,
   265,   266,   267,   268,   269,   270,   271,   272,   273,    29,
    54,   467,   468,   469,   470,   471,    30,    56,   489,   490,
   491,   492,   493,    31,    44,   319,   320,   321,   322,   323,
   324,   325,   326,   327,   328,   329,   330,   331,   332,   333,
   334,   335,   336,   337,    32,    49,   411,   412,   413,   414,
    33,    45,   341,   342,   343,   344,    34,    46,   347,   348,
   349,    35,    47,   354,   355,   356,   357,   358,    36,   143,
   144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
   154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
   164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
   174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
   184,   185,   186,   187,   392,   877,   878,   188,   391,   858,
   859,   189,   190,   191,    37,    78,    79,    80,    81,    82,
    83,    84,    85,    86,    87,    88,    89
};

static const short yypact[] = {-32768,
   630,  -154,  -196,-32768,  -194,-32768,-32768,-32768,-32768,-32768,
  -182,-32768,-32768,  -173,  -166,-32768,-32768,  -147,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   483,
  -140,   480,  -130,  -122,  -106,  -103,   -96,   467,   -80,   -71,
     8,    65,   -53,   -48,   355,   -20,-32768,   -18,   -15,    -1,
    26,-32768,-32768,-32768,-32768,   571,-32768,    17,    24,    27,
    51,    57,    59,    61,    63,    74,    76,   278,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,    49,
   706,    22,     4,   139,-32768,    85,    87,    94,   102,   108,
   114,   116,   119,   122,   123,   125,   140,   148,   161,   166,
   172,   178,   181,   188,   190,   192,   204,   208,   219,   224,
   228,   248,   257,   264,   273,   277,   296,-32768,-32768,   297,
   299,   315,   328,   331,   336,   338,   353,   361,   362,   363,
   371,   372,   153,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,    11,   256,-32768,   386,   387,   399,   403,    19,-32768,
-32768,-32768,-32768,-32768,-32768,   412,   414,    37,-32768,-32768,
-32768,   607,    16,-32768,   422,   426,   437,   439,   442,   443,
   445,   449,   451,   460,   469,   191,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,     9,   101,
   109,   240,   131,-32768,-32768,   476,   477,   504,   505,   512,
   514,   516,   518,   521,   522,   523,   524,   539,   218,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   -91,   -25,   -25,   -25,   -25,     7,    56,
    99,   126,   147,   179,-32768,-32768,   541,   543,   544,   545,
   548,   559,    47,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,   579,   594,   595,   596,   597,   610,   617,   619,   620,
   621,   627,   644,   682,   683,   684,   685,   687,    18,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   688,   689,
    41,-32768,-32768,-32768,-32768,   690,     2,-32768,-32768,-32768,
   691,   692,   693,    14,-32768,-32768,-32768,-32768,   159,   -25,
   -25,   163,   -25,   266,   -25,   -25,   325,   197,   356,   448,
   471,   205,   -25,   -25,   234,   -25,   482,   -25,   253,   284,
   -25,   -25,   -25,   -25,   -25,   -25,   517,   -25,   303,   305,
   694,   695,   -25,   573,   345,  -132,  -110,   346,   -25,   -25,
   -25,   -25,   352,   370,   -25,   217,-32768,-32768,   696,   697,
    60,-32768,-32768,-32768,-32768,   698,   699,   700,    77,-32768,
-32768,-32768,-32768,   589,   274,   593,   604,   357,-32768,   631,
   642,   395,-32768,-32768,   701,   702,   703,   704,   705,   707,
   708,   709,   710,   711,   712,   713,   714,   293,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   715,   716,   717,     5,-32768,-32768,-32768,
-32768,   726,   378,   743,   749,   755,   388,   761,   -25,   -25,
   767,   773,   423,-32768,-32768,   718,   719,   720,    30,-32768,
-32768,-32768,-32768,   727,   728,   729,-32768,   779,   389,   398,
   410,   418,   424,   799,   -25,   814,   455,   818,   461,   819,
   732,-32768,   733,   734,-32768,   -25,   -25,   -24,   646,   263,
   288,    -6,   735,   736,   737,   738,   739,   740,   741,   742,
   744,   745,-32768,   -25,   -25,   831,   -25,   -25,   -25,   746,
-32768,   835,    93,   474,   836,   837,   475,   484,   487,   838,
   839,   840,   488,   841,   842,   -25,   843,   844,   757,-32768,
   846,   847,   760,-32768,   849,   762,-32768,  -107,   851,   852,
   765,-32768,   766,   768,   170,   173,   769,   770,   312,   771,
   384,   390,   772,   774,   775,   776,   777,   778,   780,   781,
   174,   402,   782,   783,   496,   784,   194,   785,   786,   787,
   788,   203,   598,   616,   623,   206,   633,   789,   639,   790,
   791,   792,   793,   582,   629,   212,   794,   795,   796,   797,
   798,   800,   801,   802,   803,   804,   805,   220,   223,   650,
   226,   806,   807,   808,   809,   232,-32768,   855,   861,   810,
-32768,   867,   887,   911,   812,-32768,   813,   815,   816,   817,
   820,   821,   822,   823,   824,   825,-32768,   826,   827,-32768,
   500,   902,   511,   525,   526,   537,   906,   538,   907,   549,
   553,   557,   916,   829,-32768,   918,   919,   920,   833,-32768,
   834,   845,   848,   850,   853,   854,   856,   857,   858,   656,
   662,   859,   860,-32768,   923,   924,   925,   862,-32768,-32768,
-32768,-32768,   863,   864,   865,   866,   868,   869,   870,   871,
   872,   873,   874,   875,   668,   876,   877,   878,   879,   880,
   881,   882,-32768,-32768,-32768,-32768,  -145,-32768,   -25,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   -25,
   -25,   -25,   -25,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,   236,   674,   883,   680,
   255,   884,   144,-32768,   885,   886,   888,   889,   890,   891,
   892,   893,   894,   895,   896,   897,   898,   899,   900,   901,
   903,   904,   905,   908,   686,   909,   910,-32768,   912,   913,
-32768,   914,-32768,   915,   917,   921,   922,   926,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,  -135,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,  -119,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   927,
   928,-32768,   929,   930,   937,  -117,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   931,   932,   933,   934,   935,   936,   938,   939,   940,   941,
   942,   943,   944,   945,   946,   947,   948,   949,   950,   951,
   952,   953,-32768,   954,   955,   956,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   957,   958,   959,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   -29,   -29,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   582,-32768,   629,-32768,-32768,-32768,-32768,   961,
-32768,   911,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,  1032,-32768
};

static const short yypgoto[] = {-32768,
-32768,  -264,-32768,  -276,-32768,-32768,   828,-32768,-32768,-32768,
-32768,   811,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   972,-32768,-32768,-32768,-32768,
-32768,   960,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   586,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,   747,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,   622,-32768,-32768,    -7,
-32768,-32768,-32768,-32768,-32768,   962,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   576,-32768,-32768,-32768,-32768,-32768,-32768,   556,
-32768,-32768,-32768,-32768,-32768,-32768,   730,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   635,-32768,-32768,
-32768,-32768,-32768,   759,-32768,-32768,-32768,-32768,-32768,   754,
-32768,-32768,-32768,-32768,   751,-32768,-32768,-32768,-32768,-32768,
   967,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,     3,-32768,-32768,-32768,
    28,-32768,-32768,-32768,-32768,-32768,  1018,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768
};


#define	YYLAST		1221


static const short yytable[] = {   520,
   521,    40,   345,    42,   345,   463,   620,   621,   194,   485,
   518,   408,   622,   522,   350,    48,   463,   351,   301,   194,
   302,   464,   338,   303,    51,   794,    57,   304,   623,   624,
   485,    52,   464,   795,   625,   305,   306,   205,   307,    38,
    39,   338,   308,   740,   741,   742,   743,   286,   513,   286,
    55,   985,   309,   514,    58,   195,   310,    66,  1032,    59,
   408,   346,   486,   346,  1033,   205,   195,    90,   515,   515,
   287,   311,   287,   312,  1034,    91,  1041,   415,   796,   196,
  1035,   339,  1042,   486,   313,   314,   579,   515,   581,   582,
   196,    92,   465,   487,    93,   575,   576,   592,   352,   595,
   339,    94,   315,   465,   197,   603,   604,   605,   591,   607,
    60,   609,   466,   597,   487,   197,   602,   192,   409,   206,
   606,   353,   316,   466,   630,   288,   193,   288,   616,   340,
   416,   289,   317,   289,   628,   629,   417,   631,   207,   350,
   636,   290,   351,   290,   212,   318,   523,   206,   340,   213,
   291,   524,   291,    95,   996,   997,   998,   999,  1000,    96,
    97,   742,   743,   516,   516,   198,   207,   409,    98,   728,
   517,   517,    99,   292,   100,   292,   198,   239,   101,   102,
   240,   418,   516,   241,   103,   104,   488,   746,   410,   517,
   105,   214,   106,   107,   108,   525,   566,   242,   109,   679,
   526,   110,   690,   691,   111,   112,   494,   488,   571,   113,
   114,   115,   559,   428,   495,   274,   116,   117,   245,   215,
   243,   118,   275,   352,   698,   276,   119,   120,   715,   121,
   122,   432,   123,   124,   125,   563,   246,   410,   527,   726,
   727,   540,   126,   528,   127,   216,   353,   766,   767,   277,
   128,   129,   130,   247,   640,   278,   415,   279,   758,   280,
   760,   281,   763,   515,   248,   529,   515,   515,   249,   757,
   530,   645,   282,   761,   283,   217,   218,   219,    67,   785,
   250,   220,   131,   359,   132,   360,   531,   515,   133,   134,
   135,   532,   361,   434,   136,   137,   515,   435,   573,   515,
   362,   138,   577,   574,   221,   515,   363,   578,    68,   416,
   251,   436,   364,   515,   365,   417,   515,   366,   252,   515,
   367,   368,   437,   369,   497,   515,   253,   254,   139,   515,
   255,   140,   740,   741,   742,   743,   584,   438,   370,   141,
   142,   585,   439,    69,   589,   496,   371,   406,   515,   590,
    70,   256,   257,    71,   440,   214,   222,   223,   516,   372,
   418,   516,   516,   802,   373,   517,   803,   818,   517,   517,
   374,   580,   533,   593,   224,   225,   375,   441,   594,   376,
    72,   442,   516,   215,    73,   483,   377,   824,   378,   517,
   379,   516,   598,   443,   516,   258,   829,   599,   517,   833,
   516,   517,   380,   444,   445,   879,   381,   517,   516,   216,
   637,   516,   511,   891,   516,   517,   892,   382,   517,   894,
   516,   517,   383,   600,   516,   899,   384,   517,   601,   990,
   583,   517,   648,   649,   650,   651,   652,   653,   654,   217,
   218,   219,   610,   516,   612,   220,   385,   611,   994,   613,
   517,   740,   741,   742,   743,   386,   744,    74,    75,    76,
    77,   586,   387,   986,   987,   988,   989,    95,   221,   446,
   447,   388,   284,    96,    97,   389,   740,   741,   742,   743,
    67,   745,    98,    57,   618,   626,    99,   674,   100,   619,
   627,   632,   101,   102,   390,   393,   633,   394,   103,   104,
   740,   741,   742,   743,   105,   806,   106,   107,   108,   634,
    68,    58,   109,   395,   635,   110,    59,   682,   111,   112,
   222,   223,   683,   113,   114,   115,   396,   687,   704,   397,
   116,   117,   688,   705,   398,   118,   399,   706,   224,   225,
   119,   120,   707,   121,   122,    69,   123,   124,   125,   708,
   657,   400,    70,   587,   709,    71,   126,   710,   127,   401,
   402,   403,   711,   712,   128,   129,   130,    60,   713,   404,
   405,   245,   740,   741,   742,   743,   588,   808,   740,   741,
   742,   743,    72,   809,   424,   425,    73,   596,   660,   246,
   740,   741,   742,   743,   717,   819,   131,   426,   132,   718,
   720,   427,   133,   134,   135,   721,   247,   434,   136,   137,
   430,   435,   431,   768,   772,   138,   694,   248,   769,   773,
   472,   249,   608,   774,   473,   436,   776,   781,   775,  1075,
     2,   777,   782,   250,     3,   474,   437,   475,     4,   921,
   476,   477,   139,   478,   922,   140,     5,   479,     6,   480,
   924,   438,     7,   141,   142,   925,   439,     8,   481,    74,
    75,    76,    77,   251,   926,   928,     9,   482,   440,   927,
   929,   252,    10,    11,   498,   499,   930,   933,   617,   253,
   254,   931,   934,   255,   740,   741,   742,   743,   936,   822,
    12,   441,   938,   937,   647,   442,   940,   939,   655,    13,
    14,   941,   500,   501,   256,   257,   301,   443,   302,   656,
   502,   303,   503,    15,   504,   304,   505,   444,   445,   506,
   507,   508,   509,   305,   306,    16,   307,   841,   842,   843,
   308,   844,   845,   846,   847,   848,   658,   510,   849,   534,
   309,   535,   536,   537,   310,    17,   538,   659,   258,   850,
   851,   852,   853,   854,   855,   856,   857,   539,    18,   311,
    19,   312,   730,   731,   732,   733,   734,   735,   736,   737,
   738,   739,   313,   314,   860,   861,   862,   542,   863,   864,
   865,   866,   867,   446,   447,   868,   740,   741,   742,   743,
   315,   830,   543,   544,   545,   546,   869,   870,   871,   872,
   873,   874,   875,   876,   740,   741,   742,   743,   547,   831,
   316,   740,   741,   742,   743,   548,   832,   549,   550,   551,
   317,   740,   741,   742,   743,   552,   834,   740,   741,   742,
   743,   681,   836,   318,   740,   741,   742,   743,   740,   741,
   742,   743,   553,   893,   740,   741,   742,   743,   684,   957,
   740,   741,   742,   743,   685,   958,   740,   741,   742,   743,
   686,   977,   740,   741,   742,   743,   689,   991,   740,   741,
   742,   743,   692,   993,   740,   741,   742,   743,   693,  1021,
   554,   555,   556,   557,   703,   558,   561,   562,   565,   568,
   569,   570,   614,   615,   638,   639,   642,   643,   644,   661,
   662,   663,   664,   665,   714,   666,   667,   668,   669,   670,
   671,   672,   673,   676,   677,   678,   695,   696,   697,   716,
   700,   701,   702,   719,   722,   723,   724,   725,   747,   748,
   749,   750,   751,   752,   753,   754,   759,   755,   756,   764,
   765,   770,   771,   778,   779,   780,   783,   784,   786,   787,
   788,   789,   790,   791,   792,   793,   797,   798,   799,   800,
   900,   801,   804,   805,   807,   810,   901,   811,   812,   813,
   814,   815,   903,   816,   817,   820,   821,   823,   825,   826,
   827,   828,   835,   837,   838,   839,   840,   880,   881,   882,
   883,   884,   904,   885,   886,   887,   888,   889,   890,   895,
   896,   897,   898,   902,   905,   908,   909,   923,   910,   911,
   912,   932,   935,   913,   914,   915,   916,   917,   918,   919,
   920,   942,   943,   944,   945,   946,   947,   948,   961,   962,
   963,  1076,   244,   675,  1074,   433,   484,  1072,   949,   541,
   646,   950,   680,   951,   699,   641,   952,   953,   560,   954,
   955,   956,   959,   960,  1073,   964,   965,   966,   967,   968,
  1071,   969,   970,   971,   972,   973,   974,   975,   976,   978,
   979,   980,   981,   982,   983,   984,   992,   995,  1001,  1002,
  1040,  1003,  1004,  1005,  1006,  1007,  1008,  1009,  1010,  1011,
  1012,  1013,  1014,  1015,  1016,   285,  1017,  1018,  1019,   564,
   567,  1020,  1022,  1023,   572,  1024,  1025,  1026,  1027,   407,
  1028,     0,     0,     0,  1029,  1030,     0,     0,     0,  1031,
  1036,  1037,  1038,  1039,  1043,  1044,  1045,  1046,  1047,  1048,
     0,  1049,  1050,  1051,  1052,  1053,  1054,  1055,  1056,  1057,
  1058,  1059,  1060,  1061,  1062,  1063,  1064,  1065,  1066,  1067,
  1068,  1069,  1070,     0,     0,     0,     0,     0,   429,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   512
};

static const short yycheck[] = {   276,
   277,   198,     1,   198,     1,     1,   139,   140,     1,     1,
   275,     1,   145,   278,     1,   198,     1,     4,     1,     1,
     3,    17,     1,     6,   198,   133,     1,    10,   139,   140,
     1,   198,    17,   141,   145,    18,    19,     1,    21,   194,
   195,     1,    25,   189,   190,   191,   192,     1,   140,     1,
   198,   197,    35,   145,    29,    48,    39,   198,   194,    34,
     1,    60,    54,    60,   200,     1,    48,   198,    94,    94,
    24,    54,    24,    56,   194,   198,   194,     1,   186,    72,
   200,    60,   200,    54,    67,    68,   363,    94,   365,   366,
    72,   198,    88,    85,   198,   360,   361,   374,    85,   376,
    60,   198,    85,    88,    97,   382,   383,   384,   373,   386,
    85,   388,   108,   378,    85,    97,   381,   198,   108,    83,
   385,   108,   105,   108,   401,    79,   198,    79,   393,   108,
    54,    85,   115,    85,   399,   400,    60,   402,   102,     1,
   405,    95,     4,    95,   198,   128,   140,    83,   108,   198,
   104,   145,   104,     1,    11,    12,    13,    14,    15,     7,
     8,   191,   192,   189,   189,   158,   102,   108,    16,   194,
   196,   196,    20,   127,    22,   127,   158,   198,    26,    27,
   199,   105,   189,   199,    32,    33,   178,   194,   178,   196,
    38,     1,    40,    41,    42,   140,   195,   199,    46,   195,
   145,    49,   479,   480,    52,    53,   106,   178,   195,    57,
    58,    59,   195,   195,   106,   199,    64,    65,     1,    29,
   195,    69,   199,    85,   195,   199,    74,    75,   505,    77,
    78,   195,    80,    81,    82,   195,    19,   178,   140,   516,
   517,   195,    90,   145,    92,    55,   108,   155,   156,   199,
    98,    99,   100,    36,   195,   199,     1,   199,   535,   199,
   537,   199,   539,    94,    47,   140,    94,    94,    51,   534,
   145,   195,   199,   538,   199,    85,    86,    87,     1,   556,
    63,    91,   130,   199,   132,   199,   140,    94,   136,   137,
   138,   145,   199,     1,   142,   143,    94,     5,   140,    94,
   199,   149,   140,   145,   114,    94,   199,   145,    31,    54,
    93,    19,   199,    94,   199,    60,    94,   199,   101,    94,
   199,   199,    30,   199,   194,    94,   109,   110,   176,    94,
   113,   179,   189,   190,   191,   192,   140,    45,   199,   187,
   188,   145,    50,    66,   140,   106,   199,   195,    94,   145,
    73,   134,   135,    76,    62,     1,   166,   167,   189,   199,
   105,   189,   189,   194,   199,   196,   194,   194,   196,   196,
   199,   106,   194,   140,   184,   185,   199,    85,   145,   199,
   103,    89,   189,    29,   107,   195,   199,   194,   199,   196,
   199,   189,   140,   101,   189,   178,   194,   145,   196,   194,
   189,   196,   199,   111,   112,   194,   199,   196,   189,    55,
   194,   189,   195,   194,   189,   196,   194,   199,   196,   194,
   189,   196,   199,   140,   189,   194,   199,   196,   145,   194,
   106,   196,   159,   160,   161,   162,   163,   164,   165,    85,
    86,    87,   140,   189,   140,    91,   199,   145,   194,   145,
   196,   189,   190,   191,   192,   199,   194,   180,   181,   182,
   183,   106,   199,   740,   741,   742,   743,     1,   114,   177,
   178,   199,   195,     7,     8,   199,   189,   190,   191,   192,
     1,   194,    16,     1,   140,   140,    20,   195,    22,   145,
   145,   140,    26,    27,   199,   199,   145,   199,    32,    33,
   189,   190,   191,   192,    38,   194,    40,    41,    42,   140,
    31,    29,    46,   199,   145,    49,    34,   140,    52,    53,
   166,   167,   145,    57,    58,    59,   199,   140,   140,   199,
    64,    65,   145,   145,   199,    69,   199,   140,   184,   185,
    74,    75,   145,    77,    78,    66,    80,    81,    82,   140,
   194,   199,    73,   106,   145,    76,    90,   140,    92,   199,
   199,   199,   145,   140,    98,    99,   100,    85,   145,   199,
   199,     1,   189,   190,   191,   192,   106,   194,   189,   190,
   191,   192,   103,   194,   199,   199,   107,   106,   194,    19,
   189,   190,   191,   192,   140,   194,   130,   199,   132,   145,
   140,   199,   136,   137,   138,   145,    36,     1,   142,   143,
   199,     5,   199,   140,   140,   149,   194,    47,   145,   145,
   199,    51,   106,   140,   199,    19,   140,   140,   145,     0,
     1,   145,   145,    63,     5,   199,    30,   199,     9,   140,
   199,   199,   176,   199,   145,   179,    17,   199,    19,   199,
   140,    45,    23,   187,   188,   145,    50,    28,   199,   180,
   181,   182,   183,    93,   140,   140,    37,   199,    62,   145,
   145,   101,    43,    44,   199,   199,   140,   140,   106,   109,
   110,   145,   145,   113,   189,   190,   191,   192,   140,   194,
    61,    85,   140,   145,   106,    89,   140,   145,   106,    70,
    71,   145,   199,   199,   134,   135,     1,   101,     3,   106,
   199,     6,   199,    84,   199,    10,   199,   111,   112,   199,
   199,   199,   199,    18,    19,    96,    21,   146,   147,   148,
    25,   150,   151,   152,   153,   154,   106,   199,   157,   199,
    35,   199,   199,   199,    39,   116,   199,   106,   178,   168,
   169,   170,   171,   172,   173,   174,   175,   199,   129,    54,
   131,    56,   117,   118,   119,   120,   121,   122,   123,   124,
   125,   126,    67,    68,   146,   147,   148,   199,   150,   151,
   152,   153,   154,   177,   178,   157,   189,   190,   191,   192,
    85,   194,   199,   199,   199,   199,   168,   169,   170,   171,
   172,   173,   174,   175,   189,   190,   191,   192,   199,   194,
   105,   189,   190,   191,   192,   199,   194,   199,   199,   199,
   115,   189,   190,   191,   192,   199,   194,   189,   190,   191,
   192,   106,   194,   128,   189,   190,   191,   192,   189,   190,
   191,   192,   199,   194,   189,   190,   191,   192,   106,   194,
   189,   190,   191,   192,   106,   194,   189,   190,   191,   192,
   106,   194,   189,   190,   191,   192,   106,   194,   189,   190,
   191,   192,   106,   194,   189,   190,   191,   192,   106,   194,
   199,   199,   199,   199,   106,   199,   199,   199,   199,   199,
   199,   199,   199,   199,   199,   199,   199,   199,   199,   199,
   199,   199,   199,   199,   106,   199,   199,   199,   199,   199,
   199,   199,   199,   199,   199,   199,   199,   199,   199,   106,
   194,   194,   194,   106,   106,   194,   194,   194,   194,   194,
   194,   194,   194,   194,   194,   194,   106,   194,   194,   194,
   106,   106,   106,   106,   106,   106,   106,   106,   106,   106,
   194,   106,   106,   194,   106,   194,   106,   106,   194,   194,
   106,   194,   194,   194,   194,   194,   106,   194,   194,   194,
   194,   194,   106,   194,   194,   194,   194,   194,   194,   194,
   194,   194,   194,   194,   194,   194,   194,   194,   194,   194,
   194,   194,   106,   194,   194,   194,   194,   194,   194,   194,
   194,   194,   194,   194,    94,   194,   194,   106,   194,   194,
   194,   106,   106,   194,   194,   194,   194,   194,   194,   194,
   194,   106,   194,   106,   106,   106,   194,   194,   106,   106,
   106,     0,    61,   448,  1042,   208,   226,  1035,   194,   293,
   419,   194,   467,   194,   489,   411,   194,   194,   319,   194,
   194,   194,   194,   194,    94,   194,   194,   194,   194,   194,
  1033,   194,   194,   194,   194,   194,   194,   194,   194,   194,
   194,   194,   194,   194,   194,   194,   194,   194,   194,   194,
   144,   194,   194,   194,   194,   194,   194,   194,   194,   194,
   194,   194,   194,   194,   194,    78,   194,   194,   194,   341,
   347,   194,   194,   194,   354,   194,   194,   194,   194,   143,
   194,    -1,    -1,    -1,   194,   194,    -1,    -1,    -1,   194,
   194,   194,   194,   194,   194,   194,   194,   194,   194,   194,
    -1,   194,   194,   194,   194,   194,   194,   194,   194,   194,
   194,   194,   194,   194,   194,   194,   194,   194,   194,   194,
   194,   194,   194,    -1,    -1,    -1,    -1,    -1,   199,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
   259
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "bison.simple"
/* This file comes from bison-@bison_version@.  */

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

#line 217 "bison.simple"

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

case 22:
#line 311 "ircd_parser.y"
{
			yyval.number = yyvsp[0].number;
		;
    break;}
case 23:
#line 315 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number;
		;
    break;}
case 24:
#line 319 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60;
		;
    break;}
case 25:
#line 323 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60;
		;
    break;}
case 26:
#line 327 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24;
		;
    break;}
case 27:
#line 331 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 7;
		;
    break;}
case 28:
#line 335 "ircd_parser.y"
{
			/* a month has 28 days, or 4 weeks */
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 7 * 4;
		;
    break;}
case 29:
#line 340 "ircd_parser.y"
{
			/* a year has 365 days, *not* 12 months */
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 365;
		;
    break;}
case 30:
#line 345 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 365 * 10;
		;
    break;}
case 31:
#line 349 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 365 * 10 * 10;
		;
    break;}
case 32:
#line 353 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 60 * 60 * 24 * 365 * 10 * 10 * 10;
		;
    break;}
case 33:
#line 357 "ircd_parser.y"
{
			/* 2 years 3 days */
			yyval.number = yyvsp[-1].number + yyvsp[0].number;
		;
    break;}
case 34:
#line 364 "ircd_parser.y"
{
			yyval.number = yyvsp[0].number;
		;
    break;}
case 35:
#line 368 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-1].number;
		;
    break;}
case 36:
#line 372 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 1024;
		;
    break;}
case 37:
#line 376 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 1024 * 1024;
		;
    break;}
case 38:
#line 380 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 1024 * 1024 * 1024;
		;
    break;}
case 39:
#line 384 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number * 1024 * 1024 * 1024;
		;
    break;}
case 40:
#line 391 "ircd_parser.y"
{ 
			yyval.number = yyvsp[0].number;
		;
    break;}
case 41:
#line 395 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-2].number + yyvsp[0].number;
		;
    break;}
case 42:
#line 399 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-2].number - yyvsp[0].number;
		;
    break;}
case 43:
#line 403 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-2].number * yyvsp[0].number;
		;
    break;}
case 44:
#line 407 "ircd_parser.y"
{ 
			yyval.number = yyvsp[-2].number / yyvsp[0].number;
		;
    break;}
case 45:
#line 412 "ircd_parser.y"
{
			yyval.number = -yyvsp[0].number;
		;
    break;}
case 46:
#line 416 "ircd_parser.y"
{
			yyval.number = yyvsp[-1].number;
		;
    break;}
case 53:
#line 435 "ircd_parser.y"
{
#ifndef STATIC_MODULES /* NOOP in the static case */
  char *m_bn;

  m_bn = irc_basename(yylval.string);

  /* I suppose we should just ignore it if it is already loaded(since
   * otherwise we would flood the opers on rehash) -A1kmm. */
  if (findmodule_byname(m_bn) != -1)
    break;

  /* XXX - should we unload this module on /rehash, if it isn't listed? */
  load_one_module (yylval.string);

  MyFree(m_bn);
#endif
;
    break;}
case 54:
#line 454 "ircd_parser.y"
{
#ifndef STATIC_MODULES
  mod_add_path(yylval.string);
#endif
;
    break;}
case 70:
#line 480 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
  BIO *file;

  if (ServerInfo.rsa_private_key)
  {
    RSA_free(ServerInfo.rsa_private_key);
    ServerInfo.rsa_private_key = NULL;
  }

  if (ServerInfo.rsa_private_key_file)
  {
    MyFree(ServerInfo.rsa_private_key_file);
    ServerInfo.rsa_private_key_file = NULL;
  }

  ServerInfo.rsa_private_key_file = strdup(yylval.string);

  file = BIO_new_file( yylval.string, "r" );

  if (file == NULL)
  {
    sendto_realops_flags(FLAGS_ALL, L_ALL,
      "Ignoring config file entry rsa_private_key -- file open failed"
      " (%s)", yylval.string);
    break;
  }

  ServerInfo.rsa_private_key = (RSA *) PEM_read_bio_RSAPrivateKey(file,
                                                            NULL, 0, NULL);
  if (ServerInfo.rsa_private_key == NULL)
  {
    sendto_realops_flags(FLAGS_ALL, L_ALL,
      "Ignoring config file entry rsa_private_key -- couldn't extract key");
    break;
  }

  if (!RSA_check_key( ServerInfo.rsa_private_key ))
  {
    sendto_realops_flags(FLAGS_ALL, L_ALL,
      "Ignoring config file entry rsa_private_key -- invalid key");
    break;
  }

  /* require 2048 bit (256 byte) key */
  if ( RSA_size(ServerInfo.rsa_private_key) != 256 )
  {
    sendto_realops_flags(FLAGS_ALL, L_ALL,
      "Ignoring config file entry rsa_private_key -- not 2048 bit");
    break;
  }

  BIO_set_close(file, BIO_CLOSE);
  BIO_free(file);
#endif
  ;
    break;}
case 71:
#line 538 "ircd_parser.y"
{
    ServerInfo.no_hack_ops = 1;
  ;
    break;}
case 72:
#line 543 "ircd_parser.y"
{
    ServerInfo.no_hack_ops = 0;
  ;
    break;}
case 73:
#line 549 "ircd_parser.y"
{
    /* this isn't rehashable */
    if(ServerInfo.name == NULL)
      DupString(ServerInfo.name,yylval.string);
  ;
    break;}
case 74:
#line 556 "ircd_parser.y"
{
    MyFree(ServerInfo.description);
    DupString(ServerInfo.description,yylval.string);
  ;
    break;}
case 75:
#line 562 "ircd_parser.y"
{
    MyFree(ServerInfo.network_name);
    DupString(ServerInfo.network_name,yylval.string);
  ;
    break;}
case 76:
#line 568 "ircd_parser.y"
{
    MyFree(ServerInfo.network_desc);
    DupString(ServerInfo.network_desc,yylval.string);
  ;
    break;}
case 77:
#line 574 "ircd_parser.y"
{
    if(inetpton(DEF_FAM, yylval.string, &IN_ADDR(ServerInfo.ip)) <= 0)
    {
     ilog(L_ERROR, "Invalid netmask for server vhost(%s)", yylval.string);
    }
    ServerInfo.specific_ipv4_vhost = 1;
  ;
    break;}
case 78:
#line 583 "ircd_parser.y"
{
#ifdef IPV6
    if(inetpton(DEF_FAM,yylval.string, &IN_ADDR(ServerInfo.ip6)) <= 0)
      {
        ilog(L_ERROR, "Invalid netmask for server vhost(%s)", yylval.string);
      }
    ServerInfo.specific_ipv6_vhost = 1;
#endif
  ;
    break;}
case 79:
#line 594 "ircd_parser.y"
{
    ServerInfo.max_clients = yyvsp[-1].number;
  ;
    break;}
case 80:
#line 599 "ircd_parser.y"
{
    ServerInfo.max_buffer = yyvsp[-1].number;
  ;
    break;}
case 81:
#line 604 "ircd_parser.y"
{
    /* Don't become a hub if we have a lazylink active. */
    if (!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,
        "Ignoring config file line hub = yes; due to active LazyLink (%s)",
        uplink->name);
    }
    else
    {
      ServerInfo.hub = 1;
      uplink = NULL;
    }
  ;
    break;}
case 82:
#line 620 "ircd_parser.y"
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
          sendto_realops_flags(FLAGS_ALL, L_ALL,
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
case 90:
#line 654 "ircd_parser.y"
{
    MyFree(AdminInfo.name);
    DupString(AdminInfo.name, yylval.string);
  ;
    break;}
case 91:
#line 660 "ircd_parser.y"
{
    MyFree(AdminInfo.email);
    DupString(AdminInfo.email, yylval.string);
  ;
    break;}
case 92:
#line 666 "ircd_parser.y"
{
    MyFree(AdminInfo.description);
    DupString(AdminInfo.description, yylval.string);
  ;
    break;}
case 101:
#line 685 "ircd_parser.y"
{
                        ;
    break;}
case 102:
#line 689 "ircd_parser.y"
{
                        ;
    break;}
case 103:
#line 693 "ircd_parser.y"
{
                        ;
    break;}
case 104:
#line 697 "ircd_parser.y"
{ set_log_level(L_CRIT); ;
    break;}
case 105:
#line 700 "ircd_parser.y"
{ set_log_level(L_ERROR); ;
    break;}
case 106:
#line 703 "ircd_parser.y"
{ set_log_level(L_WARN); ;
    break;}
case 107:
#line 706 "ircd_parser.y"
{ set_log_level(L_NOTICE); ;
    break;}
case 108:
#line 709 "ircd_parser.y"
{ set_log_level(L_TRACE); ;
    break;}
case 109:
#line 712 "ircd_parser.y"
{ set_log_level(L_INFO); ;
    break;}
case 110:
#line 715 "ircd_parser.y"
{ set_log_level(L_DEBUG); ;
    break;}
case 111:
#line 722 "ircd_parser.y"
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
case 112:
#line 739 "ircd_parser.y"
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
case 129:
#line 785 "ircd_parser.y"
{
   int oname_len;
   MyFree(yy_achead->name);
   if ((oname_len = strlen(yylval.string)) > OPERNICKLEN)
    yylval.string[OPERNICKLEN] = 0;
   DupString(yy_achead->name, yylval.string);
  ;
    break;}
case 130:
#line 794 "ircd_parser.y"
{
    char *p;
    char *new_user;
    char *new_host;

    /* The first user= line doesn't allocate a new conf */
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
case 131:
#line 826 "ircd_parser.y"
{
    if (yy_achead->passwd)
      memset(yy_achead->passwd, 0, strlen(yy_achead->passwd));
    MyFree(yy_achead->passwd);
    DupString(yy_achead->passwd, yylval.string);
  ;
    break;}
case 132:
#line 834 "ircd_parser.y"
{
    MyFree(yy_achead->className);
    DupString(yy_achead->className, yylval.string);
  ;
    break;}
case 133:
#line 840 "ircd_parser.y"
{
    yy_achead->port |= CONF_OPER_GLOBAL_KILL;
  ;
    break;}
case 134:
#line 845 "ircd_parser.y"
{
    yy_achead->port &= ~CONF_OPER_GLOBAL_KILL;
  ;
    break;}
case 135:
#line 849 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_REMOTE;;
    break;}
case 136:
#line 851 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_REMOTE; ;
    break;}
case 137:
#line 853 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_K;;
    break;}
case 138:
#line 855 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_K; ;
    break;}
case 139:
#line 857 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_UNKLINE;;
    break;}
case 140:
#line 859 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_UNKLINE; ;
    break;}
case 141:
#line 861 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_GLINE;;
    break;}
case 142:
#line 863 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_GLINE; ;
    break;}
case 143:
#line 865 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_N;;
    break;}
case 144:
#line 867 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_N;;
    break;}
case 145:
#line 869 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_DIE; ;
    break;}
case 146:
#line 871 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_DIE; ;
    break;}
case 147:
#line 873 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_REHASH;;
    break;}
case 148:
#line 875 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_REHASH; ;
    break;}
case 149:
#line 877 "ircd_parser.y"
{ yy_achead->port |= CONF_OPER_ADMIN;;
    break;}
case 150:
#line 879 "ircd_parser.y"
{ yy_achead->port &= ~CONF_OPER_ADMIN;;
    break;}
case 151:
#line 886 "ircd_parser.y"
{
    MyFree(class_name_var);
    class_name_var = NULL;
    class_ping_time_var = 0;
    class_number_per_ip_var = 0;
    class_max_number_var = 0;
    class_sendq_var = 0;
  ;
    break;}
case 152:
#line 895 "ircd_parser.y"
{

    add_class(class_name_var,class_ping_time_var,
              class_number_per_ip_var, class_max_number_var,
              class_sendq_var );

    MyFree(class_name_var);
    class_name_var = NULL;
  ;
    break;}
case 162:
#line 917 "ircd_parser.y"
{
    MyFree(class_name_var);
    DupString(class_name_var, yylval.string);
  ;
    break;}
case 163:
#line 923 "ircd_parser.y"
{
    class_ping_time_var = yyvsp[-1].number;
  ;
    break;}
case 164:
#line 928 "ircd_parser.y"
{
    class_number_per_ip_var = yyvsp[-1].number;
  ;
    break;}
case 165:
#line 933 "ircd_parser.y"
{
    class_number_per_ip_var = yyvsp[-1].number;
  ;
    break;}
case 166:
#line 938 "ircd_parser.y"
{
    class_max_number_var = yyvsp[-1].number;
  ;
    break;}
case 167:
#line 943 "ircd_parser.y"
{
    class_sendq_var = yyvsp[-1].number;
  ;
    break;}
case 168:
#line 953 "ircd_parser.y"
{
    listener_address = NULL;
  ;
    break;}
case 169:
#line 957 "ircd_parser.y"
{
    MyFree(listener_address);
    listener_address = NULL;
  ;
    break;}
case 179:
#line 972 "ircd_parser.y"
{
  add_listener(yyvsp[0].number, listener_address);
;
    break;}
case 180:
#line 975 "ircd_parser.y"
{
  int i;
  for (i = yyvsp[-2].number; i <= yyvsp[0].number; i++)
	{
	  add_listener(i, listener_address);
	}
;
    break;}
case 181:
#line 984 "ircd_parser.y"
{
    MyFree(listener_address);
    DupString(listener_address, yylval.string);
  ;
    break;}
case 182:
#line 990 "ircd_parser.y"
{
    MyFree(listener_address);
    DupString(listener_address, yylval.string);
  ;
    break;}
case 183:
#line 1000 "ircd_parser.y"
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
case 184:
#line 1021 "ircd_parser.y"
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
case 201:
#line 1078 "ircd_parser.y"
{
    char *p;
    char *new_user;
    char *new_host;

    /* The first user= line doesn't allocate a new conf */
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
case 202:
#line 1112 "ircd_parser.y"
{
    /* be paranoid */
    if (yy_achead->passwd)
      memset(yy_achead->passwd, 0, strlen(yy_achead->passwd));
    MyFree(yy_achead->passwd);
    DupString(yy_achead->passwd,yylval.string);
  ;
    break;}
case 203:
#line 1122 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_SPOOF_NOTICE;
  ;
    break;}
case 204:
#line 1127 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_SPOOF_NOTICE;
  ;
    break;}
case 205:
#line 1132 "ircd_parser.y"
{
    MyFree(yy_achead->name);
    if(strlen(yylval.string) < HOSTLEN)
    {    
	DupString(yy_achead->name, yylval.string);
    	yy_achead->flags |= CONF_FLAGS_SPOOF_IP;
    } else
	ilog(L_ERROR, "Spoofs must be less than %d..ignoring it", HOSTLEN);
  ;
    break;}
case 206:
#line 1143 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_NOLIMIT;
  ;
    break;}
case 207:
#line 1148 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_NOLIMIT;
  ;
    break;}
case 208:
#line 1153 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_RESTRICTED;
  ;
    break;}
case 209:
#line 1158 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_RESTRICTED;
  ;
    break;}
case 210:
#line 1163 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_EXEMPTKLINE;
  ;
    break;}
case 211:
#line 1168 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_EXEMPTKLINE;
  ;
    break;}
case 212:
#line 1173 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_NEED_IDENTD;
  ;
    break;}
case 213:
#line 1178 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_NEED_IDENTD;
  ;
    break;}
case 214:
#line 1183 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_NO_TILDE;
  ;
    break;}
case 215:
#line 1188 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_NO_TILDE;
  ;
    break;}
case 216:
#line 1193 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_EXEMPTGLINE;
  ;
    break;}
case 217:
#line 1198 "ircd_parser.y"
{
    yy_achead->flags &= ~CONF_FLAGS_EXEMPTGLINE;
  ;
    break;}
case 218:
#line 1204 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_REDIR;
    MyFree(yy_achead->name);
    DupString(yy_achead->name, yylval.string);
  ;
    break;}
case 219:
#line 1211 "ircd_parser.y"
{
    yy_achead->flags |= CONF_FLAGS_REDIR;
    yy_achead->port = yyvsp[-1].number;
  ;
    break;}
case 220:
#line 1217 "ircd_parser.y"
{
    MyFree(yy_achead->className);
    DupString(yy_achead->className, yylval.string);
  ;
    break;}
case 221:
#line 1228 "ircd_parser.y"
{
    resv_reason = NULL;
  ;
    break;}
case 222:
#line 1232 "ircd_parser.y"
{
    MyFree(resv_reason);
    resv_reason = NULL;
  ;
    break;}
case 229:
#line 1243 "ircd_parser.y"
{
  MyFree(resv_reason);
  DupString(resv_reason, yylval.string);
;
    break;}
case 230:
#line 1249 "ircd_parser.y"
{
  if(IsChannelName(yylval.string))
  {
    if(resv_reason)
      create_channel_resv(yylval.string, resv_reason, 1);
    else
      create_channel_resv(yylval.string, "No Reason", 1);
  }
  /* ignore it for now.. but we really should make a warning if
   * its an erroneous name --fl_ */
;
    break;}
case 231:
#line 1262 "ircd_parser.y"
{
  if(clean_resv_nick(yylval.string))
  {
    if(resv_reason)
      create_nick_resv(yylval.string, resv_reason, 1);
    else
      create_nick_resv(yylval.string, "No Reason", 1);
  }

  /* otherwise its erroneous, but ignore it for now */
;
    break;}
case 232:
#line 1280 "ircd_parser.y"
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
case 233:
#line 1293 "ircd_parser.y"
{
    conf_add_u_conf(yy_aconf);
    yy_aconf = (struct ConfItem *)NULL;
  ;
    break;}
case 240:
#line 1304 "ircd_parser.y"
{
    MyFree(yy_aconf->name);
    DupString(yy_aconf->name, yylval.string);
  ;
    break;}
case 241:
#line 1310 "ircd_parser.y"
{
    MyFree(yy_aconf->user);
    DupString(yy_aconf->user, yylval.string);
  ;
    break;}
case 242:
#line 1316 "ircd_parser.y"
{
    MyFree(yy_aconf->host);
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 243:
#line 1326 "ircd_parser.y"
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

    /* defaults */
    yy_aconf->flags |= CONF_FLAGS_COMPRESSED;
    yy_aconf->port = PORTNUM;
  ;
    break;}
case 244:
#line 1357 "ircd_parser.y"
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
          sendto_realops_flags(FLAGS_ALL, L_ALL,
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
case 265:
#line 1448 "ircd_parser.y"
{
    if(yy_aconf->name != NULL)
      {
	sendto_realops_flags(FLAGS_ALL, L_ALL,"*** Multiple connect name entry");
	ilog(L_WARN, "Multiple connect name entry %s", yy_aconf->name);
      }

    MyFree(yy_aconf->name);
    DupString(yy_aconf->name, yylval.string);
  ;
    break;}
case 266:
#line 1460 "ircd_parser.y"
{
    MyFree(yy_aconf->host);
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 267:
#line 1466 "ircd_parser.y"
{
    if (yy_aconf->spasswd)
      memset(yy_aconf->spasswd, 0, strlen(yy_aconf->spasswd));
    MyFree(yy_aconf->spasswd);
    DupString(yy_aconf->spasswd, yylval.string);
  ;
    break;}
case 268:
#line 1474 "ircd_parser.y"
{
    if (yy_aconf->passwd)
      memset(yy_aconf->passwd, 0, strlen(yy_aconf->passwd));
    MyFree(yy_aconf->passwd);
    DupString(yy_aconf->passwd, yylval.string);
  ;
    break;}
case 269:
#line 1481 "ircd_parser.y"
{ yy_aconf->port = yyvsp[-1].number; ;
    break;}
case 270:
#line 1485 "ircd_parser.y"
{
    yy_aconf->aftype = AF_INET;
  ;
    break;}
case 271:
#line 1490 "ircd_parser.y"
{
#ifdef IPV6
    yy_aconf->aftype = AF_INET6;
#endif
  ;
    break;}
case 272:
#line 1497 "ircd_parser.y"
{
    MyFree(yy_aconf->fakename);
    DupString(yy_aconf->fakename, yylval.string);
 ;
    break;}
case 273:
#line 1503 "ircd_parser.y"
{
    yy_aconf->flags |= CONF_FLAGS_LAZY_LINK;
  ;
    break;}
case 274:
#line 1508 "ircd_parser.y"
{
    yy_aconf->flags &= ~CONF_FLAGS_LAZY_LINK;
  ;
    break;}
case 275:
#line 1513 "ircd_parser.y"
{
    yy_aconf->flags |= CONF_FLAGS_ENCRYPTED;
  ;
    break;}
case 276:
#line 1518 "ircd_parser.y"
{
    yy_aconf->flags &= ~CONF_FLAGS_ENCRYPTED;
  ;
    break;}
case 277:
#line 1523 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
    BIO *file;

    if (yy_aconf->rsa_public_key)
    {
      RSA_free(yy_aconf->rsa_public_key);
      yy_aconf->rsa_public_key = NULL;
    }

    if (yy_aconf->rsa_public_key_file)
    {
      MyFree(yy_aconf->rsa_public_key_file);
      yy_aconf->rsa_public_key_file = NULL;
    }

    yy_aconf->rsa_public_key_file = strdup(yylval.string);

    file = BIO_new_file(yylval.string, "r");

    if (file == NULL)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,
        "Ignoring rsa_public_key_file -- does %s exist?", yylval.string);
      break;
    }

    yy_aconf->rsa_public_key = (RSA *) PEM_read_bio_RSA_PUBKEY(file,
                                                    NULL, 0, NULL );

    if (yy_aconf->rsa_public_key == NULL)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,
        "Ignoring rsa_public_key_file -- Key invalid; check key syntax.");
      break;
    }

    BIO_set_close(file, BIO_CLOSE);
    BIO_free(file);
#endif /* HAVE_LIBCRYPTO */
  ;
    break;}
case 278:
#line 1566 "ircd_parser.y"
{
    yy_aconf->flags |= CONF_FLAGS_CRYPTLINK;
  ;
    break;}
case 279:
#line 1571 "ircd_parser.y"
{
    yy_aconf->flags &= ~CONF_FLAGS_CRYPTLINK;
  ;
    break;}
case 280:
#line 1576 "ircd_parser.y"
{
#ifndef HAVE_LIBZ
    sendto_realops_flags(FLAGS_ALL, L_ALL,
      "Ignoring compressed = yes; -- no zlib support");
#else
    yy_aconf->flags |= CONF_FLAGS_COMPRESSED;
#endif
  ;
    break;}
case 281:
#line 1586 "ircd_parser.y"
{
    yy_aconf->flags &= ~CONF_FLAGS_COMPRESSED;
  ;
    break;}
case 282:
#line 1591 "ircd_parser.y"
{
    yy_aconf->flags |= CONF_FLAGS_ALLOW_AUTO_CONN;
  ;
    break;}
case 283:
#line 1596 "ircd_parser.y"
{
    yy_aconf->flags &= ~CONF_FLAGS_ALLOW_AUTO_CONN;
  ;
    break;}
case 284:
#line 1601 "ircd_parser.y"
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
case 285:
#line 1621 "ircd_parser.y"
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
case 286:
#line 1641 "ircd_parser.y"
{
    MyFree(yy_aconf->className);
    DupString(yy_aconf->className, yylval.string);
  ;
    break;}
case 287:
#line 1647 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
    struct EncCapability *ecap;
    char *cipher_name;
    int found = 0;

    yy_aconf->cipher_preference = NULL;

    cipher_name = yylval.string;

    for (ecap = CipherTable; ecap->name; ecap++)
    {
      if ( (!irccmp(ecap->name, cipher_name)) &&
           (ecap->cap & CAP_ENC_MASK))
      {
        yy_aconf->cipher_preference = ecap;
        found = 1;
      }
    }

    if (!found)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL, "Invalid cipher '%s' for %s",
                           cipher_name, yy_aconf->name);
      ilog(L_ERROR, "Invalid cipher '%s' for %s",
                    cipher_name, yy_aconf->name);
    }
#else
      sendto_realops_flags(FLAGS_ALL, L_ALL,
        "Ignoring 'cipher_preference' line for %s -- no OpenSSL support",
         yy_aconf->name);
      ilog(L_ERROR, "Ignoring 'cipher_preference' line for %s -- "
                    "no OpenSSL support", yy_aconf->name);
#endif
  ;
    break;}
case 288:
#line 1688 "ircd_parser.y"
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
case 289:
#line 1698 "ircd_parser.y"
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
case 295:
#line 1717 "ircd_parser.y"
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
case 296:
#line 1742 "ircd_parser.y"
{
    MyFree(yy_aconf->passwd);
    DupString(yy_aconf->passwd, yylval.string);
  ;
    break;}
case 297:
#line 1752 "ircd_parser.y"
{
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_DLINE;
    /* default reason */
    DupString(yy_aconf->passwd,"NO REASON");
  ;
    break;}
case 298:
#line 1764 "ircd_parser.y"
{
    if (yy_aconf->host &&
        parse_netmask(yy_aconf->host, NULL, NULL) != HM_HOST)
    {
      add_conf_by_address(yy_aconf->host, CONF_DLINE, NULL, yy_aconf);
    }
    else
    {
      free_conf(yy_aconf);
    }
    yy_aconf = (struct ConfItem *)NULL;
  ;
    break;}
case 304:
#line 1784 "ircd_parser.y"
{
    MyFree(yy_aconf->host);
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 305:
#line 1790 "ircd_parser.y"
{
    MyFree(yy_aconf->passwd);
    DupString(yy_aconf->passwd, yylval.string);
  ;
    break;}
case 306:
#line 1800 "ircd_parser.y"
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
case 307:
#line 1811 "ircd_parser.y"
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
case 312:
#line 1829 "ircd_parser.y"
{
    MyFree(yy_aconf->host);
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 313:
#line 1840 "ircd_parser.y"
{
    if(yy_aconf)
      {
        free_conf(yy_aconf);
        yy_aconf = (struct ConfItem *)NULL;
      }
    yy_aconf=make_conf();
    yy_aconf->status = CONF_XLINE;
    /* default reason */
    DupString(yy_aconf->passwd,"Something about your name");
  ;
    break;}
case 314:
#line 1852 "ircd_parser.y"
{
    if(yy_aconf->host)
      conf_add_x_conf(yy_aconf);
    else
      free_conf(yy_aconf);
    yy_aconf = (struct ConfItem *)NULL;
  ;
    break;}
case 321:
#line 1867 "ircd_parser.y"
{
    MyFree(yy_aconf->host);
    DupString(yy_aconf->host, yylval.string);
  ;
    break;}
case 322:
#line 1873 "ircd_parser.y"
{
    MyFree(yy_aconf->passwd);
    DupString(yy_aconf->passwd, yylval.string);
  ;
    break;}
case 323:
#line 1879 "ircd_parser.y"
{
    yy_aconf->port = 0;
  ;
    break;}
case 324:
#line 1884 "ircd_parser.y"
{
    yy_aconf->port = 1;
  ;
    break;}
case 325:
#line 1889 "ircd_parser.y"
{
    yy_aconf->port = 2;
  ;
    break;}
case 377:
#line 1937 "ircd_parser.y"
{
    ConfigFileEntry.failed_oper_notice = 1;
  ;
    break;}
case 378:
#line 1942 "ircd_parser.y"
{
    ConfigFileEntry.failed_oper_notice = 0;
  ;
    break;}
case 379:
#line 1947 "ircd_parser.y"
{
    ConfigFileEntry.anti_nick_flood = 1;
  ;
    break;}
case 380:
#line 1952 "ircd_parser.y"
{
    ConfigFileEntry.anti_nick_flood = 0;
  ;
    break;}
case 381:
#line 1957 "ircd_parser.y"
{
    ConfigFileEntry.max_nick_time = yyvsp[-1].number; 
  ;
    break;}
case 382:
#line 1962 "ircd_parser.y"
{
    ConfigFileEntry.max_nick_changes = yyvsp[-1].number;
  ;
    break;}
case 383:
#line 1967 "ircd_parser.y"
{
    ConfigFileEntry.max_accept = yyvsp[-1].number;
  ;
    break;}
case 384:
#line 1972 "ircd_parser.y"
{
    ConfigFileEntry.anti_spam_exit_message_time = yyvsp[-1].number;
  ;
    break;}
case 385:
#line 1977 "ircd_parser.y"
{
    ConfigFileEntry.ts_warn_delta = yyvsp[-1].number;
  ;
    break;}
case 386:
#line 1982 "ircd_parser.y"
{
    ConfigFileEntry.ts_max_delta = yyvsp[-1].number;
  ;
    break;}
case 387:
#line 1987 "ircd_parser.y"
{
    ConfigFileEntry.links_delay = yyvsp[-1].number;
  ;
    break;}
case 388:
#line 1992 "ircd_parser.y"
{
  if(yyvsp[-1].number > 0)
  {
    ilog(L_CRIT, "You haven't read your config file properly.  There is a line to check youve been paying attention.");
    exit(0);
  }
;
    break;}
case 389:
#line 2000 "ircd_parser.y"
{
    ConfigFileEntry.kline_with_reason = 1;
  ;
    break;}
case 390:
#line 2005 "ircd_parser.y"
{
    ConfigFileEntry.kline_with_reason = 0;
  ;
    break;}
case 391:
#line 2010 "ircd_parser.y"
{
    ConfigFileEntry.client_exit = 1;
  ;
    break;}
case 392:
#line 2015 "ircd_parser.y"
{
    ConfigFileEntry.client_exit = 0;
  ;
    break;}
case 393:
#line 2020 "ircd_parser.y"
{
    ConfigFileEntry.kline_with_connection_closed = 1;
  ;
    break;}
case 394:
#line 2025 "ircd_parser.y"
{
    ConfigFileEntry.kline_with_connection_closed = 0;
  ;
    break;}
case 395:
#line 2030 "ircd_parser.y"
{
    ConfigFileEntry.warn_no_nline = 1;
  ;
    break;}
case 396:
#line 2035 "ircd_parser.y"
{
    ConfigFileEntry.warn_no_nline = 0;
  ;
    break;}
case 397:
#line 2040 "ircd_parser.y"
{
    ConfigFileEntry.non_redundant_klines = 1;
  ;
    break;}
case 398:
#line 2045 "ircd_parser.y"
{
    ConfigFileEntry.non_redundant_klines = 0;
  ;
    break;}
case 399:
#line 2050 "ircd_parser.y"
{
    ConfigFileEntry.stats_o_oper_only = 1;
  ;
    break;}
case 400:
#line 2055 "ircd_parser.y"
{
    ConfigFileEntry.stats_o_oper_only = 0;
  ;
    break;}
case 401:
#line 2060 "ircd_parser.y"
{
    ConfigFileEntry.stats_k_oper_only = 2;
  ;
    break;}
case 402:
#line 2065 "ircd_parser.y"
{
    ConfigFileEntry.stats_k_oper_only = 1;
  ;
    break;}
case 403:
#line 2070 "ircd_parser.y"
{
    ConfigFileEntry.stats_k_oper_only = 0;
  ;
    break;}
case 404:
#line 2075 "ircd_parser.y"
{
    ConfigFileEntry.stats_i_oper_only = 2;
  ;
    break;}
case 405:
#line 2080 "ircd_parser.y"
{
    ConfigFileEntry.stats_i_oper_only = 1;
  ;
    break;}
case 406:
#line 2085 "ircd_parser.y"
{
    ConfigFileEntry.stats_i_oper_only = 0;
  ;
    break;}
case 407:
#line 2090 "ircd_parser.y"
{
    ConfigFileEntry.pace_wait = yyvsp[-1].number;
  ;
    break;}
case 408:
#line 2095 "ircd_parser.y"
{
    ConfigFileEntry.caller_id_wait = yyvsp[-1].number;
  ;
    break;}
case 409:
#line 2100 "ircd_parser.y"
{
    ConfigFileEntry.whois_wait = yyvsp[-1].number;
  ;
    break;}
case 410:
#line 2105 "ircd_parser.y"
{
    ConfigFileEntry.short_motd = 1;
  ;
    break;}
case 411:
#line 2110 "ircd_parser.y"
{
    ConfigFileEntry.short_motd = 0;
  ;
    break;}
case 412:
#line 2115 "ircd_parser.y"
{
    ConfigFileEntry.no_oper_flood = 1;
  ;
    break;}
case 413:
#line 2120 "ircd_parser.y"
{
    ConfigFileEntry.no_oper_flood = 0;
  ;
    break;}
case 414:
#line 2125 "ircd_parser.y"
{
#if 0
    strncpy(iAuth.hostname, yylval.string, HOSTLEN)[HOSTLEN] = 0;
#endif
;
    break;}
case 415:
#line 2132 "ircd_parser.y"
{
#if 0
    iAuth.port = yyvsp[-1].number;
#endif
;
    break;}
case 416:
#line 2139 "ircd_parser.y"
{
  strncpy_irc(ConfigFileEntry.fname_userlog, yylval.string,
	      MAXPATHLEN-1)[MAXPATHLEN-1] = 0;
;
    break;}
case 417:
#line 2145 "ircd_parser.y"
{
  strncpy_irc(ConfigFileEntry.fname_foperlog, yylval.string,
	      MAXPATHLEN-1)[MAXPATHLEN-1] = 0;
;
    break;}
case 418:
#line 2151 "ircd_parser.y"
{
  strncpy_irc(ConfigFileEntry.fname_operlog, yylval.string,
	      MAXPATHLEN-1)[MAXPATHLEN-1] = 0;
;
    break;}
case 419:
#line 2157 "ircd_parser.y"
{
    ConfigFileEntry.glines = 1;
  ;
    break;}
case 420:
#line 2162 "ircd_parser.y"
{
    ConfigFileEntry.glines = 0;
  ;
    break;}
case 421:
#line 2167 "ircd_parser.y"
{
    char langenv[BUFSIZE];
    if (strlen(yylval.string) > BUFSIZE-10)
      yylval.string[BUFSIZE-9] = 0;
    ircsprintf(langenv, "LANGUAGE=%s", yylval.string);
    putenv(langenv);
  ;
    break;}
case 422:
#line 2176 "ircd_parser.y"
{
    ConfigFileEntry.gline_time = yyvsp[-1].number;
  ;
    break;}
case 423:
#line 2181 "ircd_parser.y"
{
    ConfigFileEntry.idletime = yyvsp[-1].number;
  ;
    break;}
case 424:
#line 2186 "ircd_parser.y"
{
    ConfigFileEntry.dots_in_ident = yyvsp[-1].number;
  ;
    break;}
case 425:
#line 2191 "ircd_parser.y"
{
    ConfigFileEntry.maximum_links = yyvsp[-1].number;
  ;
    break;}
case 426:
#line 2196 "ircd_parser.y"
{
    ConfigFileEntry.hide_server = 1;
  ;
    break;}
case 427:
#line 2201 "ircd_parser.y"
{
    ConfigFileEntry.hide_server = 0;
  ;
    break;}
case 428:
#line 2206 "ircd_parser.y"
{
    ConfigFileEntry.max_targets = yyvsp[-1].number;
  ;
    break;}
case 429:
#line 2211 "ircd_parser.y"
{
    MyFree(ConfigFileEntry.servlink_path);
    DupString(ConfigFileEntry.servlink_path, yylval.string);
  ;
    break;}
case 430:
#line 2217 "ircd_parser.y"
{
#ifdef HAVE_LIBCRYPTO
    struct EncCapability *ecap;
    char *cipher_name;
    int found = 0;

    ConfigFileEntry.default_cipher_preference = NULL;

    cipher_name = yylval.string;

    for (ecap = CipherTable; ecap->name; ecap++)
    {
      if ( (!irccmp(ecap->name, cipher_name)) &&
           (ecap->cap & CAP_ENC_MASK))
      {
        ConfigFileEntry.default_cipher_preference = ecap;
        found = 1;
        break;
      }
    }

    if (!found)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL, "Invalid cipher '%s'", cipher_name);
      ilog(L_ERROR, "Invalid cipher '%s'", cipher_name);
    }
#else
    sendto_realops_flags(FLAGS_ALL, L_ALL, "Ignoring 'default_cipher_preference' "
                                    "-- no OpenSSL support");
    ilog(L_ERROR, "Ignoring 'default_cipher_preference' "
                  "-- no OpenSSL support");
#endif
  ;
    break;}
case 431:
#line 2252 "ircd_parser.y"
{
    ConfigFileEntry.compression_level = yyvsp[-1].number;
#ifndef HAVE_LIBZ
    sendto_realops_flags(FLAGS_ALL, L_ALL,
      "Ignoring compression_level = %d; -- no zlib support",
       ConfigFileEntry.compression_level);
#else
    if ((ConfigFileEntry.compression_level < 1) ||
        (ConfigFileEntry.compression_level > 9))
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,
        "Ignoring invalid compression level '%d', using default",
        ConfigFileEntry.compression_level);
      ConfigFileEntry.compression_level = 0;
    }
#endif
  ;
    break;}
case 432:
#line 2271 "ircd_parser.y"
{
    ConfigFileEntry.use_egd = 1;
  ;
    break;}
case 433:
#line 2276 "ircd_parser.y"
{
    ConfigFileEntry.use_egd = 0;
  ;
    break;}
case 434:
#line 2281 "ircd_parser.y"
{
    MyFree(ConfigFileEntry.egdpool_path);
    DupString(ConfigFileEntry.egdpool_path, yylval.string);
  ;
    break;}
case 435:
#line 2287 "ircd_parser.y"
{
 ConfigFileEntry.throttle_time = yylval.number;
;
    break;}
case 436:
#line 2292 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes = 0;
  ;
    break;}
case 440:
#line 2301 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_BOTS;
  ;
    break;}
case 441:
#line 2305 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_CCONN;
  ;
    break;}
case 442:
#line 2309 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_DEBUG;
  ;
    break;}
case 443:
#line 2313 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_FULL;
  ;
    break;}
case 444:
#line 2317 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_SKILL;
  ;
    break;}
case 445:
#line 2321 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_NCHANGE;
  ;
    break;}
case 446:
#line 2325 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_REJ;
  ;
    break;}
case 447:
#line 2329 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_UNAUTH;
  ;
    break;}
case 448:
#line 2333 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_SPY;
  ;
    break;}
case 449:
#line 2337 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_EXTERNAL;
  ;
    break;}
case 450:
#line 2341 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_OPERWALL;
  ;
    break;}
case 451:
#line 2345 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_SERVNOTICE;
  ;
    break;}
case 452:
#line 2349 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_INVISIBLE;
  ;
    break;}
case 453:
#line 2353 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_WALLOP;
  ;
    break;}
case 454:
#line 2357 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_CALLERID;
  ;
    break;}
case 455:
#line 2361 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_LOCOPS;
  ;
    break;}
case 456:
#line 2365 "ircd_parser.y"
{
    ConfigFileEntry.oper_umodes |= FLAGS_DRONE;
  ;
    break;}
case 457:
#line 2370 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes = 0;
  ;
    break;}
case 461:
#line 2379 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_BOTS;
  ;
    break;}
case 462:
#line 2383 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_CCONN;
  ;
    break;}
case 463:
#line 2387 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_DEBUG;
  ;
    break;}
case 464:
#line 2391 "ircd_parser.y"
{ 
    ConfigFileEntry.oper_only_umodes |= FLAGS_FULL;
  ;
    break;}
case 465:
#line 2395 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_SKILL;
  ;
    break;}
case 466:
#line 2399 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_NCHANGE;
  ;
    break;}
case 467:
#line 2403 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_REJ;
  ;
    break;}
case 468:
#line 2407 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_UNAUTH;
  ;
    break;}
case 469:
#line 2411 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_SPY;
  ;
    break;}
case 470:
#line 2415 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_EXTERNAL;
  ;
    break;}
case 471:
#line 2419 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_OPERWALL;
  ;
    break;}
case 472:
#line 2423 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_SERVNOTICE;
  ;
    break;}
case 473:
#line 2427 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_INVISIBLE;
  ;
    break;}
case 474:
#line 2431 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_WALLOP;
  ;
    break;}
case 475:
#line 2435 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_CALLERID;
  ;
    break;}
case 476:
#line 2439 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_LOCOPS;
  ;
    break;}
case 477:
#line 2443 "ircd_parser.y"
{
    ConfigFileEntry.oper_only_umodes |= FLAGS_DRONE;
  ;
    break;}
case 478:
#line 2448 "ircd_parser.y"
{
    ConfigFileEntry.min_nonwildcard = yyvsp[-1].number;
  ;
    break;}
case 479:
#line 2452 "ircd_parser.y"
{
    ConfigFileEntry.default_floodcount = yyvsp[-1].number;
  ;
    break;}
case 480:
#line 2457 "ircd_parser.y"
{
    ConfigFileEntry.client_flood = yyvsp[-1].number;
  ;
    break;}
case 495:
#line 2484 "ircd_parser.y"
{
    ConfigChannel.use_invex = 1;
  ;
    break;}
case 496:
#line 2489 "ircd_parser.y"
{
    ConfigChannel.use_invex = 0;
  ;
    break;}
case 497:
#line 2494 "ircd_parser.y"
{
    ConfigChannel.use_except = 1;
  ;
    break;}
case 498:
#line 2499 "ircd_parser.y"
{
    ConfigChannel.use_except = 0;
  ;
    break;}
case 499:
#line 2504 "ircd_parser.y"
{
    ConfigChannel.use_knock = 1;
  ;
    break;}
case 500:
#line 2509 "ircd_parser.y"
{
    ConfigChannel.use_knock = 0;
  ;
    break;}
case 501:
#line 2514 "ircd_parser.y"
{
    ConfigChannel.vchans_oper_only = 1;
  ;
    break;}
case 502:
#line 2519 "ircd_parser.y"
{
    ConfigChannel.vchans_oper_only = 0;
  ;
    break;}
case 503:
#line 2524 "ircd_parser.y"
{
    ConfigChannel.use_vchans = 1;
  ;
    break;}
case 504:
#line 2529 "ircd_parser.y"
{
    ConfigChannel.use_vchans = 0;
  ;
    break;}
case 505:
#line 2534 "ircd_parser.y"
{
     ConfigChannel.knock_delay = yyvsp[-1].number;
   ;
    break;}
case 506:
#line 2539 "ircd_parser.y"
{
     ConfigChannel.max_chans_per_user = yyvsp[-1].number;
   ;
    break;}
case 507:
#line 2544 "ircd_parser.y"
{
     ConfigChannel.quiet_on_ban = 1;
   ;
    break;}
case 508:
#line 2549 "ircd_parser.y"
{
     ConfigChannel.quiet_on_ban = 0;
   ;
    break;}
case 509:
#line 2554 "ircd_parser.y"
{
      ConfigChannel.maxbans = yyvsp[-1].number;
   ;
    break;}
case 510:
#line 2559 "ircd_parser.y"
{
    ConfigChannel.persist_time = yyvsp[-1].number;
  ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "bison.simple"

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
#line 2563 "ircd_parser.y"
