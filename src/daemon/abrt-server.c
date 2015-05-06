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
#include "problem_api.h"
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
MANDATORY ITEMS:
-> "PID="
   number 0 - PID_MAX (/proc/sys/kernel/pid_max)
   \0
-> "EXECUTABLE="
   string
   \0
-> "BACKTRACE="
   string
   \0
-> "ANALYZER="
   string
   \0
-> "BASENAME="
   string (no slashes)
   \0
-> "REASON="
   string
   \0

You can send more messages using the same KEY=value format.
*/

static unsigned total_bytes_read = 0;

static uid_t client_uid = (uid_t)-1L;


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
    if (!dir_has_correct_permissions(dump_dir_name, DD_PERM_DAEMONS))
    {
        error_msg("Problem directory '%s' has wrong owner or group", dump_dir_name);
        return 400; /*  */
    }

    struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_FD_ONLY);
    if (dd == NULL)
    {
        perror_msg("Can't open problem directory '%s'", dump_dir_name);
        return 400;
    }
    if (!dd_accessible_by_uid(dd, client_uid))
    {
        dd_close(dd);
        if (errno == ENOTDIR)
        {
            error_msg("Path '%s' isn't problem directory", dump_dir_name);
            return 404; /* Not Found */
        }
        error_msg("Problem directory '%s' can't be accessed by user with uid %ld", dump_dir_name, (long)client_uid);
        return 403; /* Forbidden */
    }

    dd = dd_fdopendir(dd, /*flags:*/ 0);
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

static pid_t spawn_event_handler_child(const char *dump_dir_name, const char *event_name, int *fdp)
{
    char *args[7];
    args[0] = (char *) LIBEXEC_DIR"/abrt-handle-event";
    /* Do not forward ASK_* messages to parent*/
    args[1] = (char *) "-i";
    args[2] = (char *) "-e";
    args[3] = (char *) event_name;
    args[4] = (char *) "--";
    args[5] = (char *) dump_dir_name;
    args[6] = NULL;

    int pipeout[2];
    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_QUIET | EXECFLG_ERR2OUT;
    VERB1 flags &= ~EXECFLG_QUIET;

    char *env_vec[2];
    /* Intercept ASK_* messages in Client API -> don't wait for user response */
    env_vec[0] = xstrdup("REPORT_CLIENT_NONINTERACTIVE=1");
    env_vec[1] = NULL;

    pid_t child = fork_execv_on_steroids(flags, args, pipeout,
                                         env_vec, /*dir:*/ NULL,
                                         /*uid(unused):*/ 0);
    if (fdp)
        *fdp = pipeout[0];
    return child;
}

