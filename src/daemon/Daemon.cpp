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
#include <pthread.h>
#include <string>
#include <sys/inotify.h>
#include <sys/ioctl.h> /* ioctl(FIONREAD) */
#include <glib.h>
#include "abrtlib.h"
#include "abrt_exception.h"
#include "comm_layer_inner.h"
#include "Settings.h"
#include "CommLayerServerDBus.h"
#include "CrashWatcher.h"
#include "MiddleWare.h"
#include "Daemon.h"
#include "parse_options.h"

using namespace std;


#define VAR_RUN_LOCK_FILE   VAR_RUN"/abrt/abrtd.lock"
#define VAR_RUN_PIDFILE     VAR_RUN"/abrtd.pid"

#define SOCKET_FILE VAR_RUN"/abrt/abrt.socket"
#define SOCKET_PERMISSION 0666
/* Maximum number of simultaneously opened client connections. */
#define MAX_CLIENT_COUNT 10


/* Daemon initializes, then sits in glib main loop, waiting for events.
 * Events can be:
 * - inotify: something new appeared under /var/spool/abrt
 * - DBus: dbus message arrived
 * - signal: we got SIGTERM or SIGINT
 *
 * DBus methods we have:
 * - GetCrashInfos(): returns a vector_map_crash_data_t (vector_map_vector_string_t)
 *      of crashes for given uid
 *      v[N]["executable"/"uid"/"kernel"/"backtrace"][N] = "contents"
 * - StartJob(crash_id,force): starts creating a report for /var/spool/abrt/DIR with this UID:UUID.
 *      Returns job id (uint64).
 *      After thread returns, when report creation thread has finished,
 *      JobDone() dbus signal is emitted.
 * - CreateReport(crash_id): returns map_crash_data_t (map_vector_string_t)
 * - Report(map_crash_data_t (map_vector_string_t[, map_map_string_t])):
 *      "Please report this crash": calls Report() of all registered reporter plugins.
 *      Returns report_status_t (map_vector_string_t) - the status of each call.
 *      2nd parameter is the contents of user's abrt.conf.
 * - DeleteDebugDump(crash_id): delete it from DB and delete corresponding /var/spool/abrt/DIR
 * - GetPluginsInfo(): returns map_map_string_t
 *      map["plugin"] = { "Name": "plugin", "Enabled": "yes" ... }
 * - GetPluginSettings(PluginName): returns map_plugin_settings_t (map_string_t)
 * - SetPluginSettings(PluginName, map_plugin_settings_t): returns void
 * - RegisterPlugin(PluginName): returns void
 * - UnRegisterPlugin(PluginName): returns void
 * - GetSettings(): returns map_abrt_settings_t (map_map_string_t)
 * - SetSettings(map_abrt_settings_t): returns void
 *
 * DBus signals we emit:
 * - Crash(progname, crash_id, dir, uid) - a new crash occurred (new /var/spool/abrt/DIR is found)
 * - JobDone(client_dbus_ID) - see StartJob above.
 *      Sent as unicast to the client which did StartJob.
 * - Warning(msg)
 * - Update(msg)
 *      Both are sent as unicast to last client set by set_client_name(name).
 *      If set_client_name(NULL) was done, they are not sent.
 */
CCommLayerServer* g_pCommLayer;

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


/* Cron handling */

typedef struct cron_callback_data_t
{
    std::string m_sPluginName;
    std::string m_sPluginArgs;
    unsigned int m_nTimeout;

    cron_callback_data_t(
                      const std::string& pPluginName,
                      const std::string& pPluginArgs,
                      const unsigned int& pTimeout) :
        m_sPluginName(pPluginName),
        m_sPluginArgs(pPluginArgs),
        m_nTimeout(pTimeout)
    {}
} cron_callback_data_t;

static void cron_delete_callback_data_cb(gpointer data)
{
    cron_callback_data_t* cronDeleteCallbackData = static_cast<cron_callback_data_t*>(data);
    delete cronDeleteCallbackData;
}

