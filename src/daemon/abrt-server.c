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
#include "abrt_glib.h"
#include "libabrt.h"

/* Maximal length of backtrace. */
#define MAX_BACKTRACE_SIZE (1024*1024)
/* Amount of data received from one client for a message before reporting error. */
#define MAX_MESSAGE_SIZE (4*MAX_BACKTRACE_SIZE)
/* Maximal number of characters read from socket at once. */
#define INPUT_BUFFER_SIZE (8*1024)
/* We exit after this many seconds */
#define TIMEOUT 10

#define ABRT_SERVER_EVENT_ENV "ABRT_SERVER_PID"

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

static int g_signal_pipe[2];
static struct ns_ids g_ns_ids;

struct waiting_context
{
    GMainLoop *main_loop;
    const char *dirname;
    int retcode;
    enum abrt_daemon_reply
    {
        ABRT_CONTINUE,
        ABRT_INTERRUPT,
    } reply;
};

static unsigned total_bytes_read = 0;

static pid_t client_pid = (pid_t)-1L;
static uid_t client_uid = (uid_t)-1L;

static void
handle_signal(int signo)
{
    int save_errno = errno;
    uint8_t sig_caught = signo;
    if (write(g_signal_pipe[1], &sig_caught, 1))
        /* we ignore result, if () shuts up stupid compiler */;
    errno = save_errno;
}

static gboolean
handle_signal_pipe_cb(GIOChannel *gio, GIOCondition condition, gpointer user_data)
{
    gsize len = 0;
    uint8_t signals[2];

    for (;;)
    {
        GError *error = NULL;
        GIOStatus stat = g_io_channel_read_chars(gio, (void *)signals, sizeof(signals), &len, NULL);
        if (stat == G_IO_STATUS_ERROR)
            error_msg_and_die(_("Can't read from gio channel: '%s'"), error ? error->message : "");
        if (stat == G_IO_STATUS_EOF)
            return FALSE; /* Remove this GLib source */
        if (stat == G_IO_STATUS_AGAIN)
            break;

        /* G_IO_STATUS_NORMAL */
        for (unsigned signo = 0; signo < len; ++signo)
        {
            /* we did receive a signal */
            struct waiting_context *context = (struct waiting_context *)user_data;
            log_debug("Got signal %d through signal pipe", signals[signo]);
            switch (signals[signo])
            {
                case SIGUSR1: context->reply = ABRT_CONTINUE; break;
                case SIGINT:  context->reply = ABRT_INTERRUPT; break;
                default:
                {
                    error_msg("Bug - aborting - unsupported signal: %d", signals[signo]);
                    abort();
                }
            }

            g_main_loop_quit(context->main_loop);
            return FALSE; /* remove this event */
        }
    }

    return TRUE; /* "please don't remove this event" */
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
    char *args[9];
    args[0] = (char *) LIBEXEC_DIR"/abrt-handle-event";
    /* Do not forward ASK_* messages to parent*/
    args[1] = (char *) "-i";
    args[2] = (char *) "--nice";
    args[3] = (char *) "10";
    args[4] = (char *) "-e";
    args[5] = (char *) event_name;
    args[6] = (char *) "--";
    args[7] = (char *) dump_dir_name;
    args[8] = NULL;

    int pipeout[2];
    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_QUIET | EXECFLG_ERR2OUT;
    VERB1 flags &= ~EXECFLG_QUIET;

    char *env_vec[3];
    /* Intercept ASK_* messages in Client API -> don't wait for user response */
    env_vec[0] = xstrdup("REPORT_CLIENT_NONINTERACTIVE=1");
    env_vec[1] = xasprintf("%s=%d", ABRT_SERVER_EVENT_ENV, getpid());
    env_vec[2] = NULL;

    pid_t child = fork_execv_on_steroids(flags, args, pipeout,
                                         env_vec, /*dir:*/ NULL,
                                         /*uid(unused):*/ 0);
    if (fdp)
        *fdp = pipeout[0];
    return child;
}

static int problem_dump_dir_was_provoked_by_abrt_event(struct dump_dir *dd, char  **provoker)
{
    char *env_var = NULL;
    const int r = dd_get_env_variable(dd, ABRT_SERVER_EVENT_ENV, &env_var);

    /* Dump directory doesn't contain the environ file */
    if (r == -ENOENT)
        return 0;

    if (provoker != NULL)
        *provoker = env_var;
    else
        free(env_var);

    return env_var != NULL;
}

static int
emit_new_problem_signal(gpointer data)
{
    struct waiting_context *context = (struct waiting_context *)data;

    const size_t wrote = fprintf(stderr, "NEW_PROBLEM_DETECTED: %s\n", context->dirname);
    fflush(stderr);

    if (wrote <= 0)
    {
        error_msg("Failed to communicate with the daemon");
        context->retcode = 503;
        g_main_loop_quit(context->main_loop);
    }

    log_notice("Emitted new problem signal, waiting for SIGUSR1|SIGINT");
    return FALSE;
}

