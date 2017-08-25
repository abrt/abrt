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

#include <gio/gdesktopappinfo.h>
#define GDK_DISABLE_DEPRECATION_WARNINGS
/* https://bugzilla.gnome.org/show_bug.cgi?id=734826 */
#include <gtk/gtk.h>

#include <polkit/polkit.h>
#include <libnotify/notify.h>
#include <glib.h>

#include <libreport/internal_abrt_dbus.h>
#include <libreport/event_config.h>
#include <libreport/internal_libreport_gtk.h>
#include <libreport/problem_utils.h>
#include "libabrt.h"
#include "problem_api.h"

#define APP_NAME "abrt-applet"
#define GS_SCHEMA_ID_PRIVACY "org.gnome.desktop.privacy"
#define GS_PRIVACY_OPT_AUTO_REPORTING "report-technical-problems"

/* libnotify action keys */
#define A_REPORT_REPORT "REPORT"
#define A_RESTART_APPLICATION "RESTART"

#define GUI_EXECUTABLE "gnome-abrt"

#define NOTIFICATION_ICON_NAME "face-sad-symbolic"

static GNetworkMonitor *netmon;
static GList *g_deferred_crash_queue;
static guint g_deferred_timeout;
static bool g_gnome_abrt_available;
static bool g_user_is_admin;

static bool is_autoreporting_enabled(void)
{
    GSettings *settings;
    gboolean ret;

    settings = g_settings_new (GS_SCHEMA_ID_PRIVACY);
    ret = g_settings_get_boolean (settings, GS_PRIVACY_OPT_AUTO_REPORTING);
    g_object_unref (settings);
    return ret;
}

static void migrate_auto_reporting_to_gsettings(void)
{
#define OPT_NAME "AutoreportingEnabled"
    map_string_t *settings = new_map_string();
    if (!load_app_conf_file(APP_NAME, settings))
        goto finito;

    /* Silently ignore not configured options */
    int sv_logmode = logmode;
    /* but only if we run in silent mode (no -v on command line) */
    logmode = g_verbose == 0 ? 0 : sv_logmode;

    int auto_reporting = 0;
    int configured = try_get_map_string_item_as_bool(settings, OPT_NAME, &auto_reporting);

    logmode = sv_logmode;

    if (!configured)
        goto finito;

    /* Enable the GS option if AutoreportingEnabled is true because the user
     * turned the Autoreporting in abrt-applet in a before GS.
     *
     * Do not disable the GS option if AutoreportingEvent is false because the
     * GS option is false by default, thus disabling would revert the user's
     * decision to  automatically report technical problems.
     */
    if (auto_reporting)
    {
        GSettings *settings = g_settings_new(GS_SCHEMA_ID_PRIVACY);
        g_settings_set_boolean(settings, GS_PRIVACY_OPT_AUTO_REPORTING, TRUE);
        g_object_unref(settings);
    }

    remove_map_string_item(settings, OPT_NAME);
    save_app_conf_file(APP_NAME, settings);

    log_warning("Successfully migrated "APP_NAME":"OPT_NAME" to "GS_SCHEMA_ID_PRIVACY":"GS_PRIVACY_OPT_AUTO_REPORTING);

#undef OPT_NAME
finito:
    free_map_string(settings);
    return;
}

static const char *get_autoreport_event_name(void)
{
    load_user_settings(APP_NAME);
    const char *configured = get_user_setting("AutoreportingEvent");
    return configured ? configured : g_settings_autoreporting_event;
}

static bool is_networking_enabled(void)
{
    if (!g_network_monitor_get_network_available(netmon))
        return FALSE;
    return g_network_monitor_get_connectivity(netmon) == G_NETWORK_CONNECTIVITY_FULL;
}

static void show_problem_list_notification(GList *problems);
static void problem_info_unref(gpointer data);

static gboolean process_deferred_queue_timeout_fn(void)
{
    g_deferred_timeout = 0;

    GList *tmp = g_deferred_crash_queue;
    g_deferred_crash_queue = NULL;

    /* this function calls push_to_deferred_queue() which appends data to
     * g_deferred_crash_queue but the function also modifies the argument
     * so we must reset g_deferred_crash_queue before the call */
    show_problem_list_notification(tmp);

    /* Remove this timeout fn from the main loop*/
    return G_SOURCE_REMOVE;
}