static gboolean cron_activation_periodic_cb(gpointer data)
{
    cron_callback_data_t* cronPeriodicCallbackData = static_cast<cron_callback_data_t*>(data);
    VERB1 log("Activating plugin: %s", cronPeriodicCallbackData->m_sPluginName.c_str());
    RunAction(DEBUG_DUMPS_DIR,
            cronPeriodicCallbackData->m_sPluginName.c_str(),
            cronPeriodicCallbackData->m_sPluginArgs.c_str()
    );
    return TRUE;
}
static gboolean cron_activation_one_cb(gpointer data)
{
    cron_callback_data_t* cronOneCallbackData = static_cast<cron_callback_data_t*>(data);
    VERB1 log("Activating plugin: %s", cronOneCallbackData->m_sPluginName.c_str());
    RunAction(DEBUG_DUMPS_DIR,
            cronOneCallbackData->m_sPluginName.c_str(),
            cronOneCallbackData->m_sPluginArgs.c_str()
    );
    return FALSE;
}
static gboolean cron_activation_reshedule_cb(gpointer data)
{
    cron_callback_data_t* cronResheduleCallbackData = static_cast<cron_callback_data_t*>(data);
    VERB1 log("Rescheduling plugin: %s", cronResheduleCallbackData->m_sPluginName.c_str());
    cron_callback_data_t* cronPeriodicCallbackData = new cron_callback_data_t(cronResheduleCallbackData->m_sPluginName,
                                                                              cronResheduleCallbackData->m_sPluginArgs,
                                                                              cronResheduleCallbackData->m_nTimeout);
    g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                               cronPeriodicCallbackData->m_nTimeout,
                               cron_activation_periodic_cb,
                               static_cast<gpointer>(cronPeriodicCallbackData),
                               cron_delete_callback_data_cb
    );
    return FALSE;
}

static int SetUpMW()
{
    VERB1 log("Adding analyzers, actions or reporters");
    map_analyzer_actions_and_reporters_t::iterator it_aar = g_settings_mapAnalyzerActionsAndReporters.begin();
    for (; it_aar != g_settings_mapAnalyzerActionsAndReporters.end(); it_aar++)
    {
        vector_pair_string_string_t::iterator it_ar = it_aar->second.begin();
        for (; it_ar != it_aar->second.end(); it_ar++)
        {
            AddAnalyzerActionOrReporter(it_aar->first.c_str(), it_ar->first.c_str(), it_ar->second.c_str());
        }
    }
    return 0;
}

static int SetUpCron()
{
    map_cron_t::iterator it_c = g_settings_mapCron.begin();
    for (; it_c != g_settings_mapCron.end(); it_c++)
    {
        std::string::size_type pos = it_c->first.find(":");
        int timeout = 0;
        int nH = -1;
        int nM = -1;
        int nS = -1;

//TODO: rewrite using good old sscanf?

        if (pos != std::string::npos)
        {
            std::string sH;
            std::string sM;

            sH = it_c->first.substr(0, pos);
            nH = xatou(sH.c_str());
            nH = nH > 23 ? 23 : nH;
            nH = nH < 0 ? 0 : nH;
            timeout += nH * 60 * 60;
            sM = it_c->first.substr(pos + 1);
            nM = xatou(sM.c_str());
            nM = nM > 59 ? 59 : nM;
            nM = nM < 0 ? 0 : nM;
            timeout += nM * 60;
        }
        else
        {
            std::string sS;

            sS = it_c->first;
            nS = xatou(sS.c_str());
            nS = nS <= 0 ? 1 : nS;
            timeout = nS;
        }

        if (nS != -1)
        {
            vector_pair_string_string_t::iterator it_ar = it_c->second.begin();
            for (; it_ar != it_c->second.end(); it_ar++)
            {
                cron_callback_data_t* cronPeriodicCallbackData = new cron_callback_data_t(it_ar->first, it_ar->second, timeout);
                g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           timeout,
                                           cron_activation_periodic_cb,
                                           static_cast<gpointer>(cronPeriodicCallbackData),
                                           cron_delete_callback_data_cb);
            }
        }
        else
        {
            time_t actTime = time(NULL);
            struct tm locTime;
            localtime_r(&actTime, &locTime);
            locTime.tm_hour = nH;
            locTime.tm_min = nM;
            locTime.tm_sec = 0;
            time_t nextTime = mktime(&locTime);
            if (nextTime == ((time_t)-1))
            {
                /* paranoia */
                perror_msg("Can't set up cron time");
                return -1;
            }
            if (actTime > nextTime)
            {
                timeout = 24*60*60 + (nextTime - actTime);
            }
            else
            {
                timeout = nextTime - actTime;
            }
            vector_pair_string_string_t::iterator it_ar = it_c->second.begin();
            for (; it_ar != it_c->second.end(); it_ar++)
            {
                cron_callback_data_t* cronOneCallbackData = new cron_callback_data_t(it_ar->first, it_ar->second, timeout);
                g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           timeout,
                                           cron_activation_one_cb,
                                           static_cast<gpointer>(cronOneCallbackData),
                                           cron_delete_callback_data_cb);
                cron_callback_data_t* cronResheduleCallbackData = new cron_callback_data_t(it_ar->first, it_ar->second, 24 * 60 * 60);
                g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           timeout,
                                           cron_activation_reshedule_cb,
                                           static_cast<gpointer>(cronResheduleCallbackData),
                                           cron_delete_callback_data_cb);
            }
        }
    }
    return 0;
}

