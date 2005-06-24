/* This code is in the public domain.
 * $Nightmare: nightmare/include/config.h,v 1.32.2.2.2.2 2002/07/02 03:41:28 ejb Exp $
 * $Id$
 */

#ifndef _NEWCONF_H_INCLUDED
#define _NEWCONF_H_INCLUDED

#include <sys/types.h>

#include <stdio.h>

#include "tools.h"
#include "client.h"
#include "tools.h"

struct ConfEntry;

struct TopConf
{
  char *	tc_name;
  int 		(*tc_sfunc) (struct TopConf *);
  int 		(*tc_efunc) (struct TopConf *);
  dlink_list	tc_items;
};

struct ConfItem *yy_achead;
struct ConfItem *yy_aconf;
struct ConfItem *yy_aprev;
int              yy_acount;
struct ConfItem *yy_hconf;
struct ConfItem *yy_lconf;

struct ConfItem *hub_confs;
struct ConfItem *leaf_confs;
struct ConfItem *yy_aconf_next;

struct Class *yy_class;

#define CF_QSTRING	0x01
#define CF_INT		0x02
#define CF_STRING	0x03
#define CF_TIME		0x04
#define CF_YESNO	0x05
#define CF_LIST		0x06
#define CF_ONE		0x07

#define CF_MTYPE	0xFF

#define CF_FLIST	0x1000
#define CF_MFLAG	0xFF00

typedef struct conf_parm_t_stru
{
	struct	conf_parm_t_stru *	next;
	int	type;
	union
	{
		char *	string;
		int	number;
		struct	conf_parm_t_stru *	list;
	} v;
}	conf_parm_t;


struct ConfEntry
{
  char *	cf_name;
  int		cf_type;
  void 		(*cf_func) (void *);
};

extern 		dlink_list 	conf_items;

	int 	read_config		(char *);

	int 	add_top_conf		(char *, int (*)(struct TopConf *),
		 			 int (*)(struct TopConf *));
        int     remove_top_conf(char *);
	int 	add_conf_item		(char *, char *, int, void (*)(void *));
        int     remove_conf_item(char *, char *);
struct 	TopConf *find_top_conf		(char *);
struct 	ConfEntry *find_conf_item	(const struct TopConf*, const char*);
	int 	conf_start_block	(char *, char *);
	int 	conf_end_block		(struct TopConf *);
	int 	conf_call_set		(struct TopConf *, char*, conf_parm_t *, int);
	void	conf_report_error	(char *, ...);
	void	newconf_init		(void);