static void connectivity_changed_cb(GObject    *gobject,
                                    GParamSpec *pspec,
                                    gpointer    user_data)
{
    if (g_network_monitor_get_network_available(netmon) &&
        g_network_monitor_get_connectivity(netmon) == G_NETWORK_CONNECTIVITY_FULL)
    {
        if (g_deferred_timeout)
            g_source_remove(g_deferred_timeout);

        g_deferred_timeout = g_idle_add ((GSourceFunc)process_deferred_queue_timeout_fn, NULL);
    }
}

typedef struct problem_info {
    problem_data_t *problem_data;
    int refcount;
    bool foreign;
    guint count;
    bool is_packaged;
    char **envp;
    pid_t pid;
    bool known;
    bool reported;
    bool was_announced;
    bool is_writable;
    int time;
} problem_info_t;

static void push_to_deferred_queue(problem_info_t *pi)
{
    g_deferred_crash_queue = g_list_append(g_deferred_crash_queue, pi);
}

static const char *problem_info_get_dir(problem_info_t *pi)
{
    return problem_data_get_content_or_NULL(pi->problem_data, CD_DUMPDIR);
}

static const char *problem_info_get_command_line(problem_info_t *pi)
{
    return problem_data_get_content_or_NULL(pi->problem_data, FILENAME_CMDLINE);
}

static int problem_info_get_time(problem_info_t *pi)
{
    if (pi->time == -1)
    {
        const char *time_str = problem_data_get_content_or_NULL(pi->problem_data, FILENAME_TIME);

        if (time_str == NULL)
            error_msg_and_die("BUG: Problem info has data without the element time");

        pi->time = atoi(time_str);
    }

    return pi->time;
}

static const char **problem_info_get_env(problem_info_t *pi)
{
    if (pi->envp == NULL)
    {
        const char *env_str = problem_data_get_content_or_NULL(pi->problem_data, FILENAME_ENVIRON);
        pi->envp = (env_str != NULL) ? g_strsplit (env_str, "\n", -1) : NULL;
    }

    return (const char **)pi->envp;
}

static int problem_info_get_pid(problem_info_t *pi)
{
    if (pi->pid == -1)
    {
        const char *pid_str = problem_data_get_content_or_NULL(pi->problem_data, FILENAME_PID);
        pi->pid = (pid_str != NULL) ? atoi (pid_str) : -1;
    }

    return pi->pid;
}

static int problem_info_get_count(problem_info_t *pi)
{
    if (pi->count == -1)
    {
        const char *count_str = problem_data_get_content_or_NULL(pi->problem_data, FILENAME_COUNT);
        pi->count = count_str ? atoi(count_str) : 1;
    }

    return pi->count;
}

static bool problem_info_is_reported(problem_info_t *pi)
{
    return problem_data_get_content_or_NULL(pi->problem_data, FILENAME_REPORTED_TO) != NULL;
}

static void problem_info_set_dir(problem_info_t *pi, const char *dir)
{
    problem_data_add_text_noteditable(pi->problem_data, CD_DUMPDIR, dir);
}

static bool problem_info_ensure_writable(problem_info_t *pi)
{
    if (pi->is_writable)
        return true;

    /* chown the directory in any case, because kernel oopses are not foreign */
    /* but their dump directories are not writable without chowning them or */
    /* stealing them. The stealing is deprecated as it breaks the local */
    /* duplicate search and root cannot see them */
    const int res = chown_dir_over_dbus(problem_info_get_dir(pi));
    if (pi->foreign && res != 0)
    {
        error_msg(_("Can't take ownership of '%s'"), problem_info_get_dir(pi));
        return false;
    }
    pi->foreign = false;

    struct dump_dir *dd = open_directory_for_writing(problem_info_get_dir(pi), /* don't ask */ NULL);
    if (!dd)
    {
        error_msg(_("Can't open directory for writing '%s'"), problem_info_get_dir(pi));
        return false;
    }

    problem_info_set_dir(pi, dd->dd_dirname);
    pi->is_writable = true;
    dd_close(dd);
    return true;
}

static problem_info_t *problem_info_new(const char *dir)
{
    problem_info_t *pi = g_new0(problem_info_t, 1);
    pi->refcount = 1;
    pi->time = -1;
    pi->pid = -1;
    pi->count = -1;
    pi->problem_data = problem_data_new();
    problem_info_set_dir(pi, dir);
    return pi;
}

static void problem_info_unref(gpointer data)
{
    problem_info_t *pi;

    if (data == NULL)
        return;

    pi = data;
    pi->refcount--;
    if (pi->refcount > 0)
        return;

    problem_data_free(pi->problem_data);
    g_free(pi);
}

