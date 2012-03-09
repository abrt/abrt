/*
    Copyright (C) 2009  Jiri Moskovcak (jmoskovc@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#if HAVE_LOCALE_H
# include <locale.h>
#endif
#include <sys/un.h>
#include <syslog.h>
#include <sys/inotify.h>
#include <sys/ioctl.h> /* ioctl(FIONREAD) */

#include "libabrt.h"


/* I want to use -Werror, but gcc-4.4 throws a curveball:
 * "warning: ignoring return value of 'ftruncate', declared with attribute warn_unused_result"
 * and (void) cast is not enough to shut it up! Oh God...
 */
#define IGNORE_RESULT(func_call) do { if (func_call) /* nothing */; } while (0)


#define VAR_RUN_PIDFILE   VAR_RUN"/abrtd.pid"

#define SOCKET_FILE       VAR_RUN"/abrt/abrt.socket"
#define SOCKET_PERMISSION 0666
/* Maximum number of simultaneously opened client connections. */
#define MAX_CLIENT_COUNT  10


/* Daemon initializes, then sits in glib main loop, waiting for events.
 * Events can be:
 * - inotify: something new appeared under /var/spool/abrt
 * - signal: we got SIGTERM or SIGINT
 */
static volatile sig_atomic_t s_sig_caught;
static int s_signal_pipe[2];
static int s_signal_pipe_write = -1;
static int s_upload_watch = -1;
static unsigned s_timeout;
static bool s_exiting;

static GIOChannel *socket_channel = NULL;
static guint socket_channel_cb_id = 0;
static int socket_client_count = 0;


/* Helpers */

static pid_t spawn_event_handler_child(const char *dump_dir_name, const char *event_name, FILE **fpp)
{
    char *args[6];
    args[0] = (char *) "/usr/libexec/abrt-handle-event";
    args[1] = (char *) "-e";
    args[2] = (char *) event_name;
    args[3] = (char *) "--";
    args[4] = (char *) dump_dir_name;
    args[5] = NULL;

    int pipeout[2];
    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_QUIET | EXECFLG_ERR2OUT;
    VERB1 flags &= ~EXECFLG_QUIET;

    pid_t child = fork_execv_on_steroids(flags, args, pipeout,
                                         /*env_vec:*/ NULL, /*dir:*/ NULL, 0);

    *fpp = fdopen(pipeout[0], "r");
    if (!*fpp)
        die_out_of_memory();
    return child;
}

static guint add_watch_or_die(GIOChannel *channel, unsigned condition, GIOFunc func)
{
    errno = 0;
    guint r = g_io_add_watch(channel, (GIOCondition)condition, func, NULL);
    if (!r)
        perror_msg_and_die("g_io_add_watch failed");
    return r;
}


/* Socket handling */

/* Callback called by glib main loop when a client connects to ABRT's socket. */
static gboolean server_socket_cb(GIOChannel *source, GIOCondition condition, gpointer ptr_unused)
{
    /* Check the limit for number of simultaneously attached clients. */
    if (socket_client_count >= MAX_CLIENT_COUNT)
    {
        error_msg("Too many clients, refusing connections to '%s'", SOCKET_FILE);
        /* To avoid infinite loop caused by the descriptor in "ready" state,
         * the callback must be disabled.
         * It is added back in client_free(). */
        g_source_remove(socket_channel_cb_id);
        socket_channel_cb_id = 0;
        return TRUE;
    }

    int socket = accept(g_io_channel_unix_get_fd(source), NULL, NULL);
    if (socket == -1)
    {
        perror_msg("accept");
        return TRUE;
    }

    log("New client connected");
    pid_t pid = fork();
    if (pid < 0)
    {
        perror_msg("fork");
        close(socket);
        return TRUE;
    }
    if (pid == 0) /* child */
    {
        xmove_fd(socket, 0);
        xdup2(0, 1);

        char *argv[3];  /* abrt-server [-s] NULL */
        char **pp = argv;
        *pp++ = (char*)"abrt-server";
        if (logmode & LOGMODE_SYSLOG)
            *pp++ = (char*)"-s";
        *pp = NULL;

        execvp(argv[0], argv);
        perror_msg_and_die("Can't execute '%s'", argv[0]);
    }
    /* parent */
    socket_client_count++;
    close(socket);
    return TRUE;
}

