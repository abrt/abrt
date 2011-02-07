/*
    Copyright (C) 2010  Red Hat, Inc.

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
#include "parse_options.h"
#include "strbuf.h"
#include <nspr.h>
#include <nss.h>
#include <pk11pub.h>
#include <ssl.h>
#include <sslproto.h>
#include <sslerr.h>
#include <secerr.h>

static const char *dump_dir_name = NULL;
static const char *coredump = NULL;
static const char *task_id = NULL;
static const char *task_password = NULL;
static const char abrt_retrace_client_usage[] = "abrt-retrace-client <operation> [options]\nOperations: create/status/backtrace/log";

enum {
    OPT_verbose   = 1 << 0,
    OPT_syslog    = 1 << 1,
    OPT_insecure  = 1 << 2,
    OPT_dir       = 1 << 3,
    OPT_core      = 1 << 4,
    OPT_wait      = 1 << 5,
    OPT_bttodir   = 1 << 6,
    OPT_no_unlink = 1 << 7,
    OPT_task      = 1 << 8,
    OPT_password  = 1 << 9
};

/* Keep enum above and order of options below in sync! */
static struct options abrt_retrace_client_options[] = {
    OPT__VERBOSE(&g_verbose),
    OPT_BOOL(   's', "syslog", NULL, "log to syslog"),
    OPT_BOOL(   'k', "insecure", NULL, "allow insecure connection to retrace server"),
    OPT_GROUP("For create operation"),
    OPT_STRING( 'd', "dir", &dump_dir_name, "DIR", "read data from ABRT crash dump directory"),
    OPT_STRING( 'c', "core", &coredump, "COREDUMP", "read data from coredump"),
    OPT_BOOL(   'w', "wait", NULL, "keep connected to the server, display progress and then backtrace"),
    OPT_BOOL(   'r', "bttodir", NULL, "if both --dir and --wait are provided, store the backtrace to the DIR when it becomes available"),
    OPT_BOOL(   'u', "no-unlink", NULL, "(debug) do not delete temporary archive created from dump dir in /tmp"),
    OPT_GROUP("For status, backtrace, and log operations"),
    OPT_STRING( 't', "task", &task_id, "ID", "id of your task on server"),
    OPT_STRING( 'p', "password", &task_password, "PWD", "password of your task on server"),
    OPT_END()
};

/* Add an entry name to the args array if the entry name exists in a
 * dump directory. The entry is added to argindex offset to the array,
 * and the argindex is then increased.
 */
static void args_add_if_exists(const char *args[],
                               struct dump_dir *dd,
                               const char *name,
                               int *argindex)
{
    if (dd_exist(dd, name))
    {
        args[*argindex] = name;
        *argindex += 1;
    }
}

/* Create an archive with files required for retrace server and return
 * a file descriptor. Returns -1 if it fails.
 */