static problem_info_t* problem_info_ref(problem_info_t *pi)
{
    g_return_val_if_fail (pi != NULL, NULL);

    pi->refcount++;
    return pi;
}

static void run_event_async(problem_info_t *pi, const char *event_name);

struct event_processing_state
{
    pid_t child_pid;
    int   child_stdout_fd;
    struct strbuf *cmd_output;

    problem_info_t *pi;
    int flags;
};

static struct event_processing_state *new_event_processing_state(void)
{
    struct event_processing_state *p = g_new0(struct event_processing_state, 1);
    p->child_pid = -1;
    p->child_stdout_fd = -1;
    p->cmd_output = strbuf_new();
    return p;
}

static void free_event_processing_state(struct event_processing_state *p)
{
    if (!p)
        return;

    strbuf_free(p->cmd_output);
    g_free(p);
}

/* Compares the problem directories to list saved in
 * $XDG_CACHE_HOME/abrt/applet_dirlist and updates the applet_dirlist
 * with updated list.
 *
 * @param new_dirs The list where new directories are stored if caller
 * wishes it. Can be NULL.
 */
static void new_dir_exists(GList **new_dirs)
{
    GList *dirlist = get_problems_over_dbus(/*don't authorize*/false);
    if (dirlist == ERR_PTR)
        return;

    const char *cachedir = g_get_user_cache_dir();
    char *dirlist_name = concat_path_file(cachedir, "abrt");
    g_mkdir_with_parents(dirlist_name, 0777);
    free(dirlist_name);
    dirlist_name = concat_path_file(cachedir, "abrt/applet_dirlist");
    FILE *fp = fopen(dirlist_name, "r+");
    if (!fp)
        fp = fopen(dirlist_name, "w+");
    free(dirlist_name);
    if (fp)
    {
        GList *old_dirlist = NULL;
        char *line;
        while ((line = xmalloc_fgetline(fp)) != NULL)
            old_dirlist = g_list_prepend(old_dirlist, line);

        old_dirlist = g_list_reverse(old_dirlist);
        /* We will sort and compare current dir list with last known one.
         * Possible combinations:
         * DIR1 DIR1 - Both lists have the same element, advance both ptrs.
         * DIR2      - Current dir list has new element. IOW: new dir exists!
         *             Advance only current dirlist ptr.
         *      DIR3 - Only old list has element. Advance only old ptr.
         * DIR4 ==== - Old list ended, current one didn't. New dir exists!
         * ====
         */
        GList *l1 = dirlist = g_list_sort(dirlist, (GCompareFunc)strcmp);
        GList *l2 = old_dirlist = g_list_sort(old_dirlist, (GCompareFunc)strcmp);
        int different = 0;
        while (l1 && l2)
        {
            int diff = strcmp(l1->data, l2->data);
            different |= diff;
            if (diff < 0)
            {
                if (new_dirs)
                {
                    *new_dirs = g_list_prepend(*new_dirs, xstrdup(l1->data));
                    log_notice("New dir detected: %s", (char *)l1->data);
                }
                l1 = g_list_next(l1);
                continue;
            }
            l2 = g_list_next(l2);
            if (diff == 0)
                l1 = g_list_next(l1);
        }

        different |= (l1 != NULL);
        if (different && new_dirs)
        {
            while (l1)
            {
                *new_dirs = g_list_prepend(*new_dirs, xstrdup(l1->data));
                log_notice("New dir detected: %s", (char *)l1->data);
                l1 = g_list_next(l1);
            }
        }

        if (different || l2)
        {
            rewind(fp);
            if (ftruncate(fileno(fp), 0)) /* shut up gcc */;
            l1 = dirlist;
            while (l1)
            {
                fprintf(fp, "%s\n", (char*) l1->data);
                l1 = g_list_next(l1);
            }
        }
        fclose(fp);
        list_free_with_free(old_dirlist);
    }
    list_free_with_free(dirlist);
}

static bool is_gnome_abrt_available(void)
{
    GAppInfo *app;
    GError *error = NULL;
    bool ret = TRUE;

    app = g_app_info_create_from_commandline (GUI_EXECUTABLE, GUI_EXECUTABLE,
                                              G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION,
                                              &error);
    if (!app)
    {
        log_debug("Cannot find " GUI_EXECUTABLE ": %s", error->message);
        g_error_free(error);
        ret = FALSE;
    }

    g_clear_object(&app);

    return ret;
}