/* Initializes the dump socket, usually in /var/run directory
 * (the path depends on compile-time configuration).
 */
static void dumpsocket_init()
{
    unlink(SOCKET_FILE); /* not caring about the result */

    int socketfd = xsocket(AF_UNIX, SOCK_STREAM, 0);
    close_on_exec_on(socketfd);

    struct sockaddr_un local;
    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCKET_FILE);
    xbind(socketfd, (struct sockaddr*)&local, sizeof(local));
    xlisten(socketfd, MAX_CLIENT_COUNT);

    if (chmod(SOCKET_FILE, SOCKET_PERMISSION) != 0)
        perror_msg_and_die("chmod '%s'", SOCKET_FILE);

    socket_channel = g_io_channel_unix_new(socketfd);
    g_io_channel_set_close_on_unref(socket_channel, TRUE);

    socket_channel_cb_id = add_watch_or_die(socket_channel, G_IO_IN | G_IO_PRI, server_socket_cb);
}

/* Releases all resources used by dumpsocket. */
static void dumpsocket_shutdown()
{
    /* Set everything to pre-initialization state. */
    if (socket_channel)
    {
        /* Undo add_watch_or_die */
        g_source_remove(socket_channel_cb_id);
        /* Undo g_io_channel_unix_new */
        g_io_channel_unref(socket_channel);
        socket_channel = NULL;
    }
}

static int create_pidfile()
{
    /* Note:
     * No O_EXCL: we would happily overwrite stale pidfile from previous boot.
     * No O_TRUNC: we must first try to lock the file, and if lock fails,
     * there is another live abrtd. O_TRUNCing the file in this case
     * would be wrong - it'll erase the pid to empty string!
     */
    int fd = open(VAR_RUN_PIDFILE, O_WRONLY|O_CREAT, 0644);
    if (fd >= 0)
    {
        if (lockf(fd, F_TLOCK, 0) < 0)
        {
            perror_msg("Can't lock file '%s'", VAR_RUN_PIDFILE);
            return -1;
        }
        close_on_exec_on(fd);
        /* write our pid to it */
        char buf[sizeof(long)*3 + 2];
        int len = sprintf(buf, "%lu\n", (long)getpid());
        IGNORE_RESULT(write(fd, buf, len));
        IGNORE_RESULT(ftruncate(fd, len));
        /* we leak opened+locked fd intentionally */
        return 0;
    }

    perror_msg("Can't open '%s'", VAR_RUN_PIDFILE);
    return -1;
}

static void handle_signal(int signo)
{
    int save_errno = errno;

    // Enable for debugging only, malloc/printf are unsafe in signal handlers
    //VERB3 log("Got signal %d", signo);

    uint8_t sig_caught;
    s_sig_caught = sig_caught = signo;
    /* Using local copy of s_sig_caught so that concurrent signal
     * won't change it under us */
    if (s_signal_pipe_write >= 0)
        IGNORE_RESULT(write(s_signal_pipe_write, &sig_caught, 1));

    errno = save_errno;
}

/* Signal pipe handler */
static gboolean handle_signal_cb(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused)
{
    uint8_t signo;
    gsize len = 0;
    g_io_channel_read_chars(gio, (void*) &signo, 1, &len, NULL);
    if (len == 1)
    {
        /* we did receive a signal */
        VERB3 log("Got signal %d through signal pipe", signo);
        if (signo != SIGCHLD)
            s_exiting = 1;
        else
        {
            pid_t pid;
            while ((pid = safe_waitpid(-1, NULL, WNOHANG)) > 0)
            {
                if (socket_client_count)
                    socket_client_count--;
                if (!socket_channel_cb_id)
                {
                    log("Accepting connections on '%s'", SOCKET_FILE);
                    socket_channel_cb_id = add_watch_or_die(socket_channel, G_IO_IN | G_IO_PRI, server_socket_cb);
                }
            }
        }
    }
    return TRUE; /* "please don't remove this event" */
}


