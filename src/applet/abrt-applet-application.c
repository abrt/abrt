#include "abrt-applet-application.h"

#include "abrt-applet-problem-info.h"

#include <abrt-dbus.h>
#include <gio/gdesktopappinfo.h>
#include <libabrt.h>
#include <libnotify/notify.h>
#include <libreport/internal_libreport_gtk.h>
#include <libreport/problem_utils.h>
#ifdef HAVE_POLKIT
#include <polkit/polkit.h>
#endif
#include <stdbool.h>

#define A_REPORT_REPORT "REPORT"
#define A_RESTART_APPLICATION "RESTART"
#define APP_NAME "abrt-applet"
#define GUI_EXECUTABLE "gnome-abrt"
#define GS_SCHEMA_ID_PRIVACY "org.gnome.desktop.privacy"
#define GS_PRIVACY_OPT_AUTO_REPORTING "report-technical-problems"
#define NOTIFICATION_ICON_NAME "face-sad-symbolic"

struct _AbrtAppletApplication
{
    GApplication parent_instance;

    GDBusConnection *connection;
    unsigned int filter_id;

    GPtrArray *deferred_problems;
};

G_DEFINE_TYPE (AbrtAppletApplication, abrt_applet_application, G_TYPE_APPLICATION)

typedef struct
{
    pid_t child_pid;
    int child_stdout_fd;
    struct strbuf *cmd_output;

    AbrtAppletApplication *application;
    AbrtAppletProblemInfo *problem_info;
    int flags;
} EventProcessingState;

static EventProcessingState *
event_processing_state_new (void)
{
    EventProcessingState *state;

    state = g_new0(EventProcessingState, 1);

    state->child_pid = -1;
    state->child_stdout_fd = -1;
    state->cmd_output = libreport_strbuf_new ();

    return state;
}

static void
event_processing_state_free (EventProcessingState *state)
{
    libreport_strbuf_free (state->cmd_output);
    g_clear_object (&state->application);

    g_free (state);
}

static unsigned int g_deferred_timeout;
static bool g_gnome_abrt_available;
static bool g_user_is_admin;

static void abrt_applet_application_report_problems (AbrtAppletApplication *self,
                                                     GPtrArray             *problems);
static void abrt_applet_application_send_problem_notifications (AbrtAppletApplication *self,
                                                                GPtrArray             *problems);

static gboolean
process_deferred_queue (gpointer user_data)
{
    AbrtAppletApplication *self;
    g_autoptr (GPtrArray) problems = NULL;

    self = user_data;
    problems = g_steal_pointer (&self->deferred_problems);

    self->deferred_problems = g_ptr_array_new_with_free_func (g_object_unref);

    g_deferred_timeout = 0;

    abrt_applet_application_report_problems (self, problems);
    abrt_applet_application_send_problem_notifications (self, problems);

    return G_SOURCE_REMOVE;
}

static void
on_connectivity_changed (GObject    *gobject,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
    GNetworkMonitor *network_monitor;

    network_monitor = G_NETWORK_MONITOR (gobject);

    if (g_network_monitor_get_connectivity (network_monitor) == G_NETWORK_CONNECTIVITY_FULL)
    {
        if (g_deferred_timeout)
        {
            g_source_remove (g_deferred_timeout);
        }

        g_deferred_timeout = g_idle_add ((GSourceFunc) process_deferred_queue, user_data);
    }
}

static bool
is_gnome_abrt_available (void)
{
    g_autoptr (GAppInfo) app = NULL;
    g_autoptr (GError) error = NULL;

    app = g_app_info_create_from_commandline (GUI_EXECUTABLE, GUI_EXECUTABLE,
                                              G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION,
                                              &error);
    if (app == NULL)
    {
        log_debug("Cannot find " GUI_EXECUTABLE ": %s", error->message);

        return false;
    }

    return true;
}

static bool
is_user_admin (void)
{
#ifdef HAVE_POLKIT
    g_autoptr (GError) error = NULL;
    g_autoptr (GPermission) permission = NULL;

    permission = polkit_permission_new_sync ("org.freedesktop.problems.getall",
                                             NULL, NULL, &error);
    if (permission == NULL)
    {
        perror_msg_and_die ("Can't get Polkit configuration: %s", error->message);
    }

    return g_permission_get_allowed (permission);
#else
    return true;
#endif
}

