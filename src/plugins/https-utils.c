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

#include "https-utils.h"

static bool ssl_allow_insecure = false;

/* Caller must free lang->locale if not NULL */
void get_language(struct language *lang)
{
    lang->locale = NULL;
    lang->encoding = NULL;

    char *locale = setlocale(LC_ALL, NULL);
    if (!locale)
        return;

    lang->locale = xstrdup(locale);
    lang->encoding = strchr(lang->locale, '.');

    if (!lang->encoding)
        return;

    *lang->encoding = '\0';
    ++lang->encoding;
}

void alert_server_error()
{
    alert(_("An error occurred on the server side. Try again later."));
}

void alert_connection_error()
{
    alert(_("An error occurred while connecting to the server. "
            "Check your network connection and try again."));
}

static SECStatus ssl_bad_cert_handler(void *arg, PRFileDesc *sock)
{
    PRErrorCode err = PR_GetError();
    CERTCertificate *cert = SSL_PeerCertificate(sock);
    char *subject = CERT_NameToAscii(&cert->subject);
    char *subject_cn = CERT_GetCommonName(&cert->subject);
    char *issuer = CERT_NameToAscii(&cert->issuer);
    CERT_DestroyCertificate(cert);
    char *target_host = SSL_RevealURL(sock);
    if (!target_host)
        target_host = xstrdup("(unknown)");
    switch (err)
    {
    case SEC_ERROR_CA_CERT_INVALID:
        error_msg(_("Issuer certificate is invalid: '%s'."), issuer);
        break;
    case SEC_ERROR_UNTRUSTED_ISSUER:
        error_msg(_("Certificate is signed by an untrusted issuer: '%s'."), issuer);
        break;
    case SSL_ERROR_BAD_CERT_DOMAIN:
        error_msg(_("Certificate subject name '%s' does not match target host name '%s'."),
                subject_cn, target_host);
        break;
    case SEC_ERROR_EXPIRED_CERTIFICATE:
        error_msg(_("Remote certificate has expired."));
        break;
    case SEC_ERROR_UNKNOWN_ISSUER:
        error_msg(_("Certificate issuer is not recognized: '%s'."), issuer);
        break;
    default:
        error_msg(_("Bad certificate received. Subject '%s', issuer '%s'."),
                subject, issuer);
        break;
    }
    PR_Free(target_host);
    return ssl_allow_insecure ? SECSuccess : SECFailure;
}

static SECStatus ssl_handshake_callback(PRFileDesc *sock, void *arg)
{
    return SECSuccess;
}

static const char *ssl_get_configdir()
{
    struct stat buf;
    if (getenv("SSL_DIR"))
    {
        if (0 == stat(getenv("SSL_DIR"), &buf) &&
            S_ISDIR(buf.st_mode))
        {
            return getenv("SSL_DIR");
        }
    }
    if (0 == stat("/etc/pki/nssdb", &buf) &&
        S_ISDIR(buf.st_mode))
    {
        return "/etc/pki/nssdb";
    }
    return NULL;
}

static PK11GenericObject *nss_load_cacert(const char *filename)
{
    PK11SlotInfo *slot = PK11_FindSlotByName("PEM Token #0");
    if (!slot)
        error_msg_and_die(_("Failed to get slot 'PEM Token #0': %d."), PORT_GetError());

    CK_ATTRIBUTE template[4];
    CK_OBJECT_CLASS class = CKO_CERTIFICATE;

#define PK11_SETATTRS(x,id,v,l) \
    do {                        \
        (x)->type = (id);       \
        (x)->pValue=(v);        \
        (x)->ulValueLen = (l);  \
    } while (0)

    PK11_SETATTRS(&template[0], CKA_CLASS, &class, sizeof(class));
    CK_BBOOL cktrue = CK_TRUE;
    PK11_SETATTRS(&template[1], CKA_TOKEN, &cktrue, sizeof(CK_BBOOL));
    PK11_SETATTRS(&template[2], CKA_LABEL, (unsigned char*)filename, strlen(filename)+1);
    PK11_SETATTRS(&template[3], CKA_TRUST, &cktrue, sizeof(CK_BBOOL));
    PK11GenericObject *cert = PK11_CreateGenericObject(slot, template, 4, PR_FALSE);
    PK11_FreeSlot(slot);
    return cert;
}

static char *ssl_get_password(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    return NULL;
}