static int create_archive(const char *dump_dir_name, bool unlink_temp)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return -1;

    /* Open a temporary file. */
    char *filename = xstrdup("/tmp/abrt-retrace-client-archive-XXXXXX.tar.xz");
    int tempfd = mkstemps(filename, /*suffixlen:*/7);
    if (tempfd == -1)
        perror_msg_and_die("Cannot open temporary file");
    if (unlink_temp)
        xunlink(filename);
    free(filename);

    /* Run xz:
     * - xz reads input from a pipe
     * - xz writes output to the temporary file.
     */
    const char *xz_args[4];
    xz_args[0] = "xz";
    xz_args[1] = "-2";
    xz_args[2] = "-";
    xz_args[3] = NULL;

    int tar_xz_pipe[2];
    xpipe(tar_xz_pipe);
    pid_t xz_child = fork();
    if (xz_child == -1)
        perror_msg_and_die("fork");
    else if (xz_child == 0)
    {
        close(tar_xz_pipe[1]);
        xmove_fd(tar_xz_pipe[0], STDIN_FILENO);
        xdup2(tempfd, STDOUT_FILENO);
        execvp(xz_args[0], (char * const*)xz_args);
	perror_msg("Can't execute '%s'", xz_args[0]);
    }

    close(tar_xz_pipe[0]);

    /* Run tar, and set output to a pipe with xz waiting on the other
     * end.
     */
    const char *tar_args[10];
    tar_args[0] = "tar";
    tar_args[1] = "cO";
    tar_args[2] = xasprintf("--directory=%s", dump_dir_name);
    int argindex = 3;
    tar_args[argindex++] = FILENAME_COREDUMP;
    args_add_if_exists(tar_args, dd, FILENAME_ANALYZER, &argindex);
    args_add_if_exists(tar_args, dd, FILENAME_ARCHITECTURE, &argindex);
    args_add_if_exists(tar_args, dd, FILENAME_EXECUTABLE, &argindex);
    args_add_if_exists(tar_args, dd, FILENAME_PACKAGE, &argindex);
    args_add_if_exists(tar_args, dd, FILENAME_RELEASE, &argindex);
    tar_args[argindex] = NULL;

    dd_close(dd);

    pid_t tar_child = fork();
    if (tar_child == -1)
        perror_msg_and_die("fork");
    else if (tar_child == 0)
    {
        xmove_fd(xopen("/dev/null", O_RDWR), STDIN_FILENO);
        xmove_fd(tar_xz_pipe[1], STDOUT_FILENO);
        execvp(tar_args[0], (char * const*)tar_args);
	perror_msg("Can't execute '%s'", tar_args[0]);
    }

    close(tar_xz_pipe[1]);

    /* Prevent having zombie child process. */
    int status;
    VERB1 log_msg("Waiting for tar...");
    waitpid(tar_child, &status, 0);
    free((void*)tar_args[2]);
    VERB1 log_msg("Waiting for xz...");
    waitpid(xz_child, &status, 0);
    VERB1 log_msg("Done...");

    xlseek(tempfd, 0, SEEK_SET);
    return tempfd;
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
    bool allow_insecure = (bool)arg;

    switch (err)
    {
    case SEC_ERROR_CA_CERT_INVALID:
        error_msg("Issuer certificate is invalid: '%s'.", issuer);
        break;
    case SEC_ERROR_UNTRUSTED_ISSUER:
        error_msg("Certificate is signed by an untrusted issuer: '%s'.", issuer);
        break;
    case SSL_ERROR_BAD_CERT_DOMAIN:
        error_msg("Certificate subject name '%s' does not match target host name '%s'.",
                subject_cn, target_host);
        break;
    case SEC_ERROR_EXPIRED_CERTIFICATE:
        error_msg("Remote certificate has expired.");
        break;
    case SEC_ERROR_UNKNOWN_ISSUER:
        error_msg("Certificate issuer is not recognized: '%s'", issuer);
        break;
    default:
        error_msg("Bad certifiacte received. Subject '%s', issuer '%s'.",
                subject, issuer);
        break;
    }


    PR_Free(target_host);
    return allow_insecure ? SECSuccess : SECFailure;
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

static char *ssl_get_password(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    return NULL;
}

static void ssl_connect(const char *host,
                        PRFileDesc **tcp_sock,
                        PRFileDesc **ssl_sock,
                        bool allow_insecure)
{
    const char *configdir = ssl_get_configdir();
    if (configdir)
        NSS_Initialize(configdir, "", "", "", NSS_INIT_READONLY);
    else
        NSS_NoDB_Init(NULL);

    PK11_SetPasswordFunc(ssl_get_password);
    NSS_SetDomesticPolicy();

    *tcp_sock = PR_NewTCPSocket();
    if (!*tcp_sock)
        error_msg_and_die("Failed to create a TCP socket");

    PRSocketOptionData sock_option;
    sock_option.option  = PR_SockOpt_Nonblocking;
    sock_option.value.non_blocking = PR_FALSE;
    PRStatus pr_status = PR_SetSocketOption(*tcp_sock, &sock_option);
    if (PR_SUCCESS != pr_status)
    {
        PR_Close(*tcp_sock);
        error_msg_and_die("Failed to set socket blocking mode.");
    }

    *ssl_sock = SSL_ImportFD(NULL, *tcp_sock);
    if (!*ssl_sock)
    {
        PR_Close(*tcp_sock);
        error_msg_and_die("Failed to wrap TCP socket by SSL");
    }

    SECStatus sec_status = SSL_OptionSet(*ssl_sock, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to enable client handshake to SSL socket.");
    }

    if (SECSuccess != SSL_OptionSet(*ssl_sock, SSL_ENABLE_SSL2, PR_TRUE))
        error_msg_and_die("Failed to enable client handshake to SSL socket.");
    if (SECSuccess != SSL_OptionSet(*ssl_sock, SSL_ENABLE_SSL3, PR_TRUE))
        error_msg_and_die("Failed to enable client handshake to SSL socket.");
    if (SECSuccess != SSL_OptionSet(*ssl_sock, SSL_ENABLE_TLS, PR_TRUE))
        error_msg_and_die("Failed to enable client handshake to SSL socket.");

    sec_status = SSL_SetURL(*ssl_sock, host);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to set URL to SSL socket.");
    }

    char buffer[PR_NETDB_BUF_SIZE];
    PRHostEnt host_entry;
    pr_status = PR_GetHostByName(host, buffer, sizeof(buffer), &host_entry);
    if (PR_SUCCESS != pr_status)
    {
        char *error = xmalloc(PR_GetErrorTextLength());
        PRInt32 count = PR_GetErrorText(error);
        PR_Close(*ssl_sock);
        if (count)
            error_msg_and_die("Failed to get host by name: %s", error);
        else
            error_msg_and_die("Failed to get host by name: pr_status == %d, pr_error == %d", pr_status, PR_GetError());
    }

    PRNetAddr addr;
    PRInt32 rv = PR_EnumerateHostEnt(0, &host_entry, /*port:*/443, &addr);
    if (rv < 0)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to enumerate host ent.");
    }

    pr_status = PR_Connect(*ssl_sock, &addr, PR_INTERVAL_NO_TIMEOUT);
    if (PR_SUCCESS != pr_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to connect SSL address.");
    }

    if (SECSuccess != SSL_BadCertHook(*ssl_sock,
                                      (SSLBadCertHandler)ssl_bad_cert_handler,
                                      (void*)allow_insecure))
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to set certificate hook.");
    }

    if (SECSuccess != SSL_HandshakeCallback(*ssl_sock, (SSLHandshakeCallback)ssl_handshake_callback, NULL))
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to set handshake callback.");
    }

    sec_status = SSL_ResetHandshake(*ssl_sock, /*asServer:*/PR_FALSE);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to reset handshake.");
    }

    sec_status = SSL_ForceHandshake(*ssl_sock);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to force handshake: NSS error %d",
                          PR_GetError());
    }
}

