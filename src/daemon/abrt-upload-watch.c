/*
    Copyright (C) 2013  ABRT Team
    Copyright (C) 2013  Red Hat, Inc.

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
#include "abrt-inotify.h"
#include "abrt_glib.h"
#include "libabrt.h"

#define STRINGIZE_DETAIL(str) #str
#define STRINGIZE(str) STRINGIZE_DETAIL(str)

#define DEFAULT_COUNT_OF_WORKERS 10
#define DEFAULT_CACHE_MIB_SIZE 4

static int g_signal_pipe[2];

struct queue
{
    unsigned capacity;
    GQueue q;
};

static int
queue_push(struct queue *queue, char *value)
{
    if (g_queue_get_length(&queue->q) >= queue->capacity)
        return 0;

    g_queue_push_head(&queue->q, value);

    return 1;
}

static char *
queue_pop(struct queue *queue)
{
    if (g_queue_is_empty(&queue->q))
        return NULL;

    return (char *)g_queue_pop_tail(&queue->q);
}

struct process
{
    GMainLoop *main_loop;
    const char *upload_directory;
    unsigned children;
    unsigned max_children;
    struct queue queue;
};

static void
process_quit(struct process *proc)
{
    g_main_loop_quit(proc->main_loop);
}

static void
run_abrt_handle_upload(struct process *proc, const char *name)
{
    log_info("Processing file '%s' in directory '%s'", name, proc->upload_directory);

    ++proc->children;
    log_debug("Running workers: %d", proc->children);

    fflush(NULL); /* paranoia */
    pid_t pid = fork();
    if (pid < 0)
    {
        --proc->children;
        perror_msg("fork");
        return;
    }

    if (pid == 0)
    {
        /* child */
        xchdir(proc->upload_directory);
        if (g_settings_delete_uploaded)
            execlp("abrt-handle-upload", "abrt-handle-upload", "-d",
                           g_settings_dump_location, proc->upload_directory, name, (char*)NULL);
        else
            execlp("abrt-handle-upload", "abrt-handle-upload",
                           g_settings_dump_location, proc->upload_directory, name, (char*)NULL);
        perror_msg_and_die("Can't execute '%s'", "abrt-handle-upload");
    }
}

static void
handle_new_path(struct process *proc, char *name)
{
    log("Detected creation of file '%s' in upload directory '%s'", name, proc->upload_directory);

    if (proc->children < proc->max_children)
    {
        run_abrt_handle_upload(proc, name);
        free(name);
        return;
    }

    log_debug("Pushing '%s' to deferred queue", name);
    if (!queue_push(&proc->queue, name))
    {
        error_msg(_("No free workers and full buffer. Omitting archive '%s'"), name);
        free(name);
        return;
    }
}

static void
print_stats(struct process *proc)
{
    /* this is meant only for debugging, so not marking it as translatable */
    fprintf(stderr, "%i archives to process, %i active workers\n", g_queue_get_length(&proc->queue.q), proc->children);
}

static void
process_next_in_queue(struct process *proc)
{
    char *name = queue_pop(&proc->queue);
    if (!name)
    {
        log_debug("Deferred queue is empty. Running workers: %d", proc->children);
        return;
    }

    run_abrt_handle_upload(proc, name);
    free(name);
}

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
    struct process *proc = (struct process *)user_data;
    uint8_t signals[DEFAULT_COUNT_OF_WORKERS];
    gsize len = 0;

    for (;;)
    {
        GError *error = NULL;
        GIOStatus stat = g_io_channel_read_chars(gio, (void *)signals, sizeof(signals), &len, NULL);
        if (stat == G_IO_STATUS_ERROR)
        {
            error_msg_and_die(_("Can't read from gio channel: '%s'"), error ? error->message : "");
        }
        if (stat == G_IO_STATUS_AGAIN)
        {   /* We got all buffered data, but fd is still open. Done for now */
            return TRUE; /* "glib, please don't remove this event (yet)" */
        }
        if (stat == G_IO_STATUS_EOF)
            break;

        /* G_IO_STATUS_NORMAL */
        for (unsigned signo = 0; signo < len; ++signo)
        {
            /* we did receive a signal */
            log_debug("Got signal %d through signal pipe", signals[signo]);
            if (signals[signo] == SIGUSR1)
            {
                print_stats(proc);
            }
            else if (signals[signo] != SIGCHLD)
            {
                process_quit(proc);
                return FALSE; /* remove this event */
            }
            else
            {
                while (safe_waitpid(-1, NULL, WNOHANG) > 0)
                {
                    --proc->children;
                    process_next_in_queue(proc);
                    print_stats(proc);
                }
            }
        }
    }

    return TRUE; /* "please don't remove this event" */
}

static void
handle_inotify_cb(struct abrt_inotify_watch *watch, struct inotify_event *event, void *user_data)
{
    /* Was the (presumable newly created) file closed in upload dir,
     * or a file moved to upload dir? */
    if (!(event->mask & IN_ISDIR) && (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)))
    {
        const char *ext = strrchr(event->name, '.');
        if (ext && strcmp(ext + 1, "working") == 0)
            return;

        handle_new_path((struct process *)user_data, xstrdup(event->name));
    }
}