static void
abrt_applet_application_init (AbrtAppletApplication *self)
{
    GApplication *application;
    const GOptionEntry option_entries[] =
    {
        { "verbose", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &libreport_g_verbose,
          _("Be verbose"), NULL },
        { NULL },
    };
    GNetworkMonitor *network_monitor;

    application = G_APPLICATION (self);
    network_monitor = g_network_monitor_get_default ();

    g_application_add_main_option_entries (application, option_entries);
    g_application_set_option_context_summary (application,
                                              _("Applet which notifies user when new problems are detected by ABRT"));

    g_signal_connect (network_monitor, "notify::connectivity",
                      G_CALLBACK (on_connectivity_changed), self);
    g_signal_connect (network_monitor, "notify::network-available",
                      G_CALLBACK (on_connectivity_changed), NULL);

    self->deferred_problems = g_ptr_array_new_with_free_func (g_object_unref);
}

static bool
is_autoreporting_enabled (void)
{
    g_autoptr (GSettings) settings = NULL;

    settings = g_settings_new (GS_SCHEMA_ID_PRIVACY);

    return g_settings_get_boolean (settings, GS_PRIVACY_OPT_AUTO_REPORTING);
}

static void
migrate_auto_reporting_to_gsettings (void)
{
#define OPT_NAME "AutoreportingEnabled"
    map_string_t *settings_map;
    int sv_logmode;
    int auto_reporting;
    int configured;

    settings_map = libreport_new_map_string ();

    if (!libreport_load_app_conf_file (APP_NAME, settings_map))
    {
        goto finito;
    }

    /* Silently ignore not configured options */
    sv_logmode = libreport_logmode;
    /* but only if we run in silent mode (no -v on command line) */
    libreport_logmode = libreport_g_verbose == 0 ? 0 : sv_logmode;

    auto_reporting = 0;
    configured = libreport_try_get_map_string_item_as_bool (settings_map, OPT_NAME, &auto_reporting);

    libreport_logmode = sv_logmode;

    if (configured == 0)
    {
        goto finito;
    }

    /* Enable the GS option if AutoreportingEnabled is true because the user
     * turned the Autoreporting in abrt-applet in a before GS.
     *
     * Do not disable the GS option if AutoreportingEvent is false because the
     * GS option is false by default, thus disabling would revert the user's
     * decision to  automatically report technical problems.
     */
    if (auto_reporting != 0)
    {
        g_autoptr (GSettings) settings = NULL;

        settings = g_settings_new (GS_SCHEMA_ID_PRIVACY);

        g_settings_set_boolean (settings, GS_PRIVACY_OPT_AUTO_REPORTING, TRUE);
    }

    libreport_remove_map_string_item (settings_map, OPT_NAME);
    libreport_save_app_conf_file (APP_NAME, settings_map);

    log_warning ("Successfully migrated "APP_NAME":"OPT_NAME" to "GS_SCHEMA_ID_PRIVACY":"GS_PRIVACY_OPT_AUTO_REPORTING);

#undef OPT_NAME
finito:
    libreport_free_map_string (settings_map);
}

static const char *
get_autoreport_event_name (void)
{
    const char *configured;

    libreport_load_user_settings (APP_NAME);

    configured = libreport_get_user_setting ("AutoreportingEvent");

    if (configured != NULL)
    {
        return configured;
    }

    return abrt_g_settings_autoreporting_event;
}

static bool
is_networking_enabled (void)
{
    GNetworkMonitor *monitor;

    monitor = g_network_monitor_get_default ();

    return g_network_monitor_get_connectivity (monitor) == G_NETWORK_CONNECTIVITY_FULL;
}

/* Compares the problem directories to list saved in
 * $XDG_CACHE_HOME/abrt/applet_dirlist and updates the applet_dirlist
 * with updated list.
 *
 * @param new_dirs The list where new directories are stored if caller
 * wishes it. Can be NULL.
 */
