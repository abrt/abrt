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

http://www.mail-archive.com/dev-tech-crypto@lists.mozilla.org/msg01921.html
*/
#include "abrtlib.h"
#include "parse_options.h"
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

void add_if_exists(struct dump_dir *dd,
                   const char *name,
                   const char *args[],
                   int *argindex)
{
    if (dd_exist(dd, name))
    {
        args[*argindex] = name;
        *argindex += 1;
    }
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

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    const char *args[10];
    args[0] = "tar";
    args[1] = "cJvO";
    int argindex = 2;
    args[argindex++] = xasprintf("--directory=%s", dump_dir_name);
    args[argindex++] = FILENAME_COREDUMP;
    add_if_exists(dd, FILENAME_ANALYZER, args, &argindex);
    add_if_exists(dd, FILENAME_ARCHITECTURE, args, &argindex);
    add_if_exists(dd, FILENAME_EXECUTABLE, args, &argindex);
    add_if_exists(dd, FILENAME_PACKAGE, args, &argindex);
    add_if_exists(dd, FILENAME_RELEASE, args, &argindex);
    args[argindex] = NULL;

    int tempfd = mkstemp("/tmp/abrt-retrace-client-archive.tar.xz.XXXXXX");
    if (tempfd == -1)
        perror_msg_and_die("Cannot open temporary file");

    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT;
    int pipeout[2];
    pid_t child = fork_execv_on_steroids(flags, (char**)args,
                                         pipeout, NULL, NULL, 0);

    while (1)
    {
        struct pollfd pfd;
        pfd.fd = pipeout[0];
        pfd.events = POLLIN;
        poll(&pfd, 1, 1000);

        char buf[32768];
        int r = read(pipeout[0], buf, sizeof(buf));
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

        int w = 0;
        while (r)
        {
            w += write(tempfd, buf + w, r);
            if (w == -1)
            {
                if (EINTR == errno)
                    continue;

                perror_msg_and_die("Failed to write to a temp file");
            }
            if (w < r)
            {
                r -= w;
                continue;
            }
        }
    }

    close(pipeout[0]);
    close(tempfd);

    /* Prevent having zombie child process, and maybe collect status
     * (note that status == NULL is ok too) */
    int status;
    waitpid(child, &status, 0);
    free((void*)args[2]);

    dd_close(dd);


    /* Upload the archive. */
#define HOST "coding.debuntu.org"
#define PAGE "/"
#define PORT 443
#define USERAGENT "HTMLGET 1.0"

    NSS_Init("path");
    PRFileDesc *tcp_sock = PR_NewTCPSocket();
    if (!sock)
        error_msg_and_die("Failed to create a TCP socket");

    PRSocketOptionData sock_option;
    sock_option.option  = PR_SockOpt_Nonblocking;
    sock_option.value.non_blocking = PR_FALSE;

    PRStatus pr_status = PR_SetSocketOption(tcp_sock, &sock_option);
    if (PR_SUCCESS != pr_status)
    {
        PR_Close(tcp_sock);
        error_msg_and_die("Failed to set socket blocking mode.");
    }

    PRFileDesc *ssl_sock = SSL_ImportFD(NULL, tcp_sock);
    if (!ssl_sock)
    {
        PR_Close(tcp_sock);
        error_msg_and_die("Failed to wrap TCP socket by SSL");
    }

    SECStatus sec_status = SSL_OptionSet(ssl_sock, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
    if (SECSuccess != sec_status)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to enable client handshake to SSL socket.");
    }

    sec_status = SSL_OptionSet(ssl_sock, SSL_ENABLE_FDX, PR_TRUE);
    if (SECSuccess != sec_status)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to set full duplex to SSL socket.");
    }

    sec_status = SSL_SetURL(ssl_sock, HOST);
    if (SECSuccess != sec_status)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to set URL to SSL socket.");
    }

    char buffer[256];
    PRHostEnt host_entry;
    pr_status = PR_GetHostByName(HOST, buffer, sizeof(buffer), &host_entry);
    if (PR_SUCCESS != pr_status)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to get host by name.");
    }

    PRNetAddr addr;
    PRInt32 rv = PR_EnumerateHostEnt(0, &host_entry, PORT, &addr);
    if (rv < 0)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to enumerate host ent.");
    }

    pr_status = PR_Connect(ssl_sock, &addr, PR_INTERVAL_NO_TIMEOUT);
    if (PR_SUCCESS != pr_status)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to connect SSL address");
    }

    sec_status = SSL_ResetHandshake(ssl_sock, PR_FALSE);
    if (SECSuccess != sec_status)
    {
        PR_Close(ssl_sock);
        error_msg_and_die("Failed to reset handshake.");
    }

    pr_status = PR_Close(ssl_sock);
    if (PR_SUCCESS != pr_status)
        error_msg("Failed to close SSL socket.");

    SSL_ClearSessionCache();

    sec_status = NSS_Shutdown();
    if (SECSuccess != sec_status)
        error_msg("Failed to shutdown NSS.");

    PR_Cleanup();

    return 0;
}