static void
daemonize()
{
    /* forking to background */
    fflush(NULL); /* paranoia */
    pid_t pid = fork();
    if (pid < 0)
        perror_msg_and_die("fork");
    if (pid > 0)
        exit(0);

    /* Child (daemon) continues */
    if (setsid() < 0)
        perror_msg_and_die("setsid");

    /* Change the current working directory */
    xchdir("/");

    /* Reopen the standard file descriptors to "/dev/null" */
    xmove_fd(xopen("/dev/null", O_RDWR), STDIN_FILENO);
    xdup2(STDIN_FILENO, STDOUT_FILENO);
    xdup2(STDIN_FILENO, STDERR_FILENO);
}

int
main(int argc, char **argv)
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
        "& [-vs] [-w NUM] [-c MiB] [UPLOAD_DIRECTORY]\n"
        "\n"
        "\nWatches UPLOAD_DIRECTORY and unpacks incoming archives into DumpLocation"
        "\nspecified in abrt.conf"
        "\n"
        "\nIf UPLOAD_DIRECTORY is not provided, uses a value of"
        "\nWatchCrashdumpArchiveDir option from abrt.conf"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_s = 1 << 1,
        OPT_d = 1 << 2,
        OPT_w = 1 << 3,
        OPT_c = 1 << 4,
    };

    int concurrent_workers = DEFAULT_COUNT_OF_WORKERS;
    int cache_size_mib = DEFAULT_CACHE_MIB_SIZE;

    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL('s', NULL, NULL              , _("Log to syslog")),
        OPT_BOOL('d', NULL, NULL              , _("Daemonize")),
        OPT_INTEGER('w', NULL, &concurrent_workers, _("Number of concurrent workers. Default is "STRINGIZE(DEFAULT_COUNT_OF_WORKERS))),
        OPT_INTEGER('c', NULL, &cache_size_mib, _("Maximal cache size in MiB. Default is "STRINGIZE(DEFAULT_CACHE_MIB_SIZE))),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    if (concurrent_workers <= 0)
        error_msg_and_die("Invalid number of workers: %d", concurrent_workers);

    if (cache_size_mib <= 0)
        error_msg_and_die("Invalid cache size in MiB: %d", cache_size_mib);

    if (cache_size_mib > UINT_MAX / (1024 * 1024 / FILENAME_MAX))
        error_msg_and_die("Too big cache size. Maximum is : %u MiB", UINT_MAX / (1024 * 1024 / FILENAME_MAX));

    struct process proc = {0};
    proc.max_children = concurrent_workers;
    /* By default it is about 1024 entries */
    g_queue_init(&proc.queue.q);
    proc.queue.capacity = cache_size_mib * (1024 * 1024 / FILENAME_MAX);
    log_debug("Max queue size %u", proc.queue.capacity);

    argv += optind;
    if (argv[0])
    {
        proc.upload_directory = argv[0];

        if (argv[1])
            show_usage_and_die(program_usage_string, program_options);
    }

    /* Initialization */
    log_info("Loading settings");
    if (load_abrt_conf() != 0)
        return 1;

    if (!proc.upload_directory)
        proc.upload_directory = g_settings_sWatchCrashdumpArchiveDir;

    if (!proc.upload_directory)
        error_msg_and_die("Neither UPLOAD_DIRECTORY nor WatchCrashdumpArchiveDir was specified");

    if (opts & OPT_d)
        daemonize();

    msg_prefix = g_progname;
    if ((opts & OPT_d) || (opts & OPT_s) || getenv("ABRT_SYSLOG"))
    {
        logmode = LOGMODE_JOURNAL;
    }

    log_info("Creating glib main loop");
    proc.main_loop = g_main_loop_new(NULL, FALSE);

    log_notice("Setting up a file monitor for '%s'", proc.upload_directory);
    /* Never returns NULL; it will die if an error occurs */
    struct abrt_inotify_watch *aiw = abrt_inotify_watch_init(proc.upload_directory,
            IN_CLOSE_WRITE | IN_MOVED_TO,
            handle_inotify_cb, &proc);

    log_notice("Setting up a signal handler");
    /* Set up signal pipe */
    xpipe(g_signal_pipe);
    close_on_exec_on(g_signal_pipe[0]);
    close_on_exec_on(g_signal_pipe[1]);
    ndelay_on(g_signal_pipe[0]);
    ndelay_on(g_signal_pipe[1]);
    signal(SIGUSR1, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGCHLD, handle_signal);
    GIOChannel *channel_signal = abrt_gio_channel_unix_new(g_signal_pipe[0]);
    guint channel_signal_source_id = g_io_add_watch(channel_signal,
                G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
                handle_signal_pipe_cb,
                &proc);

    log_info("Starting glib main loop");

    g_main_loop_run(proc.main_loop);

    log_info("Glib main loop finished");

    g_source_remove(channel_signal_source_id);

    GError *error = NULL;
    g_io_channel_shutdown(channel_signal, FALSE, &error);
    if (error)
    {
        log_notice("Can't shutdown gio channel: '%s'", error ? error->message : "");
        g_error_free(error);
    }

    g_io_channel_unref(channel_signal);

    abrt_inotify_watch_destroy(aiw);

    if (proc.main_loop)
        g_main_loop_unref(proc.main_loop);

    free_abrt_conf_data();

    return 0;
}