static void
new_dir_exists (GList **new_dirs)
{
    GList *dirlist;
    const char *cachedir;
    char *dirlist_name;
    FILE *fp;

    dirlist = get_problems_over_dbus (/*don't authorize*/false);
    if (dirlist == ERR_PTR)
    {
        return;
    }

    cachedir = g_get_user_cache_dir ();
    dirlist_name = libreport_concat_path_file (cachedir, "abrt");

    g_mkdir_with_parents (dirlist_name, 0777);

    g_free (dirlist_name);

    dirlist_name = libreport_concat_path_file (cachedir, "abrt/applet_dirlist");
    fp = fopen (dirlist_name, "r+");
    if (fp == NULL)
    {
        fp = fopen (dirlist_name, "w+");
    }

    g_free (dirlist_name);

    if (fp != NULL)
    {
        char *line;
        GList *old_dirlist = NULL;
        GList *l1;
        GList *l2;
        int different;

        while ((line = libreport_xmalloc_fgetline (fp)) != NULL)
        {
            old_dirlist = g_list_prepend (old_dirlist, line);
        }

        old_dirlist = g_list_reverse (old_dirlist);
        /* We will sort and compare current dir list with last known one.
         * Possible combinations:
         * DIR1 DIR1 - Both lists have the same element, advance both ptrs.
         * DIR2      - Current dir list has new element. IOW: new dir exists!
         *             Advance only current dirlist ptr.
         *      DIR3 - Only old list has element. Advance only old ptr.
         * DIR4 ==== - Old list ended, current one didn't. New dir exists!
         * ====
         */
        l1 = dirlist = g_list_sort (dirlist, (GCompareFunc) strcmp);
        l2 = old_dirlist = g_list_sort (old_dirlist, (GCompareFunc) strcmp);
        different = 0;

        while (l1 != NULL && l2 != NULL)
        {
            int diff;

            diff = strcmp (l1->data, l2->data);
            different |= diff;

            if (diff < 0)
            {
                if (new_dirs != NULL)
                {
                    *new_dirs = g_list_prepend (*new_dirs, libreport_xstrdup (l1->data));

                    log_notice ("New dir detected: %s", (char *) l1->data);
                }

                l1 = g_list_next (l1);

                continue;
            }

            l2 = g_list_next (l2);

            if (diff == 0)
            {
                l1 = g_list_next (l1);
            }
        }

        different |= (l1 != NULL);
        if (different && new_dirs != NULL)
        {
            while (l1 != NULL)
            {
                *new_dirs = g_list_prepend (*new_dirs, libreport_xstrdup (l1->data));

                log_notice ("New dir detected: %s", (char *) l1->data);

                l1 = g_list_next (l1);
            }
        }

        if (different || l2 != NULL)
        {
            rewind (fp);

            if (ftruncate (fileno (fp), 0) != 0)
            {
                /* ftruncate is declared with __attribute__ ((__warn_unused_result__))
                 * in glibc, and void-casting does not suppress the warning, so
                 * here we are, wasting all sorts of whitespace.
                 */
            }

            l1 = dirlist;
            while (l1 != NULL)
            {
                fprintf (fp, "%s\n", (char*) l1->data);

                l1 = g_list_next (l1);
            }
        }

        fclose (fp);
        g_list_free_full (old_dirlist, free);
    }

    g_list_free_full (dirlist, free);
}

static gboolean
is_app_running (GAppInfo *app)
{
    /* FIXME ask gnome-shell about that */
    return FALSE;
}

static void
fork_exec_gui (const char *problem_id)
{
    g_autoptr (GAppInfo) app = NULL;
    g_autoptr (GError) error = NULL;
    g_autofree char *cmd = NULL;

    cmd = g_strdup_printf (GUI_EXECUTABLE " -p %s", problem_id);
    app = g_app_info_create_from_commandline (cmd, GUI_EXECUTABLE,
                                              G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION,
                                              &error);
    if (app == NULL)
    {
        error_msg_and_die ("Cannot find " GUI_EXECUTABLE);
    }

    if (!g_app_info_launch (G_APP_INFO (app), NULL, NULL, &error))
    {
        perror_msg_and_die ("Could not launch " GUI_EXECUTABLE ": %s", error->message);
    }

    /* Scan dirs and save new $XDG_CACHE_HOME/abrt/applet_dirlist.
     * (Otherwise, after a crash, next time applet is started,
     * it will show alert icon even if we did click on it
     * "in previous life"). We ignore function return value.
     */
    new_dir_exists (/* new dirs list */ NULL);
}

