/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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
#include <dbus/dbus.h>
#include <internal_abrt_dbus.h>
#include "libabrt.h"

/* I want to use -Werror, but gcc-4.4 throws a curveball:
 * "warning: ignoring return value of 'ftruncate', declared with attribute warn_unused_result"
 * and (void) cast is not enough to shut it up! Oh God...
 */
#define IGNORE_RESULT(func_call) do { if (func_call) /* nothing */; } while (0)


#define ABRTD_DBUS_NAME  "com.redhat.abrt"
#define ABRTD_DBUS_PATH  "/com/redhat/abrt"
#define ABRTD_DBUS_IFACE "com.redhat.abrt"


static volatile sig_atomic_t s_sig_caught;
static int s_signal_pipe[2];
static int s_signal_pipe_write = -1;
static unsigned s_timeout;
static bool s_exiting;


static void send_flush_and_unref(DBusMessage* msg)
{
    if (!g_dbus_conn)
    {
        /* Not logging this, it may recurse */
        return;
    }
    if (!dbus_connection_send(g_dbus_conn, msg, NULL /* &serial */))
        error_msg_and_die("Error sending DBus message");
    dbus_connection_flush(g_dbus_conn);
    VERB3 log("DBus message sent");
    dbus_message_unref(msg);
}

/*
 * DBus call handlers
 */