extern 	struct 	TopConf *conf_cur_block;
extern 	char *		conf_cur_block_name;

	void	conf_set_serverinfo_rsa_private_key_file	(void *data);
	void	conf_set_serverinfo_name			(void *data);
	void	conf_set_serverinfo_description			(void *data);
	void	conf_set_serverinfo_network_name		(void *data);
	void	conf_set_serverinfo_network_desc		(void *data);
	void	conf_set_serverinfo_vhost			(void *data);
	void	conf_set_serverinfo_vhost6			(void *data);
	void	conf_set_serverinfo_max_clients			(void *data);
	void	conf_set_serverinfo_max_buffer			(void *data);
	void	conf_set_serverinfo_hub				(void *data);

	void	conf_set_modules_path				(void *data);
	void	conf_set_modules_module				(void *data);

	void    conf_set_admin_name				(void *data);
	void    conf_set_admin_email				(void *data);
	void    conf_set_admin_description			(void *data);

	void    conf_set_logging_path				(void *data);
	void    conf_set_logging_oper_log			(void *data);
	void    conf_set_logging_gline_log			(void *data);
	void    conf_set_logging_log_level			(void *data);

	int	conf_begin_oper					(struct TopConf *tc);
	int	conf_end_oper					(struct TopConf *tc);

	void    conf_set_oper_name				(void *data);
	void    conf_set_oper_user				(void *data);
	void    conf_set_oper_password				(void *data);	
	void    conf_set_oper_rsa_public_key_file		(void *data);
	void    conf_set_oper_class				(void *data);
	void    conf_set_oper_global_kill			(void *data);
	void    conf_set_oper_remote				(void *data);
	void    conf_set_oper_kline				(void *data);
	void    conf_set_oper_unkline				(void *data);
	void    conf_set_oper_xline				(void *data);
	void    conf_set_oper_gline				(void *data);
	void    conf_set_oper_nick_changes			(void *data);
	void    conf_set_oper_die				(void *data);
	void    conf_set_oper_rehash				(void *data);
	void    conf_set_oper_admin				(void *data);
	void	conf_set_oper_flags				(void *data);

	int	conf_begin_class				(struct TopConf *tc);
	int	conf_end_class					(struct TopConf *tc);

	void	conf_set_class_name				(void *data);
	void	conf_set_class_ping_time			(void *data);
	void	conf_set_class_number_per_ip			(void *data);
	void	conf_set_class_number_per_ip_global		(void *data);
	void	conf_set_class_number_per_ident			(void *data);
	void	conf_set_class_connectfreq			(void *data);
	void	conf_set_class_max_number			(void *data);
	void	conf_set_class_sendq				(void *data);

	int	conf_begin_listen				(struct TopConf *tc);
	int	conf_end_listen					(struct TopConf *tc);

	void	conf_set_listen_port				(void *data);
	void	conf_set_listen_address				(void *data);

	int	conf_begin_auth					(struct TopConf *tc);
	int	conf_end_auth					(struct TopConf *tc);

	void	conf_set_auth_flags				(void *data);
	void	conf_set_auth_user				(void *data);
	void	conf_set_auth_passwd				(void *data);
	void	conf_set_auth_no_spoof_notice			(void *data);
	void	conf_set_auth_spoof				(void *data);
	void	conf_set_auth_exceed_limit			(void *data);
	void	conf_set_auth_is_restricted			(void *data);
	void	conf_set_auth_kline_exempt			(void *data);
	void	conf_set_auth_need_ident			(void *data);
	void	conf_set_auth_no_tilde				(void *data);
	void	conf_set_auth_gline_exempt			(void *data);
	void	conf_set_auth_flood_exempt			(void *data);
	void	conf_set_auth_redir_serv			(void *data);
	void	conf_set_auth_redir_port			(void *data);
	void	conf_set_auth_class				(void *data);

	int	conf_begin_resv					(struct TopConf *tc);
	int	conf_end_resv					(struct TopConf *tc);

	void	conf_set_resv_channel				(void *data);
	void	conf_set_resv_reason				(void *data);
	void	conf_set_resv_nick				(void *data);

	int	conf_begin_shared				(struct TopConf *tc);
	int	conf_end_shared					(struct TopConf *tc);

	void	conf_set_shared_name				(void *data);
	void	conf_set_shared_user				(void *data);
	void	conf_set_shared_kline				(void *data);      
	void	conf_set_shared_unkline				(void *data);

	int	conf_begin_connect				(struct TopConf *tc);
	int	conf_end_connect				(struct TopConf *tc);

	void	conf_set_connect_name				(void *data);
	void	conf_set_connect_host				(void *data);
	void	conf_set_connect_send_password			(void *data);
	void	conf_set_connect_accept_password		(void *data);
	void	conf_set_connect_port				(void *data);
	void	conf_set_connect_aftype				(void *data);
	void	conf_set_connect_fakename			(void *data);
	void	conf_set_connect_encrypted			(void *data);
	void	conf_set_connect_rsa_public_key_file		(void *data);
	void	conf_set_connect_cryptlink			(void *data);
	void	conf_set_connect_compressed			(void *data);	
	void	conf_set_connect_auto				(void *data);
	void	conf_set_connect_hub_mask			(void *data);
	void	conf_set_connect_leaf_mask			(void *data);
	void	conf_set_connect_class				(void *data);
	void	conf_set_connect_cipher_preference		(void *data);

	int	conf_begin_kill					(struct TopConf *tc);
	int	conf_end_kill					(struct TopConf *tc);

	void	conf_set_kill_user				(void *data);
	void	conf_set_kill_reason				(void *data);

	int	conf_begin_deny					(struct TopConf *tc);
	int	conf_end_deny					(struct TopConf *tc);

	void	conf_set_deny_ip				(void *data);
	void	conf_set_deny_reason				(void *data);

	int	conf_begin_exempt				(struct TopConf *tc);
	int	conf_end_exempt					(struct TopConf *tc);

	void	conf_set_exempt_ip				(void *data);

	int	conf_begin_gecos				(struct TopConf *tc);
	int	conf_end_gecos					(struct TopConf *tc);

	void	conf_set_gecos_name				(void *data);
	void	conf_set_gecos_reason				(void *data);
	void	conf_set_gecos_action				(void *data);

	void	conf_set_general_failed_oper_notice		(void *data);
	void	conf_set_general_anti_nick_flood		(void *data);
	void	conf_set_general_max_nick_time			(void *data);
	void	conf_set_general_max_nick_changes		(void *data);
	void	conf_set_general_max_accept			(void *data);
	void	conf_set_general_anti_spam_exit_message_time	(void *data);
	void	conf_set_general_ts_warn_delta			(void *data);
	void	conf_set_general_ts_max_delta			(void *data);
	void	conf_set_general_havent_read_conf		(void *data);
	void	conf_set_general_kline_with_reason		(void *data);
	void	conf_set_general_client_exit			(void *data);
	void	conf_set_general_kline_with_connection_closed	(void *data);
	void	conf_set_general_warn_no_nline			(void *data);
	void	conf_set_general_non_redundant_klines		(void *data);
	void	conf_set_general_stats_o_oper_only		(void *data);
	void	conf_set_general_stats_P_oper_only		(void *data);
	void	conf_set_general_stats_k_oper_only		(void *data);
	void	conf_set_general_stats_i_oper_only		(void *data);
	void	conf_set_general_pace_wait			(void *data);
	void	conf_set_general_caller_id_wait			(void *data);
	void	conf_set_general_pace_wait_simple		(void *data);
	void	conf_set_general_short_motd			(void *data);
	void	conf_set_general_no_oper_flood			(void *data);
	void	conf_set_general_fname_userlog			(void *data);
	void	conf_set_general_fname_foperlog			(void *data);
	void	conf_set_general_fname_operlog			(void *data);
	void	conf_set_general_glines				(void *data);
	void	conf_set_general_message_locale			(void *data);
	void	conf_set_general_gline_time			(void *data);
	void	conf_set_general_idletime			(void *data);
	void	conf_set_general_dots_in_ident			(void *data);
	void	conf_set_general_max_targets			(void *data);
	void	conf_set_general_servlink_path			(void *data);
	void	conf_set_general_default_cipher_preference	(void *data);
	void	conf_set_general_compression_level		(void *data);
	void	conf_set_general_use_egd			(void *data);
	void	conf_set_general_egdpool_path			(void *data);
	void	conf_set_general_ping_cookie			(void *data);
	void	conf_set_general_disable_auth			(void *data);
	void	conf_set_general_use_global_limits		(void *data);
	void	conf_set_general_use_help			(void *data);
        void    conf_set_general_use_whois_actually             (void *data);
	void	conf_set_general_throttle_time			(void *data);
	void	conf_set_general_oper_umodes			(void *data);
	void	conf_set_general_oper_only_umodes		(void *data);
        void    conf_set_general_tkline_expire_notices          (void *data);
	void	conf_set_general_min_nonwildcard		(void *data);
	void	conf_set_general_default_floodcount		(void *data);
	void	conf_set_general_client_flood			(void *data);
	void	conf_set_general_dot_in_ip6_addr		(void *data);
	void	conf_set_general_connect_timeout		(void *data);
	void	conf_set_channel_use_except			(void *data);
	void	conf_set_channel_use_anonops			(void *data);
	void	conf_set_channel_use_invex			(void *data);
	void	conf_set_channel_use_knock			(void *data);
	void	conf_set_channel_knock_delay			(void *data);
	void	conf_set_channel_knock_delay_channel		(void *data);
	void	conf_set_channel_max_chans_per_user		(void *data);
	void	conf_set_channel_quiet_on_ban			(void *data);
	void	conf_set_channel_max_bans			(void *data);
	void	conf_set_channel_persist_time			(void *data);
	void	conf_set_channel_default_split_user_count	(void *data);
	void	conf_set_channel_default_split_server_count	(void *data);
	void	conf_set_channel_no_create_on_split		(void *data);
	void	conf_set_channel_no_join_on_split		(void *data);
	void	conf_set_serverhide_flatten_links		(void *data);
	void	conf_set_serverhide_hide_servers		(void *data);
	void	conf_set_serverhide_disable_remote_commands	(void *data);
	void	conf_set_serverhide_disable_local_channels	(void *data);
	void	conf_set_serverhide_links_delay			(void *data);
	void	conf_set_serverhide_hidden			(void *data);
	void	conf_set_serverhide_disable_hidden		(void *data);
		
#endif