static pid_t
spawn_event_handler_child (const char *dump_dir_name,
                           const char *event_name,
                           int        *fdp)
{
    char *args[7];
    int pipeout[2];
    int flags;
    char *env_vec[2];
    pid_t child;

    args[0] = (char *) LIBEXEC_DIR"/abrt-handle-event";
    args[1] = (char *) "-i"; /* Interactive? - Sure, applet is like a user */
    args[2] = (char *) "-e";
    args[3] = (char *) event_name;
    args[4] = (char *) "--";
    args[5] = (char *) dump_dir_name;
    args[6] = NULL;

    flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_QUIET | EXECFLG_ERR2OUT;
    VERB1 flags &= ~EXECFLG_QUIET;

    /* WTF? We use 'abrt-handle-event -i' but here we export REPORT_CLIENT_NONINTERACTIVE */
    /* - Exactly, REPORT_CLIENT_NONINTERACTIVE causes that abrt-handle-event in */
    /* interactive mode replies with empty responses to all event's questions. */
    env_vec[0] = g_strdup ("REPORT_CLIENT_NONINTERACTIVE=1");
    env_vec[1] = NULL;

    child = libreport_fork_execv_on_steroids (flags, args, fdp ? pipeout : NULL,
                                    env_vec, /*dir:*/ NULL, /*uid(unused):*/ 0);

    if (fdp != NULL)
    {
        *fdp = pipeout[0];
    }

    g_free (env_vec[0]);

    return child;
}

//this action should open gnome-abrt
static void
action_report (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
    g_autoptr (GError) error = NULL;
    const char *directory;

    log_debug ("Reporting a problem!");

    directory = g_variant_get_string (parameter, NULL);
    if (directory != NULL)
    {
        fork_exec_gui (directory);
    }
}

static void
action_restart (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
    const char *command_line;
    g_autoptr (GAppInfo) app = NULL;
    g_autoptr (GError) error = NULL;

    log_debug ("Restarting an application!");

    command_line = g_variant_get_string (parameter, NULL);
    app = problem_create_app_from_cmdline (command_line);

    g_assert (app != NULL);

    if (!g_app_info_launch (G_APP_INFO (app), NULL, NULL, &error))
    {
        perror_msg ("Could not launch '%s': %s",
                    g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (app)),
                    error->message);
    }
}

static GNotification *
new_warn_notification (const char *body)
{
    GNotification *notification;
    g_autoptr (GIcon) icon = NULL;

    notification = g_notification_new (_("Oops!"));
    icon = g_themed_icon_new (NOTIFICATION_ICON_NAME);

    g_notification_set_body (notification, body);
    g_notification_set_icon (notification, icon);

    return notification;
}

static void
add_default_action (GNotification         *notification,
                    AbrtAppletProblemInfo *problem_info)
{
    const char *directory;

    if (!g_gnome_abrt_available)
    {
        return;
    }

    directory = abrt_applet_problem_info_get_directory (problem_info);

    g_notification_set_default_action_and_target (notification, "app.report",
                                                  "s", directory);
}

static void
add_send_a_report_button (GNotification         *notification,
                          AbrtAppletProblemInfo *problem_info)
{
    const char *directory;

    if (!g_gnome_abrt_available)
    {
        return;
    }

    directory = abrt_applet_problem_info_get_directory (problem_info);

    g_notification_add_button_with_target (notification, _("Report"), "app.report",
                                           "s", directory);
}

static void
add_restart_app_button (GNotification         *notification,
                        AbrtAppletProblemInfo *problem_info)
{
    const char *command_line;

    if (!g_gnome_abrt_available)
    {
        return;
    }

    command_line = abrt_applet_problem_info_get_command_line (problem_info);

    g_notification_add_button_with_target (notification, _("Restart"), "app.restart",
                                           "s", command_line);
}