static GList* scan_directory(const char *path)
{
    GList *list = NULL;

    DIR *dp = opendir(path);
    if (!dp)
    {
        /* We don't want to yell if, say, $HOME/.abrt/spool doesn't exist */
        //perror_msg("Can't open directory '%s'", path);
        return list;
    }

    struct dirent *dent;
    while ((dent = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue; /* skip "." and ".." */

        char *full_name = concat_path_file(path, dent->d_name);
        struct stat statbuf;
        if (stat(full_name, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
        {
            /* Silently ignore *any* errors, not only EACCES.
             * We saw "lock file is locked by process PID" error
             * when we raced with wizard.
             */
            int sv_logmode = logmode;
            logmode = 0;
            struct dump_dir *dd = dd_opendir(full_name, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES);
            logmode = sv_logmode;
            if (dd)
            {
                list = g_list_prepend(list, full_name);
                full_name = NULL;
                dd_close(dd);
            }
        }
        free(full_name);
    }
    closedir(dp);

    /* Why reverse?
     * Because N*prepend+reverse is faster than N*append
     */
    return g_list_reverse(list);
}

/* To test from command line:
 * dbus-send --system --type=method_call --print-reply --dest=com.redhat.abrt \
 *	/com/redhat/abrt com.redhat.abrt.GetListOfProblems
 */
static int handle_GetListOfProblems(DBusMessage* call, DBusMessage* reply)
{
    DBusMessageIter out_iter;
    dbus_message_iter_init_append(reply, &out_iter);

    GList *list = scan_directory(g_settings_dump_location);

    /* Store the array */
    DBusMessageIter sub_iter;
    if (!dbus_message_iter_open_container(&out_iter, DBUS_TYPE_ARRAY, "s", &sub_iter))
        die_out_of_memory();
    while (list)
    {
        store_string(&sub_iter, (char*)list->data);
        free(list->data);
        list = g_list_delete_link(list, list);
    }
    if (!dbus_message_iter_close_container(&out_iter, &sub_iter))
        die_out_of_memory();

    /* Send reply */
    send_flush_and_unref(reply);
    return 0;
}

/*
 * Glib integration machinery
 */

/* Callback: "a message is received to a registered object path" */
static DBusHandlerResult message_received(DBusConnection* conn, DBusMessage* msg, void* data)
{
    const char* member = dbus_message_get_member(msg);
    VERB1 log("%s(method:'%s')", __func__, member);

    //set_client_name(dbus_message_get_sender(msg));

    DBusMessage* reply = dbus_message_new_method_return(msg);
    int r = -1;
    if (strcmp(member, "GetListOfProblems") == 0)
        r = handle_GetListOfProblems(msg, reply);
// NB: C++ binding also handles "Introspect" method, which returns a string.
// It was sending "dummy" introspection answer whick looks like this:
// "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
// "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
// "<node>\n"
// "</node>\n"
// Apart from a warning from abrt-gui, just sending error back works as well.
// NB2: we may want to handle "Disconnected" here too.

    if (r < 0)
    {
        /* handle_XXX experienced an error (and did not send any reply) */
        dbus_message_unref(reply);
        if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL)
        {
            /* Create and send error reply */
            reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED, "not supported");
            if (!reply)
                die_out_of_memory();
            send_flush_and_unref(reply);
        }
    }

    //set_client_name(NULL);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static void handle_dbus_err(bool error_flag, DBusError *err)
{
    if (dbus_error_is_set(err))
    {
        error_msg("dbus error: %s", err->message);
        /* dbus_error_free(&err); */
        error_flag = true;
    }
    if (!error_flag)
        return;
    error_msg_and_die(
            "Error requesting DBus name %s, possible reasons: "
            "abrt run by non-root; dbus config is incorrect; "
            "or dbus daemon needs to be restarted to reload dbus config",
            ABRTD_DBUS_NAME);
}

static int init_dbus(void)
{
    DBusConnection* conn;
    DBusError err;

    dbus_error_init(&err);
    VERB3 log("dbus_bus_get");
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    handle_dbus_err(conn == NULL, &err);
    // dbus api says:
    // "If dbus_bus_get obtains a new connection object never before returned
    // from dbus_bus_get(), it will call dbus_connection_set_exit_on_disconnect(),
    // so the application will exit if the connection closes. You can undo this
    // by calling dbus_connection_set_exit_on_disconnect() yourself after you get
    // the connection."
    // ...
    // "When a connection is disconnected, you are guaranteed to get a signal
    // "Disconnected" from the interface DBUS_INTERFACE_LOCAL, path DBUS_PATH_LOCAL"
    //
    // dbus-daemon drops connections if it recvs a malformed message
    // (we actually observed this when we sent bad UTF-8 string).
    // Currently, in this case abrtd just exits with exit code 1.
    // (symptom: last two log messages are "abrtd: remove_watch()")
    // If we want to have better logging or other nontrivial handling,
    // here we need to do:
    //
    //dbus_connection_set_exit_on_disconnect(conn, FALSE);
    //dbus_connection_add_filter(conn, handle_message, NULL, NULL);
    //
    // and need to code up handle_message to check for "Disconnected" dbus signal

    /* Also sets g_dbus_conn to conn. */
    attach_dbus_conn_to_glib_main_loop(conn, "/com/redhat/abrt", message_received);

    VERB3 log("dbus_bus_request_name");
    int rc = dbus_bus_request_name(conn, ABRTD_DBUS_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
//maybe check that r == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER instead?
    handle_dbus_err(rc < 0, &err);
    VERB3 log("dbus init done");

    /* dbus_bus_request_name can already read some data. For example,
     * if we were autostarted, the call which caused autostart arrives
     * at this moment. Thus while dbus fd hasn't any data anymore,
     * dbus library can buffer a message or two.
     * If we don't do this, the data won't be processed
     * until next dbus data arrives.
     */
    int cnt = 10;
    while (dbus_connection_dispatch(conn) != DBUS_DISPATCH_COMPLETE && --cnt)
        VERB3 log("processed initial buffered dbus message");

    return 0;
}

static void deinit_dbus(void)
{
    if (g_dbus_conn != NULL)
    {
        dbus_connection_unref(g_dbus_conn);
        g_dbus_conn = NULL;
    }
}

/*
 * Signal pipe handling
 */

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
        s_exiting = 1;
    }
    return TRUE; /* "please don't remove this event" */
}


static void start_syslog_logging(void)
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

/* Run main loop with idle timeout.
 * Basically, almost like glib's g_main_run(loop)
 */
static void run_main_loop(GMainLoop *loop)
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
        OPT_t = 1 << 3,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(   'd', NULL, NULL      , _("Do not daemonize")),
        OPT_BOOL(   's', NULL, NULL      , _("Log to syslog even with -d")),
        OPT_INTEGER('t', NULL, &s_timeout, _("Exit after NUM seconds of inactivity")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    /* When dbus daemon starts us, it doesn't set PATH
     * (I saw it set only DBUS_STARTER_ADDRESS and DBUS_STARTER_BUS_TYPE).
     * In this case, set something sane:
     */
    const char *env_path = getenv("PATH");
    if (!env_path || !env_path[0])
        putenv((char*)"PATH=/usr/sbin:/usr/bin:/sbin:/bin");

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

    GMainLoop *pMainloop = NULL;
    GIOChannel *channel_signal = NULL;
    guint channel_signal_event_id = 0;

    /* Initialization */

    VERB1 log("Loading settings");
    if (load_abrt_conf() != 0)
        goto init_error;

    VERB1 log("Creating glib main loop");
    pMainloop = g_main_loop_new(NULL, FALSE);

    /* Add an event source which waits for INT/TERM signal */
    VERB1 log("Adding signal pipe watch to glib main loop");
    channel_signal = g_io_channel_unix_new(s_signal_pipe[0]);
    channel_signal_event_id = g_io_add_watch(channel_signal,
                                             G_IO_IN,
                                             handle_signal_cb,
                                             NULL);

    /* Note: this already may process a few dbus messages,
     * therefore it should be the last thing to initialize.
     */
    VERB1 log("Initializing dbus");
    if (init_dbus() != 0)
        goto init_error;

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

    if (channel_signal_event_id > 0)
        g_source_remove(channel_signal_event_id);
    if (channel_signal)
        g_io_channel_unref(channel_signal);

    deinit_dbus();

    if (pMainloop)
        g_main_loop_unref(pMainloop);

    free_abrt_conf_data();

    /* Exiting */
    if (s_sig_caught && s_sig_caught != SIGALRM)
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
