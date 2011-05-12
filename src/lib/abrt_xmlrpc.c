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
#include "abrtlib.h"
#include "abrt_xmlrpc.h"

void abrt_xmlrpc_die(xmlrpc_env *env)
{
    error_msg_and_die("fatal: XML-RPC(%d): %s", env->fault_code, env->fault_string);
}

void abrt_xmlrpc_error(xmlrpc_env *env)
{
    error_msg("error: XML-RPC (%d): %s", env->fault_code, env->fault_string);
}

struct abrt_xmlrpc *abrt_xmlrpc_new_client(const char *url, int ssl_verify)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    struct abrt_xmlrpc *ax = xzalloc(sizeof(struct abrt_xmlrpc));

    /* This should be done at program startup, once. We do it in main */
    /* xmlrpc_client_setup_global_const(&env); */

    /* URL - bugzilla.redhat.com/show_bug.cgi?id=666893 Unable to make sense of
     * XML-RPC response from server
     *
     * By default, XML data from the network may be no larger than 512K.
     * XMLRPC_XML_SIZE_LIMIT_DEFAULT is #defined to (512*1024) in xmlrpc-c/base.h
     *
     * Users reported trouble with 733402 byte long responses, hope raising the
     * limit to 2*512k is enough
     */
    xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, 2 * XMLRPC_XML_SIZE_LIMIT_DEFAULT);

    struct xmlrpc_curl_xportparms curl_parms;
    memset(&curl_parms, 0, sizeof(curl_parms));
    /* curlParms.network_interface = NULL; - done by memset */
    curl_parms.no_ssl_verifypeer = !ssl_verify;
    curl_parms.no_ssl_verifyhost = !ssl_verify;
#ifdef VERSION
    curl_parms.user_agent        = PACKAGE_NAME"/"VERSION;
#else
    curl_parms.user_agent        = "abrt";
#endif

    struct xmlrpc_clientparms client_parms;
    memset(&client_parms, 0, sizeof(client_parms));
    client_parms.transport          = "curl";
    client_parms.transportparmsP    = &curl_parms;
    client_parms.transportparm_size = XMLRPC_CXPSIZE(user_agent);

    xmlrpc_client_create(&env, XMLRPC_CLIENT_NO_FLAGS,
                         PACKAGE_NAME, VERSION,
                         &client_parms, XMLRPC_CPSIZE(transportparm_size),
                         &ax->ax_client);

    if (env.fault_occurred)
        abrt_xmlrpc_die(&env);

    ax->ax_server_info = xmlrpc_server_info_new(&env, url);
    if (env.fault_occurred)
    {
        xmlrpc_client_destroy(ax->ax_client);
        abrt_xmlrpc_die(&env);
    }

    return ax;
}

void abrt_xmlrpc_free_client(struct abrt_xmlrpc *ax)
{
    if (!ax)
        return;

    if (ax->ax_server_info)
        xmlrpc_server_info_free(ax->ax_server_info);

    if (ax->ax_client)
        xmlrpc_client_destroy(ax->ax_client);

    free(ax);
}

/* die or return expected results */
xmlrpc_value *abrt_xmlrpc_call(struct abrt_xmlrpc *ax,
                               const char* method, const char* format, ...)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = NULL;
    const char* suffix;
    va_list args;

    va_start(args, format);
    xmlrpc_build_value_va(&env, format, args, &param, &suffix);
    va_end(args);
    if (env.fault_occurred)
        abrt_xmlrpc_die(&env);

    xmlrpc_value* result = NULL;
    if (*suffix != '\0')
    {
        xmlrpc_env_set_fault_formatted(
            &env, XMLRPC_INTERNAL_ERROR, "Junk after the argument "
            "specifier: '%s'.  There must be exactly one argument.",
            suffix);
    }
    else
    {
        xmlrpc_client_call2(&env, ax->ax_client, ax->ax_server_info, method,
                            param, &result);
    }
    xmlrpc_DECREF(param);
    if (env.fault_occurred)
        abrt_xmlrpc_die(&env);

    return result;
}