struct response
{
    int code;
    char *message;
};

#define RESPONSE_SETTER(r, c, m) \
    do { if (r != NULL) { r->message = m; r->code = c; } else { free(m); }} while (0)

#define RESPONSE_RETURN(r, c ,m) \
    do { RESPONSE_SETTER(r, c, m); \
         return c; } while (0)


static int run_post_create(const char *dirname, struct response *resp)
{
    /* If doesn't start with "g_settings_dump_location/"... */
    if (!dir_is_in_dump_location(dirname))
    {
        /* Then refuse to operate on it (someone is attacking us??) */
        error_msg("Bad problem directory name '%s', should start with: '%s'", dirname, g_settings_dump_location);
        RESPONSE_RETURN(resp, 400, NULL);
    }
    if (!dir_has_correct_permissions(dirname, DD_PERM_EVENTS))
    {
        error_msg("Problem directory '%s' has wrong owner or group", dirname);
        RESPONSE_RETURN(resp, 400, NULL);
    }
    /* Check completness */
    {
        struct dump_dir *dd = dd_opendir(dirname, DD_OPEN_READONLY);

        char *provoker = NULL;
        const bool event_dir = dd && problem_dump_dir_was_provoked_by_abrt_event(dd, &provoker);
        if (event_dir)
        {
            if (g_settings_debug_level == 0)
            {
                error_msg("Removing problem provoked by ABRT(pid:%s): '%s'", provoker, dirname);
                dd_delete(dd);
            }
            else
            {
                char *dumpdir = NULL;
                char *event   = NULL;
                char *reason  = NULL;
                char *cmdline = NULL;

                /* Ignore errors */
                dd_get_env_variable(dd, "DUMP_DIR", &dumpdir);
                dd_get_env_variable(dd, "EVENT",    &event);
                reason  = dd_load_text(dd, FILENAME_REASON);
                cmdline = dd_load_text(dd, FILENAME_CMDLINE);

                error_msg("ABRT_SERVER_PID=%s;DUMP_DIR='%s';EVENT='%s';REASON='%s';CMDLINE='%s'",
                           provoker, dumpdir, event, reason, cmdline);

            }

            free(provoker);
            return 400;
        }

        const bool complete = dd && problem_dump_dir_is_complete(dd);
        dd_close(dd);
        if (complete)
        {
            error_msg("Problem directory '%s' has already been processed", dirname);
            RESPONSE_RETURN(resp, 403, NULL);
        }
    }

    /*
     * The post-create event cannot be run concurrently for more problem
     * directories. The problem is in searching for duplicates process
     * in case when two concurrently processed directories are duplicates
     * of each other. Both of the directories are marked as duplicates
     * of each other and are deleted.
     */
    log_debug("Creating glib main loop");
    struct waiting_context context = {0};
    context.main_loop = g_main_loop_new(NULL, FALSE);
    context.dirname = strrchr(dirname, '/') + 1;

    log_debug("Setting up a signal handler");
    /* Set up signal pipe */
    xpipe(g_signal_pipe);
    close_on_exec_on(g_signal_pipe[0]);
    close_on_exec_on(g_signal_pipe[1]);
    ndelay_on(g_signal_pipe[0]);
    ndelay_on(g_signal_pipe[1]);
    signal(SIGUSR1, handle_signal);
    signal(SIGINT, handle_signal);
    GIOChannel *channel_signal = abrt_gio_channel_unix_new(g_signal_pipe[0]);
    g_io_add_watch(channel_signal, G_IO_IN | G_IO_PRI, handle_signal_pipe_cb, &context);

    g_idle_add(emit_new_problem_signal, &context);

    g_main_loop_run(context.main_loop);

    g_main_loop_unref(context.main_loop);
    g_io_channel_unref(channel_signal);
    close(g_signal_pipe[1]);
    close(g_signal_pipe[0]);

    log_notice("Waiting finished");

    if (context.retcode != 0)
        RESPONSE_RETURN(resp, context.retcode, NULL);

    if (context.reply != ABRT_CONTINUE)
        /* The only reason for the interruption is removed problem directory */
        RESPONSE_RETURN(resp, 413, NULL);
    /*
     * The post-create event synchronization done.
     */

    int child_stdout_fd;
    int child_pid = spawn_event_handler_child(dirname, "post-create", &child_stdout_fd);

    char *dup_of_dir = NULL;
    struct strbuf *cmd_output = strbuf_new();

    bool child_is_post_create = 1; /* else it is a notify child */

 read_child_output:
    //log_warning("Reading from event fd %d", child_stdout_fd);

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
                log_warning("%s", msg);

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
            log_warning("'post-create' on '%s' killed by signal %d",
                            dirname, WTERMSIG(status));
            goto delete_bad_dir;
        }
        /* else: it is WIFEXITED(status) */
        if (!dup_of_dir)
        {
            log_warning("'post-create' on '%s' exited with %d",
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
                log_warning("Using current time for the last occurrence file which may be incorrect.");
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
    //log_warning("Started notify, fd %d -> %d", fd, child_stdout_fd);
    xmove_fd(fd, child_stdout_fd);
    child_is_post_create = 0;
    if (dup_of_dir)
        RESPONSE_SETTER(resp, 303, dup_of_dir);
    else
    {
        RESPONSE_SETTER(resp, 200, NULL);
        free(dup_of_dir);
    }
    dup_of_dir = NULL;
    strbuf_clear(cmd_output);
    goto read_child_output;

 delete_bad_dir:
    log_warning("Deleting problem directory '%s'", dirname);
    delete_dump_dir(dirname);
    /* TODO - better code to allow detection on client's side */
    RESPONSE_SETTER(resp, 403, NULL);

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
    struct dump_dir *dd = dd_create(path, /*fs owner*/0, DEFAULT_DUMP_DIR_MODE);
    if (!dd)
    {
        error_msg_and_die("Error creating problem directory '%s'", path);
    }

    const int proc_dir_fd = open_proc_pid_dir(pid);
    char *rootdir = NULL;

    if (proc_dir_fd < 0)
    {
        pwarn_msg("Cannot open /proc/%d:", pid);
    }
    else if (process_has_own_root_at(proc_dir_fd))
    {
        /* Obtain the root directory path only if process' root directory is
         * not the same as the init's root directory
         */
        rootdir = get_rootdir_at(proc_dir_fd);
    }

    /* Reading data from an arbitrary root directory is not secure. */
    if (proc_dir_fd >= 0 && g_settings_explorechroots)
    {
        char proc_pid_root[sizeof("/proc/[pid]/root") + sizeof(pid_t) * 3];
        const size_t w = snprintf(proc_pid_root, sizeof(proc_pid_root), "/proc/%d/root", pid);
        assert(sizeof(proc_pid_root) > w);

        /* Yes, test 'rootdir' but use 'source_filename' because 'rootdir' can
         * be '/' for a process with own namespace. 'source_filename' is /proc/[pid]/root. */
        dd_create_basic_files(dd, client_uid, (rootdir != NULL) ? proc_pid_root : NULL);
    }
    else
    {
        dd_create_basic_files(dd, client_uid, NULL);
    }

    if (proc_dir_fd >= 0)
    {
        /* Obtain and save the command line. */
        char *cmdline = get_cmdline_at(proc_dir_fd);
        if (cmdline)
        {
            dd_save_text(dd, FILENAME_CMDLINE, cmdline);
            free(cmdline);
        }

        /* Obtain and save the environment variables. */
        char *environ = get_environ_at(proc_dir_fd);
        if (environ)
        {
            dd_save_text(dd, FILENAME_ENVIRON, environ);
            free(environ);
        }

        dd_copy_file_at(dd, FILENAME_CGROUP,    proc_dir_fd, "cgroup");
        dd_copy_file_at(dd, FILENAME_MOUNTINFO, proc_dir_fd, "mountinfo");

        FILE *open_fds = dd_open_item_file(dd, FILENAME_OPEN_FDS, O_RDWR);
        if (open_fds)
        {
            if (dump_fd_info_at(proc_dir_fd, open_fds) < 0)
                dd_delete_item(dd, FILENAME_OPEN_FDS);
            fclose(open_fds);
        }

        const int init_proc_dir_fd = open_proc_pid_dir(1);
        FILE *namespaces = dd_open_item_file(dd, FILENAME_NAMESPACES, O_RDWR);
        if (namespaces && init_proc_dir_fd >= 0)
        {
            if (dump_namespace_diff_at(init_proc_dir_fd, proc_dir_fd, namespaces) < 0)
                dd_delete_item(dd, FILENAME_NAMESPACES);
        }
        if (init_proc_dir_fd >= 0)
            close(init_proc_dir_fd);
        if (namespaces)
            fclose(namespaces);

        /* The process's root directory isn't the same as the init's root
         * directory. */
        if (rootdir)
        {
            if (strcmp(rootdir, "/") ==  0)
            {   /* We are dealing containerized process because root's
                 * directory path is '/' and that means that the process
                 * has mounted its own root.
                 * Seriously, it is possible if the process is running in its
                 * own MOUNT namespaces.
                 */
                log_debug("Process %d is considered to be containerized", pid);
                pid_t container_pid;
                if (get_pid_of_container_at(proc_dir_fd, &container_pid) == 0)
                {
                    char *container_cmdline = get_cmdline(container_pid);
                    dd_save_text(dd, FILENAME_CONTAINER_CMDLINE, container_cmdline);
                    free(container_cmdline);
                }
            }
            else
            {   /* We are dealing chrooted process. */
                dd_save_text(dd, FILENAME_ROOTDIR, rootdir);
            }
        }
        close(proc_dir_fd);
    }
    free(rootdir);

    /* Store id of the user whose application crashed. */
    char uid_str[sizeof(long) * 3 + 2];
    sprintf(uid_str, "%lu", (long)client_uid);
    dd_save_text(dd, FILENAME_UID, uid_str);

    GHashTableIter iter;
    gpointer gpkey;
    gpointer gpvalue;
    g_hash_table_iter_init(&iter, problem_info);
    while (g_hash_table_iter_next(&iter, &gpkey, &gpvalue))
    {
        dd_save_text(dd, (gchar *) gpkey, (gchar *) gpvalue);
    }

    dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);

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

    /* Closing STDIN_FILENO (abrtd duped the socket to stdin and stdout) and
     * not-replacing it with something else to let abrt-server die on reading
     * from invalid stdin - to catch bugs. */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    xdup2(STDERR_FILENO, STDOUT_FILENO); /* paranoia: don't leave stdout fd closed */

    /* Trim old problem directories if necessary */
    if (g_settings_nMaxCrashReportsSize > 0)
    {
        trim_problem_dirs(g_settings_dump_location, g_settings_nMaxCrashReportsSize * (double)(1024*1024), path);
    }

    run_post_create(path, NULL);

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

    return allowed_new_user_problem_entry(client_uid, key, value);
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

static int perform_http_xact(struct response *rsp)
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

    int ret = 0;
    if (url_type == CREATION_NOTIFICATION)
    {
        if (client_uid != 0)
        {
            error_msg("UID=%ld is not authorized to trigger post-create processing", (long)client_uid);
            ret = 403; /* Forbidden */
            goto out;
        }

        messagebuf_data[messagebuf_len] = '\0';
        return run_post_create(messagebuf_data, rsp);
    }

    die_if_data_is_missing(problem_info);

    /* Save problem dir */
    char *executable = g_hash_table_lookup(problem_info, FILENAME_EXECUTABLE);
    if (executable)
    {
        char *last_file = concat_path_file(g_settings_dump_location, "last-via-server");
        int repeating_crash = check_recent_crash_file(last_file, executable);
        free(last_file);
        if (repeating_crash) /* Only pretend that we saved it */
        {
            error_msg("Not saving repeating crash in '%s'", executable);
            goto out; /* ret is 0: "success" */
        }
    }

#if 0
//TODO:
    /* At least it should generate local problem identifier UUID */
    problem_data_add_basics(problem_info);
//...the problem being that problem_info here is not a problem_data_t!
#endif
    unsigned pid = convert_pid(problem_info);
    struct ns_ids client_ids;
    if (get_ns_ids(client_pid, &client_ids) < 0)
        error_msg_and_die("Cannot get peer's Namespaces from /proc/%d/ns", client_pid);

    if (client_ids.nsi_ids[PROC_NS_ID_PID] != g_ns_ids.nsi_ids[PROC_NS_ID_PID])
    {
        log_notice("Client is running in own PID Namespace, using PID %d instead of %d", client_pid, pid);
        pid = client_pid;
    }

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

    /* Get uid of the connected client */
    struct ucred cr;
    socklen_t crlen = sizeof(cr);
    if (0 != getsockopt(STDIN_FILENO, SOL_SOCKET, SO_PEERCRED, &cr, &crlen))
        perror_msg_and_die("getsockopt(SO_PEERCRED)");
    if (crlen != sizeof(cr))
        error_msg_and_die("%s: bad crlen %d", "getsockopt(SO_PEERCRED)", (int)crlen);

    if (client_uid == (uid_t)-1L)
        client_uid = cr.uid;

    client_pid = cr.pid;

    pid_t pid = getpid();
    if (get_ns_ids(getpid(), &g_ns_ids) < 0)
        error_msg_and_die("Cannot get own Namespaces from /proc/%d/ns", pid);

    load_abrt_conf();

    struct response rsp = { 0 };
    int r = perform_http_xact(&rsp);
    if (r == 0)
        r = 200;

    if (rsp.code == 0)
        rsp.code = r;

    free_abrt_conf_data();

    printf("HTTP/1.1 %u \r\n\r\n", rsp.code);
    if (rsp.message != NULL)
    {
        printf("%s", rsp.message);
        fflush(stdout);
        free(rsp.message);
    }

    return (r >= 400); /* Error if 400+ */
}