/* Inotify handler */

static problem_data_t *load_problem_data(const char *dump_dir_name)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;

    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
    dd_close(dd);

    add_to_problem_data_ext(problem_data, CD_DUMPDIR, dump_dir_name,
                          CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

    return problem_data;
}

typedef enum {
    MW_OK,       /* No error */
    MW_OCCURRED, /* No error, but thus dir is a dup */
    MW_ERROR,    /* Other error (such as "post-create" event exited with nonzero */
} mw_result_t;

static mw_result_t run_post_create_and_load_data(const char *dump_dir_name, problem_data_t **problem_data)
{
    FILE *fp;
    pid_t child = spawn_event_handler_child(dump_dir_name, "post-create", &fp);

    char *buf;
    char *dup_of_dir = NULL;
    while ((buf = xmalloc_fgetline(fp)) != NULL)
    {
        /* Hmm, DUP_OF_DIR: ends up in syslog. move log() into 'else'? */
        log("%s", buf);

        if (strncmp("DUP_OF_DIR: ", buf, strlen("DUP_OF_DIR: ")) == 0)
        {
            overlapping_strcpy(buf, buf + strlen("DUP_OF_DIR: "));
            free(dup_of_dir);
            dup_of_dir = buf;
        }
        else
            free(buf);
    }
    fclose(fp);

    /* Prevent having zombie child process */
    int status;
    safe_waitpid(child, &status, 0);

    /* exit 0 means "this is a good, non-dup dir" */
    /* exit with 1 + "DUP_OF_DIR: dir" string => dup */
    if (status != 0)
    {
        if (WIFSIGNALED(status))
        {
            log("'post-create' on '%s' killed by signal %d",
                            dump_dir_name, WTERMSIG(status));
            return MW_ERROR;
        }
        /* else: it is WIFEXITED(status) */
        if (!dup_of_dir)
        {
            log("'post-create' on '%s' exited with %d",
                            dump_dir_name, WEXITSTATUS(status));
            return MW_ERROR;
        }
        dump_dir_name = dup_of_dir;
    }

    /* Loads problem_data (from the *first dir* if this one is a dup)
     * Returns:
     * MW_OCCURRED: "count is != 1" (iow: it is > 1 - dup)
     * MW_OK: "count is 1" (iow: this is a new problem, not a dup)
     * else: MW_ERROR
     */
    mw_result_t res = MW_ERROR;
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        /* dd_opendir already emitted error msg */
        goto ret;

    /* Reset mode/uig/gid to correct values for all files created by event run */
    dd_sanitize_mode_and_owner(dd);

    /* Update count */
    char *count_str = dd_load_text_ext(dd, FILENAME_COUNT, DD_FAIL_QUIETLY_ENOENT);
    unsigned long count = strtoul(count_str, NULL, 10);

    /* Don't increase crash count if we are working with newly uploaded
     * directory (remote crash) which already has it's crash count set.
     */
    if((status != 0 && dup_of_dir) || count == 0) {
        count++;
        char new_count_str[sizeof(long)*3 + 2];
        sprintf(new_count_str, "%lu", count);
        dd_save_text(dd, FILENAME_COUNT, new_count_str);
    }
    dd_close(dd);

    *problem_data = load_problem_data(dump_dir_name);
    if (*problem_data != NULL)
    {
        res = MW_OK;
        if (count > 1)
        {
            log("Problem directory is a duplicate of %s", dump_dir_name);
            res = MW_OCCURRED;
        }
    }
    /* else: load_problem_data already emitted error msg */

 ret:
    free(dup_of_dir);
    return res;
}

