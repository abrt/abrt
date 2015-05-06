/*
  Copyright (C) 2010  ABRT team

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
#include <syslog.h>
#include "libabrt.h"

/* Maximal length of backtrace. */
#define MAX_BACKTRACE_SIZE (1024*1024)
/* Amount of data received from one client for a message before reporting error. */
#define MAX_MESSAGE_SIZE (4*MAX_BACKTRACE_SIZE)
/* Maximal number of characters read from socket at once. */
#define INPUT_BUFFER_SIZE (8*1024)
/* We exit after this many seconds */
#define TIMEOUT 10


/*
Unix socket in ABRT daemon for creating new dump directories.

Why to use socket for creating dump dirs? Security. When a Python
script throws unexpected exception, ABRT handler catches it, running
as a part of that broken Python application. The application is running
with certain SELinux privileges, for example it can not execute other
programs, or to create files in /var/cache or anything else required
to properly fill a problem directory. Adding these privileges to every
application would weaken the security.
The most suitable solution is for the Python application
to open a socket where ABRT daemon is listening, write all relevant
data to that socket, and close it. ABRT daemon handles the rest.

** Protocol

Initializing new dump:
open /var/run/abrt.socket

Providing dump data (hook writes to the socket):
-> "PID="
   number 0 - PID_MAX (/proc/sys/kernel/pid_max)
   \0
-> "EXECUTABLE="
   string (maximum length ~MAX_PATH)
   \0
-> "BACKTRACE="
   string (maximum length 1 MB)
   \0
-> "ANALYZER="
   string (maximum length 100 bytes)
   \0
-> "BASENAME="
   string (maximum length 100 bytes, no slashes)
   \0
-> "REASON="
   string (maximum length 512 bytes)
   \0

Finalizing dump creation:
-> "DONE"
   \0
*/

static unsigned total_bytes_read = 0;

static uid_t client_uid = (uid_t)-1L;

static int   pid;
static char *executable;
static char *backtrace;
/* "python", "ruby" etc. */
static char *analyzer;
static char *type;
/* Directory base name: "pyhook", "ruby" etc. */
static char *dir_basename;
/* Crash reason.
 * Python example:
 * "CCMainWindow.py:1:<module>:ZeroDivisionError: integer division or modulo by zero"
 */
static char *reason;

/* Create a new debug dump from client session.
 * Caller must ensure that all fields in struct client
 * are properly filled.
 */
static int create_debug_dump()
{
    /* Create temp directory with the debug dump.
       This directory is renamed to final directory name after
       all files have been stored into it.
    */
    char *path = xasprintf("%s/%s-%s-%u.new",
                           g_settings_dump_location,
                           dir_basename,
                           iso_date_string(NULL),
                           pid);
    /* No need to check the path length, as all variables used are limited, and dd_create()
       fails if the path is too long. */

    struct dump_dir *dd = dd_create(path, g_settings_privatereports ? 0 : client_uid, 0640);
    if (!dd)
    {
        error_msg_and_die("Error creating crash dump %s", path);
    }
    dd_create_basic_files(dd, client_uid, NULL);
    dd_save_text(dd, "abrt_version", VERSION);

    dd_save_text(dd, FILENAME_ANALYZER, analyzer);
    dd_save_text(dd, FILENAME_TYPE, type != NULL ? type : analyzer);
    dd_save_text(dd, FILENAME_EXECUTABLE, executable);
    dd_save_text(dd, FILENAME_BACKTRACE, backtrace);
    dd_save_text(dd, FILENAME_REASON, reason);

    /* Obtain and save the command line. */
    char *cmdline = get_cmdline(pid);
    dd_save_text(dd, FILENAME_CMDLINE, cmdline ? : "");
    free(cmdline);

    /* Store id of the user whose application crashed. */
    char uid_str[sizeof(long) * 3 + 2];
    sprintf(uid_str, "%lu", (long)client_uid);
    dd_save_text(dd, FILENAME_UID, uid_str);

    dd_save_text(dd, "abrt_version", VERSION);

    dd_close(dd);

    /* Move the completely created debug dump to
       final directory. */
    char *newpath = xstrndup(path, strlen(path) - strlen(".new"));
    if (rename(path, newpath) == 0)
        strcpy(path, newpath);
    free(newpath);

    log("Saved %s crash dump of pid %u to %s", analyzer, pid, path);

    /* Trim old crash dumps if necessary */
    load_abrt_conf();
    if (g_settings_nMaxCrashReportsSize > 0)
    {
        /* x1.25 and round up to 64m: go a bit up, so that usual in-daemon trimming
         * kicks in first, and we don't "fight" with it:
         */
        unsigned maxsize = g_settings_nMaxCrashReportsSize + g_settings_nMaxCrashReportsSize / 4;
        maxsize |= 63;
        check_free_space(maxsize, g_settings_dump_location);
        trim_problem_dirs(g_settings_dump_location, maxsize * (double)(1024*1024), path);
    }
    free_abrt_conf_data();

    free(path);

    return 201; /* Created */
}