static int run_post_create(const char *dirname)
{
    /* If doesn't start with "g_settings_dump_location/"... */
    if (!dir_is_in_dump_location(dirname))
    {
        /* Then refuse to operate on it (someone is attacking us??) */
        error_msg("Bad problem directory name '%s', should start with: '%s'", dirname, g_settings_dump_location);
        return 400; /* Bad Request */
    }
    if (!dir_has_correct_permissions(dirname, DD_PERM_EVENTS))
    {
        error_msg("Problem directory '%s' has wrong owner or group", dirname);
        return 400; /*  */
    }
    /* Check completness */
    {
        struct dump_dir *dd = dd_opendir(dirname, DD_OPEN_READONLY);
        const bool complete = dd && problem_dump_dir_is_complete(dd);
        dd_close(dd);
        if (complete)
        {
            error_msg("Problem directory '%s' has already been processed", dirname);
            return 403;
        }
    }
    if (!dump_dir_accessible_by_uid(dirname, client_uid))
    {
        if (errno == ENOTDIR)
        {
            error_msg("Path '%s' isn't problem directory", dirname);
            return 404; /* Not Found */
        }
        error_msg("Problem directory '%s' can't be accessed by user with uid %ld", dirname, (long)client_uid);
        return 403; /* Forbidden */
    }

    int child_stdout_fd;
    int child_pid = spawn_event_handler_child(dirname, "post-create", &child_stdout_fd);

    char *dup_of_dir = NULL;
    struct strbuf *cmd_output = strbuf_new();

    bool child_is_post_create = 1; /* else it is a notify child */

 read_child_output:
    //log("Reading from event fd %d", child_stdout_fd);

    /* Read streamed data and split lines */
    for (;;)
    {
        char buf[250]; /* usually we get one line, no need to have big buf */
        errno = 0;
        int r = safe_read(child_stdout_fd, buf, sizeof(buf) - 1);
        if (r <= 0)
            break;
        buf[r] = '\0';

        /* split lines in the current buffer */
        char *raw = buf;
        char *newline;
        while ((newline = strchr(raw, '\n')) != NULL)
        {
            *newline = '\0';
            strbuf_append_str(cmd_output, raw);
            char *msg = cmd_output->buf;

            if (child_is_post_create
             && prefixcmp(msg, "DUP_OF_DIR: ") == 0
            ) {
                free(dup_of_dir);
                dup_of_dir = xstrdup(msg + strlen("DUP_OF_DIR: "));
            }
            else
                log("%s", msg);

            strbuf_clear(cmd_output);
            /* jump to next line */
            raw = newline + 1;
        }

        /* beginning of next line. the line continues by next read */
        strbuf_append_str(cmd_output, raw);
    }

    /* EOF/error */

    /* Wait for child to actually exit, collect status */
    int status = 0;
    if (safe_waitpid(child_pid, &status, 0) <= 0)
    /* should not happen */
        perror_msg("waitpid(%d)", child_pid);

    /* If it was a "notify[-dup]" event, then we're done */
    if (!child_is_post_create)
        goto ret;

    /* exit 0 means "this is a good, non-dup dir" */
    /* exit with 1 + "DUP_OF_DIR: dir" string => dup */
    if (status != 0)
    {
        if (WIFSIGNALED(status))
        {
            log("'post-create' on '%s' killed by signal %d",
                            dirname, WTERMSIG(status));
            goto delete_bad_dir;
        }
        /* else: it is WIFEXITED(status) */
        if (!dup_of_dir)
        {
            log("'post-create' on '%s' exited with %d",
                            dirname, WEXITSTATUS(status));
            goto delete_bad_dir;
        }
    }

    const char *work_dir = (dup_of_dir ? dup_of_dir : dirname);

    /* Load problem_data (from the *first dir* if this one is a dup) */
    struct dump_dir *dd = dd_opendir(work_dir, /*flags:*/ 0);
    if (!dd)
        /* dd_opendir already emitted error msg */
        goto delete_bad_dir;

    /* Update count */
    char *count_str = dd_load_text_ext(dd, FILENAME_COUNT, DD_FAIL_QUIETLY_ENOENT);
    unsigned long count = strtoul(count_str, NULL, 10);

    /* Don't increase crash count if we are working with newly uploaded
     * directory (remote crash) which already has its crash count set.
     */
    if ((status != 0 && dup_of_dir) || count == 0)
    {
        count++;
        char new_count_str[sizeof(long)*3 + 2];
        sprintf(new_count_str, "%lu", count);
        dd_save_text(dd, FILENAME_COUNT, new_count_str);

        /* This condition can be simplified to either
         * (status * != 0 && * dup_of_dir) or (count == 1). But the
         * chosen form is much more reliable and safe. We must not call
         * dd_opendir() to locked dd otherwise we go into a deadlock.
         */
        if (strcmp(dd->dd_dirname, dirname) != 0)
        {
            /* Update the last occurrence file by the time file of the new problem */
            struct dump_dir *new_dd = dd_opendir(dirname, DD_OPEN_READONLY);
            char *last_ocr = NULL;
            if (new_dd)
            {
                /* TIME must exists in a valid dump directory but we don't want to die
                 * due to broken duplicated dump directory */
                last_ocr = dd_load_text_ext(new_dd, FILENAME_TIME,
                            DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE | DD_FAIL_QUIETLY_ENOENT);
                dd_close(new_dd);
            }
            else
            {   /* dd_opendir() already produced a message with good information about failure */
                error_msg("Can't read the last occurrence file from the new dump directory.");
            }

            if (!last_ocr)
            {   /* the new dump directory may lie in the dump location for some time */
                log("Using current time for the last occurrence file which may be incorrect.");
                time_t t = time(NULL);
                last_ocr = xasprintf("%lu", (long)t);
            }

            dd_save_text(dd, FILENAME_LAST_OCCURRENCE, last_ocr);

            free(last_ocr);
        }
    }

    /* Reset mode/uig/gid to correct values for all files created by event run */
    dd_sanitize_mode_and_owner(dd);

    dd_close(dd);

    if (!dup_of_dir)
        log_notice("New problem directory %s, processing", work_dir);
    else
    {
        log_warning("Deleting problem directory %s (dup of %s)",
                    strrchr(dirname, '/') + 1,
                    strrchr(dup_of_dir, '/') + 1);
        delete_dump_dir(dirname);
    }

    /* Run "notify[-dup]" event */
    int fd;
    child_pid = spawn_event_handler_child(
                work_dir,
                (dup_of_dir ? "notify-dup" : "notify"),
                &fd
    );
    //log("Started notify, fd %d -> %d", fd, child_stdout_fd);
    xmove_fd(fd, child_stdout_fd);
    child_is_post_create = 0;
    strbuf_clear(cmd_output);
    free(dup_of_dir);
    dup_of_dir = NULL;
    goto read_child_output;

 delete_bad_dir:
    log_warning("Deleting problem directory '%s'", dirname);
    delete_dump_dir(dirname);

 ret:
    strbuf_free(cmd_output);
    free(dup_of_dir);
    close(child_stdout_fd);
    return 0;
}