static void
abrt_applet_application_send_problem_notification (AbrtAppletApplication *self,
                                                   AbrtAppletProblemInfo *problem_info)
{
    gboolean auto_reporting;
    gboolean network_available;
    g_autofree char *notify_body = NULL;
    g_autoptr (GAppInfo) app = NULL;
    g_autoptr (GNotification) notification = NULL;

    if (abrt_applet_problem_info_is_announced (problem_info))
    {
        g_clear_object (&problem_info);
        return;
    }

    app = problem_create_app_from_env (abrt_applet_problem_info_get_environment (problem_info),
                                       abrt_applet_problem_info_get_pid (problem_info));
    if (app == NULL)
    {
        const char *const cmd_line = abrt_applet_problem_info_get_command_line (problem_info);
        if (cmd_line != NULL)
        {
            app = problem_create_app_from_cmdline (cmd_line);
        }
    }

    /* For each problem we'll need to know:
     * - Whether or not the crash happened in an “app”
     * - Whether the app is packaged (in Fedora) or not
     * - Whether the app is back up and running
     * - Whether the user is the one for which the app crashed
     * - Whether the problem has already been reported on this machine
     */
    gboolean is_app = (app != NULL);
    gboolean is_packaged = abrt_applet_problem_info_is_packaged (problem_info);;
    gboolean is_running_again = is_app_running(app);
    gboolean is_current_user = !abrt_applet_problem_info_is_foreign (problem_info);
    gboolean already_reported = abrt_applet_problem_info_get_count (problem_info) > 1;

    gboolean report_button = FALSE;
    gboolean restart_button = FALSE;

    auto_reporting = is_autoreporting_enabled ();
    network_available = is_networking_enabled ();

    if (is_app)
    {
        if (auto_reporting)
        {
            if (is_packaged)
            {
                if (network_available)
                {
                    notify_body = g_strdup_printf (_("We're sorry, it looks like %s crashed. The problem has been automatically reported."),
                                                   g_app_info_get_display_name (app));
                }
                else
                {
                    notify_body = g_strdup_printf (_("We’re sorry, it looks like %s crashed. The problem will be reported when the internet is available."),
                                                   g_app_info_get_display_name (app));
                }
            }
            else if (!already_reported)
            {
                notify_body = g_strdup_printf (_("We're sorry, it looks like %s crashed. Please contact the developer if you want to report the issue."),
                                               g_app_info_get_display_name (app));
            }
        }
        else
        {
            if (is_packaged)
            {
                notify_body = g_strdup_printf (_("We're sorry, it looks like %s crashed. If you'd like to help resolve the issue, please send a report."),
                                               g_app_info_get_display_name (app));
                report_button = TRUE;
            }
            else if (!already_reported)
            {
                notify_body = g_strdup_printf (_("We're sorry, it looks like %s crashed. Please contact the developer if you want to report the issue."),
                                               g_app_info_get_display_name (app));
            }
        }
        if (is_current_user && !is_running_again)
            restart_button = TRUE;
    } else {
        if (!already_reported)
        {
            if (auto_reporting && is_packaged)
            {
                if (network_available)
                {
                    notify_body = g_strdup (_("We're sorry, it looks like a problem occurred in a component. The problem has been automatically reported."));
                }
                else
                {
                    notify_body = g_strdup (_("We're sorry, it looks like a problem occurred in a component. The problem will be reported when the internet is available."));
                }
            }
            else if (!auto_reporting && is_packaged)
            {
                notify_body = g_strdup (_("We're sorry, it looks like a problem occurred. If you'd like to help resolve the issue, please send a report."));
                report_button = TRUE;
            }
            else
            {
                g_autofree char *binary = NULL;

                binary = problem_get_argv0 (abrt_applet_problem_info_get_command_line (problem_info));
                notify_body = g_strdup_printf (_("We're sorry, it looks like %s crashed. Please contact the developer if you want to report the issue."),
                                               binary);
            }
        }
    }

    if (notify_body == NULL)
    {
#define BOOL_AS_STR(x)  x ? "true" : "false"
        log_debug ("Not showing a notification, as we have no message to show:");
        log_debug ("auto reporting:    %s", BOOL_AS_STR (auto_reporting));
        log_debug ("network available: %s", BOOL_AS_STR (network_available));
        log_debug ("is app:            %s", BOOL_AS_STR (is_app));
        log_debug ("is packaged:       %s", BOOL_AS_STR (is_packaged));
        log_debug ("is running again:  %s", BOOL_AS_STR (is_running_again));
        log_debug ("is current user:   %s", BOOL_AS_STR (is_current_user));
        log_debug ("already reported:  %s", BOOL_AS_STR (already_reported));

        return;
    }

    notification = new_warn_notification (notify_body);

    abrt_applet_problem_info_set_announced (problem_info, true);

    if (report_button)
    {
        add_send_a_report_button (notification, problem_info);
    }
    if (restart_button)
    {
        add_restart_app_button (notification, problem_info);
    }

    add_default_action (notification, problem_info);

    log_debug ("Showing a notification");
    g_application_send_notification (G_APPLICATION (self), NULL, notification);
}