/* Checks if a string has certain prefix. */
static bool starts_with(const char *str, const char *start)
{
    return strncmp(str, start, strlen(start)) == 0;
}

/* Remove dump dir */
static int delete_path(const char *dump_dir_name)
{
    /* If doesn't start with "g_settings_dump_location/"... */
    if (!dir_is_in_dump_location(dump_dir_name))
    {
        /* Then refuse to operate on it (someone is attacking us??) */
        error_msg("Bad problem directory name '%s', should start with: '%s'", dump_dir_name, g_settings_dump_location);
        return 400; /* Bad Request */
    }
    if (!dir_has_correct_permissions(dump_dir_name))
    {
        error_msg("Problem directory '%s' isn't owned by root:abrt or others are not restricted from access", dump_dir_name);
        return 400; /*  */
    }

    int dir_fd = dd_openfd(dump_dir_name);
    if (dir_fd < 0)
    {
        perror_msg("Can't open problem directory '%s'", dump_dir_name);
        return 400;
    }
    if (!fdump_dir_accessible_by_uid(dir_fd, client_uid))
    {
        close(dir_fd);
        if (errno == ENOTDIR)
        {
            error_msg("Path '%s' isn't problem directory", dump_dir_name);
            return 404; /* Not Found */
        }
        error_msg("Problem directory '%s' can't be accessed by user with uid %ld", dump_dir_name, (long)client_uid);
        return 403; /* Forbidden */
    }

    struct dump_dir *dd = dd_fdopendir(dir_fd, dump_dir_name, /*flags:*/ 0);
    if (dd)
    {
        if (dd_delete(dd) != 0)
        {
            error_msg("Failed to delete problem directory '%s'", dump_dir_name);
            dd_close(dd);
            return 400;
        }
    }

    return 0; /* success */
}

/* Checks if a string contains only printable characters. */
static bool printable_str(const char *str)
{
    do {
        if ((unsigned char)(*str) < ' ' || *str == 0x7f)
            return false;
        str++;
    } while (*str);
    return true;
}

/* @returns
 *  Caller is responsible to call free() on the returned
 *  pointer.
 *  If NULL is returned, string extraction failed.
 */
static char *try_to_get_string(const char *message,
                               const char *tag,
                               size_t max_len,
                               bool printable,
                               bool allow_slashes)
{
    if (!starts_with(message, tag))
        return NULL;

    const char *contents = message + strlen(tag);
    if ((printable && !printable_str(contents))
     || (!allow_slashes && strchr(contents, '/'))
    ) {
        error_msg("Received %s contains invalid characters, skipping", tag);
        return NULL;
    }

    if (strlen(contents) > max_len)
    {
        error_msg("Received %s too long, trimming to %lu", tag, (long)max_len);
    }

    return xstrndup(contents, max_len);
}