static void ssl_disconnect(PRFileDesc *ssl_sock)
{
    PRStatus pr_status = PR_Close(ssl_sock);
    if (PR_SUCCESS != pr_status)
        error_msg("Failed to close SSL socket.");

    SSL_ClearSessionCache();

    SECStatus sec_status = NSS_Shutdown();
    if (SECSuccess != sec_status)
        error_msg("Failed to shutdown NSS.");

    PR_Cleanup();
}

/**
 * Parse a header's value from HTTP message. Only alnum values are supported.
 * @returns
 * Caller must free the returned value.
 * If no header is found, NULL is returned.
 */
static char *http_get_header_value(const char *message,
                                   const char *header_name)
{
    char *headers_end = strstr(message, "\r\n\r\n");
    if (!headers_end)
        return NULL;

    char *search_string = xasprintf("\r\n%s:", header_name);
    char *header = strcasestr(message, search_string);
    if (!header || header > headers_end)
        return NULL;

    header += strlen(search_string);
    free(search_string);
    while (*header == ' ')
        ++header;
    int len = 0;
    while (isalnum(header[len]))
        ++len;
    return xstrndup(header, len);
}

/**
 * @returns
 * Caller must free the returned value.
 */
static char *tcp_read_response(PRFileDesc *tcp_sock)
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
            error_msg_and_die("Receiving of data failed: NSS error %d", PR_GetError());

    } while (received > 0);

    return strbuf_free_nobuf(strbuf);
}

static int run_create(const char *dump_dir,
                      const char *coredump,
                      bool wait,
                      bool bttodir,
                      bool ssl_allow_insecure,
                      bool delete_temp_archive)
{
    int tempfd = create_archive(dump_dir_name, delete_temp_archive);
    if (-1 == tempfd)
        return 1;

    /* Get the file size. */
    struct stat tempfd_buf;
    fstat(tempfd, &tempfd_buf);

    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect("retrace01.fedoraproject.org", &tcp_sock, &ssl_sock,
                ssl_allow_insecure);

    /* Upload the archive. */
    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "POST /create HTTP/1.1\r\n"
                       "Host: retrace01.fedoraproject.org\r\n"
                       "Content-Type: application/x-xz-compressed-tar\r\n"
                       "Content-Length: %lld\r\n"
                       "Connection: close\r\n"
                       "\r\n", (long long)tempfd_buf.st_size);

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to send HTTP header of length %d: NSS error %d",
                          http_request->len, PR_GetError());
    }

    strbuf_free(http_request);

    int result = 0;

    while (1)
    {
        char buf[32768];
        int r = read(tempfd, buf, sizeof(buf));
        if (r <= 0)
        {
            if (r == -1)
            {
                if (EINTR == errno || EAGAIN == errno || EWOULDBLOCK == errno)
                    continue;
                perror_msg_and_die("Failed to read from a pipe");
            }
            break;
        }

        written = PR_Send(tcp_sock, buf, r,
                          /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
        if (written == -1)
        {
            /* Print error message, but do not exit.  We need to check
               if the server send some explanation regarding the
               error. */
            result = 1;
            error_msg("Failed to send data: NSS error %d (%s): %s",
                      PR_GetError(),
                      PR_ErrorToName(PR_GetError()),
                      PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
            break;
        }
    }

    close(tempfd);

    /* Read the HTTP header of the response from server. */
    char *http_response = tcp_read_response(tcp_sock);
    char *task_id = http_get_header_value(http_response, "X-Task-Id");
    if (!task_id)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Invalid response from server: missing X-Task-Id");
    }

    char *task_password = http_get_header_value(http_response, "X-Task-Password");
    if (!task_password)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Invalid response from server: missing X-Task-Password");
    }

    printf("Task Id: %s\nTask Password: %s\n", task_id, task_password);
    free(http_response);
    free(task_id);
    free(task_password);

    ssl_disconnect(ssl_sock);
    return result;
}