void ssl_connect(struct https_cfg *cfg, PRFileDesc **tcp_sock, PRFileDesc **ssl_sock)
{
    PRAddrInfo *addrinfo = PR_GetAddrInfoByName(cfg->url, PR_AF_UNSPEC, PR_AI_ADDRCONFIG);
    if (!addrinfo)
    {
        alert_connection_error();
        error_msg_and_die(_("Failed to get host by name: NSS error %d."), PR_GetError());
    }

    void *enumptr = NULL;
    PRNetAddr addr;
    *tcp_sock = NULL;
    ssl_allow_insecure = cfg->ssl_allow_insecure;

    while ((enumptr = PR_EnumerateAddrInfo(enumptr, addrinfo, cfg->port, &addr)))
    {
        if (addr.raw.family == PR_AF_INET || addr.raw.family == PR_AF_INET6)
        {
            *tcp_sock = PR_OpenTCPSocket(addr.raw.family);
            break;
        }
    }

    PR_FreeAddrInfo(addrinfo);

    if (!*tcp_sock)
        error_msg_and_die(_("Failed to create a TCP socket"));
    PRSocketOptionData sock_option;
    sock_option.option  = PR_SockOpt_Nonblocking;
    sock_option.value.non_blocking = PR_FALSE;
    PRStatus pr_status = PR_SetSocketOption(*tcp_sock, &sock_option);
    if (PR_SUCCESS != pr_status)
    {
        PR_Close(*tcp_sock);
        error_msg_and_die(_("Failed to set socket blocking mode."));
    }
    *ssl_sock = SSL_ImportFD(NULL, *tcp_sock);
    if (!*ssl_sock)
    {
        PR_Close(*tcp_sock);
        error_msg_and_die(_("Failed to wrap TCP socket by SSL."));
    }
    SECStatus sec_status = SSL_OptionSet(*ssl_sock, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die(_("Failed to enable client handshake to SSL socket."));
    }

    if (SECSuccess != SSL_OptionSet(*ssl_sock, SSL_ENABLE_SSL2, PR_TRUE))
        error_msg_and_die(_("Failed to enable client handshake to SSL socket."));
    if (SECSuccess != SSL_OptionSet(*ssl_sock, SSL_ENABLE_SSL3, PR_TRUE))
        error_msg_and_die(_("Failed to enable client handshake to SSL socket."));
    if (SECSuccess != SSL_OptionSet(*ssl_sock, SSL_ENABLE_TLS, PR_TRUE))
        error_msg_and_die(_("Failed to enable client handshake to SSL socket."));

    sec_status = SSL_SetURL(*ssl_sock, cfg->url);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die(_("Failed to set URL to SSL socket."));
    }
    pr_status = PR_Connect(*ssl_sock, &addr, PR_INTERVAL_NO_TIMEOUT);
    if (PR_SUCCESS != pr_status)
    {
        PR_Close(*ssl_sock);
        alert_connection_error();
        error_msg_and_die(_("Failed to connect SSL address."));
    }
    if (SECSuccess != SSL_BadCertHook(*ssl_sock,
                                      (SSLBadCertHandler)ssl_bad_cert_handler,
                                      NULL))
    {
        PR_Close(*ssl_sock);
        error_msg_and_die(_("Failed to set certificate hook."));
    }
    if (SECSuccess != SSL_HandshakeCallback(*ssl_sock,
                                            (SSLHandshakeCallback)ssl_handshake_callback,
                                            NULL))
    {
        PR_Close(*ssl_sock);
        error_msg_and_die(_("Failed to set handshake callback."));
    }
    sec_status = SSL_ResetHandshake(*ssl_sock, /*asServer:*/PR_FALSE);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        alert_server_error();
        error_msg_and_die(_("Failed to reset handshake."));
    }
    sec_status = SSL_ForceHandshake(*ssl_sock);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        alert_server_error();
        error_msg_and_die(_("Failed to force handshake: NSS error %d."),
                          PR_GetError());
    }
}

void ssl_disconnect(PRFileDesc *ssl_sock)
{
    PRStatus pr_status = PR_Close(ssl_sock);
    if (PR_SUCCESS != pr_status)
        error_msg(_("Failed to close SSL socket."));
}

/**
 * Parse a header's value from HTTP message. Only alnum values are supported.
 * @returns
 * Caller must free the returned value.
 * If no header is found, NULL is returned.
 */
char *http_get_header_value(const char *message,
                            const char *header_name)
{
    char *headers_end = strstr(message, "\r\n\r\n");
    if (!headers_end)
        return NULL;
    char *search_string = xasprintf("\r\n%s:", header_name);
    char *header = strcasestr(message, search_string);
    if (!header || header > headers_end)
    {
        free(search_string);
        return NULL;
    }
    header += strlen(search_string);
    free(search_string);
    while (*header == ' ')
        ++header;
    int len = 0;
    while (header[len] && header[len] != '\r' && header[len] != '\n')
        ++len;
    while (header[len - 1] == ' ') /* strip spaces from right */
        --len;
    return xstrndup(header, len);
}