/* Create a new problem directory from client session.
 * Caller must ensure that all fields in struct client
 * are properly filled.
 */
static int create_problem_dir(GHashTable *problem_info, unsigned pid)
{
    /* Exit if free space is less than 1/4 of MaxCrashReportsSize */
    if (g_settings_nMaxCrashReportsSize > 0)
    {
        if (low_free_space(g_settings_nMaxCrashReportsSize, g_settings_dump_location))
            exit(1);
    }

    /* Create temp directory with the problem data.
     * This directory is renamed to final directory name after
     * all files have been stored into it.
     */

    gchar *dir_basename = g_hash_table_lookup(problem_info, "basename");
    if (!dir_basename)
        dir_basename = g_hash_table_lookup(problem_info, FILENAME_TYPE);

    char *path = xasprintf("%s/%s-%s-%u.new",
                           g_settings_dump_location,
                           dir_basename,
                           iso_date_string(NULL),
                           pid);

    /* This item is useless, don't save it */
    g_hash_table_remove(problem_info, "basename");

    /* No need to check the path length, as all variables used are limited,
     * and dd_create() fails if the path is too long.
     */
    struct dump_dir *dd = dd_create(path, client_uid, DEFAULT_DUMP_DIR_MODE);
    if (!dd)
    {
        error_msg_and_die("Error creating problem directory '%s'", path);
    }

    dd_create_basic_files(dd, client_uid, NULL);
    dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);

    gpointer gpkey = g_hash_table_lookup(problem_info, FILENAME_CMDLINE);
    if (!gpkey)
    {
        /* Obtain and save the command line. */
        char *cmdline = get_cmdline(pid);
        if (cmdline)
        {
            dd_save_text(dd, FILENAME_CMDLINE, cmdline);
            free(cmdline);
        }
    }

    /* Store id of the user whose application crashed. */
    char uid_str[sizeof(long) * 3 + 2];
    sprintf(uid_str, "%lu", (long)client_uid);
    dd_save_text(dd, FILENAME_UID, uid_str);

    GHashTableIter iter;
    gpointer gpvalue;
    g_hash_table_iter_init(&iter, problem_info);
    while (g_hash_table_iter_next(&iter, &gpkey, &gpvalue))
    {
        dd_save_text(dd, (gchar *) gpkey, (gchar *) gpvalue);
    }

    dd_close(dd);

    /* Not needing it anymore */
    g_hash_table_destroy(problem_info);

    /* Move the completely created problem directory
     * to final directory.
     */
    char *newpath = xstrndup(path, strlen(path) - strlen(".new"));
    if (rename(path, newpath) == 0)
        strcpy(path, newpath);
    free(newpath);

    log_notice("Saved problem directory of pid %u to '%s'", pid, path);

    /* We let the peer know that problem dir was created successfully
     * _before_ we run potentially long-running post-create.
     */
    printf("HTTP/1.1 201 Created\r\n\r\n");
    fflush(NULL);
    close(STDOUT_FILENO);
    xdup2(STDERR_FILENO, STDOUT_FILENO); /* paranoia: don't leave stdout fd closed */

    /* Trim old problem directories if necessary */
    if (g_settings_nMaxCrashReportsSize > 0)
    {
        trim_problem_dirs(g_settings_dump_location, g_settings_nMaxCrashReportsSize * (double)(1024*1024), path);
    }

    run_post_create(path);

    /* free(path); */
    exit(0);
}