static int run_status(const char *taskid,
                      const char *password,
                      bool ssl_allow_insecure)
{
    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect("retrace01.fedoraproject.org", &tcp_sock, &ssl_sock,
                ssl_allow_insecure);

    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "GET /%s HTTP/1.1\r\n"
                       "Host: retrace01.fedoraproject.org\r\n"
                       "X-Task-Password: %s\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "\r\n", taskid, password);

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to send HTTP header of length %d: NSS error %d",
                          http_request->len, PR_GetError());
    }

    strbuf_free(http_request);

    char *http_response = tcp_read_response(tcp_sock);
    printf(http_response);
    free(http_response);

    ssl_disconnect(ssl_sock);
    return 0;
}

static int run_backtrace(const char *taskid,
                         const char *password,
                         bool ssl_allow_insecure)
{
    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect("retrace01.fedoraproject.org", &tcp_sock, &ssl_sock,
                ssl_allow_insecure);

    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "GET /%s/backtrace HTTP/1.1\r\n"
                       "Host: retrace01.fedoraproject.org\r\n"
                       "X-Task-Password: %s\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "\r\n", taskid, password);

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to send HTTP header of length %d: NSS error %d",
                          http_request->len, PR_GetError());
    }

    strbuf_free(http_request);

    char *http_response = tcp_read_response(tcp_sock);
    printf(http_response);
    free(http_response);

    ssl_disconnect(ssl_sock);
    return 0;
}

static int run_log(const char *taskid,
                   const char *password,
                   bool ssl_allow_insecure)
{
    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect("retrace01.fedoraproject.org", &tcp_sock, &ssl_sock,
                ssl_allow_insecure);

    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "GET /%s/log HTTP/1.1\r\n"
                       "Host: retrace01.fedoraproject.org\r\n"
                       "X-Task-Password: %s\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "\r\n", taskid, password);

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to send HTTP header of length %d: NSS error %d",
                          http_request->len, PR_GetError());
    }

    strbuf_free(http_request);

    char *http_response = tcp_read_response(tcp_sock);
    printf(http_response);
    free(http_response);

    ssl_disconnect(ssl_sock);
    return 0;
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    unsigned opts = parse_opts(argc, argv,
                               abrt_retrace_client_options,
                               abrt_retrace_client_usage);

    if (opts & OPT_syslog)
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    const char *operation = NULL;
    if (optind < argc)
        operation = argv[optind];
    else
    {
        parse_usage_and_die(abrt_retrace_client_usage,
                            abrt_retrace_client_options);
    }

    if (0 == strcasecmp(operation, "create"))
    {
        if (!dump_dir_name && !coredump)
            error_msg_and_die("Either dump directory or coredump is needed.");
        return run_create(dump_dir_name, coredump, opts & OPT_wait,
                          opts & OPT_bttodir, opts & OPT_insecure,
                          opts & OPT_no_unlink);
    }
    else if (0 == strcasecmp(operation, "status"))
    {
        if (!task_id)
            error_msg_and_die("Task id is needed.");
        if (!task_password)
            error_msg_and_die("Task password is needed.");
        return run_status(task_id, task_password, opts & OPT_insecure);
    }
    else if (0 == strcasecmp(operation, "backtrace"))
    {
        if (!task_id)
            error_msg_and_die("Task id is needed.");
        if (!task_password)
            error_msg_and_die("Task password is needed.");
        return run_backtrace(task_id, task_password, opts & OPT_insecure);
    }
    else if (0 == strcasecmp(operation, "log"))
    {
        if (!task_id)
            error_msg_and_die("Task id is needed.");
        if (!task_password)
            error_msg_and_die("Task password is needed.");
        return run_log(task_id, task_password, opts & OPT_insecure);
    }
    else
        error_msg_and_die("Unknown operation: %s", operation);

    return 0;
}