/* Handles a message received from client over socket. */
static void process_message(const char *message)
{
/* @param tag
 *  The message identifier. Message starting with it
 *  is handled by this macro.
 * @param field
 *  Member in struct client, which should be filled by
 *  the field contents.
 * @param max_len
 *  Maximum length of the field in bytes.
 *  Exceeding bytes are trimmed.
 * @param printable
 *  Whether to limit the field contents to ASCII only.
 * @param allow_slashes
 *  Whether to allow slashes to be a part of input.
 */
#define HANDLE_INCOMING_STRING(tag, field, max_len, printable, allow_slashes) \
{ \
    char *s = try_to_get_string(message, tag, max_len, printable, allow_slashes); \
    if (s) \
    { \
        free(field); \
        field = s; \
        VERB3 log("Saved %s%s", tag, s); \
        return; \
    } \
}

    HANDLE_INCOMING_STRING("EXECUTABLE=", executable, PATH_MAX, true, true);
    HANDLE_INCOMING_STRING("BACKTRACE=", backtrace, MAX_BACKTRACE_SIZE, false, true);
    HANDLE_INCOMING_STRING("BASENAME=", dir_basename, 100, true, false);
    HANDLE_INCOMING_STRING("ANALYZER=", analyzer, 100, true, true);
    HANDLE_INCOMING_STRING("TYPE=", type, 100, true, true);
    HANDLE_INCOMING_STRING("REASON=", reason, 512, false, true);

#undef HANDLE_INCOMING_STRING

    /* PID is not handled as a string, we convert it to pid_t. */
    if (starts_with(message, "PID="))
    {
        pid = xatou(message + strlen("PID="));
        if (pid < 1)
            /* pid == 0 is error, the lowest PID is 1. */
            error_msg_and_die("Malformed or out-of-range number: '%s'", message + strlen("PID="));
        VERB3 log("Saved PID %u", pid);
        return;
    }
}

