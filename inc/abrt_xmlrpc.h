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

#include <curl/curl.h>
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

/*
 * Simple class holding XMLRPC connection data.
 * Used mainly to ensure we always destroy xmlrpc client and server_info
 * on return or throw.
 */

struct abrt_xmlrpc_conn {
    xmlrpc_client* m_pClient;
    xmlrpc_server_info* m_pServer_info;

    abrt_xmlrpc_conn(const char* url, bool no_ssl_verify) { new_xmlrpc_client(url, no_ssl_verify); }
    /* this never throws exceptions - calls C functions only */
    ~abrt_xmlrpc_conn() { destroy_xmlrpc_client(); }

    void new_xmlrpc_client(const char* url, bool no_ssl_verify);
    void destroy_xmlrpc_client();
};

/* Utility functions */
void throw_xml_fault(xmlrpc_env *env);
void throw_if_xml_fault_occurred(xmlrpc_env *env);

#endif
