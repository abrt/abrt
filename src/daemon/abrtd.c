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

#include "abrt_glib.h"
#include "abrt-inotify.h"
#include "libabrt.h"
#include "problem_api.h"


/* I want to use -Werror, but gcc-4.4 throws a curveball:
 * "warning: ignoring return value of 'ftruncate', declared with attribute warn_unused_result"
 * and (void) cast is not enough to shut it up! Oh God...
 */
#define IGNORE_RESULT(func_call) do { if (func_call) /* nothing */; } while (0)


#define VAR_RUN_PIDFILE   VAR_RUN"/abrt/abrtd.pid"

#define SOCKET_FILE       VAR_RUN"/abrt/abrt.socket"
#define SOCKET_PERMISSION 0666
/* Maximum number of simultaneously opened client connections. */
#define MAX_CLIENT_COUNT  10

#define IN_DUMP_LOCATION_FLAGS (IN_DELETE_SELF | IN_MOVE_SELF)

/* Daemon initializes, then sits in glib main loop, waiting for events.
 * Events can be:
 * - inotify: something new appeared under /var/tmp/abrt or /var/spool/abrt-upload
 * - signal: we got SIGTERM, SIGINT, SIGALRM or SIGCHLD
 * - new socket connection
 */
static volatile sig_atomic_t s_sig_caught;
static int s_signal_pipe[2];
static int s_signal_pipe_write = -1;
static unsigned s_timeout;
static bool s_exiting;

static GIOChannel *channel_socket = NULL;
static guint channel_id_socket = 0;
static int child_count = 0;

/* Helpers */
static guint add_watch_or_die(GIOChannel *channel, unsigned condition, GIOFunc func)
{
    errno = 0;
    guint r = g_io_add_watch(channel, (GIOCondition)condition, func, NULL);
    if (!r)
        perror_msg_and_die("g_io_add_watch failed");
    return r;
}

static void increment_child_count(void)
{
    if (++child_count >= MAX_CLIENT_COUNT)
    {
        error_msg("Too many clients, refusing connections to '%s'", SOCKET_FILE);
        /* To avoid infinite loop caused by the descriptor in "ready" state,
         * the callback must be disabled.
         */
        g_source_remove(channel_id_socket);
        channel_id_socket = 0;
    }
}

static gboolean server_socket_cb(GIOChannel *source, GIOCondition condition, gpointer ptr_unused);

static void decrement_child_count(void)
{
    if (child_count)
        child_count--;
    if (child_count < MAX_CLIENT_COUNT && !channel_id_socket)
    {
        log_info("Accepting connections on '%s'", SOCKET_FILE);
        channel_id_socket = add_watch_or_die(channel_socket, G_IO_IN | G_IO_PRI | G_IO_HUP, server_socket_cb);
    }
}

/* Callback called by glib main loop when a client connects to ABRT's socket. */
static gboolean server_socket_cb(GIOChannel *source, GIOCondition condition, gpointer ptr_unused)
{
    int socket = accept(g_io_channel_unix_get_fd(source), NULL, NULL);
    if (socket == -1)
    {
        perror_msg("accept");
        return TRUE;
    }

    log_notice("New client connected");
    fflush(NULL); /* paranoia */
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
        if (logmode & LOGMODE_JOURNAL)
            *pp++ = (char*)"-s";
        *pp = NULL;

        execvp(argv[0], argv);
        perror_msg_and_die("Can't execute '%s'", argv[0]);
    }
    /* parent */
    increment_child_count();
    close(socket);
    return TRUE;
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
        log_debug("Got signal %d through signal pipe", signo);
        if (signo != SIGCHLD)
            s_exiting = 1;
        else
        {
            while (safe_waitpid(-1, NULL, WNOHANG) > 0)
            {
                decrement_child_count();
            }
        }
    }
    return TRUE; /* "please don't remove this event" */
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

    if ((sb.st_uid != pw->pw_uid || sb.st_gid != pw->pw_gid) && lchown(dir, pw->pw_uid, pw->pw_gid) != 0)
        perror_msg_and_die("Can't set owner %u:%u on '%s'", (unsigned int)pw->pw_uid, (unsigned int)pw->pw_gid, dir);
    if ((sb.st_mode & 07777) != mode && chmod(dir, mode) != 0)
        perror_msg_and_die("Can't set mode %o on '%s'", mode, dir);
}

static void sanitize_dump_dir_rights(void)
{
    /* We can't allow everyone to create dumps: otherwise users can flood
     * us with thousands of bogus or malicious dumps */
    /* 07000 bits are setuid, setgit, and sticky, and they must be unset */
    /* 00777 bits are usual "rwxrwxrwx" access rights */
    ensure_writable_dir(g_settings_dump_location, 0755, "abrt");
    /* temp dir */
    ensure_writable_dir(VAR_RUN"/abrt", 0755, "root");
}