static int perform_http_xact(void)
{
    /* Read header */
    char *body_start = NULL;
    char *messagebuf_data = NULL;
    unsigned messagebuf_len = 0;
    /* Loop until EOF/error/timeout/end_fo_header */
    while (1)
    {
        messagebuf_data = xrealloc(messagebuf_data, messagebuf_len + INPUT_BUFFER_SIZE);
        char *p = messagebuf_data + messagebuf_len;
        int rd = read(STDIN_FILENO, p, INPUT_BUFFER_SIZE);
        if (rd < 0)
        {
            if (errno == EINTR) /* SIGALRM? */
                error_msg_and_die("Timed out");
            perror_msg_and_die("read");
        }
        if (rd == 0)
            break;

        VERB3 log("Received %u bytes of data", rd);
        messagebuf_len += rd;
        total_bytes_read += rd;
        if (total_bytes_read > MAX_MESSAGE_SIZE)
            error_msg_and_die("Message is too long, aborting");

        /* Check whether we see end of header */
        /* Note: we support both [\r]\n\r\n and \n\n */
        char *past_end = messagebuf_data + messagebuf_len;
        if (p > messagebuf_data+1)
            p -= 2; /* start search from two last bytes in last read - they might be '\n\r' */
        while (p < past_end)
        {
            p = memchr(p, '\n', past_end - p);
            if (!p)
                break;
            p++;
            if (p >= past_end)
                break;
            if (*p == '\n'
             || (*p == '\r' && p+1 < past_end && p[1] == '\n')
            ) {
                body_start = p + 1 + (*p == '\r');
                *p = '\0';
                goto found_end_of_header;
            }
        }
    } /* while (read) */
 found_end_of_header: ;
    VERB3 log("Request: %s", messagebuf_data);

    /* Sanitize and analyze header.
     * Header now is in messagebuf_data, NUL terminated string,
     * with last empty line deleted (by placement of NUL).
     * \r\n are not (yet) converted to \n, multi-line headers also
     * not converted.
     */
    /* First line must be "op<space>[http://host]/path<space>HTTP/n.n".
     * <space> is exactly one space char.
     */
    if (strncmp(messagebuf_data, "DELETE ", strlen("DELETE ")) == 0)
    {
        messagebuf_data += strlen("DELETE ");
        char *space = strchr(messagebuf_data, ' ');
        if (!space || strncmp(space+1, "HTTP/", strlen("HTTP/")) != 0)
            return 400; /* Bad Request */
        *space = '\0';
        //decode_url(messagebuf_data); %20 => ' '
        alarm(0);
        return delete_path(messagebuf_data);
    }

    if (strncmp(messagebuf_data, "PUT ", strlen("PUT ")) != 0)
    {
        return 400; /* Bad Request */;
    }

    /* Read body */
    if (!body_start)
    {
        VERB1 log("EOF detected, exiting");
        return 400; /* Bad Request */
    }

    messagebuf_len -= (body_start - messagebuf_data);
    memmove(messagebuf_data, body_start, messagebuf_len);
    VERB3 log("Body so far: %u bytes, '%s'", messagebuf_len, messagebuf_data);

    /* Loop until EOF/error/timeout */
    while (1)
    {
        while (1)
        {
            unsigned len = strnlen(messagebuf_data, messagebuf_len);
            if (len >= messagebuf_len)
                break;
            /* messagebuf has at least one NUL - process the line */
            process_message(messagebuf_data);
            messagebuf_len -= (len + 1);
            memmove(messagebuf_data, messagebuf_data + len + 1, messagebuf_len);
        }

        messagebuf_data = xrealloc(messagebuf_data, messagebuf_len + INPUT_BUFFER_SIZE);
        int rd = read(STDIN_FILENO, messagebuf_data + messagebuf_len, INPUT_BUFFER_SIZE);
        if (rd < 0)
        {
            if (errno == EINTR) /* SIGALRM? */
                error_msg_and_die("Timed out");
            perror_msg_and_die("read");
        }
        if (rd == 0)
            break;

        VERB3 log("Received %u bytes of data", rd);
        messagebuf_len += rd;
        total_bytes_read += rd;
        if (total_bytes_read > MAX_MESSAGE_SIZE)
            error_msg_and_die("Message is too long, aborting");
    }

    /* Creates debug dump if all fields were already provided. */
    if (!pid || !backtrace || !executable
     || !analyzer || !dir_basename || !reason
    ) {
        error_msg_and_die("Some data are missing. Aborting");
    }

    /* Write out the crash dump. Don't let alarm to interrupt here */
    alarm(0);

    char *last_file = concat_path_file(g_settings_dump_location, "last-via-server");
    int repeating_crash = check_recent_crash_file(last_file, executable);
    free(last_file);
    if (repeating_crash) /* Only pretend that we saved it */
        return 0; /* ret is 0: "success" */

    return create_debug_dump();
}

static void dummy_handler(int sig_unused) {}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [options]"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_u = 1 << 1,
        OPT_s = 1 << 2,
        OPT_p = 1 << 3,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_INTEGER('u', NULL, &client_uid, _("Use UID as client uid")),
        OPT_BOOL(   's', NULL, NULL       , _("Log to syslog")),
        OPT_BOOL(   'p', NULL, NULL       , _("Add program names to log")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(opts & OPT_p);

    msg_prefix = xasprintf("%s[%u]", g_progname, getpid());
    if (opts & OPT_s)
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    /* Set up timeout handling */
    /* Part 1 - need this to make SIGALRM interrupt syscalls
     * (as opposed to restarting them): I want read syscall to be interrupted
     */
    struct sigaction sa;
    /* sa.sa_flags.SA_RESTART bit is clear: make signal interrupt syscalls */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = dummy_handler; /* pity, SIG_DFL won't do */
    sigaction(SIGALRM, &sa, NULL);
    /* Part 2 - set the timeout per se */
    alarm(TIMEOUT);

    if (client_uid == (uid_t)-1L)
    {
        /* Get uid of the connected client */
        struct ucred cr;
        socklen_t crlen = sizeof(cr);
        if (0 != getsockopt(STDIN_FILENO, SOL_SOCKET, SO_PEERCRED, &cr, &crlen))
            perror_msg_and_die("getsockopt(SO_PEERCRED)");
        if (crlen != sizeof(cr))
            error_msg_and_die("%s: bad crlen %d", "getsockopt(SO_PEERCRED)", (int)crlen);
        client_uid = cr.uid;
    }

    load_abrt_conf();

    int r = perform_http_xact();
    if (r == 0)
        r = 200;

    free_abrt_conf_data();

    printf("HTTP/1.1 %u \r\n\r\n", r);

    return (r >= 400); /* Error if 400+ */
}