static bool is_user_admin(void)
{
    GError *error = NULL;
    bool ret = false;
    GPermission *perm = polkit_permission_new_sync ("org.freedesktop.problems.getall",
                                                    NULL, NULL, &error);
    if (!perm)
        perror_msg_and_die("Can't get Polkit configuration: %s", error->message);

    ret = g_permission_get_allowed (perm);

    g_object_unref (perm);

    return ret;
}

static gboolean
is_app_running (GAppInfo *app)
{
    /* FIXME ask gnome-shell about that */
    return FALSE;
}

static void fork_exec_gui(const char *problem_id)
{
    GAppInfo *app;
    GError *error = NULL;
    char *cmd;

    cmd = g_strdup_printf (GUI_EXECUTABLE " -p %s", problem_id);
    app = g_app_info_create_from_commandline (cmd, GUI_EXECUTABLE,
                                              G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION,
                                              &error);
    g_free(cmd);

    if (!app)
        error_msg_and_die("Cannot find " GUI_EXECUTABLE);

    if (!g_app_info_launch(G_APP_INFO(app), NULL, NULL, &error))
        perror_msg_and_die("Could not launch " GUI_EXECUTABLE ": %s", error->message);

    /* Scan dirs and save new $XDG_CACHE_HOME/abrt/applet_dirlist.
     * (Otherwise, after a crash, next time applet is started,
     * it will show alert icon even if we did click on it
     * "in previous life"). We ignore function return value.
     */
    new_dir_exists(/* new dirs list */ NULL);
}

static pid_t spawn_event_handler_child(const char *dump_dir_name, const char *event_name, int *fdp)
{
    char *args[7];
    args[0] = (char *) LIBEXEC_DIR"/abrt-handle-event";
    args[1] = (char *) "-i"; /* Interactive? - Sure, applet is like a user */
    args[2] = (char *) "-e";
    args[3] = (char *) event_name;
    args[4] = (char *) "--";
    args[5] = (char *) dump_dir_name;
    args[6] = NULL;

    int pipeout[2];
    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_QUIET | EXECFLG_ERR2OUT;
    VERB1 flags &= ~EXECFLG_QUIET;

    char *env_vec[2];

    /* WTF? We use 'abrt-handle-event -i' but here we export REPORT_CLIENT_NONINTERACTIVE */
    /* - Exactly, REPORT_CLIENT_NONINTERACTIVE causes that abrt-handle-event in */
    /* interactive mode replies with empty responses to all event's questions. */
    env_vec[0] = g_strdup("REPORT_CLIENT_NONINTERACTIVE=1");
    env_vec[1] = NULL;

    pid_t child = fork_execv_on_steroids(flags, args, fdp ? pipeout : NULL,
            env_vec, /*dir:*/ NULL, /*uid(unused):*/ 0);

    if (fdp)
        *fdp = pipeout[0];

    g_free(env_vec[0]);

    return child;
}

//this action should open gnome-abrt
static void action_report(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    log_debug("Reporting a problem!");
    /* must be closed before ask_yes_no dialog run */
    GError *err = NULL;
    notify_notification_close(notification, &err);
    if (err != NULL)
    {
        error_msg(_("Can't close notification: %s"), err->message);
        g_error_free(err);
    }

    problem_info_t *pi = (problem_info_t *)user_data;
    if (problem_info_get_dir(pi))
        fork_exec_gui(problem_info_get_dir(pi));
}

static void action_restart(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    GAppInfo *app;
    log_debug("Restarting an application!");
    /* must be closed before ask_yes_no dialog run */
    GError *err = NULL;
    notify_notification_close(notification, &err);
    if (err != NULL)
    {
        error_msg(_("Can't close notification: %s"), err->message);
        g_error_free(err);
    }

    problem_info_t *pi = (problem_info_t *)user_data;
    app = problem_create_app_from_cmdline (problem_info_get_command_line(pi));
    g_assert (app);

    if (!g_app_info_launch(G_APP_INFO(app), NULL, NULL, &err))
    {
        perror_msg("Could not launch '%s': %s",
                   g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (app)),
                   err->message);
    }
    g_object_unref (app);
}

static void on_notify_close(NotifyNotification *notification, gpointer user_data)
{
    log_debug("Notify closed!");
    g_object_unref(notification);

    /* Scan dirs and save new $XDG_CACHE_HOME/abrt/applet_dirlist.
     * (Otherwise, after a crash, next time applet is started,
     * it will show alert icon even if we did click on it
     * "in previous life"). We ignore finction return value.
     */
    new_dir_exists(/* new dirs list */ NULL);
}