/* Inotify handler */

static void handle_inotify_cb(struct abrt_inotify_watch *watch, struct inotify_event *event, gpointer ptr_unused)
{
/* We no longer need a name of event */
#if 0
        const char *name = NULL;
        if (event->len)
            name = event->name;
#endif
        //log("i:%d len:%d event->mask:%x IN_ISDIR:%x IN_CLOSE_WRITE:%x event->len:%d",
        //    i, len, event->mask, IN_ISDIR, IN_CLOSE_WRITE, event->len);

        if (event->mask & IN_DELETE_SELF || event->mask & IN_MOVE_SELF)
        {
            log_warning("Recreating deleted dump location '%s'", g_settings_dump_location);

            sanitize_dump_dir_rights();
            abrt_inotify_watch_reset(watch, g_settings_dump_location, IN_DUMP_LOCATION_FLAGS);
        }
/* We no longer watch for subdirectory creations */
#if 0
        if (!(event->mask & IN_ISDIR) || !name)
        {
            /* ignore lock files and such */
            // Happens all the time during normal run
            //log_debug("File '%s' creation detected, ignoring", name);
            continue;
        }
        const char *ext = strrchr(name, '.');
        if (ext && strcmp(ext, ".new") == 0)
        {
            //log_debug("Directory '%s' creation detected, ignoring", name);
            continue;
        }
        log("Directory '%s' creation detected", name);
        ...
#endif
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

/* Initializes the dump socket, usually in /var/run directory
 * (the path depends on compile-time configuration).
 */
static void dumpsocket_init(void)
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

    channel_socket = abrt_gio_channel_unix_new(socketfd);

    channel_id_socket = add_watch_or_die(channel_socket, G_IO_IN | G_IO_PRI | G_IO_HUP, server_socket_cb);
}

/* Releases all resources used by dumpsocket. */
static void dumpsocket_shutdown(void)
{
    /* Set everything to pre-initialization state. */
    if (channel_socket)
    {
        /* Undo add_watch_or_die */
        g_source_remove(channel_id_socket);
        /* Undo g_io_channel_unix_new */
        g_io_channel_unref(channel_socket);
        channel_socket = NULL;
    }
}