static void FindNewDumps(const char* pPath)
{
    /* Get all debugdump directories in the pPath directory */
    GList *dirs = NULL;
    DIR *dp = opendir(pPath);
    if (dp == NULL)
    {
        perror_msg("Can't open directory '%s'", pPath);
        return;
    }
    struct dirent *ep;
    while ((ep = readdir(dp)))
    {
        if (dot_or_dotdot(ep->d_name))
            continue; /* skip "." and ".." */
        char *dname = concat_path_file(pPath, ep->d_name);
        struct stat stats;
        if (lstat(dname, &stats) == 0)
        {
            if (S_ISDIR(stats.st_mode))
            {
                VERB1 log("Will check directory '%s'", ep->d_name);
                dirs = g_list_append(dirs, dname);
                continue;
            }
        }
        free(dname);
    }
    closedir(dp);

    unsigned size = g_list_length(dirs);
    if (size == 0)
        return;
    log("Checking for unsaved crashes (dirs to check:%u)", size);

    /* Get potentially non-processed debugdumps */
    for (GList *li = dirs; li != NULL; li = g_list_next(li))
    {
        try
        {
            const char *dir_name = (char*)dirs->data;
            map_crash_data_t crashinfo;
            mw_result_t res = SaveDebugDump(dir_name, crashinfo);
            switch (res)
            {
                case MW_OK:
                    /* Not VERB1: this is new, unprocessed crash dump.
                     * Last abrtd somehow missed it - need to inform user */
                    log("Non-processed crash in %s, saving into database", dir_name);
                    break;
                case MW_IN_DB:
                    /* This debugdump was found in DB, nothing else was done
                     * by SaveDebugDump or needs to be done by us */
                    VERB1 log("%s is already saved in database", dir_name);
                    break;
                case MW_REPORTED: /* already reported dup */
                case MW_OCCURRED: /* not-yet-reported dup */
                    VERB1 log("Duplicate crash %s, deleting", dir_name);
                    delete_debug_dump_dir(dir_name);
                    break;
                default:
                    log("Corrupted or bad crash %s (res:%d), deleting", dir_name, (int)res);
                    delete_debug_dump_dir(dir_name);
                    break;
            }
        }
        catch (CABRTException& e)
        {
            error_msg("%s", e.what());
        }
    }

    for (GList *li = dirs; li != NULL; li = g_list_next(li))
        free(li->data);

    g_list_free(dirs);

    log("Done checking for unsaved crashes");
}

static int CreatePidFile()
{
    int fd;

    /* JIC */
    unlink(VAR_RUN_PIDFILE);

    /* open the pidfile */
    fd = open(VAR_RUN_PIDFILE, O_WRONLY|O_CREAT|O_EXCL, 0644);
    if (fd >= 0)
    {
        /* write our pid to it */
        char buf[sizeof(long)*3 + 2];
        int len = sprintf(buf, "%lu\n", (long)getpid());
        write(fd, buf, len);
        close(fd);
        return 0;
    }

    /* something went wrong */
    perror_msg("Can't open '%s'", VAR_RUN_PIDFILE);
    return -1;
}

static int Lock()
{
    int lfd = open(VAR_RUN_LOCK_FILE, O_RDWR|O_CREAT, 0640);
    if (lfd < 0)
    {
        perror_msg("Can't open '%s'", VAR_RUN_LOCK_FILE);
        return -1;
    }
    if (lockf(lfd, F_TLOCK, 0) < 0)
    {
        perror_msg("Can't lock file '%s'", VAR_RUN_LOCK_FILE);
        return -1;
    }
    close_on_exec_on(lfd);
    return 0;
    /* we leak opened lfd intentionally */
}