/**
 * Parse body from HTTP message.
 * Caller must free the returned value.
 */
char *http_get_body(const char *message)
{
    char *body = strstr(message, "\r\n\r\n");
    if (!body)
        return NULL;

    body += strlen("\r\n\r\n");
    strtrimch(body, ' ');
    return xstrdup(body);
}

int http_get_response_code(const char *message)
{
    if (0 != strncmp(message, "HTTP/", strlen("HTTP/")))
    {
        alert_server_error();
        error_msg_and_die(_("Invalid response from server: HTTP header not found."));
    }
    char *space = strstr(message, " ");
    if (!space)
    {
        alert_server_error();
        error_msg_and_die(_("Invalid response from server: HTTP header not found."));
    }
    int response_code;
    if (1 != sscanf(space + 1, "%d", &response_code))
    {
        alert_server_error();
        error_msg_and_die(_("Invalid response from server: HTTP header not found."));
    }
    return response_code;
}

void http_print_headers(FILE *file, const char *message)
{
    const char *headers_end = strstr(message, "\r\n\r\n");
    const char *c;
    if (!headers_end)
        headers_end = message + strlen(message);
    for (c = message; c != headers_end + 2; ++c)
    {
        if (*c == '\r')
            continue;
        putc(*c, file);
    }
}

/**
 * @returns
 * Caller must free the returned value.
 */
char *tcp_read_response(PRFileDesc *tcp_sock)
{
    struct strbuf *strbuf = strbuf_new();
    char buf[32768];
    PRInt32 received = 0;
    do {
        received = PR_Recv(tcp_sock, buf, sizeof(buf) - 1, /*flags:*/0,
                           PR_INTERVAL_NO_TIMEOUT);
        if (received > 0)
        {
            buf[received] = '\0';
            strbuf_append_str(strbuf, buf);
        }
        if (received == -1)
        {
            alert_connection_error();
            error_msg_and_die(_("Receiving of data failed: NSS error %d."),
                              PR_GetError());
        }
    } while (received > 0);
    return strbuf_free_nobuf(strbuf);
}

/**
 * Joins HTTP response body if the Transfer-Encoding is chunked.
 * @param body raw HTTP response body (response without headers)
 *             the function operates on the input, but returns it
 *             to the initial state when done
 * @returns Joined HTTP response body. Caller must free the value.
*/
char *http_join_chunked(char *body, int bodylen)
{
    struct strbuf *result = strbuf_new();
    unsigned len;
    int blen = bodylen > 0 ? bodylen : strlen(body);
    char prevchar;
    char *cursor = body;
    while (cursor - body < blen)
    {
        if (sscanf(cursor, "%x", &len) != 1)
            break;

        /* jump to next line */
        cursor = strchr(cursor, '\n');
        if (!cursor)
            error_msg_and_die(_("Malformed chunked response."));
        ++cursor;

        /* split chunk and append to result */
        prevchar = cursor[len];
        cursor[len] = '\0';
        strbuf_append_str(result, cursor);
        cursor[len] = prevchar;

        /* len + strlen("\r\n") */
        cursor += len + 2;
    }

    return strbuf_free_nobuf(result);
}

void nss_init(SECMODModule **mod, PK11GenericObject **cert)
{
    SECStatus sec_status;
    const char *configdir = ssl_get_configdir();
    if (configdir)
        sec_status = NSS_Initialize(configdir, "", "", "", NSS_INIT_READONLY);
    else
        sec_status = NSS_NoDB_Init(NULL);
    if (SECSuccess != sec_status)
        error_msg_and_die(_("Failed to initialize NSS."));

    char *user_module = xstrdup("library=libnsspem.so name=PEM");
    *mod = SECMOD_LoadUserModule(user_module, NULL, PR_FALSE);
    free(user_module);
    if (!*mod || !(*mod)->loaded)
        error_msg_and_die(_("Failed to initialize security module."));

    *cert = nss_load_cacert("/etc/pki/tls/certs/ca-bundle.crt");
    PK11_SetPasswordFunc(ssl_get_password);
    NSS_SetDomesticPolicy();
}

void nss_close(SECMODModule *mod, PK11GenericObject *cert)
{
    SSL_ClearSessionCache();
    PK11_DestroyGenericObject(cert);
    SECMOD_UnloadUserModule(mod);
    SECMOD_DestroyModule(mod);
    SECStatus sec_status = NSS_Shutdown();
    if (SECSuccess != sec_status)
        error_msg(_("Failed to shutdown NSS."));

    PR_Cleanup();
}