static gboolean key_value_ok(gchar *key, gchar *value)
{
    char *i;

    /* check key, it has to be valid filename and will end up in the
     * bugzilla */
    for (i = key; *i != 0; i++)
    {
        if (!isalpha(*i) && (*i != '-') && (*i != '_') && (*i != ' '))
            return FALSE;
    }

    /* check value of 'basename', it has to be valid non-hidden directory
     * name */
    if (strcmp(key, "basename") == 0
     || strcmp(key, FILENAME_TYPE) == 0
    )
    {
        if (!str_is_correct_filename(value))
        {
            error_msg("Value of '%s' ('%s') is not a valid directory name",
                      key, value);
            return FALSE;
        }
    }

    return TRUE;
}

/* Handles a message received from client over socket. */
static void process_message(GHashTable *problem_info, char *message)
{
    gchar *key, *value;

    value = strchr(message, '=');
    if (value)
    {
        key = g_ascii_strdown(message, value - message); /* result is malloced */
//TODO: is it ok? it uses g_malloc, not malloc!

        value++;
        if (key_value_ok(key, value))
        {
            if (strcmp(key, FILENAME_UID) == 0)
            {
                error_msg("Ignoring value of %s, will be determined later",
                          FILENAME_UID);
            }
            else
            {
                g_hash_table_insert(problem_info, key, xstrdup(value));
                /* Compat, delete when FILENAME_ANALYZER is replaced by FILENAME_TYPE: */
                if (strcmp(key, FILENAME_TYPE) == 0)
                    g_hash_table_insert(problem_info, xstrdup(FILENAME_ANALYZER), xstrdup(value));
                /* Prevent freeing key later: */
                key = NULL;
            }
        }
        else
        {
            /* should use error_msg_and_die() here? */
            error_msg("Invalid key or value format: %s", message);
        }
        free(key);
    }
    else
    {
        /* should use error_msg_and_die() here? */
        error_msg("Invalid message format: '%s'", message);
    }
}

static void die_if_data_is_missing(GHashTable *problem_info)
{
    gboolean missing_data = FALSE;
    gchar **pstring;
    static const gchar *const needed[] = {
        FILENAME_TYPE,
        FILENAME_REASON,
        /* FILENAME_BACKTRACE, - ECC errors have no such elements */
        /* FILENAME_EXECUTABLE, */
        NULL
    };

    for (pstring = (gchar**) needed; *pstring; pstring++)
    {
        if (!g_hash_table_lookup(problem_info, *pstring))
        {
            error_msg("Element '%s' is missing", *pstring);
            missing_data = TRUE;
        }
    }

    if (missing_data)
        error_msg_and_die("Some data is missing, aborting");
}

/*
 * Takes hash table, looks for key FILENAME_PID and tries to convert its value
 * to int.
 */
unsigned convert_pid(GHashTable *problem_info)
{
    long ret;
    gchar *pid_str = (gchar *) g_hash_table_lookup(problem_info, FILENAME_PID);
    char *err_pos;

    if (!pid_str)
        error_msg_and_die("PID data is missing, aborting");

    errno = 0;
    ret = strtol(pid_str, &err_pos, 10);
    if (errno || pid_str == err_pos || *err_pos != '\0'
        || ret > UINT_MAX || ret < 1)
        error_msg_and_die("Malformed or out-of-range PID number: '%s'", pid_str);

    return (unsigned) ret;
}

