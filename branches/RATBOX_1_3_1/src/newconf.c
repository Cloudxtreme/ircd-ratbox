/* This code is in the public domain.
 * $Id$
 */

#include "stdinc.h"

#ifdef HAVE_LIBCRYPTO
#include <openssl/pem.h>
#include <openssl/rsa.h>
#endif

#include "memory.h"
#include "newconf.h"
#include "tools.h"
#include "ircd_defs.h"
#include "sprintf_irc.h"
#include "common.h"
#include "s_log.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_newconf.h"
#include "send.h"
#include "setup.h"
#include "modules.h"
#include "listener.h"
#include "hostmask.h"
#include "s_serv.h"
#include "event.h"
#include "hash.h"
#include "cluster.h"
#include "cache.h"
#include "ircd.h"

#define CF_TYPE(x) ((x) & CF_MTYPE)

struct TopConf *conf_cur_block;
static char *conf_cur_block_name;

static dlink_list conf_items;

/* XXX This _really_ needs to go away */
static struct ConfItem *yy_achead = NULL;
static struct ConfItem *yy_aconf = NULL;
static struct ConfItem *yy_aprev = NULL;
static int yy_acount = 0;
static struct ConfItem *yy_hconf;
static struct ConfItem *yy_lconf;

static struct ConfItem *hub_confs;
static struct ConfItem *leaf_confs;
static struct ConfItem *yy_aconf_next;

static struct Class *yy_class = NULL;

static struct rxconf *yy_rxconf = NULL;
static struct shared *yy_uconf = NULL;
static struct cluster *yy_cconf = NULL;

static char *resv_reason;

static const char *
conf_strtype(int type)
{
	switch (type & CF_MTYPE)
	{
	case CF_INT:
		return "integer value";
	case CF_STRING:
		return "unquoted string";
	case CF_YESNO:
		return "yes/no value";
	case CF_QSTRING:
		return "quoted string";
	case CF_TIME:
		return "time/size value";
	default:
		return "unknown type";
	}
}



static int
add_top_conf(const char *name, int (*sfunc) (struct TopConf *), int (*efunc) (struct TopConf *))
{
	struct TopConf *tc;

	tc = MyMalloc(sizeof(struct TopConf));

	DupString(tc->tc_name, name);
	tc->tc_sfunc = sfunc;
	tc->tc_efunc = efunc;

	dlinkAddAlloc(tc, &conf_items);
	return 0;
}

static struct TopConf *
find_top_conf(const char *name)
{
	dlink_node *d;
	struct TopConf *tc;

	DLINK_FOREACH(d, conf_items.head)
	{
		tc = d->data;
		if(strcasecmp(tc->tc_name, name) == 0)
			return tc;
	}

	return NULL;
}


static struct ConfEntry *
find_conf_item(const struct TopConf *top, const char *name)
{
	dlink_node *d;
	struct ConfEntry *cf;

	DLINK_FOREACH(d, top->tc_items.head)
	{
		cf = d->data;
		if(strcasecmp(cf->cf_name, name) == 0)
			return cf;
	}

	return NULL;
}

#if 0				/* XXX unused */
static int
remove_top_conf(char *name)
{
	struct TopConf *tc;
	dlink_node *ptr;

	if((tc = find_top_conf(name)) == NULL)
		return -1;

	if((ptr = dlinkFind(&conf_items, tc)) == NULL)
		return -1;

	dlinkDestroy(ptr, &conf_items);
	MyFree(tc->tc_name);
	MyFree(tc);

	return 0;
}
#endif

static void
conf_set_serverinfo_name(void *data)
{
	if(ServerInfo.name == NULL)
	{
		const char *s;
		int dots = 0;

		for(s = data; *s != '\0'; s++)
		{
			if(!IsServChar(*s))
			{
				conf_report_error("Ignoring serverinfo::name "
						  "-- bogus servername.");
				return;
			}
			else if(*s == '.')
				++dots;
		}

		if(!dots)
		{
			conf_report_error("Ignoring serverinfo::name -- must contain '.'");
			return;
		}

		/* the ircd will exit() in main() if we dont set one */
		if(strlen((char *) data) <= HOSTLEN)
			DupString(ServerInfo.name, (char *) data);
	}
}

static void
conf_set_serverinfo_sid(void *data)
{
	char *sid = data;

	if(ServerInfo.sid[0] == '\0')
	{
		if(!IsDigit(sid[0]) || !IsIdChar(sid[1]) ||
		   !IsIdChar(sid[2]) || sid[3] != '\0')
		{
			conf_report_error("Ignoring serverinfo::sid "
					  "-- bogus sid.");
			return;
		}

		strcpy(ServerInfo.sid, sid);
	}
}

static void
conf_set_serverinfo_description(void *data)
{
	MyFree(ServerInfo.description);
	DupString(ServerInfo.description, (char *) data);
}

static void
conf_set_serverinfo_network_name(void *data)
{
	char *p;

	if((p = strchr((char *) data, ' ')))
		*p = '\0';

	MyFree(ServerInfo.network_name);
	DupString(ServerInfo.network_name, (char *) data);
}

static void
conf_set_serverinfo_network_desc(void *data)
{
	MyFree(ServerInfo.network_desc);
	DupString(ServerInfo.network_desc, (char *) data);
}

static void
conf_set_serverinfo_vhost(void *data)
{
	if(inetpton(AF_INET, (char *) data, &ServerInfo.ip.sin_addr) <= 0)
	{
		conf_report_error("Invalid netmask for server IPv4 vhost (%s)", (char *) data);
		return;
	}

	ServerInfo.specific_ipv4_vhost = 1;
}

static void
conf_set_serverinfo_vhost6(void *data)
{
#ifdef IPV6
	if(inetpton(AF_INET6, (char *) data, &ServerInfo.ip6.sin6_addr) <= 0)
	{
		conf_report_error("Invalid netmask for server IPv6 vhost (%s)", (char *) data);
		return;
	}

	ServerInfo.specific_ipv6_vhost = 1;
#else
	conf_report_error("Warning -- ignoring serverinfo::vhost6 -- IPv6 support not available.");
#endif
}

static void
conf_set_serverinfo_hub(void *data)
{
	int hub = *(int *) data;

	ServerInfo.hub = hub;
}

static void
conf_set_serverinfo_use_ts6(void *data)
{
	int use_ts6 = *(int *) data;

	/* note, it doesnt matter if this is disabled when we have TS6
	 * links, because its only checked when a new server links --fl
	 */
	ServerInfo.use_ts6 = use_ts6;
}

static void
conf_set_modules_module(void *data)
{
#ifndef STATIC_MODULES
	char *m_bn;

	m_bn = irc_basename((char *) data);

	if(findmodule_byname(m_bn) != -1)
		return;

	load_one_module((char *) data, 0);

	MyFree(m_bn);
#else
	conf_report_error("Ignoring modules::module -- loadable module support not present.");
#endif
}

static void
conf_set_modules_path(void *data)
{
#ifndef STATIC_MODULES
	mod_add_path((char *) data);
#else
	conf_report_error("Ignoring modules::path -- loadable module support net present.");
#endif
}

static void
conf_set_admin_name(void *data)
{
	MyFree(AdminInfo.name);
	DupString(AdminInfo.name, (char *) data);
}

static void
conf_set_admin_email(void *data)
{
	MyFree(AdminInfo.email);
	DupString(AdminInfo.email, (char *) data);
}

static void
conf_set_admin_description(void *data)
{
	MyFree(AdminInfo.description);
	DupString(AdminInfo.description, (char *) data);
}

static void
conf_set_log_fname_userlog(void *data)
{
	strlcpy(ConfigFileEntry.fname_userlog, data, sizeof(ConfigFileEntry.fname_userlog));
}

static void
conf_set_log_fname_fuserlog(void *data)
{
	strlcpy(ConfigFileEntry.fname_fuserlog, data, sizeof(ConfigFileEntry.fname_fuserlog));
}

static void
conf_set_log_fname_foperlog(void *data)
{
	strlcpy(ConfigFileEntry.fname_foperlog, data, sizeof(ConfigFileEntry.fname_foperlog));
}

static void
conf_set_log_fname_operlog(void *data)
{
	strlcpy(ConfigFileEntry.fname_operlog, data, sizeof(ConfigFileEntry.fname_operlog));
}

static void
conf_set_log_fname_serverlog(void *data)
{
	strlcpy(ConfigFileEntry.fname_serverlog, data, sizeof(ConfigFileEntry.fname_serverlog));
}

static void
conf_set_log_fname_glinelog(void *data)
{
	strlcpy(ConfigFileEntry.fname_glinelog, data, sizeof(ConfigFileEntry.fname_glinelog));
}

static void
conf_set_log_fname_klinelog(void *data)
{
	strlcpy(ConfigFileEntry.fname_klinelog, data, sizeof(ConfigFileEntry.fname_klinelog));
}

static void
conf_set_log_fname_operspylog(void *data)
{
	strlcpy(ConfigFileEntry.fname_operspylog, data,
		sizeof(ConfigFileEntry.fname_operspylog));
}

static void
conf_set_log_fname_ioerrorlog(void *data)
{
	strlcpy(ConfigFileEntry.fname_ioerrorlog, data,
		sizeof(ConfigFileEntry.fname_ioerrorlog));
}

struct mode_table
{
	const char *name;
	int mode;
};

/* *INDENT-OFF* */
static struct mode_table umode_table[] = {
	{"bots",	UMODE_BOTS	},
	{"cconn",	UMODE_CCONN	},
	{"debug",	UMODE_DEBUG	},
	{"full",	UMODE_FULL	},
	{"callerid",	UMODE_CALLERID	},
	{"invisible",	UMODE_INVISIBLE	},
	{"skill",	UMODE_SKILL	},
	{"locops",	UMODE_LOCOPS	},
	{"nchange",	UMODE_NCHANGE	},
	{"rej",		UMODE_REJ	},
	{"servnotice",	UMODE_SERVNOTICE},
	{"unauth",	UMODE_UNAUTH	},
	{"wallop",	UMODE_WALLOP	},
	{"external",	UMODE_EXTERNAL	},
	{"spy",		UMODE_SPY	},
	{"operwall",	UMODE_OPERWALL	},
	{"operspy",	UMODE_OPERSPY	},
	{NULL, 0}
};