static NotifyNotification *new_warn_notification(const char *body)
{
    NotifyNotification *notification;

    notification = notify_notification_new(_("Oops!"), body, NOTIFICATION_ICON_NAME);

    g_signal_connect(notification, "closed", G_CALLBACK(on_notify_close), NULL);

    notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(notification, NOTIFY_EXPIRES_DEFAULT);
    notify_notification_set_hint(notification, "desktop-entry", g_variant_new_string(APP_NAME));

    return notification;
}

static void
add_default_action (NotifyNotification *notification,
                    problem_info_t     *pi)
{
    if (!g_gnome_abrt_available)
        return;

    /* Using the same action as for report */
    notify_notification_add_action(notification, "default", _("Report"),
            NOTIFY_ACTION_CALLBACK(action_report),
            problem_info_ref (pi), problem_info_unref);
}

static void
add_send_a_report_button (NotifyNotification *notification,
                          problem_info_t     *pi)
{
    if (!g_gnome_abrt_available)
        return;

    notify_notification_add_action(notification, A_REPORT_REPORT, _("Report"),
            NOTIFY_ACTION_CALLBACK(action_report),
            problem_info_ref (pi), problem_info_unref);
}

static void
add_restart_app_button (NotifyNotification *notification,
                        problem_info_t     *pi)
{
    notify_notification_add_action(notification, A_RESTART_APPLICATION, _("Restart"),
            NOTIFY_ACTION_CALLBACK(action_restart),
            problem_info_ref (pi), problem_info_unref);
}

/*
 * Destroys the problems argument
 */
static void notify_problem_list(GList *problems)
{
    if (problems == NULL)
    {
        log_debug("Not showing any notification bubble because the list of problems is empty.");
        return;
    }

    /* For the whole system, we'll need to know:
     * - Whether automatic reporting is enabled or not
     * - Whether the network is available
     */
    gboolean auto_reporting = is_autoreporting_enabled();
    gboolean network_available = is_networking_enabled();

    for (GList *iter = problems; iter; iter = g_list_next(iter))
    {
        char *notify_body = NULL;
        GAppInfo *app;

        problem_info_t *pi = iter->data;

        if (pi->was_announced)
        {
            problem_info_unref (pi);
            continue;
        }

        app = problem_create_app_from_env (problem_info_get_env(pi), problem_info_get_pid(pi));

        if (!app)
        {
            const char *const cmd_line = problem_info_get_command_line(pi);
            if (cmd_line != NULL)
                app = problem_create_app_from_cmdline(cmd_line);
        }

        /* For each problem we'll need to know:
         * - Whether or not the crash happened in an “app”
         * - Whether the app is packaged (in Fedora) or not
         * - Whether the app is back up and running
         * - Whether the user is the one for which the app crashed
         * - Whether the problem has already been reported on this machine
         */
        gboolean is_app = (app != NULL);
        gboolean is_packaged = pi->is_packaged;
        gboolean is_running_again = is_app_running(app);
        gboolean is_current_user = !pi->foreign;
        gboolean already_reported = problem_info_get_count(pi) > 1;

        gboolean report_button = FALSE;
        gboolean restart_button = FALSE;

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
                    char *binary = problem_get_argv0 (problem_info_get_command_line(pi));
                    notify_body = g_strdup_printf (_("We're sorry, it looks like %s crashed. Please contact the developer if you want to report the issue."),
                                                   binary);
                    g_free (binary);
                }
            }
        }

        if (!notify_body)
        {
#define BOOL_AS_STR(x)  x ? "true" : "false"
            log_debug ("Not showing a notification, as we have no message to show:");
            log_debug ("auto reporting:    %s", BOOL_AS_STR(auto_reporting));
            log_debug ("network available: %s", BOOL_AS_STR(network_available));
            log_debug ("is app:            %s", BOOL_AS_STR(is_app));
            log_debug ("is packaged:       %s", BOOL_AS_STR(is_packaged));
            log_debug ("is running again:  %s", BOOL_AS_STR(is_running_again));
            log_debug ("is current user:   %s", BOOL_AS_STR(is_current_user));
            log_debug ("already reported:  %s", BOOL_AS_STR(already_reported));

            g_clear_object (&app);
            problem_info_unref (pi);
            continue;
        }

        NotifyNotification *notification = new_warn_notification(notify_body);
        g_free(notify_body);

        pi->was_announced = true;

        if (report_button)
            add_send_a_report_button (notification, pi);
        if (restart_button)
            add_restart_app_button (notification, pi);

        add_default_action (notification, pi);

        GError *err = NULL;
        log_debug("Showing a notification");
        notify_notification_show(notification, &err);
        if (err != NULL)
        {
            error_msg(_("Can't show notification: %s"), err->message);
            g_error_free(err);
        }

        problem_info_unref (pi);
    }

    g_list_free(problems);
}