static gboolean handle_inotify_cb(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused)
{
    /* Default size: 128 simultaneous actions (about 1/2 meg) */
#define INOTIFY_BUF_SIZE ((sizeof(struct inotify_event) + FILENAME_MAX)*128)
    /* Determine how much to read (it usually is much smaller) */
    /* NB: this variable _must_ be int-sized, ioctl expects that! */
    int inotify_bytes = INOTIFY_BUF_SIZE;
    if (ioctl(g_io_channel_unix_get_fd(gio), FIONREAD, &inotify_bytes) != 0
     || inotify_bytes < sizeof(struct inotify_event)
     || inotify_bytes > INOTIFY_BUF_SIZE
    ) {
        inotify_bytes = INOTIFY_BUF_SIZE;
    }
    VERB3 log("FIONREAD:%d", inotify_bytes);

    char *buf = (char*)xmalloc(inotify_bytes);
    errno = 0;
    gsize len;
    GError *gerror = NULL;
    GIOStatus err = g_io_channel_read_chars(gio, buf, inotify_bytes, &len, &gerror);
    if (err != G_IO_STATUS_NORMAL)
    {
        perror_msg("Error reading inotify fd: %s", gerror ? gerror->message : "unknown");
        free(buf);
        return FALSE; /* "remove this event" (huh??) */
    }

    /* Reconstruct each event */
    gsize i = 0;
    while (i < len)
    {
        struct inotify_event *event = (struct inotify_event *) &buf[i];
        const char *name = NULL;
        if (event->len)
            name = event->name;
        //log("i:%d len:%d event->mask:%x IN_ISDIR:%x IN_CLOSE_WRITE:%x event->len:%d",
        //    i, len, event->mask, IN_ISDIR, IN_CLOSE_WRITE, event->len);
        i += sizeof(*event) + event->len;

        if (event->wd == s_upload_watch)
        {
            /* Was the (presumable newly created) file closed in upload dir,
             * or a file moved to upload dir? */
            if (!(event->mask & IN_ISDIR)
             && event->mask & (IN_CLOSE_WRITE|IN_MOVED_TO)
             && name
            ) {
                const char *ext = strrchr(name, '.');
                if (ext && strcmp(ext + 1, "working") == 0)
                    continue;

                const char *dir = g_settings_sWatchCrashdumpArchiveDir;
                log("Detected creation of file '%s' in upload directory '%s'", name, dir);
                if (fork() == 0)
                {
                    xchdir(dir);
                    execlp("abrt-handle-upload", "abrt-handle-upload",
                           g_settings_dump_location, dir, name, (char*)NULL);
                    error_msg_and_die("Can't execute '%s'", "abrt-handle-upload");
                }
            }
            continue;
        }

        if (!(event->mask & IN_ISDIR) || !name)
        {
            /* ignore lock files and such */
            // Happens all the time during normal run
            //VERB3 log("File '%s' creation detected, ignoring", name);
            continue;
        }
        const char *ext = strrchr(name, '.');
        if (ext && strcmp(ext, ".new") == 0)
        {
            //VERB3 log("Directory '%s' creation detected, ignoring", name);
            continue;
        }
        log("Directory '%s' creation detected", name);

        if (g_settings_nMaxCrashReportsSize > 0)
        {
            char *worst_dir = NULL;
            while (g_settings_nMaxCrashReportsSize > 0
             && get_dirsize_find_largest_dir(g_settings_dump_location, &worst_dir, name) / (1024*1024) >= g_settings_nMaxCrashReportsSize
             && worst_dir
            ) {
                log("Size of '%s' >= %u MB, deleting '%s'",
                    g_settings_dump_location, g_settings_nMaxCrashReportsSize, worst_dir);
                /* deletes both directory and DB record */
                char *d = concat_path_file(g_settings_dump_location, worst_dir);
                free(worst_dir);
                worst_dir = NULL;
                delete_dump_dir(d);
                free(d);
            }
        }

        char *fullname = concat_path_file(g_settings_dump_location, name);
        problem_data_t *problem_data = NULL;
        mw_result_t res = run_post_create_and_load_data(fullname, &problem_data);
        switch (res)
        {
            case MW_OK:
                log("New problem directory %s, processing", fullname);
                /* Fall through */

            case MW_OCCURRED: /* dup */
            {
                const char *first = NULL;
                if (res != MW_OK)
                {
                    /* Fetch the name of older directory of which we are a dup */
                    first = get_problem_item_content_or_NULL(problem_data, CD_DUMPDIR);

                    log("Deleting problem directory %s (dup of %s)",
                            strrchr(fullname, '/') + 1,
                            strrchr(first, '/') + 1);
                    delete_dump_dir(fullname);
                }

                /* Run "notify[_dup]" event */
                FILE *fp;
                pid_t child = spawn_event_handler_child(
                                (first ? first : fullname),
                                (first ? "notify_dup" : "notify"),
                                &fp
                );
                char *buf;
                while ((buf = xmalloc_fgetline(fp)) != NULL)
                {
                    log("%s", buf);
                    free(buf);
                }
                fclose(fp);
                /* Prevent having zombie child process */
                safe_waitpid(child, NULL, 0);

                break;
            }
            default: /* always MW_ERROR */
                log("Corrupted or bad directory %s, deleting", fullname);
                delete_dump_dir(fullname);
                break;
        }
        free(fullname);
        free_problem_data(problem_data);
    } /* while */

    free(buf);
    return TRUE; /* "please don't remove this event" */
}