static void
abrt_applet_application_send_problem_notifications (AbrtAppletApplication *self,
                                                    GPtrArray             *problems)
{
    for (size_t i = 0; i < problems->len; i++)
    {
        AbrtAppletProblemInfo *problem_info;

        problem_info = g_ptr_array_index (problems, i);

        abrt_applet_application_send_problem_notification (self, problem_info);
    }
}

/* Event-processing child output handler */
static gboolean
handle_event_output_cb (GIOChannel   *gio,
                        GIOCondition  condition,
                        gpointer      data)
{
    EventProcessingState *state;
    AbrtAppletProblemInfo *problem_info;
    int status;

    state = data;
    problem_info = state->problem_info;

    /* Read streamed data and split lines */
    for (;;)
    {
        char buf[250]; /* usually we get one line, no need to have big buf */
        size_t bytes_read;
        g_autoptr (GError) error = NULL;
        GIOStatus stat;
        char *raw;
        char *newline;

        bytes_read = 0;
        stat = g_io_channel_read_chars (gio, buf, sizeof (buf) - 1, &bytes_read, &error);
        if (stat == G_IO_STATUS_ERROR)
        {   /* TODO: Terminate child's process? */
            error_msg (_("Can't read from gio channel: '%s'"), error ? error->message : "");
            break;
        }
        if (stat == G_IO_STATUS_AGAIN)
        {   /* We got all buffered data, but fd is still open. Done for now */
            return G_SOURCE_CONTINUE;
        }
        if (stat == G_IO_STATUS_EOF)
        {
            break;
        }

        buf[bytes_read] = '\0';

        raw = buf;

        /* split lines in the current buffer */
        while ((newline = strchr (raw, '\n')) != NULL)
        {
            *newline = '\0';

            libreport_strbuf_append_str (state->cmd_output, raw);

            log_debug ("%s", state->cmd_output->buf);

            libreport_strbuf_clear(state->cmd_output);
            /* jump to next line */
            raw = newline + 1;
        }

        /* beginning of next line. the line continues by next read */
        libreport_strbuf_append_str (state->cmd_output, raw);
    }

    /* EOF/error */

    /* Wait for child to actually exit, collect status */
    status = 1;
    if (libreport_safe_waitpid (state->child_pid, &status, 0) <= 0)
    {
        perror_msg ("waitpid(%d)", (int) state->child_pid);
    }

    if (WIFEXITED (status) && WEXITSTATUS (status) == EXIT_STOP_EVENT_RUN)
    {
        abrt_applet_problem_info_set_known (problem_info, true);
        status = 0;
    }

    if (status == 0)
    {
        abrt_applet_problem_info_set_reported (problem_info, true);

        log_debug ("fast report finished successfully");
        abrt_applet_application_send_problem_notification (state->application, problem_info);
    }
    else
    {
        log_debug ("fast report failed, deferring");
        g_ptr_array_add (state->application->deferred_problems, problem_info);
    }

    event_processing_state_free (state);

    /* We stop using this channel */
    g_io_channel_unref (gio);

    return G_SOURCE_REMOVE;
}

static GIOChannel *
my_io_channel_unix_new (int fd)
{
    GIOChannel *channel;
    g_autoptr (GError) error = NULL;

    channel = g_io_channel_unix_new (fd);
    /* Need to set the encoding otherwise we get:
     * "Invalid byte sequence in conversion input".
     * According to manual "NULL" is safe for binary data.
     */
    g_io_channel_set_encoding(channel, NULL, &error);
    if (error != NULL)
    {
        perror_msg_and_die(_("Can't set encoding on gio channel: %s"), error->message);
    }
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, &error);
    if (error != NULL)
    {
        perror_msg_and_die(_("Can't turn on nonblocking mode for gio channel: %s"), error->message);
    }

    g_io_channel_set_close_on_unref (channel, true);

    return channel;
}

static void
export_event_configuration (const char *event_name)
{
    static bool exported = false;
    event_config_t *event_config;
    g_autoptr (GList) ex_env = NULL;

    if (exported)
    {
        return;
    }

    exported = true;
    event_config = get_event_config (event_name);
    /* load event config data only for the event */
    if (event_config != NULL)
    {
        libreport_load_single_event_config_data_from_user_storage(event_config);
    }
    ex_env = export_event_config (event_name);
}