static void notify_problem(problem_info_t *pi)
{
    GList *problems = g_list_append(NULL, pi);
    notify_problem_list(problems);
}

/* Event-processing child output handler */
static gboolean handle_event_output_cb(GIOChannel *gio, GIOCondition condition, gpointer ptr)
{
    struct event_processing_state *state = ptr;
    problem_info_t *pi = state->pi;

    /* Read streamed data and split lines */
    for (;;)
    {
        char buf[250]; /* usually we get one line, no need to have big buf */
        errno = 0;
        gsize r = 0;
        GError *error = NULL;
        GIOStatus stat = g_io_channel_read_chars(gio, buf, sizeof(buf) - 1, &r, &error);
        if (stat == G_IO_STATUS_ERROR)
        {   /* TODO: Terminate child's process? */
            error_msg(_("Can't read from gio channel: '%s'"), error ? error->message : "");
            g_error_free(error);
            break;
        }
        if (stat == G_IO_STATUS_AGAIN)
        {   /* We got all buffered data, but fd is still open. Done for now */
            return TRUE; /* "glib, please don't remove this event (yet)" */
        }
        if (stat == G_IO_STATUS_EOF)
            break;

        buf[r] = '\0';

        /* split lines in the current buffer */
        char *raw = buf;
        char *newline;
        while ((newline = strchr(raw, '\n')) != NULL)
        {
            *newline = '\0';
            strbuf_append_str(state->cmd_output, raw);
            char *msg = state->cmd_output->buf;

            log_debug("%s", msg);

            strbuf_clear(state->cmd_output);
            /* jump to next line */
            raw = newline + 1;
        }

        /* beginning of next line. the line continues by next read */
        strbuf_append_str(state->cmd_output, raw);
    }

    /* EOF/error */

    /* Wait for child to actually exit, collect status */
    int status = 1;
    if (safe_waitpid(state->child_pid, &status, 0) <= 0)
        perror_msg("waitpid(%d)", (int)state->child_pid);

    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_STOP_EVENT_RUN)
    {
        pi->known = 1;
        status = 0;
    }

    if (status == 0)
    {
        pi->reported = 1;

        log_debug("fast report finished successfully");
        notify_problem(pi);
    }
    else
    {
        log_debug("fast report failed, deferring");
        push_to_deferred_queue(pi);
    }

    free_event_processing_state(state);

    /* We stop using this channel */
    g_io_channel_unref(gio);

    return FALSE;
}

static GIOChannel *my_io_channel_unix_new(int fd)
{
    GIOChannel *ch = g_io_channel_unix_new(fd);

    /* Need to set the encoding otherwise we get:
     * "Invalid byte sequence in conversion input".
     * According to manual "NULL" is safe for binary data.
     */
    GError *error = NULL;
    g_io_channel_set_encoding(ch, NULL, &error);
    if (error)
        perror_msg_and_die(_("Can't set encoding on gio channel: %s"), error->message);

    g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, &error);
    if (error)
        perror_msg_and_die(_("Can't turn on nonblocking mode for gio channel: %s"), error->message);

    g_io_channel_set_close_on_unref(ch, TRUE);

    return ch;
}

static void export_event_configuration(const char *event_name)
{
    static bool exported = false;
    if (exported)
        return;

    exported = true;

    event_config_t *event_config = get_event_config(event_name);

    /* load event config data only for the event */
    if (event_config != NULL)
        load_single_event_config_data_from_user_storage(event_config);

    GList *ex_env = export_event_config(event_name);
    g_list_free(ex_env);
}

static void run_event_async(problem_info_t *pi, const char *event_name)
{
    if (!problem_info_ensure_writable(pi))
    {
        problem_info_unref(pi);
        return;
    }

    export_event_configuration(event_name);

    struct event_processing_state *state = new_event_processing_state();
    state->pi = pi;

    state->child_pid = spawn_event_handler_child(problem_info_get_dir(state->pi), event_name, &state->child_stdout_fd);

    GIOChannel *channel_event_output = my_io_channel_unix_new(state->child_stdout_fd);
    g_io_add_watch(channel_event_output, G_IO_IN | G_IO_PRI | G_IO_HUP,
                   handle_event_output_cb, state);
}

/*
 * Destroys the problems argument
 */