/* Run main loop with idle timeout.
 * Basically, almost like glib's g_main_run(loop)
 */
static void run_main_loop(GMainLoop* loop)
{
    time_t cur_time = time(NULL);
    GMainContext *context = g_main_loop_get_context(loop);
    int fds_size = 0;
    GPollFD *fds = NULL;

    while (!s_exiting)
    {
        gboolean some_ready;
        gint max_priority;
        gint timeout;
        gint nfds;

        some_ready = g_main_context_prepare(context, &max_priority);
        if (some_ready)
            g_main_context_dispatch(context);

        while (1)
        {
            nfds = g_main_context_query(context, max_priority, &timeout, fds, fds_size);
            if (nfds <= fds_size)
                break;
            fds_size = nfds + 16; /* +16: optimizing realloc frequency */
            fds = (GPollFD *)xrealloc(fds, fds_size * sizeof(fds[0]));
        }

        if (s_timeout != 0)
            alarm(s_timeout);
        g_poll(fds, nfds, timeout);
        if (s_timeout != 0)
            alarm(0);

        time_t new_time = time(NULL);
        if (cur_time != new_time)
        {
            cur_time = new_time;
            load_abrt_conf();
//TODO: react to changes in g_settings_sWatchCrashdumpArchiveDir
        }

        some_ready = g_main_context_check(context, max_priority, fds, nfds);
        if (some_ready)
            g_main_context_dispatch(context);
    }

    free(fds);
    g_main_context_unref(context);
}

static void start_syslog_logging()
{
    /* Open stdin to /dev/null */
    xmove_fd(xopen("/dev/null", O_RDWR), STDIN_FILENO);
    /* We must not leave fds 0,1,2 closed.
     * Otherwise fprintf(stderr) dumps messages into random fds, etc. */
    xdup2(STDIN_FILENO, STDOUT_FILENO);
    xdup2(STDIN_FILENO, STDERR_FILENO);
    openlog(g_progname, 0, LOG_DAEMON);
    logmode = LOGMODE_SYSLOG;
    putenv((char*)"ABRT_SYSLOG=1");
}

static void ensure_writable_dir(const char *dir, mode_t mode, const char *user)
{
    struct stat sb;

    if (mkdir(dir, mode) != 0 && errno != EEXIST)
        perror_msg_and_die("Can't create '%s'", dir);
    if (stat(dir, &sb) != 0 || !S_ISDIR(sb.st_mode))
        error_msg_and_die("'%s' is not a directory", dir);

    struct passwd *pw = getpwnam(user);
    if (!pw)
        perror_msg_and_die("Can't find user '%s'", user);

    if ((sb.st_uid != pw->pw_uid || sb.st_gid != pw->pw_gid) && chown(dir, pw->pw_uid, pw->pw_gid) != 0)
        perror_msg_and_die("Can't set owner %u:%u on '%s'", (unsigned int)pw->pw_uid, (unsigned int)pw->pw_gid, dir);
    if ((sb.st_mode & 07777) != mode && chmod(dir, mode) != 0)
        perror_msg_and_die("Can't set mode %o on '%s'", mode, dir);
}