static void
abrt_applet_application_run_event_async (AbrtAppletApplication *self,
                                         AbrtAppletProblemInfo *problem_info,
                                         const char            *event_name)
{
    EventProcessingState *state;
    GIOChannel *channel_event_output;

    if (!abrt_applet_problem_info_ensure_writable (problem_info))
    {
        g_object_unref (problem_info);
        return;
    }

    export_event_configuration (event_name);

    state = event_processing_state_new ();

    state->application = g_object_ref (self);
    state->problem_info = problem_info;
    state->child_pid = spawn_event_handler_child(abrt_applet_problem_info_get_directory (state->problem_info),
                                                 event_name, &state->child_stdout_fd);

    channel_event_output = my_io_channel_unix_new (state->child_stdout_fd);

    g_io_add_watch (channel_event_output, G_IO_IN | G_IO_PRI | G_IO_HUP,
                    handle_event_output_cb, state);
}

static bool
abrt_applet_application_report_problem (AbrtAppletApplication *self,
                                        AbrtAppletProblemInfo *problem_info)
{
    if (abrt_applet_problem_info_is_foreign (problem_info) && !g_user_is_admin)
    {
        return false;
    }

    if (!is_networking_enabled ())
    {
        g_ptr_array_add (self->deferred_problems, problem_info);

        return false;
    }

    abrt_applet_application_run_event_async (self, problem_info, get_autoreport_event_name ());

    return true;
}

static void
abrt_applet_application_report_problems (AbrtAppletApplication *self,
                                         GPtrArray             *problems)
{
    if (!is_autoreporting_enabled ())
    {
        return;
    }

    for (size_t i = 0; i < problems->len; i++)
    {
        AbrtAppletProblemInfo *problem_info;

        problem_info = g_ptr_array_index (problems, i);

        if (abrt_applet_application_report_problem (self, problem_info))
        {
            g_ptr_array_remove_index (problems, i);
        }
    }
}

static void
abrt_applet_application_dispose (GObject *object)
{
    AbrtAppletApplication *self;

    self = ABRT_APPLET_APPLICATION (object);

    g_clear_pointer (&self->deferred_problems, g_ptr_array_unref);
}

static void
handle_message (GDBusConnection *connection,
                const gchar     *sender_name,
                const gchar     *object_path,
                const gchar     *interface_name,
                const gchar     *signal_name,
                GVariant        *parameters,
                gpointer         user_data)
{
    const char *package_name;
    const char *dir;
    const char *uid_str;
    const char *uuid;
    const char *duphash;
    AbrtAppletProblemInfo *problem_info;
    AbrtAppletApplication *self;

    g_debug ("Received signal: sender_name: %s, object_path: %s, "
             "interface_name: %s, signal_name: %s",
             sender_name, object_path, interface_name, signal_name);

    log_debug ("Crash recorded");

    g_variant_get (parameters, "(&s&s&s&s&s)",
                   &package_name,
                   &dir,
                   &uid_str,
                   &uuid,
                   &duphash);

    bool foreign_problem = false;
    if (uid_str[0] != '\0')
    {
        char *end;
        unsigned long uid_num;

        errno = 0;

        uid_num = strtoul (uid_str, &end, 10);

        if (errno != 0 || *end != '\0' || uid_num != getuid ())
        {
            foreign_problem = true;
            log_notice ("foreign problem %i", foreign_problem);
        }
    }

    /* Non-admins shouldn't see other people's crashes */
    if (foreign_problem && !g_user_is_admin)
    {
        return;
    }

    problem_info = abrt_applet_problem_info_new (dir);

    abrt_applet_problem_info_load_over_dbus (problem_info);

    abrt_applet_problem_info_set_foreign (problem_info, foreign_problem);

    self = user_data;

    /*
     * Can't append dir to the seen list because of directory stealing
     *
     * append_dirlist(dir);
     *
     */
    if (is_autoreporting_enabled ())
    {
        abrt_applet_application_report_problem (self, problem_info);
    }
    abrt_applet_application_send_problem_notification (self, problem_info);
}