static struct mode_table flag_table[] = {
	{"local_kill",		OPER_LOCAL_KILL		},
	{"global_kill",		OPER_GLOBAL_KILL|OPER_LOCAL_KILL	},
	{"remote",		OPER_REMOTE		},
	{"kline",		OPER_K			},
	{"unkline",		OPER_UNKLINE		},
	{"gline",		OPER_GLINE		},
	{"nick_changes",	OPER_N			},
	{"rehash",		OPER_REHASH		},
	{"die",			OPER_DIE		},
	{"admin",		OPER_ADMIN		},
	{"hidden_admin",	OPER_HIDDENADMIN	},
	{"xline",		OPER_XLINE		},
	{"operwall",		OPER_OPERWALL		},
	{"oper_spy",		OPER_SPY		},
	{"oper_invisible",	OPER_INVIS		},
	{NULL, 0}
};

static struct mode_table auth_table[] = {
	{"spoof_notice",	CONF_FLAGS_SPOOF_NOTICE	},
	{"exceed_limit",	CONF_FLAGS_NOLIMIT	},
	{"kline_exempt",	CONF_FLAGS_EXEMPTKLINE	},
	{"gline_exempt",	CONF_FLAGS_EXEMPTGLINE	},
	{"flood_exempt",	CONF_FLAGS_EXEMPTFLOOD	},
	{"spambot_exempt",	CONF_FLAGS_EXEMPTSPAMBOT },
	{"shide_exempt",	CONF_FLAGS_EXEMPTSHIDE	},
	{"no_tilde",		CONF_FLAGS_NO_TILDE	},
	{"restricted",		CONF_FLAGS_RESTRICTED	},
	{"need_ident",		CONF_FLAGS_NEED_IDENTD	},
	{"have_ident",		CONF_FLAGS_NEED_IDENTD	},
	{NULL, 0}
};

static struct mode_table cluster_table[] = {
	{ "kline",	CLUSTER_KLINE	},
	{ "unkline",	CLUSTER_UNKLINE	},
	{ "locops",	CLUSTER_LOCOPS	},
	{ "xline",	CLUSTER_XLINE	},
	{ "unxline",	CLUSTER_UNXLINE	},
	{ "resv",	CLUSTER_RESV	},
	{ "unresv",	CLUSTER_UNRESV	},
	{ "all",	CLUSTER_ALL	},
	{NULL, 0}
};

static struct mode_table shared_table[] =
{
	{ "kline",	OPER_K		},
	{ "unkline",	OPER_UNKLINE	},
	{ "xline",	OPER_XLINE	},
	{ "resv",	OPER_RESV	},
	{ "all",	OPER_K|OPER_UNKLINE|OPER_XLINE },
	{NULL, 0}
};
/* *INDENT-ON* */

static int
find_umode(struct mode_table *tab, char *name)
{
	int i;

	for (i = 0; tab[i].name; i++)
	{
		if(strcmp(tab[i].name, name) == 0)
			return tab[i].mode;
	}

	return 0;
}

static void
set_modes_from_table(int *modes, const char *whatis, struct mode_table *tab, conf_parm_t * args)
{
	for (; args; args = args->next)
	{
		int mode;

		if((args->type & CF_MTYPE) != CF_STRING)
		{
			conf_report_error("Warning -- %s is not a string; ignoring.", whatis);
			continue;
		}

		mode = find_umode(tab, args->v.string);

		if(!mode)
		{
			conf_report_error("Warning -- unknown %s %s.", whatis, args->v.string);
			continue;
		}

		*modes |= mode;
	}
}

static int
conf_begin_oper(struct TopConf *tc)
{
	struct ConfItem *yy_tmp;

	yy_tmp = yy_achead;
	while (yy_tmp)
	{
		yy_aconf = yy_tmp;
		yy_tmp = yy_tmp->next;
		yy_aconf->next = NULL;
		free_conf(yy_aconf);
	}
	yy_acount = 0;
	yy_achead = yy_aconf = make_conf();
	yy_aconf->status = CONF_OPERATOR;
	yy_aconf->flags |= CONF_FLAGS_ENCRYPTED;
	return 0;
}