static void show_problem_list_notification(GList *problems)
{
    if (is_autoreporting_enabled())
    {
        /* Automatically report only own problems */
        /* and skip foreign problems */
        for (GList *iter = problems; iter;)
        {
            problem_info_t *pi = (problem_info_t *)iter->data;
            GList *next = g_list_next(iter);

            if (!pi->foreign || g_user_is_admin)
            {
                if (is_networking_enabled ())
                {
                    run_event_async(pi, get_autoreport_event_name());
                    problems = g_list_delete_link(problems, iter);
                }
                else
                {
                    /* Don't remove from the list, we'll tell the user
                     * we'll report later, if it's not a dupe */
                    push_to_deferred_queue(pi);
                }
            }

            iter = next;
        }

    }

    /* report the rest:
     *  - only foreign if autoreporting is enabled
     *  - the whole list otherwise
     */
    if (problems)
        notify_problem_list(problems);
}

static void show_problem_notification(problem_info_t *pi)
{
    GList *problems = g_list_append(NULL, pi);
    show_problem_list_notification(problems);
}

static void Crash(GVariant *parameters)
{
    const char *package_name, *dir, *uid_str, *uuid, *duphash;

    log_debug("Crash recorded");

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
        errno = 0;
        unsigned long uid_num = strtoul(uid_str, &end, 10);
        if (errno || *end != '\0' || uid_num != getuid())
        {
            foreign_problem = true;
            log_notice("foreign problem %i", foreign_problem);
        }
    }

    /* Non-admins shouldn't see other people's crashes */
    if (foreign_problem && !g_user_is_admin)
        return;

    static const char *elements[] = {
        FILENAME_CMDLINE,
        FILENAME_COUNT,
        FILENAME_UUID,
        FILENAME_DUPHASH,
        FILENAME_COMPONENT,
        FILENAME_ENVIRON,
        FILENAME_PID,
        NULL,
    };

    problem_info_t *pi = problem_info_new(dir);
    fill_problem_data_over_dbus(dir, elements, pi->problem_data);

    pi->foreign = foreign_problem;
    pi->is_packaged = (package_name != NULL);

    /*
     * Can't append dir to the seen list because of directory stealing
     *
     * append_dirlist(dir);
     *
     */
    show_problem_notification(pi);
}

static void handle_message(GDBusConnection *connection,
                           const gchar     *sender_name,
                           const gchar     *object_path,
                           const gchar     *interface_name,
                           const gchar     *signal_name,
                           GVariant        *parameters,
                           gpointer         user_data)
{
    g_debug ("Received signal: sender_name: %s, object_path: %s, "
             "interface_name: %s, signal_name: %s",
             sender_name, object_path, interface_name, signal_name);

    Crash(parameters);
}

static void
name_acquired_handler (GDBusConnection *connection,
                       const gchar *name,
                       gpointer user_data)
{
    static const char *elements[] = {
        FILENAME_CMDLINE,
        FILENAME_COUNT,
        FILENAME_UUID,
        FILENAME_DUPHASH,
        FILENAME_COMPONENT,
        FILENAME_UID,
        FILENAME_TIME,
        FILENAME_REPORTED_TO,
        FILENAME_NOT_REPORTABLE,
        NULL
    };

    /* If some new dirs appeared since our last run, let user know it */
    GList *new_dirs = NULL;
    GList *notify_list = NULL;
    new_dir_exists(&new_dirs);

#define time_before_ndays(n) (time(NULL) - (n)*24*60*60)

    /* Age limit = now - 3 days */
    const unsigned long min_born_time = (unsigned long)(time_before_ndays(3));

    for ( ; new_dirs != NULL; new_dirs = g_list_next(new_dirs))
    {
        const char *problem_id = (const char *)new_dirs->data;
        problem_info_t *pi = problem_info_new(problem_id);

        if (fill_problem_data_over_dbus(problem_id, elements, pi->problem_data) != 0)
        {
            log_notice("'%s' is not a dump dir - ignoring\n", problem_id);
            problem_info_unref(pi);
            continue;
        }

        /* TODO: add a filter for only complete problems to GetProblems D-Bus method */
        if (!dbus_problem_is_complete(problem_id))
        {
            log_notice("Ignoring incomplete problem '%s'", problem_id);
            problem_info_unref(pi);
            continue;
        }

        /* TODO: add a filter for max-old reported problems to GetProblems D-Bus method */
        if (problem_info_get_time(pi) < min_born_time)
        {
            log_notice("Ignoring outdated problem '%s'", problem_id);
            problem_info_unref(pi);
            continue;
        }

        /* TODO: add a filter for not-yet reported problems to GetProblems D-Bus method */
        if (problem_info_is_reported(pi))
        {
            log_notice("Ignoring already reported problem '%s'", problem_id);
            problem_info_unref(pi);
            continue;
        }

        /* Can't be foreig because new_dir_exists() returns only own problems */
        pi->foreign = false;
        notify_list = g_list_prepend(notify_list, pi);
    }

    if (notify_list)
        show_problem_list_notification(notify_list);

    list_free_with_free(new_dirs);

    /*
     * We want to update "seen directories" list on SIGTERM.
     * Updating it after each notification doesn't account for stealing directories:
     * if directory is stolen after seen list is updated,
     * on next startup applet will notify user about stolen directory. WRONG.
     *
     * SIGTERM handler simply stops GTK main loop and the applet saves user
     * settings, releases notify resources, releases dbus resources and updates
     * the seen list.
     */

}

