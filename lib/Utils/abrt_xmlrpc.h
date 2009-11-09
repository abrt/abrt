#ifndef ABRT_XMLRPC_H_
#define ABRT_XMLRPC_H_ 1

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
    ~abrt_xmlrpc_conn() { destroy_xmlrpc_client(); }

    void new_xmlrpc_client(const char* url, bool no_ssl_verify);
    void destroy_xmlrpc_client();
};

/* Utility function */
void throw_if_xml_fault_occurred(xmlrpc_env *env);

#endif