static int
conf_end_oper(struct TopConf *tc)
{
	struct ConfItem *yy_tmp;
	struct ConfItem *yy_next;

	if(conf_cur_block_name != NULL)
	{
		MyFree(yy_achead->name);
		DupString(yy_achead->name, conf_cur_block_name);
	}
	/* copy over settings from first struct */
	for (yy_tmp = yy_achead->next; yy_tmp; yy_tmp = yy_tmp->next)
	{
		if(yy_achead->className)
			DupString(yy_tmp->className, yy_achead->className);
		if(yy_achead->name)
			DupString(yy_tmp->name, yy_achead->name);
		if(yy_achead->passwd)
			DupString(yy_tmp->passwd, yy_achead->passwd);
		yy_tmp->port = yy_achead->port;
		yy_tmp->flags = yy_achead->flags;

#ifdef HAVE_LIBCRYPTO
		if(yy_achead->rsa_public_key_file)
			DupString(yy_tmp->rsa_public_key_file, yy_achead->rsa_public_key_file);

		if(yy_achead->rsa_public_key)
		{
			BIO *file;

			file = BIO_new_file(yy_achead->rsa_public_key_file, "r");
			yy_tmp->rsa_public_key =
				(RSA *) PEM_read_bio_RSA_PUBKEY(file, NULL, 0, NULL);
			BIO_set_close(file, BIO_CLOSE);
			BIO_free(file);
		}
#endif
	}

	for (yy_tmp = yy_achead; yy_tmp; yy_tmp = yy_next)
	{
		yy_next = yy_tmp->next;

#ifdef HAVE_LIBCRYPTO
		if(yy_tmp->name && (yy_tmp->passwd || yy_aconf->rsa_public_key) && yy_tmp->host)
#else
		if(yy_tmp->name && yy_tmp->passwd && yy_tmp->host)
#endif
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

	return 0;
}

static void
conf_set_oper_flags(void *data)
{
	conf_parm_t *args = data;

	set_modes_from_table(&yy_achead->port, "flag", flag_table, args);
}

static void
conf_set_oper_name(void *data)
{
	int oname_len;

	MyFree(yy_achead->name);

	if((oname_len = strlen((char *) data)) > OPERNICKLEN)
		((char *) data)[OPERNICKLEN] = 0;

	DupString(yy_achead->name, (char *) data);
}

static void
conf_set_oper_user(void *data)
{
	char *p;
	char *new_user;
	char *new_host;
	char *host = (char *) data;

	/* The first user= line doesn't allocate a new conf */
	if(yy_acount++)
	{
		yy_aconf = (yy_aconf->next = make_conf());
		yy_aconf->status = CONF_OPERATOR;
	}

	if((p = strchr(host, '@')))
	{
		*p = '\0';
		DupString(new_user, host);
		MyFree(yy_aconf->user);
		yy_aconf->user = new_user;
		p++;
		DupString(new_host, p);
		MyFree(yy_aconf->host);
		yy_aconf->host = new_host;
	}
	else
	{
		MyFree(yy_aconf->host);
		DupString(yy_aconf->host, host);
		DupString(yy_aconf->user, "*");
	}
}

static void
conf_set_oper_password(void *data)
{
	if(yy_achead->passwd)
		memset(yy_achead->passwd, 0, strlen(yy_achead->passwd));

	MyFree(yy_achead->passwd);
	DupString(yy_achead->passwd, (char *) data);
}

static void
conf_set_oper_rsa_public_key_file(void *data)
{
#ifdef HAVE_LIBCRYPTO
	BIO *file;

	if(yy_achead->rsa_public_key)
	{
		RSA_free(yy_achead->rsa_public_key);
		yy_achead->rsa_public_key = NULL;
	}

	if(yy_achead->rsa_public_key_file)
	{
		MyFree(yy_achead->rsa_public_key_file);
		yy_achead->rsa_public_key_file = NULL;
	}

	DupString(yy_achead->rsa_public_key_file, (char *) data);

	file = BIO_new_file((char *) data, "r");

	if(file == NULL)
	{
		conf_report_error("Ignoring rsa_public_key_file -- does %s exist?", (char *) data);
		return;
	}

	yy_achead->rsa_public_key = (RSA *) PEM_read_bio_RSA_PUBKEY(file, NULL, 0, NULL);

	if(yy_achead->rsa_public_key == NULL)
	{
		conf_report_error("Ignoring rsa_public_key_file -- Key invalid; check key syntax.");
		return;
	}

	BIO_set_close(file, BIO_CLOSE);
	BIO_free(file);
#else
	conf_report_error("Warning -- ignoring rsa_public_key_file (OpenSSL support not available");
#endif
}

static void
conf_set_oper_encrypted(void *data)
{
	int yesno = *(unsigned int *) data;

	if(yesno)
		yy_achead->flags |= CONF_FLAGS_ENCRYPTED;
	else
		yy_achead->flags &= ~CONF_FLAGS_ENCRYPTED;
}

static void
conf_set_oper_class(void *data)
{
	MyFree(yy_achead->className);
	DupString(yy_achead->className, (char *) data);
}

static int
conf_begin_class(struct TopConf *tc)
{
	yy_class = make_class();
	return 0;
}

static int
conf_end_class(struct TopConf *tc)
{
	if(conf_cur_block_name != NULL)
	{
		MyFree(yy_class->class_name);
		DupString(yy_class->class_name, conf_cur_block_name);
	}

	if(yy_class->class_name)
		add_class(yy_class);
	else
		free_class(yy_class);
	return 0;
}

static void
conf_set_class_name(void *data)
{
	DupString(yy_class->class_name, (char *) data);
}

static void
conf_set_class_ping_time(void *data)
{
	yy_class->ping_freq = *(unsigned int *) data;
}

static void
conf_set_class_cidr_bitlen(void *data)
{
#ifdef IPV6
	unsigned int maxsize = 128;
#else
	unsigned int maxsize = 32;
#endif
	if(*(unsigned int *) data > maxsize)
		conf_report_error
			("class::cidr_bitlen argument exceeds maxsize (%d > %d) - ignoring.",
			 *(unsigned int *) data, maxsize);
	else
		yy_class->cidr_bitlen = *(unsigned int *) data;

}
static void
conf_set_class_number_per_cidr(void *data)
{
	yy_class->cidr_amount = *(unsigned int *) data;
}

static void
conf_set_class_number_per_ip(void *data)
{
	yy_class->max_local = *(unsigned int *) data;
}


static void
conf_set_class_number_per_ip_global(void *data)
{
	yy_class->max_global = *(unsigned int *) data;
}

static void
conf_set_class_number_per_ident(void *data)
{
	yy_class->max_ident = *(unsigned int *) data;
}

static void
conf_set_class_connectfreq(void *data)
{
	yy_class->con_freq = *(unsigned int *) data;
}

static void
conf_set_class_max_number(void *data)
{
	yy_class->max_total = *(unsigned int *) data;
}

static void
conf_set_class_sendq(void *data)
{
	yy_class->max_sendq = *(unsigned int *) data;
}

static void
conf_set_class_sendq_eob(void *data)
{
	yy_class->max_sendq_eob = *(unsigned int *) data;
}

static char *listener_address;

static int
conf_begin_listen(struct TopConf *tc)
{
	listener_address = NULL;
	return 0;
}

static int
conf_end_listen(struct TopConf *tc)
{
	MyFree(listener_address);
	listener_address = NULL;
	return 0;
}

static void
conf_set_listen_port(void *data)
{
	conf_parm_t *args = data;
	for (; args; args = args->next)
	{
		if((args->type & CF_MTYPE) != CF_INT)
		{
			conf_report_error
				("listener::port argument is not an integer " "-- ignoring.");
			continue;
		}
                if(listener_address == NULL)
                {
			add_listener(args->v.number, listener_address, AF_INET);
#ifdef IPV6
			add_listener(args->v.number, listener_address, AF_INET6);
#endif
                } else
                {
			int family;
#ifdef IPV6
			if(strchr(listener_address, ':') != NULL)
				family = AF_INET6;
			else 
#endif
				family = AF_INET;
		
			add_listener(args->v.number, listener_address, family);
                
                }

	}
}

static void
conf_set_listen_address(void *data)
{
	MyFree(listener_address);
	DupString(listener_address, data);
}

static int
conf_begin_auth(struct TopConf *tc)
{
	struct ConfItem *yy_tmp;

	yy_tmp = yy_achead;
	while (yy_tmp)
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

	return 0;
}

static int
conf_end_auth(struct TopConf *tc)
{
	struct ConfItem *yy_tmp;
	struct ConfItem *yy_next;

	/* copy over settings from first struct */
	for (yy_tmp = yy_achead->next; yy_tmp; yy_tmp = yy_tmp->next)
	{
		if(yy_achead->passwd)
			DupString(yy_tmp->passwd, yy_achead->passwd);
		if(yy_achead->name)
			DupString(yy_tmp->name, yy_achead->name);
		if(yy_achead->className)
			DupString(yy_tmp->className, yy_achead->className);

		yy_tmp->flags = yy_achead->flags;
		yy_tmp->port = yy_achead->port;
	}

	for (yy_tmp = yy_achead; yy_tmp; yy_tmp = yy_next)
	{
		yy_next = yy_tmp->next;	/* yy_tmp->next is used by conf_add_conf */
		yy_tmp->next = NULL;

		if(yy_tmp->name == NULL)
			DupString(yy_tmp->name, "NOMATCH");

		conf_add_class_to_conf(yy_tmp);

		if(yy_tmp->user == NULL)
			DupString(yy_tmp->user, "*");
		else
			collapse(yy_tmp->user);

		if(yy_tmp->host == NULL)
			continue;
		else
			collapse(yy_tmp->host);

		add_conf_by_address(yy_tmp->host, CONF_CLIENT, yy_tmp->user, yy_tmp);
	}

	yy_achead = NULL;
	yy_aconf = NULL;
	yy_aprev = NULL;
	yy_acount = 0;
	return 0;
}

static void
conf_set_auth_user(void *data)
{
	char *p;
	char *new_user;
	char *new_host;

	/* The first user= line doesn't allocate a new conf */
	if(yy_acount++)
	{
		yy_aprev = yy_aconf;
		yy_aconf = (yy_aconf->next = make_conf());
		yy_aconf->status = CONF_CLIENT;
	}

	if((p = strchr(data, '@')))
	{
		*p = '\0';
		DupString(new_user, data);
		MyFree(yy_aconf->user);
		yy_aconf->user = new_user;
		p++;
		MyFree(yy_aconf->host);
		DupString(new_host, p);
		yy_aconf->host = new_host;
	}
	else
	{
		MyFree(yy_aconf->host);
		DupString(yy_aconf->host, data);
		DupString(yy_aconf->user, "*");
	}
}

static void
conf_set_auth_passwd(void *data)
{
	if(yy_achead->passwd)
		memset(yy_achead->passwd, 0, strlen(yy_achead->passwd));
	MyFree(yy_achead->passwd);
	DupString(yy_achead->passwd, data);
}

static void
conf_set_auth_encrypted(void *data)
{
	int yesno = *(unsigned int *) data;

	if(yesno)
		yy_achead->flags |= CONF_FLAGS_ENCRYPTED;
	else
		yy_achead->flags &= ~CONF_FLAGS_ENCRYPTED;
}

static void
conf_set_auth_spoof(void *data)
{
	char *p;
	char *user = NULL;
	char *host = NULL;

	host = data;

	/* user@host spoof */
	if((p = strchr(host, '@')) != NULL)
	{
		*p = '\0';
		user = data;
		host = p+1;

		if(EmptyString(user))
		{
			conf_report_error("Warning -- spoof ident empty.");
			return;
		}

		if(strlen(user) > USERLEN)
		{
			conf_report_error("Warning -- spoof ident length invalid.");
			return;
		}

		if(!valid_username(user))
		{
			conf_report_error("Warning -- invalid spoof (ident).");
			return;
		}

		/* this must be restored! */
		*p = '@';
	}

	if(EmptyString(host))
	{
		conf_report_error("Warning -- spoof host empty.");
		return;
	}

	if(strlen(host) > HOSTLEN)
	{
		conf_report_error("Warning -- spoof host length invalid.");
		return;
	}

	if(!valid_hostname(host) || (strchr(host, '.') == NULL))
	{
		conf_report_error("Warning -- invalid spoof (host).");
		return;
	}

	MyFree(yy_achead->name);
	DupString(yy_achead->name, data);
	yy_achead->flags |= CONF_FLAGS_SPOOF_IP;
}

static void
conf_set_auth_flags(void *data)
{
	conf_parm_t *args = data;

	set_modes_from_table((int *) &yy_achead->flags, "flag", auth_table, args);
}

static void
conf_set_auth_redir_serv(void *data)
{
	yy_achead->flags |= CONF_FLAGS_REDIR;
	MyFree(yy_achead->name);
	DupString(yy_achead->name, data);
}

static void
conf_set_auth_redir_port(void *data)
{
	int port = *(unsigned int *) data;

	yy_achead->flags |= CONF_FLAGS_REDIR;
	yy_achead->port = port;
}

static void
conf_set_auth_class(void *data)
{
	MyFree(yy_achead->className);
	DupString(yy_achead->className, data);
}

static int
conf_begin_resv(struct TopConf *tc)
{
	resv_reason = NULL;
	return 0;
}

static int
conf_end_resv(struct TopConf *tc)
{
	MyFree(resv_reason);
	resv_reason = NULL;
	return 0;
}

static void
conf_set_resv_channel(void *data)
{
	if(IsChannelName((char *) data))
	{
		if(find_channel_resv((char *) data))
		{
			conf_report_error("Warning -- channel '%s' in resv is already resv'd.",
					  (char *) data);
			return;
		}

		yy_rxconf = make_rxconf(data, 
					EmptyString(resv_reason) ? "No Reason" : resv_reason, 
					0, CONF_RESV|RESV_CHANNEL);
		add_rxconf(yy_rxconf);
	}
	else
	{
		conf_report_error
			("Warning -- channel '%s' in resv is not a valid channel name.",
			 (char *) data);
	}
}

static void
conf_set_resv_reason(void *data)
{
	MyFree(resv_reason);
	DupString(resv_reason, data);
}

static void
conf_set_resv_nick(void *data)
{
	if(clean_resv_nick(data))
	{
		if(find_nick_resv((char *) data))
		{
			conf_report_error("Warning -- nick '%s' in resv is already resv'd.",
					  (char *) data);
			return;
		}

		yy_rxconf = make_rxconf(data,
					EmptyString(resv_reason) ? "No Reason" : resv_reason,
					0, CONF_RESV|RESV_NICK);
		add_rxconf(yy_rxconf);
	}
	else
	{
		conf_report_error
			("Warning -- nickname '%s' in resv is not a valid nickname.",
			 (char *) data);
	}
}

static int
conf_begin_shared(struct TopConf *tc)
{
	yy_uconf = make_shared();
	yy_uconf->flags = OPER_K;
	return 0;
}

static int
conf_end_shared(struct TopConf *tc)
{
	dlinkAddAlloc(yy_uconf, &shared_list);
	yy_uconf = NULL;
	return 0;
}

static void
conf_set_shared_name(void *data)
{
	MyFree(yy_uconf->servername);
	DupString(yy_uconf->servername, data);
}

static void
conf_set_shared_user(void *data)
{
	char *p;

	if((p = strchr(data, '@')))
	{
		*p++ = '\0';
		MyFree(yy_uconf->username);
		DupString(yy_uconf->username, data);

		MyFree(yy_uconf->host);
		DupString(yy_uconf->host, p);
	}
}

static void
conf_set_shared_kline(void *data)
{
	int yesno = *(unsigned int *) data;

	if(yesno)
		yy_uconf->flags |= OPER_K;
	else
		yy_uconf->flags &= ~OPER_K;
}

static void
conf_set_shared_unkline(void *data)
{
	int yesno = *(unsigned int *) data;

	if(yesno)
		yy_uconf->flags |= OPER_UNKLINE;
	else
		yy_uconf->flags &= ~OPER_UNKLINE;
}

static void
conf_set_shared_type(void *data)
{
	conf_parm_t *args = data;

	/* if theyre setting a type, remove the 'kline' default */
	yy_uconf->flags = 0;
	set_modes_from_table(&yy_uconf->flags, "flag", shared_table, args);
}

static int
conf_begin_connect(struct TopConf *tc)
{
	hub_confs = NULL;

	if(yy_aconf)
	{
		free_conf(yy_aconf);
		yy_aconf = NULL;
	}

	if(yy_hconf)
	{
		free_conf(yy_hconf);
		yy_hconf = NULL;
	}

	if(yy_lconf)
	{
		free_conf(yy_lconf);
		yy_lconf = NULL;
	}

	yy_aconf = make_conf();
	yy_aconf->passwd = NULL;
	yy_aconf->status = CONF_SERVER;
	yy_aconf->port = PORTNUM;
	return 0;
}

static int
conf_end_connect(struct TopConf *tc)
{
	if(conf_cur_block_name != NULL)
	{
		MyFree(yy_aconf->name);
		DupString(yy_aconf->name, conf_cur_block_name);
	}
	if(yy_aconf->host && yy_aconf->passwd && yy_aconf->spasswd)
	{
		if(conf_add_server(yy_aconf, scount) >= 0)
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
		if(yy_aconf->name)
		{
			if(!yy_aconf->host)
				conf_report_error
					("Ignoring connect block for %s -- missing host.",
					 yy_aconf->name);
			else if(!yy_aconf->passwd || !yy_aconf->spasswd)
				conf_report_error
					("Ignoring connect block for %s -- missing password.",
					 yy_aconf->name);
		}

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
		if(yy_aconf != NULL)
		{
			DupString(yy_hconf->name, yy_aconf->name);
			conf_add_conf(yy_hconf);
		}
		else
			free_conf(yy_hconf);
	}

	for (yy_lconf = leaf_confs; yy_lconf; yy_lconf = yy_aconf_next)
	{
		yy_aconf_next = yy_lconf->next;
		if(yy_aconf != NULL)
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
	return 0;
}

static void
conf_set_connect_name(void *data)
{
	if(yy_aconf->name != NULL)
	{
		conf_report_error("Warning -- connect::name specified multiple times.");
	}

	MyFree(yy_aconf->name);
	DupString(yy_aconf->name, data);
}

static void
conf_set_connect_host(void *data)
{
	MyFree(yy_aconf->host);
	DupString(yy_aconf->host, data);
}

static void
conf_set_connect_vhost(void *data)
{
	if(inetpton_sock(data, &yy_aconf->my_ipnum) <= 0)
	{
		conf_report_error("Invalid netmask for server vhost (%s)",
		    		  (char *) data);
		return;
	}

	yy_aconf->flags |= CONF_FLAGS_VHOSTED;
}

static void
conf_set_connect_send_password(void *data)
{
	if(yy_aconf->spasswd)
		memset(yy_aconf->spasswd, 0, strlen(yy_aconf->spasswd));
	MyFree(yy_aconf->spasswd);
	DupString(yy_aconf->spasswd, data);
}

static void
conf_set_connect_accept_password(void *data)
{
	if(yy_aconf->passwd)
		memset(yy_aconf->passwd, 0, strlen(yy_aconf->passwd));
	MyFree(yy_aconf->passwd);
	DupString(yy_aconf->passwd, data);
}

static void
conf_set_connect_port(void *data)
{
	yy_aconf->port = *(unsigned int *) data;
}

static void
conf_set_connect_aftype(void *data)
{
	char *aft = data;

	if(strcasecmp(aft, "ipv4") == 0)
		yy_aconf->aftype = AF_INET;
#ifdef IPV6
	else if(strcasecmp(aft, "ipv6") == 0)
		yy_aconf->aftype = AF_INET6;
#endif
	else
		conf_report_error("connect::aftype '%s' is unknown.", aft);
}

static void
conf_set_connect_encrypted(void *data)
{
	int yesno = *(unsigned int *) data;

	if(yesno)
		yy_aconf->flags |= CONF_FLAGS_ENCRYPTED;
	else
		yy_aconf->flags &= ~CONF_FLAGS_ENCRYPTED;
}

static void
conf_set_connect_rsa_public_key_file(void *data)
{
#ifdef HAVE_LIBCRYPTO
	BIO *file;

	if(yy_aconf->rsa_public_key)
	{
		RSA_free(yy_aconf->rsa_public_key);
		yy_aconf->rsa_public_key = NULL;
	}

	if(yy_aconf->rsa_public_key_file)
	{
		MyFree(yy_aconf->rsa_public_key_file);
		yy_aconf->rsa_public_key_file = NULL;
	}

	DupString(yy_aconf->rsa_public_key_file, data);

	file = BIO_new_file(data, "r");

	if(file == NULL)
	{
		conf_report_error
			("Ignoring connect::rsa_public_key_file %s -- couldn't open file.", data);
		return;
	}

	yy_aconf->rsa_public_key = (RSA *) PEM_read_bio_RSA_PUBKEY(file, NULL, 0, NULL);

	if(yy_aconf->rsa_public_key == NULL)
	{
		conf_report_error
			("Ignoring connect::rsa_public_key_file -- Key invalid; check syntax.");
		return;
	}

	BIO_set_close(file, BIO_CLOSE);
	BIO_free(file);
#else
	conf_report_error
		("Ignoring connect::rsa_public_key_file -- OpenSSL support not available.");
#endif
}

static void
conf_set_connect_compressed(void *data)
{
#ifdef HAVE_LIBZ
	int yesno = *(unsigned int *) data;

	if(yesno)
		yy_aconf->flags |= CONF_FLAGS_COMPRESSED;
	else
		yy_aconf->flags &= ~CONF_FLAGS_COMPRESSED;
#else
	conf_report_error("Ignoring connect::compressed -- zlib not available.");
#endif
}

static void
conf_set_connect_topicburst(void *data)
{
	int yesno = *(unsigned int *) data;

	if(yesno)
		yy_aconf->flags |= CONF_FLAGS_TB;
	else
		yy_aconf->flags &= ~CONF_FLAGS_TB;
}

static void
conf_set_connect_auto(void *data)
{
	int yesno = *(unsigned int *) data;

	if(yesno)
		yy_aconf->flags |= CONF_FLAGS_ALLOW_AUTO_CONN;
	else
		yy_aconf->flags &= ~CONF_FLAGS_ALLOW_AUTO_CONN;
}

static void
conf_set_connect_hub_mask(void *data)
{
	if(hub_confs == NULL)
	{
		hub_confs = make_conf();
		hub_confs->status = CONF_HUB;
		DupString(hub_confs->host, data);
		DupString(hub_confs->user, "*");
	}
	else
	{
		yy_hconf = make_conf();
		yy_hconf->status = CONF_HUB;
		DupString(yy_hconf->host, data);
		DupString(yy_hconf->user, "*");
		yy_hconf->next = hub_confs;
		hub_confs = yy_hconf;
	}
}

static void
conf_set_connect_leaf_mask(void *data)
{
	if(leaf_confs == NULL)
	{
		leaf_confs = make_conf();
		leaf_confs->status = CONF_LEAF;
		DupString(leaf_confs->host, data);
		DupString(leaf_confs->user, "*");
	}
	else
	{
		yy_lconf = make_conf();
		yy_lconf->status = CONF_LEAF;
		DupString(yy_lconf->host, data);
		DupString(yy_lconf->user, "*");
		yy_lconf->next = leaf_confs;
		leaf_confs = yy_lconf;
	}
}

static void
conf_set_connect_class(void *data)
{
	MyFree(yy_aconf->className);
	DupString(yy_aconf->className, data);
}

static int
conf_begin_kill(struct TopConf *tc)
{
	if(yy_aconf)
	{
		free_conf(yy_aconf);
		yy_aconf = NULL;
	}

	yy_aconf = make_conf();
	yy_aconf->status = CONF_KILL;
	return 0;
}

static int
conf_end_kill(struct TopConf *tc)
{
	if(yy_aconf->user && yy_aconf->passwd && yy_aconf->host)
	{
		if(yy_aconf->host != NULL)
			add_conf_by_address(yy_aconf->host, CONF_KILL, yy_aconf->user, yy_aconf);
	}
	else
	{
		free_conf(yy_aconf);
	}
	yy_aconf = NULL;
	return 0;
}

static void
conf_set_kill_user(void *data)
{
	char *p;
	char *new_user;
	char *new_host;

	if((p = strchr(data, '@')))
	{
		*p = '\0';
		DupString(new_user, data);
		yy_aconf->user = new_user;
		p++;
		DupString(new_host, p);
		MyFree(yy_aconf->host);
		yy_aconf->host = new_host;
	}
	else
	{
		MyFree(yy_aconf->host);
		DupString(yy_aconf->host, data);
		MyFree(yy_aconf->user);
		DupString(yy_aconf->user, "*");
	}
}

static void
conf_set_kill_reason(void *data)
{
	MyFree(yy_aconf->passwd);
	DupString(yy_aconf->passwd, data);
}

static int
conf_begin_deny(struct TopConf *tc)
{
	if(yy_aconf)
	{
		free_conf(yy_aconf);
		yy_aconf = NULL;
	}

	yy_aconf = make_conf();
	yy_aconf->status = CONF_DLINE;
	/* default reason */
	DupString(yy_aconf->passwd, "No Reason");

	return 0;
}

static int
conf_end_deny(struct TopConf *tc)
{
	if(yy_aconf->host && parse_netmask(yy_aconf->host, NULL, NULL) != HM_HOST)
	{
		add_conf_by_address(yy_aconf->host, CONF_DLINE, NULL, yy_aconf);
	}
	else
	{
		free_conf(yy_aconf);
		conf_report_error("Ignoring deny -- invalid hostname.");
	}
	yy_aconf = NULL;
	return 0;
}

static void
conf_set_deny_ip(void *data)
{
	MyFree(yy_aconf->host);
	DupString(yy_aconf->host, data);
}

static void
conf_set_deny_reason(void *data)
{
	MyFree(yy_aconf->passwd);
	DupString(yy_aconf->passwd, data);
}

static int
conf_begin_exempt(struct TopConf *tc)
{
	if(yy_aconf)
	{
		free_conf(yy_aconf);
		yy_aconf = NULL;
	}

	yy_aconf = make_conf();
	DupString(yy_aconf->passwd, "*");
	yy_aconf->status = CONF_EXEMPTDLINE;

	return 0;
}

static int
conf_end_exempt(struct TopConf *tc)
{
	if(yy_aconf->host && parse_netmask(yy_aconf->host, NULL, NULL) != HM_HOST)
	{
		add_conf_by_address(yy_aconf->host, CONF_EXEMPTDLINE, NULL, yy_aconf);
	}
	else
	{
		conf_report_error("Ignoring exempt -- invalid exempt::ip.");
		free_conf(yy_aconf);
	}

	yy_aconf = NULL;
	return 0;
}

static void
conf_set_exempt_ip(void *data)
{
	MyFree(yy_aconf->host);
	DupString(yy_aconf->host, data);
}

static int
conf_begin_gecos(struct TopConf *tc)
{
	yy_rxconf = make_rxconf(NULL, "No Reason", 0, CONF_XLINE);
	return 0;
}

static int
conf_end_gecos(struct TopConf *tc)
{
	if(!EmptyString(yy_rxconf->name))
	{
		if(find_xline(yy_rxconf->name) != NULL)
		{
			conf_report_error("Warning -- name '%s' in gecos is already xlined.",
					  yy_rxconf->name);
			free_rxconf(yy_rxconf);
		}
		else
			add_rxconf(yy_rxconf);
	}
	else
	{
		conf_report_error("Ignoring gecos -- invalid gecos::name.");
		free_rxconf(yy_rxconf);
	}

	yy_rxconf = NULL;

	return 0;
}

static void
conf_set_gecos_name(void *data)
{
	MyFree(yy_rxconf->name);
	DupString(yy_rxconf->name, data);
	collapse(yy_rxconf->name);
}

static void
conf_set_gecos_reason(void *data)
{
	MyFree(yy_rxconf->reason);

	if(strchr((char *) data, ':') == NULL)
		DupString(yy_rxconf->reason, data);
	else
		DupString(yy_rxconf->reason, "No Reason");
}

static void
conf_set_gecos_action(void *data)
{
	char *act = data;

	if(strcasecmp(act, "warn") == 0)
		yy_rxconf->type = 0;
	else if(strcasecmp(act, "reject") == 0)
		yy_rxconf->type = 1;
	else if(strcasecmp(act, "silent") == 0)
		yy_rxconf->type = 2;
	else
		conf_report_error("Warning -- invalid gecos::action.");
}

static int
conf_begin_cluster(struct TopConf *tc)
{
	yy_cconf = make_cluster();
	return 0;
}

static int
conf_end_cluster(struct TopConf *tc)
{
	if(!EmptyString(yy_cconf->name))
	{
		dlinkAddAlloc(yy_cconf, &cluster_list);
	}
	else
	{
		conf_report_error("Ignoring cluster -- invalid cluster::server");
		free_cluster(yy_cconf);
	}

	yy_cconf = NULL;
	return 0;
}

static void
conf_set_cluster_name(void *data)
{
	MyFree(yy_cconf->name);
	DupString(yy_cconf->name, data);
}

static void
conf_set_cluster_type(void *data)
{
	conf_parm_t *args = data;

	set_modes_from_table(&yy_cconf->type, "flag", cluster_table, args);
}

static void
conf_set_general_default_operstring(void *data)
{
	if(EmptyString((char *) data))
	{
		conf_report_error("Warning -- operstring must exist");
		strlcpy(ConfigFileEntry.default_operstring, "is an IRC operator",
			sizeof(ConfigFileEntry.default_operstring));
	}
	else
	{
		strlcpy(ConfigFileEntry.default_operstring, data,
			sizeof(ConfigFileEntry.default_operstring));
	}
}

static void
conf_set_general_default_adminstring(void *data)
{
	if(EmptyString((char *) data))
	{
		conf_report_error("Warning -- adminstring must exist");
		strlcpy(ConfigFileEntry.default_adminstring, "is a Server Administrator",
			sizeof(ConfigFileEntry.default_adminstring));
	}
	else
	{
		strlcpy(ConfigFileEntry.default_adminstring, data,
			sizeof(ConfigFileEntry.default_adminstring));
	}
}

static void
conf_set_general_failed_oper_notice(void *data)
{
	ConfigFileEntry.failed_oper_notice = *(unsigned int *) data;
}

static void
conf_set_general_anti_nick_flood(void *data)
{
	ConfigFileEntry.anti_nick_flood = *(unsigned int *) data;
}

static void
conf_set_general_max_nick_time(void *data)
{
	ConfigFileEntry.max_nick_time = *(unsigned int *) data;
}

static void
conf_set_general_max_nick_changes(void *data)
{
	ConfigFileEntry.max_nick_changes = *(unsigned int *) data;
}

static void
conf_set_general_max_accept(void *data)
{
	ConfigFileEntry.max_accept = *(unsigned int *) data;
}

static void
conf_set_general_anti_spam_exit_message_time(void *data)
{
	ConfigFileEntry.anti_spam_exit_message_time = *(unsigned int *) data;
}

static void
conf_set_general_ts_warn_delta(void *data)
{
	ConfigFileEntry.ts_warn_delta = *(unsigned int *) data;
}

static void
conf_set_general_ts_max_delta(void *data)
{
	ConfigFileEntry.ts_max_delta = *(unsigned int *) data;
}

static void
conf_set_general_havent_read_conf(void *data)
{
	if(*(unsigned int *) data)
	{
		conf_report_error("You haven't read your config file properly.");
		conf_report_error
			("There is a line in the example conf that will kill your server if not removed.");
		conf_report_error
			("Consider actually reading/editing the conf file, and removing this line.");
		if (!testing_conf)
			exit(0);
	}
}

static void
conf_set_general_hide_error_messages(void *data)
{
	char *val = data;

	if(strcasecmp(val, "yes") == 0)
		ConfigFileEntry.hide_error_messages = 2;
	else if(strcasecmp(val, "opers") == 0)
		ConfigFileEntry.hide_error_messages = 1;
	else if(strcasecmp(val, "no") == 0)
		ConfigFileEntry.hide_error_messages = 0;
	else
		conf_report_error("Invalid setting '%s' for general::hide_error_messages.", val);
}

static void
conf_set_general_kline_with_reason(void *data)
{
	ConfigFileEntry.kline_with_reason = *(unsigned int *) data;
}

static void
conf_set_general_kline_with_connection_closed(void *data)
{
	ConfigFileEntry.kline_with_connection_closed = *(unsigned int *) data;
}

static void
conf_set_general_kline_delay(void *data)
{
	ConfigFileEntry.kline_delay = *(unsigned int *) data;

	/* THIS MUST BE HERE to stop us being unable to check klines */
	kline_queued = 0;
}

static void
conf_set_general_client_exit(void *data)
{
	ConfigFileEntry.client_exit = *(unsigned int *) data;
}

static void
conf_set_general_warn_no_nline(void *data)
{
	ConfigFileEntry.warn_no_nline = *(unsigned int *) data;
}

static void
conf_set_general_non_redundant_klines(void *data)
{
	ConfigFileEntry.non_redundant_klines = *(unsigned int *) data;
}

static void
conf_set_general_stats_c_oper_only(void *data)
{
	ConfigFileEntry.stats_c_oper_only = *(unsigned int *) data;
}

static void
conf_set_general_stats_h_oper_only(void *data)
{
	ConfigFileEntry.stats_h_oper_only = *(unsigned int *) data;
}

static void
conf_set_general_stats_y_oper_only(void *data)
{
	ConfigFileEntry.stats_y_oper_only = *(unsigned int *) data;
}

static void
conf_set_general_stats_o_oper_only(void *data)
{
	ConfigFileEntry.stats_o_oper_only = *(unsigned int *) data;
}

static void
conf_set_general_stats_P_oper_only(void *data)
{
	ConfigFileEntry.stats_P_oper_only = *(unsigned int *) data;
}

static void
conf_set_general_stats_k_oper_only(void *data)
{
	char *val = data;

	if(strcasecmp(val, "yes") == 0)
		ConfigFileEntry.stats_k_oper_only = 2;
	else if(strcasecmp(val, "masked") == 0)
		ConfigFileEntry.stats_k_oper_only = 1;
	else if(strcasecmp(val, "no") == 0)
		ConfigFileEntry.stats_k_oper_only = 0;
	else
		conf_report_error("Invalid setting '%s' for general::stats_k_oper_only.", val);
}

static void
conf_set_general_stats_i_oper_only(void *data)
{
	char *val = data;

	if(strcasecmp(val, "yes") == 0)
		ConfigFileEntry.stats_i_oper_only = 2;
	else if(strcasecmp(val, "masked") == 0)
		ConfigFileEntry.stats_i_oper_only = 1;
	else if(strcasecmp(val, "no") == 0)
		ConfigFileEntry.stats_i_oper_only = 0;
	else
		conf_report_error("Invalid setting '%s' for general::stats_i_oper_only.", val);
}

static void
conf_set_general_map_oper_only(void *data)
{
	ConfigFileEntry.map_oper_only = *(unsigned int *) data;
}

static void
conf_set_general_operspy_admin_only(void *data)
{
	ConfigFileEntry.operspy_admin_only = *(unsigned int *) data;
}

static void
conf_set_general_pace_wait(void *data)
{
	ConfigFileEntry.pace_wait = *(unsigned int *) data;
}

static void
conf_set_general_caller_id_wait(void *data)
{
	ConfigFileEntry.caller_id_wait = *(unsigned int *) data;
}

static void
conf_set_general_pace_wait_simple(void *data)
{
	ConfigFileEntry.pace_wait_simple = *(unsigned int *) data;
}

static void
conf_set_general_short_motd(void *data)
{
	ConfigFileEntry.short_motd = *(unsigned int *) data;
}

static void
conf_set_general_no_oper_flood(void *data)
{
	ConfigFileEntry.no_oper_flood = *(unsigned int *) data;
}

static void
conf_set_general_glines(void *data)
{
	ConfigFileEntry.glines = *(unsigned int *) data;
}

static void
conf_set_general_gline_time(void *data)
{
	ConfigFileEntry.gline_time = *(unsigned int *) data;
}

static void
conf_set_general_gline_min_cidr(void *data)
{
	ConfigFileEntry.gline_min_cidr = *(unsigned int *) data;
}

static void
conf_set_general_gline_min_cidr6(void *data)
{
	ConfigFileEntry.gline_min_cidr6 = *(unsigned int *) data;
}

static void
conf_set_general_idletime(void *data)
{
	ConfigFileEntry.idletime = *(unsigned int *) data;
}

static void
conf_set_general_dots_in_ident(void *data)
{
	ConfigFileEntry.dots_in_ident = *(unsigned int *) data;
}

static void
conf_set_general_max_targets(void *data)
{
	ConfigFileEntry.max_targets = *(unsigned int *) data;
}

static void
conf_set_general_htm_messages(void *data)
{
	ConfigFileEntry.htm_messages = *(unsigned int *) data;
}

static void
conf_set_general_htm_interval(void *data)
{
	ConfigFileEntry.htm_interval = *(unsigned int *) data;
}

static void
conf_set_general_htm_trigger(void *data)
{
	ConfigFileEntry.htm_trigger = *(unsigned int *) data;
}
static void
conf_set_general_servlink_path(void *data)
{
	MyFree(ConfigFileEntry.servlink_path);
	DupString(ConfigFileEntry.servlink_path, data);
}

static void
conf_set_general_compression_level(void *data)
{
#ifdef HAVE_LIBZ
	ConfigFileEntry.compression_level = *(unsigned int *) data;

	if((ConfigFileEntry.compression_level < 1) || (ConfigFileEntry.compression_level > 9))
	{
		conf_report_error
			("Invalid general::compression_level %d -- using default.",
			 ConfigFileEntry.compression_level);
		ConfigFileEntry.compression_level = 0;
	}
#else
	conf_report_error("Ignoring general::compression_level -- zlib not available.");
#endif
}

static void
conf_set_general_use_egd(void *data)
{
	ConfigFileEntry.use_egd = *(unsigned int *) data;
}

static void
conf_set_general_egdpool_path(void *data)
{
	MyFree(ConfigFileEntry.egdpool_path);
	DupString(ConfigFileEntry.egdpool_path, data);
}

static void
conf_set_general_ping_cookie(void *data)
{
	ConfigFileEntry.ping_cookie = *(unsigned int *) data;
}

static void
conf_set_general_disable_auth(void *data)
{
	ConfigFileEntry.disable_auth = *(unsigned int *) data;
}

static void
conf_set_general_use_whois_actually(void *data)
{
	ConfigFileEntry.use_whois_actually = *(unsigned int *) data;
}

static void
conf_set_general_tkline_expire_notices(void *data)
{
	ConfigFileEntry.tkline_expire_notices = *(unsigned int *) data;
}

static void
conf_set_general_connect_timeout(void *data)
{
	ConfigFileEntry.connect_timeout = *(unsigned int *) data;
}

static void
conf_set_general_burst_away(void *data)
{
	ConfigFileEntry.burst_away = *(unsigned int *) data;
}

#ifdef IPV6
static void
conf_set_general_fallback_to_ip6_int(void *data)
{
	ConfigFileEntry.fallback_to_ip6_int = *(unsigned int *) data;
}
#endif

static void
conf_set_general_oper_umodes(void *data)
{
	set_modes_from_table(&ConfigFileEntry.oper_umodes, "umode", umode_table, data);
}

static void
conf_set_general_oper_only_umodes(void *data)
{
	set_modes_from_table(&ConfigFileEntry.oper_only_umodes, "umode", umode_table, data);
}

static void
conf_set_general_min_nonwildcard(void *data)
{
	ConfigFileEntry.min_nonwildcard = *(unsigned int *) data;
}

static void
conf_set_general_min_nonwildcard_simple(void *data)
{
	ConfigFileEntry.min_nonwildcard_simple = *(unsigned int *) data;
}

static void
conf_set_general_default_floodcount(void *data)
{
	ConfigFileEntry.default_floodcount = *(unsigned int *) data;
}

static void
conf_set_general_client_flood(void *data)
{
	ConfigFileEntry.client_flood = *(unsigned int *) data;
}

static void
conf_set_general_dot_in_ip6_addr(void *data)
{
	ConfigFileEntry.dot_in_ip6_addr = *(unsigned int *) data;
}

static void
conf_set_general_reject_ban_time(void *data)
{
	ConfigFileEntry.reject_ban_time = *(unsigned int *) data;
}

static void
conf_set_general_reject_after_count(void *data)
{
	ConfigFileEntry.reject_after_count = *(unsigned int *) data;
}

static void
conf_set_general_reject_duration(void *data)
{
	ConfigFileEntry.reject_duration = *(unsigned int *) data;
}

static void
conf_set_channel_use_except(void *data)
{
	ConfigChannel.use_except = *(unsigned int *) data;
}

static void
conf_set_channel_use_invex(void *data)
{
	ConfigChannel.use_invex = *(unsigned int *) data;
}

static void
conf_set_channel_use_knock(void *data)
{
	ConfigChannel.use_knock = *(unsigned int *) data;
}

static void
conf_set_channel_knock_delay(void *data)
{
	ConfigChannel.knock_delay = *(unsigned int *) data;
}

static void
conf_set_channel_knock_delay_channel(void *data)
{
	ConfigChannel.knock_delay_channel = *(unsigned int *) data;
}

static void
conf_set_channel_max_chans_per_user(void *data)
{
	ConfigChannel.max_chans_per_user = *(unsigned int *) data;
}

static void
conf_set_channel_quiet_on_ban(void *data)
{
	ConfigChannel.quiet_on_ban = *(unsigned int *) data;
}

static void
conf_set_channel_max_bans(void *data)
{
	ConfigChannel.max_bans = *(unsigned int *) data;
}

static void
conf_set_channel_default_split_user_count(void *data)
{
	ConfigChannel.default_split_user_count = *(unsigned int *) data;
}

static void
conf_set_channel_default_split_server_count(void *data)
{
	ConfigChannel.default_split_server_count = *(unsigned int *) data;
}

static void
conf_set_channel_default_split_delay(void *data)
{
	ConfigChannel.default_split_delay = *(unsigned int *) data;
}

static void
conf_set_channel_no_create_on_split(void *data)
{
	ConfigChannel.no_create_on_split = *(unsigned int *) data;
}

static void
conf_set_channel_no_join_on_split(void *data)
{
	ConfigChannel.no_join_on_split = *(unsigned int *) data;
}

static void
conf_set_channel_no_oper_resvs(void *data)
{
	ConfigChannel.no_oper_resvs = *(unsigned int *) data;
}

static void
conf_set_channel_burst_topicwho(void *data)
{
	ConfigChannel.burst_topicwho = *(unsigned int *) data;
}

static void
conf_set_serverhide_flatten_links(void *data)
{
	ConfigServerHide.flatten_links = *(unsigned int *) data;
}

static void
conf_set_serverhide_links_delay(void *data)
{
	int val = *(unsigned int *) data;

	if((val > 0) && ConfigServerHide.links_disabled == 1)
	{
		eventAddIsh("cache_links", cache_links, NULL, val);
		ConfigServerHide.links_disabled = 0;
	}

	ConfigServerHide.links_delay = val;
}

static void
conf_set_serverhide_hidden(void *data)
{
	ConfigServerHide.hidden = *(unsigned int *) data;
}

static void
conf_set_serverhide_disable_hidden(void *data)
{
	ConfigServerHide.disable_hidden = *(unsigned int *) data;
}

/* public functions */


void
conf_report_error(const char *fmt, ...)
{
	va_list ap;
	char msg[IRCD_BUFSIZE + 1] = { 0 };

	va_start(ap, fmt);
	ircvsnprintf(msg, IRCD_BUFSIZE, fmt, ap);
	va_end(ap);

	if (testing_conf)
	{
		fprintf(stderr, "\"%s\", line %d: %s\n", current_file, lineno + 1, msg);
		return;
	}

	ilog(L_MAIN, "\"%s\", line %d: %s", current_file, lineno + 1, msg);
	sendto_realops_flags(UMODE_ALL, L_ALL, "\"%s\", line %d: %s", current_file, lineno + 1, msg);
}

int
conf_start_block(char *block, char *name)
{
	if((conf_cur_block = find_top_conf(block)) == NULL)
	{
		conf_report_error("Configuration block '%s' is not defined.", block);
		return -1;
	}

	if(name)
		DupString(conf_cur_block_name, name);
	else
		conf_cur_block_name = NULL;

	if(conf_cur_block->tc_sfunc)
		if(conf_cur_block->tc_sfunc(conf_cur_block) < 0)
			return -1;

	return 0;
}

int
conf_end_block(struct TopConf *tc)
{
	if(tc->tc_efunc)
		return tc->tc_efunc(tc);

	MyFree(conf_cur_block_name);
	return 0;
}

int
conf_call_set(struct TopConf *tc, char *item, conf_parm_t * value, int type)
{
	struct ConfEntry *cf;
	conf_parm_t *cp;

	if(!tc)
		return -1;

	if((cf = find_conf_item(tc, item)) == NULL)
	{
		conf_report_error
			("Non-existant configuration setting %s::%s.", tc->tc_name, (char *) item);
		return -1;
	}

	/* if it takes one thing, make sure they only passed one thing,
	   and handle as needed. */
	if(value->type & CF_FLIST && !cf->cf_type & CF_FLIST)
	{
		conf_report_error
			("Option %s::%s does not take a list of values.", tc->tc_name, item);
		return -1;
	}

	cp = value->v.list;


	if(CF_TYPE(value->v.list->type) != CF_TYPE(cf->cf_type))
	{
		/* if it expects a string value, but we got a yesno, 
		 * convert it back
		 */
		if((CF_TYPE(value->v.list->type) == CF_YESNO) &&
		   (CF_TYPE(cf->cf_type) == CF_STRING))
		{
			value->v.list->type = CF_STRING;

			if(cp->v.number == 1)
				DupString(cp->v.string, "yes");
			else
				DupString(cp->v.string, "no");
		}

		/* maybe it's a CF_TIME and they passed CF_INT --
		   should still be valid */
		else if(!((CF_TYPE(value->v.list->type) == CF_INT) &&
			  (CF_TYPE(cf->cf_type) == CF_TIME)))
		{
			conf_report_error
				("Wrong type for %s::%s (expected %s, got %s)",
				 tc->tc_name, (char *) item,
				 conf_strtype(cf->cf_type), conf_strtype(value->v.list->type));
			return -1;
		}
	}

	if(cf->cf_type & CF_FLIST)
	{
		/* just pass it the extended argument list */
		cf->cf_func(value->v.list);
	}
	else
	{
		/* it's old-style, needs only one arg */
		switch (cf->cf_type)
		{
		case CF_INT:
		case CF_TIME:
		case CF_YESNO:
			cf->cf_func(&cp->v.number);
			break;
		case CF_STRING:
		case CF_QSTRING:
			cf->cf_func(cp->v.string);
			break;
		}
	}


	return 0;
}

int
add_conf_item(const char *topconf, const char *name, int type, void (*func) (void *))
{
	struct TopConf *tc;
	struct ConfEntry *cf;

	if((tc = find_top_conf(topconf)) == NULL)
		return -1;

	if((cf = find_conf_item(tc, name)) != NULL)
		return -1;

	cf = MyMalloc(sizeof(struct ConfEntry));

	DupString(cf->cf_name, name);
	cf->cf_type = type;
	cf->cf_func = func;

	dlinkAddAlloc(cf, &tc->tc_items);

	return 0;
}

int
remove_conf_item(const char *topconf, const char *name)
{
	struct TopConf *tc;
	struct ConfEntry *cf;
	dlink_node *ptr;

	if((tc = find_top_conf(topconf)) == NULL)
		return -1;

	if((cf = find_conf_item(tc, name)) == NULL)
		return -1;

	if((ptr = dlinkFind(&tc->tc_items, cf)) == NULL)
		return -1;

	dlinkDestroy(ptr, &tc->tc_items);
	MyFree(cf->cf_name);
	MyFree(cf);

	return 0;
}




void
newconf_init()
{
	add_top_conf("modules", NULL, NULL);
	add_conf_item("modules", "path", CF_QSTRING, conf_set_modules_path);
	add_conf_item("modules", "module", CF_QSTRING, conf_set_modules_module);

	add_top_conf("serverinfo", NULL, NULL);
	add_conf_item("serverinfo", "name", CF_QSTRING, conf_set_serverinfo_name);
	add_conf_item("serverinfo", "sid", CF_QSTRING, conf_set_serverinfo_sid);
	add_conf_item("serverinfo", "description", CF_QSTRING, conf_set_serverinfo_description);
	add_conf_item("serverinfo", "network_name", CF_QSTRING, conf_set_serverinfo_network_name);
	add_conf_item("serverinfo", "network_desc", CF_QSTRING, conf_set_serverinfo_network_desc);
	add_conf_item("serverinfo", "vhost", CF_QSTRING, conf_set_serverinfo_vhost);
	add_conf_item("serverinfo", "vhost6", CF_QSTRING, conf_set_serverinfo_vhost6);
	add_conf_item("serverinfo", "hub", CF_YESNO, conf_set_serverinfo_hub);
	add_conf_item("serverinfo", "use_ts6", CF_YESNO, conf_set_serverinfo_use_ts6);

	add_top_conf("admin", NULL, NULL);
	add_conf_item("admin", "name", CF_QSTRING, conf_set_admin_name);
	add_conf_item("admin", "description", CF_QSTRING, conf_set_admin_description);
	add_conf_item("admin", "email", CF_QSTRING, conf_set_admin_email);

	add_top_conf("log", NULL, NULL);
	add_conf_item("log", "fname_userlog", CF_QSTRING, conf_set_log_fname_userlog);
	add_conf_item("log", "fname_fuserlog", CF_QSTRING, conf_set_log_fname_fuserlog);
	add_conf_item("log", "fname_operlog", CF_QSTRING, conf_set_log_fname_operlog);
	add_conf_item("log", "fname_foperlog", CF_QSTRING, conf_set_log_fname_foperlog);
	add_conf_item("log", "fname_serverlog", CF_QSTRING, conf_set_log_fname_serverlog);
	add_conf_item("log", "fname_glinelog", CF_QSTRING, conf_set_log_fname_glinelog);
	add_conf_item("log", "fname_klinelog", CF_QSTRING, conf_set_log_fname_klinelog);
	add_conf_item("log", "fname_operspylog", CF_QSTRING, conf_set_log_fname_operspylog);
	add_conf_item("log", "fname_ioerrorlog", CF_QSTRING, conf_set_log_fname_ioerrorlog);

	add_top_conf("operator", conf_begin_oper, conf_end_oper);
	add_conf_item("operator", "name", CF_QSTRING, conf_set_oper_name);
	add_conf_item("operator", "user", CF_QSTRING, conf_set_oper_user);
	add_conf_item("operator", "password", CF_QSTRING, conf_set_oper_password);
	add_conf_item("operator", "class", CF_QSTRING, conf_set_oper_class);
	add_conf_item("operator", "encrypted", CF_YESNO, conf_set_oper_encrypted);
	add_conf_item("operator", "rsa_public_key_file", CF_QSTRING,
		      conf_set_oper_rsa_public_key_file);
	add_conf_item("operator", "flags", CF_STRING | CF_FLIST, conf_set_oper_flags);

	add_top_conf("class", conf_begin_class, conf_end_class);
	add_conf_item("class", "name", CF_QSTRING, conf_set_class_name);
	add_conf_item("class", "ping_time", CF_TIME, conf_set_class_ping_time);
	add_conf_item("class", "cidr_bitlen", CF_INT, conf_set_class_cidr_bitlen);
	add_conf_item("class", "number_per_cidr", CF_INT, conf_set_class_number_per_cidr);
	add_conf_item("class", "number_per_ip", CF_INT, conf_set_class_number_per_ip);
	add_conf_item("class", "number_per_ip_global", CF_INT, conf_set_class_number_per_ip_global);
	add_conf_item("class", "number_per_ident", CF_INT, conf_set_class_number_per_ident);
	add_conf_item("class", "connectfreq", CF_TIME, conf_set_class_connectfreq);
	add_conf_item("class", "max_number", CF_INT, conf_set_class_max_number);
	add_conf_item("class", "sendq", CF_TIME, conf_set_class_sendq);
	add_conf_item("class", "sendq_eob", CF_TIME, conf_set_class_sendq_eob);

	add_top_conf("listen", conf_begin_listen, conf_end_listen);
	add_conf_item("listen", "port", CF_INT | CF_FLIST, conf_set_listen_port);
	add_conf_item("listen", "ip", CF_QSTRING, conf_set_listen_address);
	add_conf_item("listen", "host", CF_QSTRING, conf_set_listen_address);

	add_top_conf("auth", conf_begin_auth, conf_end_auth);
	add_conf_item("auth", "user", CF_QSTRING, conf_set_auth_user);
	add_conf_item("auth", "password", CF_QSTRING, conf_set_auth_passwd);
	add_conf_item("auth", "encrypted", CF_YESNO, conf_set_auth_encrypted);
	add_conf_item("auth", "class", CF_QSTRING, conf_set_auth_class);
	add_conf_item("auth", "spoof", CF_QSTRING, conf_set_auth_spoof);
	add_conf_item("auth", "redirserv", CF_QSTRING, conf_set_auth_redir_serv);
	add_conf_item("auth", "redirport", CF_INT, conf_set_auth_redir_port);
	add_conf_item("auth", "flags", CF_STRING | CF_FLIST, conf_set_auth_flags);

	add_top_conf("resv", conf_begin_resv, conf_end_resv);
	add_conf_item("resv", "reason", CF_QSTRING, conf_set_resv_reason);
	add_conf_item("resv", "channel", CF_QSTRING, conf_set_resv_channel);
	add_conf_item("resv", "nick", CF_QSTRING, conf_set_resv_nick);

	add_top_conf("shared", conf_begin_shared, conf_end_shared);
	add_conf_item("shared", "name", CF_QSTRING, conf_set_shared_name);
	add_conf_item("shared", "user", CF_QSTRING, conf_set_shared_user);
	add_conf_item("shared", "kline", CF_YESNO, conf_set_shared_kline);
	add_conf_item("shared", "unkline", CF_YESNO, conf_set_shared_unkline);
	add_conf_item("shared", "type", CF_STRING | CF_FLIST, conf_set_shared_type);

	add_top_conf("connect", conf_begin_connect, conf_end_connect);
	add_conf_item("connect", "name", CF_QSTRING, conf_set_connect_name);
	add_conf_item("connect", "host", CF_QSTRING, conf_set_connect_host);
	add_conf_item("connect", "vhost", CF_QSTRING, conf_set_connect_vhost);
	add_conf_item("connect", "send_password", CF_QSTRING, conf_set_connect_send_password);
	add_conf_item("connect", "accept_password", CF_QSTRING, conf_set_connect_accept_password);
	add_conf_item("connect", "port", CF_INT, conf_set_connect_port);
	add_conf_item("connect", "aftype", CF_STRING, conf_set_connect_aftype);
	add_conf_item("connect", "hub_mask", CF_QSTRING, conf_set_connect_hub_mask);
	add_conf_item("connect", "leaf_mask", CF_QSTRING, conf_set_connect_leaf_mask);
	add_conf_item("connect", "class", CF_QSTRING, conf_set_connect_class);
	add_conf_item("connect", "autoconn", CF_YESNO, conf_set_connect_auto);
	add_conf_item("connect", "encrypted", CF_YESNO, conf_set_connect_encrypted);
	add_conf_item("connect", "compressed", CF_YESNO, conf_set_connect_compressed);
	add_conf_item("connect", "topicburst", CF_YESNO, conf_set_connect_topicburst);
	add_conf_item("connect", "rsa_public_key_file", CF_QSTRING,
		      conf_set_connect_rsa_public_key_file);

	add_top_conf("kill", conf_begin_kill, conf_end_kill);
	add_conf_item("kill", "user", CF_QSTRING, conf_set_kill_user);
	add_conf_item("kill", "reason", CF_QSTRING, conf_set_kill_reason);

	add_top_conf("deny", conf_begin_deny, conf_end_deny);
	add_conf_item("deny", "ip", CF_QSTRING, conf_set_deny_ip);
	add_conf_item("deny", "reason", CF_QSTRING, conf_set_deny_reason);

	add_top_conf("exempt", conf_begin_exempt, conf_end_exempt);
	add_conf_item("exempt", "ip", CF_QSTRING, conf_set_exempt_ip);

	add_top_conf("gecos", conf_begin_gecos, conf_end_gecos);
	add_conf_item("gecos", "name", CF_QSTRING, conf_set_gecos_name);
	add_conf_item("gecos", "reason", CF_QSTRING, conf_set_gecos_reason);
	add_conf_item("gecos", "action", CF_STRING, conf_set_gecos_action);

	add_top_conf("cluster", conf_begin_cluster, conf_end_cluster);
	add_conf_item("cluster", "name", CF_QSTRING, conf_set_cluster_name);
	add_conf_item("cluster", "type", CF_STRING | CF_FLIST, conf_set_cluster_type);

	add_top_conf("general", NULL, NULL);
	add_conf_item("general", "default_operstring", CF_QSTRING,
		      conf_set_general_default_operstring);
	add_conf_item("general", "default_adminstring", CF_QSTRING,
		      conf_set_general_default_adminstring);
	add_conf_item("general", "failed_oper_notice", CF_YESNO,
		      conf_set_general_failed_oper_notice);
	add_conf_item("general", "anti_nick_flood", CF_YESNO, conf_set_general_anti_nick_flood);
	add_conf_item("general", "max_nick_time", CF_TIME, conf_set_general_max_nick_time);
	add_conf_item("general", "max_nick_changes", CF_INT, conf_set_general_max_nick_changes);
	add_conf_item("general", "max_accept", CF_INT, conf_set_general_max_accept);
	add_conf_item("general", "anti_spam_exit_message_time", CF_TIME,
		      conf_set_general_anti_spam_exit_message_time);
	add_conf_item("general", "ts_warn_delta", CF_TIME, conf_set_general_ts_warn_delta);
	add_conf_item("general", "ts_max_delta", CF_TIME, conf_set_general_ts_max_delta);
	add_conf_item("general", "kline_with_reason", CF_YESNO, conf_set_general_kline_with_reason);
	add_conf_item("general", "kline_with_connection_closed", CF_YESNO,
		      conf_set_general_kline_with_connection_closed);
	add_conf_item("general", "kline_delay", CF_TIME,
		      conf_set_general_kline_delay);
	add_conf_item("general", "warn_no_nline", CF_YESNO, conf_set_general_warn_no_nline);
	add_conf_item("general", "non_redundant_klines", CF_YESNO,
		      conf_set_general_non_redundant_klines);
	add_conf_item("general", "dots_in_ident", CF_INT, conf_set_general_dots_in_ident);
	add_conf_item("general", "stats_c_oper_only", CF_YESNO, conf_set_general_stats_c_oper_only);
	add_conf_item("general", "stats_y_oper_only", CF_YESNO, conf_set_general_stats_y_oper_only);
	add_conf_item("general", "stats_h_oper_only", CF_YESNO, conf_set_general_stats_h_oper_only);
	add_conf_item("general", "stats_P_oper_only", CF_YESNO, conf_set_general_stats_P_oper_only);
	add_conf_item("general", "stats_o_oper_only", CF_YESNO, conf_set_general_stats_o_oper_only);
	add_conf_item("general", "stats_k_oper_only", CF_STRING,
		      conf_set_general_stats_k_oper_only);
	add_conf_item("general", "stats_i_oper_only", CF_STRING,
		      conf_set_general_stats_i_oper_only);
	add_conf_item("general", "map_oper_only", CF_YESNO, conf_set_general_map_oper_only);
	add_conf_item("general", "operspy_admin_only", CF_YESNO, conf_set_general_operspy_admin_only);
	add_conf_item("general", "pace_wait", CF_TIME, conf_set_general_pace_wait);
	add_conf_item("general", "pace_wait_simple", CF_TIME, conf_set_general_pace_wait_simple);
	add_conf_item("general", "short_motd", CF_YESNO, conf_set_general_short_motd);
	add_conf_item("general", "no_oper_flood", CF_YESNO, conf_set_general_no_oper_flood);
	add_conf_item("general", "glines", CF_YESNO, conf_set_general_glines);
	add_conf_item("general", "gline_time", CF_TIME, conf_set_general_gline_time);
	add_conf_item("general", "gline_min_cidr", CF_INT, conf_set_general_gline_min_cidr);
	add_conf_item("general", "gline_min_cidr6", CF_INT, conf_set_general_gline_min_cidr6);
	add_conf_item("general", "hide_error_messages", CF_STRING,
		      conf_set_general_hide_error_messages);
	add_conf_item("general", "idletime", CF_TIME, conf_set_general_idletime);
	add_conf_item("general", "client_exit", CF_YESNO, conf_set_general_client_exit);
	add_conf_item("general", "oper_only_umodes", CF_STRING | CF_FLIST,
		      conf_set_general_oper_only_umodes);
	add_conf_item("general", "max_targets", CF_INT, conf_set_general_max_targets);
	add_conf_item("general", "use_egd", CF_YESNO, conf_set_general_use_egd);
	add_conf_item("general", "egdpool_path", CF_QSTRING, conf_set_general_egdpool_path);
	add_conf_item("general", "oper_umodes", CF_STRING | CF_FLIST, conf_set_general_oper_umodes);
	add_conf_item("general", "caller_id_wait", CF_TIME, conf_set_general_caller_id_wait);
	add_conf_item("general", "default_floodcount", CF_INT, conf_set_general_default_floodcount);
	add_conf_item("general", "min_nonwildcard", CF_INT, conf_set_general_min_nonwildcard);
	add_conf_item("general", "min_nonwildcard_simple", CF_INT,
		      conf_set_general_min_nonwildcard_simple);
	add_conf_item("general", "servlink_path", CF_QSTRING, conf_set_general_servlink_path);
	add_conf_item("general", "tkline_expire_notices", CF_YESNO,
		      conf_set_general_tkline_expire_notices);
	add_conf_item("general", "use_whois_actually", CF_YESNO,
		      conf_set_general_use_whois_actually);
	add_conf_item("general", "compression_level", CF_INT, conf_set_general_compression_level);
	add_conf_item("general", "client_flood", CF_INT, conf_set_general_client_flood);
	add_conf_item("general", "havent_read_conf", CF_YESNO, conf_set_general_havent_read_conf);
	add_conf_item("general", "dot_in_ip6_addr", CF_YESNO, conf_set_general_dot_in_ip6_addr);
	add_conf_item("general", "ping_cookie", CF_YESNO, conf_set_general_ping_cookie);
	add_conf_item("general", "disable_auth", CF_YESNO, conf_set_general_disable_auth);
	add_conf_item("general", "connect_timeout", CF_TIME, conf_set_general_connect_timeout);
	add_conf_item("general", "burst_away", CF_YESNO, conf_set_general_burst_away);
	add_conf_item("general", "htm_messages", CF_YESNO, conf_set_general_htm_messages);
	add_conf_item("general", "htm_interval", CF_TIME, conf_set_general_htm_interval);
	add_conf_item("general", "htm_trigger", CF_INT, conf_set_general_htm_trigger);
	add_conf_item("general", "reject_ban_time", CF_TIME, conf_set_general_reject_ban_time);
	add_conf_item("general", "reject_after_count", CF_INT, conf_set_general_reject_after_count);
	add_conf_item("general", "reject_duration", CF_TIME, conf_set_general_reject_duration);

	
#ifdef IPV6
	add_conf_item("general", "fallback_to_ip6_int", CF_YESNO,
		      conf_set_general_fallback_to_ip6_int);
#endif
	add_top_conf("channel", NULL, NULL);
	add_conf_item("channel", "use_except", CF_YESNO, conf_set_channel_use_except);
	add_conf_item("channel", "use_invex", CF_YESNO, conf_set_channel_use_invex);
	add_conf_item("channel", "use_knock", CF_YESNO, conf_set_channel_use_knock);
	add_conf_item("channel", "max_bans", CF_INT, conf_set_channel_max_bans);
	add_conf_item("channel", "knock_delay", CF_TIME, conf_set_channel_knock_delay);
	add_conf_item("channel", "knock_delay_channel", CF_TIME,
		      conf_set_channel_knock_delay_channel);
	add_conf_item("channel", "max_chans_per_user", CF_INT, conf_set_channel_max_chans_per_user);
	add_conf_item("channel", "quiet_on_ban", CF_YESNO, conf_set_channel_quiet_on_ban);
	add_conf_item("channel", "default_split_user_count", CF_INT,
		      conf_set_channel_default_split_user_count);
	add_conf_item("channel", "default_split_server_count", CF_INT,
		      conf_set_channel_default_split_server_count);
	add_conf_item("channel", "default_split_delay", CF_TIME,
		      conf_set_channel_default_split_delay);
	add_conf_item("channel", "no_create_on_split", CF_YESNO,
		      conf_set_channel_no_create_on_split);
	add_conf_item("channel", "no_join_on_split", CF_YESNO, conf_set_channel_no_join_on_split);
	add_conf_item("channel", "no_oper_resvs", CF_YESNO, conf_set_channel_no_oper_resvs);
	add_conf_item("channel", "burst_topicwho", CF_YESNO, conf_set_channel_burst_topicwho);

	add_top_conf("serverhide", NULL, NULL);
	add_conf_item("serverhide", "flatten_links", CF_YESNO, conf_set_serverhide_flatten_links);
	add_conf_item("serverhide", "links_delay", CF_TIME, conf_set_serverhide_links_delay);
	add_conf_item("serverhide", "disable_hidden", CF_YESNO, conf_set_serverhide_disable_hidden);
	add_conf_item("serverhide", "hidden", CF_YESNO, conf_set_serverhide_hidden);
}
