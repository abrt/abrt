#if HAVE_CONFIG_H
# include "config.h"
#endif
#include "abrtlib.h"
#include "abrt_xmlrpc.h"
#include "ABRTException.h"

void throw_if_xml_fault_occurred(xmlrpc_env *env)
{
    if (env->fault_occurred)
    {
        std::string errmsg = ssprintf("XML-RPC Fault: %s(%d)", env->fault_string, env->fault_code);
        xmlrpc_env_clean(env); // this is needed ONLY if fault_occurred
        xmlrpc_env_init(env); // just in case user catches ex and _continues_ to use env
        error_msg("%s", errmsg.c_str()); // show error in daemon log
        throw CABRTException(EXCEP_PLUGIN, errmsg.c_str());
    }
}

void abrt_xmlrpc_conn::new_xmlrpc_client(const char* url, bool no_ssl_verify)
{
    m_pClient = NULL;
    m_pServer_info = NULL;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    /* This should be done at program startup, once.
     * We do it in abrtd's main */
    /* xmlrpc_client_setup_global_const(&env); */

    struct xmlrpc_curl_xportparms curlParms;
    memset(&curlParms, 0, sizeof(curlParms));
    /* curlParms.network_interface = NULL; - done by memset */
    curlParms.no_ssl_verifypeer = no_ssl_verify;
    curlParms.no_ssl_verifyhost = no_ssl_verify;
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
    throw_if_xml_fault_occurred(&env);

    m_pServer_info = xmlrpc_server_info_new(&env, url);
    if (env.fault_occurred)
    {
        xmlrpc_client_destroy(m_pClient);
        m_pClient = NULL;
    }
    throw_if_xml_fault_occurred(&env);
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
