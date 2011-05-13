/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#ifndef ABRT_XMLRPC_H_
#define ABRT_XMLRPC_H_ 1

/* include/stdint.h: typedef int int32_t;
 * include/xmlrpc-c/base.h: typedef int32_t xmlrpc_int32;
 */

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#ifdef __cplusplus
extern "C" {
#endif

struct abrt_xmlrpc {
    xmlrpc_client *ax_client;
    xmlrpc_server_info *ax_server_info;
};

struct abrt_xmlrpc *abrt_xmlrpc_new_client(const char *url, int ssl_verify);
void abrt_xmlrpc_free_client(struct abrt_xmlrpc *ax);
void abrt_xmlrpc_die(xmlrpc_env *env) __attribute__((noreturn));
void abrt_xmlrpc_error(xmlrpc_env *env);

/* die or return expected results */
xmlrpc_value *abrt_xmlrpc_call(struct abrt_xmlrpc *ax,
                               const char *method, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
