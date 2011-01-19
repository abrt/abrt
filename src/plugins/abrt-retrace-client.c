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

static const char *dump_dir_name = ".";
static const char abrt_retrace_client_usage[] = "abrt-retrace-client [options] -d DIR";

enum {
    OPT_v = 1 << 0,
    OPT_d = 1 << 1,
    OPT_s = 1 << 2,
};

/* Keep enum above and order of options below in sync! */
static struct options abrt_retrace_client_options[] = {
    OPT__VERBOSE(&g_verbose),
    OPT_STRING( 'd', NULL, &dump_dir_name, "DIR", "Crash dump directory"),
    OPT_BOOL(   's', NULL, NULL,                  "Log to syslog"),
    OPT_END()
};

/* Add an entry name to the args array if the entry name exists in a
 * dump directory. The entry is added to argindex offset to the array,
 * and the argindex is then increased.
 */
void args_add_if_exists(const char *args[],
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
int create_archive(const char *dump_dir_name)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return -1;

    /* Open a temporary file. */
    char *filename = xstrdup("/tmp/abrt-retrace-client-archive-XXXXXX.tar.xz");
    int tempfd = mkstemps(filename, /*suffixlen:*/7);
    if (tempfd == -1)
        perror_msg_and_die("Cannot open temporary file");
    //xunlink(filename);
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

    return tempfd;
}

void ssl_connect(const char *host, PRFileDesc **tcp_sock, PRFileDesc **ssl_sock)
{
    NSS_Init("/etc/ssl");
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

    sec_status = SSL_OptionSet(*ssl_sock, SSL_ENABLE_FDX, PR_TRUE);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to set full duplex to SSL socket.");
    }

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

    sec_status = SSL_ResetHandshake(*ssl_sock, PR_FALSE);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to reset handshake.");
    }

    sec_status = SSL_ForceHandshake(*ssl_sock);
    if (SECSuccess != sec_status)
    {
        PR_Close(*ssl_sock);
        error_msg_and_die("Failed to force handshake.");
    }
}

void ssl_disconnect(PRFileDesc *ssl_sock)
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

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    unsigned opts = parse_opts(argc, argv, abrt_retrace_client_options,
                               abrt_retrace_client_usage);

    if (opts & OPT_s)
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    int tempfd = create_archive(dump_dir_name);

    /* Get the file size. */
    struct stat tempfd_buf;
    fstat(tempfd, &tempfd_buf);

    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect("retrace01.fedoraproject.org", &tcp_sock, &ssl_sock);

    /* Upload the archive. */
    struct strbuf *request = strbuf_new();
    strbuf_append_strf(request,
                       "POST /create HTTP/1.1\r\n"
                       "Host: retrace01.fedoraproject.org\r\n"
                       "Content-Type: application/x-xz-compressed-tar\r\n"
                       "Content-Length: %lld\r\n"
                       "Connection: close\r\n"
                       "\r\n", (long long)tempfd_buf.st_size);

    PRInt32 written = PR_Send(tcp_sock, request->buf, request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        char *error = xmalloc(PR_GetErrorTextLength());
        PRInt32 count = PR_GetErrorText(error);
        PR_Close(ssl_sock);
        if (count)
            error_msg_and_die("Failed to send HTTP header of length %d: %s", request->len, error);
        else
            error_msg_and_die("Failed to send HTTP header of length %d: pr_error == %d",
                              request->len, PR_GetError());
    }

    xlseek(tempfd, 0, SEEK_SET);

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
            PR_Close(ssl_sock);
            error_msg_and_die("Failed to send data.");
        }
    }

    //PRInt32 received = PR_Recv(tcp_sock, buf, amount, /*flags:*/0,
    //                           PR_INTERVAL_NO_TIMEOUT);

    ssl_disconnect(ssl_sock);
    close(tempfd);
    return 0;
}