static void handle_signal(int signo)
{
    int save_errno = errno;

    // Enable for debugging only, malloc/printf are unsafe in signal handlers
    //VERB3 log("Got signal %d", signo);

    uint8_t l_sig_caught;
    s_sig_caught = l_sig_caught = signo;
    /* Using local copy of s_sig_caught so that concurrent signal
     * won't change it under us */
    if (s_signal_pipe_write >= 0)
        write(s_signal_pipe_write, &l_sig_caught, 1);

    errno = save_errno;
}

/* Signal pipe handler */
static gboolean handle_signal_cb(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused)
{
    char signo;
    gsize len = 0;
    g_io_channel_read(gio, &signo, 1, &len);
    if (len == 1)
    {
        /* we did receive a signal */
        VERB3 log("Got signal %d through signal pipe", signo);
        if (signo != SIGCHLD)
            s_exiting = 1;
        else
        {
            if (socket_client_count)
                socket_client_count--;
            if (!socket_channel_cb_id)
            {
                log("Accepting connections on '%s'", SOCKET_FILE);
                socket_channel_cb_id = add_watch_or_die(socket_channel, G_IO_IN | G_IO_PRI, server_socket_cb);
            }
        }
        return TRUE;
    }
    return FALSE;
}

/* Inotify handler */
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
    GIOError err = g_io_channel_read(gio, buf, inotify_bytes, &len);
    if (err != G_IO_ERROR_NONE)
    {
        perror_msg("Error reading inotify fd");
        free(buf);
        return FALSE;
    }

    /* Reconstruct each event and send message to the dbus */
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
                    execlp("abrt-handle-upload", "abrt-handle-upload", DEBUG_DUMPS_DIR, dir, name, (char*)NULL);
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
        if (strcmp(strchrnul(name, '.'), ".new") == 0)
        {
            //VERB3 log("Directory '%s' creation detected, ignoring", name);
            continue;
        }
        log("Directory '%s' creation detected", name);

        if (g_settings_nMaxCrashReportsSize > 0)
        {
            char *worst_dir = NULL;
            while (g_settings_nMaxCrashReportsSize > 0
             && get_dirsize_find_largest_dir(DEBUG_DUMPS_DIR, &worst_dir, name) / (1024*1024) >= g_settings_nMaxCrashReportsSize
             && worst_dir
            ) {
                log("Size of '%s' >= %u MB, deleting '%s'", DEBUG_DUMPS_DIR, g_settings_nMaxCrashReportsSize, worst_dir);
                g_pCommLayer->QuotaExceed(_("The size of the report exceeded the quota. Please check system's MaxCrashReportsSize value in abrt.conf."));
                /* deletes both directory and DB record */
                char *d = concat_path_file(DEBUG_DUMPS_DIR, worst_dir);
                free(worst_dir);
                worst_dir = NULL;
                DeleteDebugDump_by_dir(d);
                free(d);
            }
        }

        char *fullname = NULL;
        try
        {
            fullname = concat_path_file(DEBUG_DUMPS_DIR, name);
            /* Note: SaveDebugDump does not save crashinfo, it _fetches_ crashinfo */
            map_crash_data_t crashinfo;
            mw_result_t res = SaveDebugDump(fullname, crashinfo);
            switch (res)
            {
                case MW_OK:
                    log("New crash %s, processing", fullname);
                    /* Fall through */

                case MW_REPORTED: /* already reported dup */
                case MW_OCCURRED: /* not-yet-reported dup */
                {
                    if (res != MW_OK)
                    {
                        const char *first = get_crash_data_item_content(crashinfo, CD_DUMPDIR).c_str();
                        log("Deleting crash %s (dup of %s), sending dbus signal",
                                strrchr(fullname, '/') + 1,
                                strrchr(first, '/') + 1);
                        delete_debug_dump_dir(fullname);
                    }

                    const char *analyzer = get_crash_data_item_content(crashinfo, FILENAME_ANALYZER).c_str();
                    const char *uid_str = get_crash_data_item_content(crashinfo, CD_UID).c_str();

                    /* Send dbus signal */
                    //if (analyzer_has_InformAllUsers(analyzer))
                    //    uid_str = NULL;
                    char *crash_id = xasprintf("%s:%s",
                                    get_crash_data_item_content(crashinfo, CD_UID).c_str(),
                                    get_crash_data_item_content(crashinfo, CD_UUID).c_str()
                    );
                    g_pCommLayer->Crash(get_crash_data_item_content(crashinfo, FILENAME_PACKAGE).c_str(),
                                    crash_id,
                                    fullname,
                                    uid_str
                    );
                    free(crash_id);
                    break;
                }
                case MW_IN_DB:
                    log("Huh, this crash is already in db?! Nothing to do");
                    break;
                case MW_CORRUPTED:
                case MW_GPG_ERROR:
                default:
                    log("Corrupted or bad crash %s (res:%d), deleting", fullname, (int)res);
                    delete_debug_dump_dir(fullname);
                    break;
            }
        }
        catch (CABRTException& e)
        {
            error_msg("%s", e.what());
        }
        catch (...)
        {
            free(fullname);
            free(buf);
            throw;
        }
        free(fullname);
    } /* while */

    free(buf);
    return TRUE;
}