#if 0

// TODO: example of SSLed connection

#include <openssl/ssl.h>
#include <openssl/err.h>
    if (flags & OPT_SSL) {
        /* load key and cert files */
        SSL_CTX *ctx;
        SSL *ssl;

        ctx = init_ssl_context();
        if (SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) <= 0
         || SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0
        ) {
            ERR_print_errors_fp(stderr);
            error_msg_and_die("SSL certificates err\n");
        }
        if (!SSL_CTX_check_private_key(ctx)) {
            error_msg_and_die("Private key does not match public key\n");
        }
        (void)SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

        //TODO more errors?
        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sockfd_in);
        //SSL_set_accept_state(ssl);
        if (SSL_accept(ssl) == 1) {
            //while whatever serve
            while (serve(ssl, flags))
                continue;
            //TODO errors
            SSL_shutdown(ssl);
        }
        SSL_free(ssl);
        SSL_CTX_free(ctx);
    } else {
        while (serve(&sockfd_in, flags))
            continue;
    }


        err = (flags & OPT_SSL) ? SSL_read(sock, buffer, READ_BUF-1):
                                  read(*(int*)sock, buffer, READ_BUF-1);

        if ( err < 0 ) {
            //TODO handle errno ||  SSL_get_error(ssl,err);
            break;
        }
        if ( err == 0 ) break;

        if (!head) {
            buffer[err] = '\0';
            clean[i%2] = delete_cr(buffer);
            cut = g_strstr_len(buffer, -1, "\n\n");
            if ( cut == NULL ) {
                g_string_append(headers, buffer);
            } else {
                g_string_append_len(headers, buffer, cut-buffer);
            }
        }

        /* end of header section? */
        if ( !head && ( cut != NULL || (clean[(i+1)%2] && buffer[0]=='\n') ) ) {
            parse_head(&request, headers);
            head = TRUE;
            c_len = has_body(&request);

            if ( c_len ) {
                //if we want to read body some day - this will be the right place to begin
                //malloc body append rest of the (fixed) buffer at the beginning of a body
                //if clean buffer[1];
            } else {
                break;
            }
            break; //because we don't support body yet
        } else if ( head == TRUE ) {
            /* body-reading stuff
             * read body, check content-len
             * save body to request
             */
            break;
        } else {
            // count header size
            len += err;
            if ( len > READ_BUF-1 ) {
                //TODO header is too long
                break;
            }
        }

        i++;
    }

    g_string_free(headers, true); //because we allocated it

    rt = generate_response(&request, &response);

    /* write headers */
    if ( flags & OPT_SSL ) {
        //TODO err
        err = SSL_write(sock, response.response_line, strlen(response.response_line));
        err = SSL_write(sock, response.head->str , strlen(response.head->str));
        err = SSL_write(sock, "\r\n", 2);
    } else {
        //TODO err
        err = write(*(int*)sock, response.response_line, strlen(response.response_line));
        err = write(*(int*)sock, response.head->str , strlen(response.head->str));
        err = write(*(int*)sock, "\r\n", 2);
    }
#endif