static int create_pidfile(void)
{
    /* Note:
     * No O_EXCL: we would happily overwrite stale pidfile from previous boot.
     * No O_TRUNC: we must first try to lock the file, and if lock fails,
     * there is another live abrtd. O_TRUNCing the file in this case
     * would be wrong - it'll erase the pid to empty string!
     */
    int fd = open(VAR_RUN_PIDFILE, O_RDWR|O_CREAT, 0644);
    if (fd >= 0)
    {
        if (lockf(fd, F_TLOCK, 0) < 0)
        {
            perror_msg("Can't lock file '%s'", VAR_RUN_PIDFILE);
            /* should help with problems like rhbz#859724 */
            char pid_str[sizeof(long)*3 + 4];
            int r = full_read(fd, pid_str, sizeof(pid_str));
            close(fd);

            /* File can contain garbage. Be careful interpreting it as PID */
            if (r > 0)
            {
                pid_str[r] = '\0';
                errno = 0;
                long locking_pid = strtol(pid_str, NULL, 10);
                if (!errno && locking_pid > 0 && locking_pid <= INT_MAX)
                {
                    char *cmdline = get_cmdline(locking_pid);
                    if (cmdline)
                    {
                        error_msg("Process %lu '%s' is holding the lock", locking_pid, cmdline);
                        free(cmdline);
                    }
                }
            }

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
    //log_debug("Got signal %d", signo);

    uint8_t sig_caught;
    s_sig_caught = sig_caught = signo;
    /* Using local copy of s_sig_caught so that concurrent signal
     * won't change it under us */
    if (s_signal_pipe_write >= 0)
        IGNORE_RESULT(write(s_signal_pipe_write, &sig_caught, 1));

    errno = save_errno;
}


static void start_logging(void)
{
    /* Open stdin to /dev/null */
    xmove_fd(xopen("/dev/null", O_RDWR), STDIN_FILENO);
    /* We must not leave fds 0,1,2 closed.
     * Otherwise fprintf(stderr) dumps messages into random fds, etc. */
    xdup2(STDIN_FILENO, STDOUT_FILENO);
    xdup2(STDIN_FILENO, STDERR_FILENO);
    logmode = LOGMODE_JOURNAL;
    putenv((char*)"ABRT_SYSLOG=1");
}

/* The function expects that FILENAME_COUNT dump dir element is created by
 * abrtd after all post-create events are successfully done. Thus if
 * FILENAME_COUNT element doesn't exist abrtd can consider the dump directory
 * as unprocessed.
 *
 * Relying on content of dump directory has one problem. If a hook provides
 * FILENAME_COUNT abrtd will consider the dump directory as processed.
 */
static void mark_unprocessed_dump_dirs_not_reportable(const char *path)
{
    log_notice("Searching for unprocessed dump directories");

    DIR *dp = opendir(path);
    if (!dp)
    {
        perror_msg("Can't open directory '%s'", path);
        return;
    }

    struct dirent *dent;
    while ((dent = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue; /* skip "." and ".." */

        char *full_name = concat_path_file(path, dent->d_name);

        struct stat stat_buf;
        if (stat(full_name, &stat_buf) != 0)
        {
            perror_msg("Can't access path '%s'", full_name);
            goto next_dd;
        }

        if (S_ISDIR(stat_buf.st_mode) == 0)
            /* This is expected. The dump location contains some aux files */
            goto next_dd;

        struct dump_dir *dd = dd_opendir(full_name, /*flags*/0);
        if (dd)
        {
            if (!problem_dump_dir_is_complete(dd) && !dd_exist(dd, FILENAME_NOT_REPORTABLE))
            {
                log_warning("Marking '%s' not reportable (no '"FILENAME_COUNT"' item)", full_name);

                dd_save_text(dd, FILENAME_NOT_REPORTABLE, _("The problem data are "
                            "incomplete. This usually happens when a problem "
                            "is detected while computer is shutting down or "
                            "user is logging out. In order to provide "
                            "valuable problem reports, ABRT will not allow "
                            "you to submit this problem. If you have time and "
                            "want to help the developers in their effort to "
                            "sort out this problem, please contact them directly."));

            }
            dd_close(dd);
        }

  next_dd:
        free(full_name);
    }
    closedir(dp);
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
        start_logging();

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

    GMainLoop* pMainloop = NULL;
    GIOChannel* channel_signal = NULL;
    guint channel_id_signal_event = 0;
    bool pidfile_created = false;
    struct abrt_inotify_watch *aiw = NULL;

    /* Initialization */
    log_notice("Loading settings");
    if (load_abrt_conf() != 0)
        goto init_error;

    /* Moved before daemonization because parent waits for signal from daemon
     * only for short period and time consumed by
     * mark_unprocessed_dump_dirs_not_reportable() is slightly unpredictable.
     */
    sanitize_dump_dir_rights();
    mark_unprocessed_dump_dirs_not_reportable(g_settings_dump_location);

    /* Daemonize unless -d */
    if (!(opts & OPT_d))
    {
        /* forking to background */
        fflush(NULL); /* paranoia */
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
        if (setsid() < 0)
            perror_msg_and_die("setsid");
        if (g_verbose == 0 && logmode != LOGMODE_JOURNAL)
            start_logging();
    }

    log_notice("Creating glib main loop");
    pMainloop = g_main_loop_new(NULL, FALSE);

    /* Watching 'g_settings_dump_location' for delete self
     * because hooks expects that the dump location exists if abrtd is running
     */
    aiw = abrt_inotify_watch_init(g_settings_dump_location,
            IN_DUMP_LOCATION_FLAGS, handle_inotify_cb, /*user data*/NULL);

    /* Add an event source which waits for INT/TERM signal */
    log_notice("Adding signal pipe watch to glib main loop");
    channel_signal = abrt_gio_channel_unix_new(s_signal_pipe[0]);
    channel_id_signal_event = add_watch_or_die(channel_signal,
                        G_IO_IN | G_IO_PRI | G_IO_HUP,
                        handle_signal_cb);

    /* Mark the territory */
    log_notice("Creating pid file");
    if (create_pidfile() != 0)
        goto init_error;
    pidfile_created = true;

    /* Open socket to receive new problem data (from python etc). */
    dumpsocket_init();

    /* Inform parent that we initialized ok */
    if (!(opts & OPT_d))
    {
        log_notice("Signalling parent");
        kill(parent_pid, SIGTERM);
        if (logmode != LOGMODE_JOURNAL)
            start_logging();
    }

    /* Only now we want signal pipe to work */
    s_signal_pipe_write = s_signal_pipe[1];

    /* Enter the event loop */
    log_debug("Init complete, entering main loop");
    run_main_loop(pMainloop);

 cleanup:
    /* Error or INT/TERM. Clean up, in reverse order.
     * Take care to not undo things we did not do.
     */
    dumpsocket_shutdown();
    if (pidfile_created)
        unlink(VAR_RUN_PIDFILE);

    if (channel_id_signal_event > 0)
        g_source_remove(channel_id_signal_event);
    if (channel_signal)
        g_io_channel_unref(channel_signal);

    abrt_inotify_watch_destroy(aiw);

    if (pMainloop)
        g_main_loop_unref(pMainloop);

    free_abrt_conf_data();

    /* Exiting */
    if (s_sig_caught && s_sig_caught != SIGALRM && s_sig_caught != SIGCHLD)
    {
        /* We use TERM to stop abrtd, so not printing out error message. */
        if (s_sig_caught != SIGTERM)
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