static int perform_http_xact(void)
{
    /* use free instead of g_free so that we can use xstr* functions from
     * libreport/lib/xfuncs.c
     */
    GHashTable *problem_info = g_hash_table_new_full(g_str_hash, g_str_equal,
                                     free, free);
    /* Read header */
    char *body_start = NULL;
    char *messagebuf_data = NULL;
    unsigned messagebuf_len = 0;
    /* Loop until EOF/error/timeout/end_of_header */
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

        log_debug("Received %u bytes of data", rd);
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
    log_debug("Request: %s", messagebuf_data);

    /* Sanitize and analyze header.
     * Header now is in messagebuf_data, NUL terminated string,
     * with last empty line deleted (by placement of NUL).
     * \r\n are not (yet) converted to \n, multi-line headers also
     * not converted.
     */
    /* First line must be "op<space>[http://host]/path<space>HTTP/n.n".
     * <space> is exactly one space char.
     */
    if (prefixcmp(messagebuf_data, "DELETE ") == 0)
    {
        messagebuf_data += strlen("DELETE ");
        char *space = strchr(messagebuf_data, ' ');
        if (!space || prefixcmp(space+1, "HTTP/") != 0)
            return 400; /* Bad Request */
        *space = '\0';
        //decode_url(messagebuf_data); %20 => ' '
        alarm(0);
        return delete_path(messagebuf_data);
    }

    /* We erroneously used "PUT /" to create new problems.
     * POST is the correct request in this case:
     * "PUT /" implies creation or replace of resource named "/"!
     * Delete PUT in 2014.
     */
    if (prefixcmp(messagebuf_data, "PUT ") != 0
     && prefixcmp(messagebuf_data, "POST ") != 0
    ) {
        return 400; /* Bad Request */
    }

    enum {
        CREATION_NOTIFICATION,
        CREATION_REQUEST,
    };
    int url_type;
    char *url = skip_non_whitespace(messagebuf_data) + 1; /* skip "POST " */
    if (prefixcmp(url, "/creation_notification ") == 0)
        url_type = CREATION_NOTIFICATION;
    else if (prefixcmp(url, "/ ") == 0)
        url_type = CREATION_REQUEST;
    else
        return 400; /* Bad Request */

    /* Read body */
    if (!body_start)
    {
        log_warning("Premature EOF detected, exiting");
        return 400; /* Bad Request */
    }

    messagebuf_len -= (body_start - messagebuf_data);
    memmove(messagebuf_data, body_start, messagebuf_len);
    log_debug("Body so far: %u bytes, '%s'", messagebuf_len, messagebuf_data);

    /* Loop until EOF/error/timeout */
    while (1)
    {
        if (url_type == CREATION_REQUEST)
        {
            while (1)
            {
                unsigned len = strnlen(messagebuf_data, messagebuf_len);
                if (len >= messagebuf_len)
                    break;
                /* messagebuf has at least one NUL - process the line */
                process_message(problem_info, messagebuf_data);
                messagebuf_len -= (len + 1);
                memmove(messagebuf_data, messagebuf_data + len + 1, messagebuf_len);
            }
        }

        messagebuf_data = xrealloc(messagebuf_data, messagebuf_len + INPUT_BUFFER_SIZE + 1);
        int rd = read(STDIN_FILENO, messagebuf_data + messagebuf_len, INPUT_BUFFER_SIZE);
        if (rd < 0)
        {
            if (errno == EINTR) /* SIGALRM? */
                error_msg_and_die("Timed out");
            perror_msg_and_die("read");
        }
        if (rd == 0)
            break;

        log_debug("Received %u bytes of data", rd);
        messagebuf_len += rd;
        total_bytes_read += rd;
        if (total_bytes_read > MAX_MESSAGE_SIZE)
            error_msg_and_die("Message is too long, aborting");
    }

    /* Body received, EOF was seen. Don't let alarm to interrupt after this. */
    alarm(0);

    if (url_type == CREATION_NOTIFICATION)
    {
        messagebuf_data[messagebuf_len] = '\0';
        return run_post_create(messagebuf_data);
    }

    /* Save problem dir */
    int ret = 0;
    unsigned pid = convert_pid(problem_info);
    die_if_data_is_missing(problem_info);

    char *executable = g_hash_table_lookup(problem_info, FILENAME_EXECUTABLE);
    if (executable)
    {
        char *last_file = concat_path_file(g_settings_dump_location, "last-via-server");
        int repeating_crash = check_recent_crash_file(last_file, executable);
        free(last_file);
        if (repeating_crash) /* Only pretend that we saved it */
            goto out; /* ret is 0: "success" */
    }

#if 0
//TODO:
    /* At least it should generate local problem identifier UUID */
    problem_data_add_basics(problem_info);
//...the problem being that problem_info here is not a problem_data_t!
#endif

    create_problem_dir(problem_info, pid);
    /* does not return */

 out:
    g_hash_table_destroy(problem_info);
    return ret; /* Used as HTTP response code */
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
        OPT_INTEGER('u', NULL, &client_uid, _("Use NUM as client uid")),
        OPT_BOOL(   's', NULL, NULL       , _("Log to syslog")),
        OPT_BOOL(   'p', NULL, NULL       , _("Add program names to log")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(opts & OPT_p);

    msg_prefix = xasprintf("%s[%u]", g_progname, getpid());
    if (opts & OPT_s)
    {
        logmode = LOGMODE_JOURNAL;
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