static void
name_lost_handler (GDBusConnection *connection,
                   const gchar *name,
                   gpointer user_data)
{
  if (connection == NULL)
    error_msg_and_die("Problem connecting to dbus");

  gtk_main_quit ();
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
    notify_init("Problem detected");

    /* Monitor NetworkManager state */
    netmon = g_network_monitor_get_default ();
    g_signal_connect (G_OBJECT (netmon), "notify::connectivity",
                      G_CALLBACK (connectivity_changed_cb), NULL);
    g_signal_connect (G_OBJECT (netmon), "notify::network-available",
                      G_CALLBACK (connectivity_changed_cb), NULL);

    g_set_prgname("abrt");
    gtk_init(&argc, &argv);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] [DIR]...\n"
        "\n"
        "Applet which notifies user when new problems are detected by ABRT\n"
    );
    enum {
        OPT_v = 1 << 0,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    migrate_to_xdg_dirs();
    migrate_auto_reporting_to_gsettings();

    export_abrt_envvars(0);
    msg_prefix = g_progname;

    load_abrt_conf();
    load_event_config_data();
    load_user_settings(APP_NAME);

    /* Initialize our (dbus_abrt) machinery by filtering
     * for signals:
     *     signal sender=:1.73 -> path=/org/freedesktop/problems; interface=org.freedesktop.problems; member=Crash
     *       string "coreutils-7.2-3.fc11"
     *       string "0"
     */
    GError *error = NULL;
    GDBusConnection *system_conn = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                                                   NULL, &error);
    if (system_conn == NULL)
        perror_msg_and_die("Can't connect to system dbus: %s", error->message);
    guint filter_id = g_dbus_connection_signal_subscribe(system_conn,
                                                         NULL,
                                                         ABRT_DBUS_NAME,
                                                         "Crash",
                                                         ABRT_DBUS_OBJECT,
                                                         NULL,
                                                         G_DBUS_SIGNAL_FLAGS_NONE,
                                                         handle_message,
                                                         NULL, NULL);

    guint name_own_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                        ABRT_DBUS_NAME".applet",
                                        G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT | G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                        NULL,
                                        name_acquired_handler,
                                        name_lost_handler,
                                        NULL, NULL);

    g_user_is_admin = is_user_admin();

    g_gnome_abrt_available = is_gnome_abrt_available();

    /* Enter main loop
     */
    gtk_main();

    g_bus_unown_name (name_own_id);

    g_dbus_connection_signal_unsubscribe(system_conn, filter_id);

    /* new_dir_exists() is called for each notification and if user clicks on
     * the abrt icon. Those calls cover 99.97% of detected crashes
     *
     * The rest of detected crashes:
     *
     * 0.01%
     * applet doesn't append a repeated crash to the seen list if the crash was
     * the last caught crash before exit (notification is not shown in case of
     * repeated crash)
     *
     * 0.01%
     * applet doesn't append a stolen directory to the seen list if
     * notification was closed before the notified directory had been stolen
     *
     * 0.1%
     * crashes of abrt-applet
     */
    new_dir_exists(/* new dirs list */ NULL);

    if (notify_is_initted())
        notify_uninit();

    /* It does not make much sense to save settings at exit and after
     * introduction of system-config-abrt it is wrong to do that. abrt-applet
     * is long-running application and user can modify the configuration files
     * while abrt-applet run. Thus, saving configuration at desktop session
     * exit would make someone's life really hard.
     *
     * abrt-applet saves configuration immediately after user input.
     *
     * save_user_settings();
     */

    return 0;
}