/* Run main loop with idle timeout.
 * Basically, almost like glib's g_main_run(loop)
 */
static void run_main_loop(GMainLoop* loop)
{
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

        if (s_timeout)
            alarm(s_timeout);
        g_poll(fds, nfds, timeout);
        if (s_timeout)
            alarm(0);

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
    openlog("abrtd", 0, LOG_DAEMON);
    logmode = LOGMODE_SYSLOG;
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
    ensure_writable_dir(DEBUG_DUMPS_DIR, 0755, "abrt");
    /* debuginfo cache */
    ensure_writable_dir(DEBUG_INFO_DIR, 0755, "root");
    /* temp dir */
    ensure_writable_dir(VAR_RUN"/abrt", 0755, "root");
}

static char *timeout_opt;
static const char* abrtd_usage = _("abrtd [options]");
enum {
    OPT_v = 1 << 0,
    OPT_d = 1 << 1,
    OPT_s = 1 << 2,
    OPT_t = 1 << 3,
};
/* Keep enum above and order of options below in sync! */
static struct options abrtd_options[] = {
    OPT__VERBOSE(&g_verbose),
    OPT_BOOL( 'd' , 0, NULL, _("Do not daemonize")),
    OPT_BOOL( 's' , 0, NULL, _("Log to syslog even with -d")),
    OPT_INTEGER( 't' , 0, &timeout_opt, _("Exit after SEC seconds of inactivity")),
    OPT_END()
};

