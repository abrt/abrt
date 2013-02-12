/*
    Copyright (C) 2012  ABRT Team
    Copyright (C) 2012  Red Hat, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef ABRT_HTTPS_UTILS_H_
#define ABRT_HTTPS_UTILS_H_

#include <libreport/client.h>
#include <nspr.h>
#include <nss.h>
#include <pk11pub.h>
#include <ssl.h>
#include <sslproto.h>
#include <sslerr.h>
#include <secerr.h>
#include <secmod.h>
#include "libabrt.h"

#if HAVE_LOCALE_H
#include <locale.h>
#endif

struct language
{
    char *locale;
    char *encoding;
};

struct https_cfg
{
    const char *url;
    unsigned port;
    bool ssl_allow_insecure;
};

void get_language(struct language *lang);
void alert_server_error(const char *peer_name);
void alert_connection_error(const char *peer_name);
void ssl_connect(struct https_cfg *cfg, PRFileDesc **tcp_sock, PRFileDesc **ssl_sock);
void ssl_disconnect(PRFileDesc *ssl_sock);
char *http_get_header_value(const char *message, const char *header_name);
char *http_get_body(const char *message);
int http_get_response_code(const char *message);
void http_print_headers(FILE *file, const char *message);
char *tcp_read_response(PRFileDesc *tcp_sock);
char *http_join_chunked(char *body, int bodylen);
void nss_init(SECMODModule **mod, PK11GenericObject **cert);
void nss_close(SECMODModule *mod, PK11GenericObject *cert);

#endif