static void sanitize_dump_dir_rights()
{
    /* We can't allow everyone to create dumps: otherwise users can flood
     * us with thousands of bogus or malicious dumps */
    /* 07000 bits are setuid, setgit, and sticky, and they must be unset */
    /* 00777 bits are usual "rwxrwxrwx" access rights */
    ensure_writable_dir(g_settings_dump_location, 0755, "abrt");
    /* temp dir */
    ensure_writable_dir(VAR_RUN"/abrt", 0755, "root");
}

int main(int argc, char** argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    int parent_pid = getpid();

    const char *program_usage_string = _(
        "& [options]"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_s = 1 << 2,
// TODO: get rid of -t NUM, it is no longer useful since dbus is moved to a separate tool
        OPT_t = 1 << 3,
        OPT_p = 1 << 4,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(   'd', NULL, NULL      , _("Do not daemonize")),
        OPT_BOOL(   's', NULL, NULL      , _("Log to syslog even with -d")),
        OPT_INTEGER('t', NULL, &s_timeout, _("Exit after NUM seconds of inactivity")),
        OPT_BOOL(   'p', NULL, NULL      , _("Add program names to log")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(opts & OPT_p);

#if 0 /* We no longer use dbus */
    /* When dbus daemon starts us, it doesn't set PATH
     * (I saw it set only DBUS_STARTER_ADDRESS and DBUS_STARTER_BUS_TYPE).
     * In this case, set something sane:
     */
    const char *env_path = getenv("PATH");
    if (!env_path || !env_path[0])
        putenv((char*)"PATH=/usr/sbin:/usr/bin:/sbin:/bin");
#endif

    unsetenv("ABRT_SYSLOG");
    msg_prefix = g_progname; /* for log(), error_msg() and such */

    if (getuid() != 0)
        error_msg_and_die("Must be run as root");

    if (opts & OPT_s)
        start_syslog_logging();

    xpipe(s_signal_pipe);
    close_on_exec_on(s_signal_pipe[0]);
    close_on_exec_on(s_signal_pipe[1]);
    ndelay_on(s_signal_pipe[0]); /* I/O should not block - */
    ndelay_on(s_signal_pipe[1]); /* especially writes! they happen in signal handler! */
    signal(SIGTERM, handle_signal);
    signal(SIGINT,  handle_signal);
    signal(SIGCHLD, handle_signal);
    if (s_timeout != 0)
        signal(SIGALRM, handle_signal);

    /* Daemonize unless -d */
    if (!(opts & OPT_d))
    {
        /* forking to background */
        pid_t pid = fork();
        if (pid < 0)
        {
            perror_msg_and_die("fork");
        }
        if (pid > 0)
        {
            /* Parent */
            /* Wait for child to notify us via SIGTERM that it feels ok */
            int i = 20; /* 2 sec */
            while (s_sig_caught == 0 && --i)
            {
                usleep(100 * 1000);
            }
            if (s_sig_caught == SIGTERM)
            {
                exit(0);
            }
            if (s_sig_caught)
            {
                error_msg_and_die("Failed to start: got sig %d", s_sig_caught);
            }
            error_msg_and_die("Failed to start: timeout waiting for child");
        }
        /* Child (daemon) continues */
        setsid(); /* never fails */
        if (g_verbose == 0 && logmode != LOGMODE_SYSLOG)
            start_syslog_logging();
    }

    GMainLoop* pMainloop = NULL;
    GIOChannel* channel_inotify = NULL;
    guint channel_inotify_event_id = 0;
    GIOChannel* channel_signal = NULL;
    guint channel_signal_event_id = 0;
    bool pidfile_created = false;

    /* Initialization */
    VERB1 log("Loading settings");
    if (load_abrt_conf() != 0)
        goto init_error;

    sanitize_dump_dir_rights();

    VERB1 log("Creating glib main loop");
    pMainloop = g_main_loop_new(NULL, FALSE);

    VERB1 log("Initializing inotify");
    errno = 0;
    int inotify_fd = inotify_init();
    if (inotify_fd == -1)
        perror_msg_and_die("inotify_init failed");
    close_on_exec_on(inotify_fd);

    /* Watching 'g_settings_dump_location' for new files... */
    if (inotify_add_watch(inotify_fd, g_settings_dump_location, IN_CREATE | IN_MOVED_TO) < 0)
    {
        perror_msg("inotify_add_watch failed on '%s'", g_settings_dump_location);
        goto init_error;
    }
    if (g_settings_sWatchCrashdumpArchiveDir)
    {
        s_upload_watch = inotify_add_watch(inotify_fd, g_settings_sWatchCrashdumpArchiveDir, IN_CLOSE_WRITE|IN_MOVED_TO);
        if (s_upload_watch < 0)
        {
            perror_msg("inotify_add_watch failed on '%s'", g_settings_sWatchCrashdumpArchiveDir);
            goto init_error;
        }
    }
    VERB1 log("Adding inotify watch to glib main loop");
    channel_inotify = g_io_channel_unix_new(inotify_fd);

    GError *gerror = NULL;
    g_io_channel_set_encoding(channel_inotify, NULL, &gerror);
    /* need to set the encoding otherwise we get:
     * Invalid byte sequence in conversion input
     * according to manual "NULL" is safe for binary data
    */
    if (gerror)
        perror_msg("Can't set encoding on gio channel: '%s'", gerror->message);

    channel_inotify_event_id = g_io_add_watch(channel_inotify,
                                              G_IO_IN,
                                              handle_inotify_cb,
                                              NULL);

    /* Add an event source which waits for INT/TERM signal */
    VERB1 log("Adding signal pipe watch to glib main loop");
    channel_signal = g_io_channel_unix_new(s_signal_pipe[0]);
    channel_signal_event_id = g_io_add_watch(channel_signal,
                                             G_IO_IN,
                                             handle_signal_cb,
                                             NULL);

    /* Mark the territory */
    VERB1 log("Creating pid file");
    if (create_pidfile() != 0)
        goto init_error;
    pidfile_created = true;

    /* Open socket to receive new problem data (from python etc). */
    dumpsocket_init();

    /* Inform parent that we initialized ok */
    if (!(opts & OPT_d))
    {
        VERB1 log("Signalling parent");
        kill(parent_pid, SIGTERM);
        if (logmode != LOGMODE_SYSLOG)
            start_syslog_logging();
    }

    /* Only now we want signal pipe to work */
    s_signal_pipe_write = s_signal_pipe[1];

    /* Enter the event loop */
    log("Init complete, entering main loop");
    run_main_loop(pMainloop);

 cleanup:
    /* Error or INT/TERM. Clean up, in reverse order.
     * Take care to not undo things we did not do.
     */
    dumpsocket_shutdown();
    if (pidfile_created)
        unlink(VAR_RUN_PIDFILE);

    if (channel_signal_event_id > 0)
        g_source_remove(channel_signal_event_id);
    if (channel_signal)
        g_io_channel_unref(channel_signal);
    if (channel_inotify_event_id > 0)
        g_source_remove(channel_inotify_event_id);
    if (channel_inotify)
        g_io_channel_unref(channel_inotify);

    if (pMainloop)
        g_main_loop_unref(pMainloop);

    free_abrt_conf_data();

    /* Exiting */
    if (s_sig_caught && s_sig_caught != SIGALRM && s_sig_caught != SIGCHLD)
    {
        error_msg("Got signal %d, exiting", s_sig_caught);
        signal(s_sig_caught, SIG_DFL);
        raise(s_sig_caught);
    }
    error_msg_and_die("Exiting");

 init_error:
    /* Initialization error */
    error_msg("Error while initializing daemon");
    /* Inform parent that initialization failed */
    if (!(opts & OPT_d))
        kill(parent_pid, SIGINT);
    goto cleanup;
}
