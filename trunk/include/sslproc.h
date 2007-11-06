/*
 *  sslproc.h: An interface to the ratbox ssld helper daemon
 *  Copyright (C) 2007 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2007 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: dns.c 24244 2007-08-22 19:04:55Z androsyn $
 */

struct _ssl_ctl;
typedef struct _ssl_ctl ssl_ctl_t;

int start_ssldaemon(int count, const char *ssl_cert, const char *ssl_private_key, const char *ssl_dh_params);
ssl_ctl_t *start_ssld_accept(rb_fde_t *sslF, rb_fde_t *plainF);
ssl_ctl_t * start_ssld_connect(rb_fde_t *sslF, rb_fde_t *plainF);
void start_zlib_session(struct Client *server);
void send_new_ssl_certs(const char *ssl_cert, const char *ssl_private_key, const char *ssl_dh_params);