int main(int argc, char** argv)
{
    int parent_pid = getpid();

    setlocale(LC_ALL, "");

#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    if (getuid() != 0)
        error_msg_and_die("ABRT daemon must be run as root");

    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    unsigned opts = parse_opts(argc, argv, abrtd_options, abrtd_usage);

    if (opts & OPT_s)
        start_syslog_logging();

    /* When dbus daemon starts us, it doesn't set PATH
     * (I saw it set only DBUS_STARTER_ADDRESS and DBUS_STARTER_BUS_TYPE).
     * In this case, set something sane:
     */
    /* Need to add LIBEXEC_DIR to PATH, because otherwise abrt-action-*
     * are not found by exec()
     */
    const char *env_path = getenv("PATH");
    if (!env_path || !env_path[0])
        env_path = "/usr/sbin:/usr/bin:/sbin:/bin";
    putenv(xasprintf("PATH=%s:%s", LIBEXEC_DIR, env_path));

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    msg_prefix = "abrtd"; /* for log(), error_msg() and such */

    xpipe(s_signal_pipe);
    close_on_exec_on(s_signal_pipe[0]);
    close_on_exec_on(s_signal_pipe[1]);
    signal(SIGTERM, handle_signal);
    signal(SIGINT,  handle_signal);
    signal(SIGCHLD, handle_signal);
    if (s_timeout)
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
    bool lockfile_created = false;
    bool pidfile_created = false;
    CCrashWatcher watcher;

    /* Initialization */
    try
    {
        init_daemon_logging(&watcher);

        VERB1 log("Loading settings");
        if (LoadSettings() != 0)
            throw 1;

        VERB1 log("Creating glib main loop");
        pMainloop = g_main_loop_new(NULL, FALSE);

        VERB1 log("Initializing inotify");
        sanitize_dump_dir_rights();
        errno = 0;
        int inotify_fd = inotify_init();
        if (inotify_fd == -1)
            perror_msg_and_die("inotify_init failed");
        close_on_exec_on(inotify_fd);
        /* Watching DEBUG_DUMPS_DIR for new files... */
        if (inotify_add_watch(inotify_fd, DEBUG_DUMPS_DIR, IN_CREATE | IN_MOVED_TO) < 0)
            perror_msg_and_die("inotify_add_watch failed on '%s'", DEBUG_DUMPS_DIR);
        if (g_settings_sWatchCrashdumpArchiveDir)
        {
            s_upload_watch = inotify_add_watch(inotify_fd, g_settings_sWatchCrashdumpArchiveDir, IN_CLOSE_WRITE|IN_MOVED_TO);
            if (s_upload_watch < 0)
                perror_msg_and_die("inotify_add_watch failed on '%s'", g_settings_sWatchCrashdumpArchiveDir);
        }
        VERB1 log("Adding inotify watch to glib main loop");
        channel_inotify = g_io_channel_unix_new(inotify_fd);
        channel_inotify_event_id = g_io_add_watch(channel_inotify,
                                                  G_IO_IN,
                                                  handle_inotify_cb,
                                                  NULL);

        VERB1 log("Loading plugins from "PLUGINS_LIB_DIR);
        g_pPluginManager = new CPluginManager();
        g_pPluginManager->LoadPlugins();

        if (SetUpMW() != 0) /* logging is inside */
            throw 1;
        if (SetUpCron() != 0)
            throw 1;

        /* Add an event source which waits for INT/TERM signal */
        VERB1 log("Adding signal pipe watch to glib main loop");
        channel_signal = g_io_channel_unix_new(s_signal_pipe[0]);
        channel_signal_event_id = g_io_add_watch(channel_signal,
                                                 G_IO_IN,
                                                 handle_signal_cb,
                                                 NULL);

        /* Mark the territory */
        VERB1 log("Creating lock file");
        if (Lock() != 0)
            throw 1;
        lockfile_created = true;
        VERB1 log("Creating pid file");
        if (CreatePidFile() != 0)
            throw 1;
        pidfile_created = true;

        /* Open socket to receive new crashes. */
        dumpsocket_init();

        /* Note: this already may process a few dbus messages,
         * therefore it should be the last thing to initialize.
         */
        VERB1 log("Initializing dbus");
        g_pCommLayer = new CCommLayerServerDBus();
        if (g_pCommLayer->m_init_error)
            throw 1;
    }
    catch (...)
    {
        /* Initialization error */
        error_msg("Error while initializing daemon");
        /* Inform parent that initialization failed */
        if (!(opts & OPT_d))
            kill(parent_pid, SIGINT);
        goto cleanup;
    }

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
    try
    {
        /* This may take a while, therefore we don't do it in init section */
        FindNewDumps(DEBUG_DUMPS_DIR);
        log("Init complete, entering main loop");
        run_main_loop(pMainloop);
    }
    catch (CABRTException& e)
    {
        error_msg("Error: %s", e.what());
    }
    catch (std::exception& e)
    {
        error_msg("Error: %s", e.what());
    }

 cleanup:
    /* Error or INT/TERM. Clean up, in reverse order.
     * Take care to not undo things we did not do.
     */
    dumpsocket_shutdown();
    if (pidfile_created)
        unlink(VAR_RUN_PIDFILE);
    if (lockfile_created)
        unlink(VAR_RUN_LOCK_FILE);

    if (channel_signal_event_id > 0)
        g_source_remove(channel_signal_event_id);
    if (channel_signal)
        g_io_channel_unref(channel_signal);
    if (channel_inotify_event_id > 0)
        g_source_remove(channel_inotify_event_id);
    if (channel_inotify)
        g_io_channel_unref(channel_inotify);

    delete g_pCommLayer;
    if (g_pPluginManager)
    {
        /* This restores /proc/sys/kernel/core_pattern, among other things: */
        g_pPluginManager->UnLoadPlugins();
        delete g_pPluginManager;
    }
    if (pMainloop)
        g_main_loop_unref(pMainloop);

    settings_free();
    /* Exiting */
    if (s_sig_caught && s_sig_caught != SIGALRM && s_sig_caught != SIGCHLD)
    {
        error_msg_and_die("Got signal %d, exiting", s_sig_caught);
        signal(s_sig_caught, SIG_DFL);
        raise(s_sig_caught);
    }
    error_msg_and_die("Exiting");
}
