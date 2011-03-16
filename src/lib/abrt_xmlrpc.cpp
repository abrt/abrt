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

void throw_xml_fault(xmlrpc_env *env)
{
    error_msg_and_die("XML-RPC Fault(%d): %s", env->fault_code, env->fault_string);
}

void throw_if_xml_fault_occurred(xmlrpc_env *env)
{
    if (env->fault_occurred)
    {
        throw_xml_fault(env);
    }
}

void abrt_xmlrpc_conn::new_xmlrpc_client(const char* url, bool ssl_verify)
{
    m_pClient = NULL;
    m_pServer_info = NULL;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

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

    struct xmlrpc_curl_xportparms curlParms;
    memset(&curlParms, 0, sizeof(curlParms));
    /* curlParms.network_interface = NULL; - done by memset */
    curlParms.no_ssl_verifypeer = !ssl_verify;
    curlParms.no_ssl_verifyhost = !ssl_verify;
#ifdef VERSION
    curlParms.user_agent        = PACKAGE_NAME"/"VERSION;
#else
    curlParms.user_agent        = "abrt";
#endif

    struct xmlrpc_clientparms clientParms;
    memset(&clientParms, 0, sizeof(clientParms));
    clientParms.transport          = "curl";
    clientParms.transportparmsP    = &curlParms;
    clientParms.transportparm_size = XMLRPC_CXPSIZE(user_agent);

    xmlrpc_client_create(&env, XMLRPC_CLIENT_NO_FLAGS,
                        PACKAGE_NAME, VERSION,
                        &clientParms, XMLRPC_CPSIZE(transportparm_size),
                        &m_pClient);
    if (env.fault_occurred)
        throw_xml_fault(&env);

    m_pServer_info = xmlrpc_server_info_new(&env, url);
    if (env.fault_occurred)
    {
        xmlrpc_client_destroy(m_pClient);
        m_pClient = NULL;
        throw_xml_fault(&env);
    }
}

void abrt_xmlrpc_conn::destroy_xmlrpc_client()
{
    if (m_pServer_info)
    {
        xmlrpc_server_info_free(m_pServer_info);
        m_pServer_info = NULL;
    }
    if (m_pClient)
    {
        xmlrpc_client_destroy(m_pClient);
        m_pClient = NULL;
    }
}
