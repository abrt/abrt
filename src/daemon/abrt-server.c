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
#include "abrtlib.h"
#include "parse_options.h"

#define PROGNAME "abrt-server"

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
to properly fill a dump directory. Adding these privileges to every
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

/* Buffer for incomplete incoming messages. */
static char *messagebuf_data = NULL;
static unsigned messagebuf_len = 0;

static unsigned total_bytes_read = 0;

static uid_t client_uid = (uid_t)-1L;

static int   pid;
static char *executable;
static char *backtrace;
/* "python", "ruby" etc. */
static char *analyzer;
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
static void create_debug_dump()
{
    /* Create temp directory with the debug dump.
       This directory is renamed to final directory name after
       all files have been stored into it.
    */
    char *path = xasprintf(DEBUG_DUMPS_DIR"/%s-%s-%u.new",
                           dir_basename,
                           iso_date_string(NULL),
                           pid);
    /* No need to check the path length, as all variables used are limited, and dd_create()
       fails if the path is too long. */

    struct dump_dir *dd = dd_create(path, client_uid, 0640);
    if (!dd)
    {
        error_msg_and_die("Error creating crash dump %s", path);
    }
    dd_create_basic_files(dd, client_uid);

    dd_save_text(dd, FILENAME_ANALYZER, analyzer);
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

    dd_close(dd);

    /* Move the completely created debug dump to
       final directory. */
    char *newpath = xstrndup(path, strlen(path) - strlen(".new"));
    if (rename(path, newpath) == 0)
        strcpy(path, newpath);
    free(newpath);

    log("Saved %s crash dump of pid %u to %s", analyzer, pid, path);

    /* Trim old crash dumps if necessary */
    unsigned maxCrashReportsSize = 0;
    parse_conf(NULL, &maxCrashReportsSize, NULL, NULL);
    if (maxCrashReportsSize > 0)
    {
        check_free_space(maxCrashReportsSize);
        trim_debug_dumps(DEBUG_DUMPS_DIR, maxCrashReportsSize * (double)(1024*1024), path);
    }

    free(path);
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

/* Checks if a string has certain prefix. */
static bool starts_with(const char *str, const char *start)
{
    return strncmp(str, start, strlen(start)) == 0;
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

    /* Creates debug dump if all fields were already provided. */
    if (starts_with(message, "DONE"))
    {
        if (!pid || !backtrace || !executable
         || !analyzer || !dir_basename || !reason
        ) {
            error_msg_and_die("Got DONE, but some data are missing. Aborting");
        }

        /* Write out the crash dump. Don't let alarm to interrupt here */
        alarm(0);
        create_debug_dump();

        /* Reset alarm and the counter which detects oversized dumps */
        alarm(TIMEOUT);
        total_bytes_read = 0;
    }
}

static void dummy_handler(int sig_unused) {}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [options]"
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

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));
    msg_prefix = xasprintf(PROGNAME"[%u]", getpid());
    if (opts & OPT_p)
        putenv((char*)"ABRT_PROG_PREFIX=1");
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

    /* Loop until EOF/error/timeout */
    while (1)
    {
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
    }

    VERB1 log("EOF detected, exiting");
    return 0;
}