static void
abrt_applet_application_process_new_directories (AbrtAppletApplication *self)
{
    /* If some new dirs appeared since our last run, let user know it */
    GList *new_dirs = NULL;
    g_autoptr (GPtrArray) problems = NULL;
#define time_before_ndays(n) (time(NULL) - (n)*24*60*60)
     /* Age limit = now - 3 days */
    const unsigned long min_born_time = (unsigned long)(time_before_ndays(3));

    problems = g_ptr_array_new_with_free_func (g_object_unref);

    new_dir_exists (&new_dirs);

    for ( ; new_dirs != NULL; new_dirs = g_list_next (new_dirs))
    {
        const char *problem_id;
        g_autoptr (AbrtAppletProblemInfo) problem_info = NULL;

        problem_id = new_dirs->data;
        problem_info = abrt_applet_problem_info_new (problem_id);

        if (!abrt_applet_problem_info_load_over_dbus (problem_info))
        {
            log_notice("'%s' is not a dump dir - ignoring\n", problem_id);
            continue;
        }

        /* TODO: add a filter for only complete problems to GetProblems D-Bus method */
        if (!dbus_problem_is_complete (problem_id))
        {
            log_notice ("Ignoring incomplete problem '%s'", problem_id);
            continue;
        }

        /* TODO: add a filter for max-old reported problems to GetProblems D-Bus method */
        if (abrt_applet_problem_info_get_time (problem_info) < min_born_time)
        {
            log_notice ("Ignoring outdated problem '%s'", problem_id);
            continue;
        }

        /* TODO: add a filter for not-yet reported problems to GetProblems D-Bus method */
        if (abrt_applet_problem_info_is_reported (problem_info))
        {
            log_notice ("Ignoring already reported problem '%s'", problem_id);
            continue;
        }

        /* Can't be foreign because new_dir_exists() returns only own problems */
        abrt_applet_problem_info_set_foreign (problem_info, false);

        g_ptr_array_add (problems, g_steal_pointer (&problem_info));
    }

    abrt_applet_application_report_problems (self, problems);
    abrt_applet_application_send_problem_notifications (self, problems);

    g_list_free_full (new_dirs, free);
}

static void
add_action_entries (GApplication *application)
{
    static const GActionEntry action_entries[] =
    {
        { "report", action_report, "s", NULL, NULL },
        { "restart", action_restart, "s", NULL, NULL },
    };

    g_action_map_add_action_entries (G_ACTION_MAP (application),
                                     action_entries, G_N_ELEMENTS (action_entries),
                                     NULL);
}

static void
abrt_applet_application_startup (GApplication *application)
{
    AbrtAppletApplication *self;
    g_autoptr (GError) error = NULL;

    G_APPLICATION_CLASS (abrt_applet_application_parent_class)->startup (application);

    self = ABRT_APPLET_APPLICATION (application);

    migrate_to_xdg_dirs ();
    migrate_auto_reporting_to_gsettings ();

    libreport_export_abrt_envvars (0);
    libreport_msg_prefix = libreport_g_progname;

    abrt_load_abrt_conf ();
    load_event_config_data ();
    libreport_load_user_settings (APP_NAME);

    self->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (self->connection == NULL)
    {
        perror_msg_and_die ("Can't connect to system dbus: %s", error->message);
    }

    self->filter_id = g_dbus_connection_signal_subscribe (self->connection,
                                                          NULL,
                                                          ABRT_DBUS_NAME,
                                                          "Crash",
                                                          ABRT_DBUS_OBJECT,
                                                          NULL,
                                                          G_DBUS_SIGNAL_FLAGS_NONE,
                                                          handle_message,
                                                          g_object_ref (self),
                                                          g_object_unref);

    g_user_is_admin = is_user_admin ();

    g_gnome_abrt_available = is_gnome_abrt_available ();

    abrt_applet_application_process_new_directories (self);
    add_action_entries (application);

    g_application_hold (application);
}

static void
abrt_applet_application_activate (GApplication *application)
{
}

static void
abrt_applet_application_class_init (AbrtAppletApplicationClass *klass)
{
    GObjectClass *object_class;
    GApplicationClass *application_class;

    object_class = G_OBJECT_CLASS (klass);
    application_class = G_APPLICATION_CLASS (klass);

    object_class->dispose = abrt_applet_application_dispose;

    application_class->startup = abrt_applet_application_startup;
    application_class->activate = abrt_applet_application_activate;
}

GApplication *
abrt_applet_application_new (void)
{
    return g_object_new (ABRT_APPLET_TYPE_APPLICATION,
                         "application-id", ABRT_DBUS_NAME ".applet",
                         "flags", G_APPLICATION_FLAGS_NONE,
                         NULL);
}
