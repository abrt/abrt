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

#define GDK_DISABLE_DEPRECATION_WARNINGS
/* https://bugzilla.gnome.org/show_bug.cgi?id=734826 */
#include <gtk/gtk.h>

#include <libnotify/notify.h>
#include <glib.h>

#include <libreport/internal_abrt_dbus.h>
#include <libreport/event_config.h>
#include <libreport/internal_libreport_gtk.h>
#include "libabrt.h"
#include "problem_api.h"

/* libnotify action keys */
#define A_KNOWN_OPEN_GUI "OPEN"
#define A_REPORT_REPORT "REPORT"
#define A_REPORT_AUTOREPORT "AUTOREPORT"

#define GUI_EXECUTABLE "gnome-abrt"

enum
{
    /*
     * show autoreport button on notification
     */
    JUST_DETECTED_PROBLEM = 1 << 1,
};


static GNetworkMonitor *netmon;
static char **s_dirs;
static GList *g_deferred_crash_queue;
static guint g_deferred_timeout;
static ignored_problems_t *g_ignore_set;
/* Used only for selection of the last notified problem if a user clicks on the systray icon */
static char *g_last_notified_problem_id;

static bool get_configured_bool_or_default(const char *opt_name, bool def)
{
    /* User config always takes precedence */
    load_user_settings("abrt-applet");
    const char *configured = get_user_setting(opt_name);
    if (configured)
        return string_to_bool(configured);

    return def;
}

static bool is_autoreporting_enabled(void)
{
    GSettings *settings;
    gboolean ret;

    settings = g_settings_new ("org.gnome.desktop.privacy");
    ret = g_settings_get_boolean (settings, "report-technical-problems");
    g_object_unref (settings);
    return ret;
}

static const char *get_autoreport_event_name(void)
{
    load_user_settings("abrt-applet");
    const char *configured = get_user_setting("AutoreportingEvent");
    return configured ? configured : g_settings_autoreporting_event;
}

static bool is_shortened_reporting_enabled()
{
    return get_configured_bool_or_default("ShortenedReporting", g_settings_shortenedreporting);
}

static bool is_silent_shortened_reporting_enabled(void)
{
    return get_configured_bool_or_default("SilentShortenedReporting", 0);
}

static bool is_notification_of_incomplete_problems_enabled(void)
{
    return get_configured_bool_or_default("NotifyIncompleteProblems", 0);
}

static bool is_networking_enabled(void)
{
    if (!g_network_monitor_get_network_available(netmon))
        return FALSE;
    return g_network_monitor_get_connectivity(netmon) == G_NETWORK_CONNECTIVITY_FULL;
}

static void show_problem_list_notification(GList *problems, int flags);

static gboolean process_deferred_queue_timeout_fn(GList *queue)
{
    g_deferred_timeout = 0;
    show_problem_list_notification(queue, /* process these crashes as new crashes */ 0);

    /* Remove this timeout fn from the main loop*/
    return FALSE;
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

        g_deferred_timeout = g_timeout_add(30 * 1000 /* give NM 30s to configure network */,
                                           (GSourceFunc)process_deferred_queue_timeout_fn,
                                           g_deferred_crash_queue);
    }
}

typedef struct problem_info {
    problem_data_t *problem_data;
    bool foreign;
    bool known;
    bool incomplete;
    bool reported;
    bool was_announced;
    bool is_writable;
} problem_info_t;

static void push_to_deferred_queue(problem_info_t *pi)
{
    g_deferred_crash_queue = g_list_append(g_deferred_crash_queue, pi);
}

static const char *problem_info_get_dir(problem_info_t *pi)
{
    return problem_data_get_content_or_NULL(pi->problem_data, CD_DUMPDIR);
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
    problem_info_t *pi = xzalloc(sizeof(*pi));
    pi->problem_data = problem_data_new();
    problem_info_set_dir(pi, dir);
    return pi;
}

static void problem_info_free(problem_info_t *pi)
{
    if (pi == NULL)
        return;

    problem_data_free(pi->problem_data);
    free(pi);
}

static void run_event_async(problem_info_t *pi, const char *event_name, int flags);

enum {
    REPORT_UNKNOWN_PROBLEM_IMMEDIATELY = 1 << 0,
};

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
    struct event_processing_state *p = xzalloc(sizeof(*p));
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
    free(p);
}

static char *build_message(problem_info_t *pi)
{
    char *package_name = problem_data_get_content_or_NULL(pi->problem_data, FILENAME_COMPONENT);

    char *msg = NULL;
    if (package_name == NULL || package_name[0] == '\0')
        msg = xasprintf(_("A problem has been detected"));
    else
        msg = xasprintf(_("A problem in the %s package has been detected"), package_name);

    if (pi->incomplete)
    {
        const char *not_reportable = problem_data_get_content_or_NULL(pi->problem_data, FILENAME_NOT_REPORTABLE);
        log_notice("%s", not_reportable);
        if (not_reportable)
            msg = xasprintf("%s\n\n%s", msg, not_reportable);
    }
    else if (pi->reported)
        msg = xasprintf(_("%s and the diagnostic data has been submitted"), msg);

    return msg;
}

static GList *add_dirs_to_dirlist(GList *dirlist, const char *dirname)
{
    DIR *dir = opendir(dirname);
    if (!dir)
        return dirlist;

    struct dirent *dent;
    while ((dent = readdir(dir)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue;
        char *full_name = concat_path_file(dirname, dent->d_name);
        struct stat statbuf;
        if (lstat(full_name, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
            dirlist = g_list_prepend(dirlist, full_name);
        else
            free(full_name);
    }
    closedir(dir);

    return g_list_reverse(dirlist);
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
    GList *dirlist = NULL;
    char **pp = s_dirs;
    while (*pp)
    {
        dirlist = add_dirs_to_dirlist(dirlist, *pp);
        pp++;
    }

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
         * DIR4 ==== - Old list ended, cuurent one didn't. New dir exists!
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

static void fork_exec_gui(const char *problem_id)
{
    fflush(NULL); /* paranoia */
    pid_t pid = fork();
    if (pid < 0)
    {
        perror_msg("fork");
        goto record_dirs;
    }

    if (pid == 0)
    {
        /* child */
        /* double fork to avoid GUI zombies */
        pid_t grand_child = fork();
        if (grand_child != 0)
        {
            /* child */
            if (grand_child < 0)
                perror_msg("fork");
            _exit(0);
        }

        // pass s_[] as DIR param(s) to 'gui executable'
        char *gui_args[4];
        char **pp = gui_args;
        *pp++ = (char *)GUI_EXECUTABLE;
        if (problem_id != NULL)
        {
            *pp++ = (char *)"-p";
            *pp++ = (char *)problem_id;
        }
        *pp = NULL;

        /* grandchild */
        execv(BIN_DIR"/"GUI_EXECUTABLE, gui_args);
        /* Did not find 'gui executable' in installation directory. Oh well */
        /* Trying to find it in PATH */
        execvp(GUI_EXECUTABLE, gui_args);
        perror_msg_and_die(_("Can't execute '%s'"), GUI_EXECUTABLE);
    }

    /* parent */
    safe_waitpid(pid, /* status */ NULL, /* options */ 0);

 record_dirs:
    /* Scan dirs and save new $XDG_CACHE_HOME/abrt/applet_dirlist.
     * (Oterwise, after a crash, next time applet is started,
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
    env_vec[0] = xstrdup("REPORT_CLIENT_NONINTERACTIVE=1");
    env_vec[1] = NULL;

    pid_t child = fork_execv_on_steroids(flags, args, fdp ? pipeout : NULL,
            env_vec, /*dir:*/ NULL, /*uid(unused):*/ 0);

    if (fdp)
        *fdp = pipeout[0];

    free(env_vec[0]);

    return child;
}

static void run_report_from_applet(problem_info_t *pi)
{
    if (!problem_info_ensure_writable(pi))
        return;

    const char *dirname = problem_info_get_dir(pi);

    fflush(NULL); /* paranoia */
    pid_t pid = fork();
    if (pid < 0)
    {
        perror_msg("fork");
        return;
    }
    if (pid == 0)
    {
        /* child */
        /* prevent zombies - another fork inside: */
        spawn_event_handler_child(dirname, "report-gui", NULL);
        _exit(0);
    }
    safe_waitpid(pid, /* status */ NULL, /* options */ 0);
}

//this action should open the reporter dialog directly, without showing the main window
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
    {
        if (strcmp(A_REPORT_REPORT, action) == 0)
        {
            run_report_from_applet(pi);
            problem_info_free(pi);
        }
        else
        {
            /* if shortened reporting is configured don't start reporting process
             * when problem is unknown (just show notification) */
            run_event_async(pi, get_autoreport_event_name(),
                is_shortened_reporting_enabled() ? 0 : REPORT_UNKNOWN_PROBLEM_IMMEDIATELY);
        }
    }
    else
        problem_info_free(pi);
}

static void action_ignore(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    problem_info_t *pi = (problem_info_t *)user_data;

    const char *const message = _(
            "You are going to mute notifications of a particular problem. " \
            "You will never see a notification bubble for this problem again, " \
            "however, ABRT will be detecting it and you will be able " \
            "to report it from ABRT GUI." \
            "\n\n" \
            "Do you want to continue?");

    if (run_ask_yes_no_yesforever_dialog("AskIgnoreForever", message, NULL))
    {
        log_debug("Ignoring problem '%s'", problem_info_get_dir(pi));
        ignored_problems_add_problem_data(g_ignore_set, pi->problem_data);
    }

    GError *err = NULL;
    notify_notification_close(notification, &err);
    if (err != NULL)
    {
        error_msg("%s", err->message);
        g_error_free(err);
    }
}

static void action_known(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    log_debug("Handle known action '%s'!", action);
    problem_info_t *pi = (problem_info_t *)user_data;

    if (strcmp(A_KNOWN_OPEN_GUI, action) == 0)
        fork_exec_gui(problem_info_get_dir(pi));
    else
        /* This should not happen; otherwise it's a bug */
        error_msg("%s:%d %s(): BUG Unknown action '%s'", __FILE__, __LINE__, __func__, action);

    GError *err = NULL;
    notify_notification_close(notification, &err);
    if (err != NULL)
    {
        error_msg(_("Can't close notification: %s"), err->message);
        g_error_free(err);
    }
}

static void on_notify_close(NotifyNotification *notification, gpointer user_data)
{
    log_debug("Notify closed!");
    g_object_unref(notification);

    /* Scan dirs and save new $XDG_CACHE_HOME/abrt/applet_dirlist.
     * (Oterwise, after a crash, next time applet is started,
     * it will show alert icon even if we did click on it
     * "in previous life"). We ignore finction return value.
     */
    new_dir_exists(/* new dirs list */ NULL);
}

static NotifyNotification *new_warn_notification(bool persistence)
{
    NotifyNotification *notification;

/* the fourth argument was removed in libnotify 0.7.0 */
#if !defined(NOTIFY_VERSION_MINOR) || (NOTIFY_VERSION_MAJOR == 0 && NOTIFY_VERSION_MINOR < 7)
    notification = notify_notification_new(_("Warning"), NULL, NULL, NULL);
#else
    notification = notify_notification_new(_("Warning"), NULL, NULL);
#endif

    g_signal_connect(notification, "closed", G_CALLBACK(on_notify_close), NULL);

    GdkPixbuf *pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                "abrt", 48, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);

    if (pixbuf)
        notify_notification_set_icon_from_pixbuf(notification, pixbuf);
    notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(notification, persistence ? NOTIFY_EXPIRES_NEVER
                                                              : NOTIFY_EXPIRES_DEFAULT);
    notify_notification_set_hint(notification, "desktop-entry", g_variant_new_string("abrt-applet"));

    return notification;
}

static void notify_problem_list(GList *problems, int flags)
{
    if (!notify_is_initted())
        notify_init(_("Problem detected"));

    GList *last_item = g_list_last(problems);
    if (last_item == NULL)
    {
        log_debug("Not showing any notification bubble because the list of problems is empty.");
        return;
    }

    problem_info_t *last_problem = (problem_info_t *)last_item->data;
    free(g_last_notified_problem_id);
    g_last_notified_problem_id = xstrdup(problem_info_get_dir(last_problem));

    char *notify_body = NULL;
    for (GList *iter = problems; iter; iter = g_list_next(iter))
    {
        problem_info_t *pi = iter->data;
        if (ignored_problems_contains_problem_data(g_ignore_set, pi->problem_data))
        {   /* In case of shortened reporting, show the problem notification only once. */
            problem_info_free(pi);
            continue;
        }

        /* Don't show persistent notification (let notification bubble expire
         * and disappear in few seconds) with ShortenedReporting mode enabled
         * and already announced problem.
         */
        const bool persistent_notification = (!(is_shortened_reporting_enabled() && pi->was_announced));

        NotifyNotification *notification = new_warn_notification(persistent_notification);
        notify_notification_add_action(notification, "IGNORE", _("Ignore forever"),
                NOTIFY_ACTION_CALLBACK(action_ignore),
                pi, NULL);

        notify_body = build_message(pi);

        pi->was_announced = true;

        if (pi->known)
        {   /* Problem has been 'autoreported' and is considered as KNOWN
             */
            notify_notification_add_action(notification, A_KNOWN_OPEN_GUI, _("Open"),
                    NOTIFY_ACTION_CALLBACK(action_known),
                    pi, NULL);

            notify_notification_update(notification, pi->was_announced ?
                    _("The Problem has already been Reported") : _("A Known Problem has Occurred"),
                    notify_body, NULL);
        }
        else
        {
            if (flags & JUST_DETECTED_PROBLEM)
            {   /* Problem cannot be 'autoreported' because its data are incomplete
                 */
                if (pi->incomplete)
                {
                    notify_notification_add_action(notification, A_KNOWN_OPEN_GUI, _("Open"),
                            NOTIFY_ACTION_CALLBACK(action_known),
                            pi, NULL);
                }
                else
                {
                   /* Problem has not yet been 'autoreported' and can be
                    * 'autoreported' on user request.
                    */
                    notify_notification_add_action(notification, A_REPORT_AUTOREPORT, _("Report"),
                            NOTIFY_ACTION_CALLBACK(action_report),
                            pi, NULL);
                }

                notify_notification_update(notification, _("A Problem has Occurred"), notify_body, NULL);
            }
            else
            {   /* Problem has been 'autoreported' and is considered as UNKNOWN
                 *
                 * In case of shortened reporting don't scare (confuse, bother)
                 * user with the 'Report' button and simply announce that some
                 * problem has been reported.
                 *
                 * Otherwise let user decide if he wants to start the standard
                 * reporting process of a new problem by clicking on the
                 * 'Report' button.
                 */
                if (is_shortened_reporting_enabled())
                {
                    /* Users dislike "useless" notification of reported
                     * problems allowing only to disable future notifications
                     * of the same problem.
                     */
                    if (is_silent_shortened_reporting_enabled())
                    {
                        problem_info_free(pi);
                        g_object_unref(notification);
                        goto next_problem_to_notify;
                    }

                    notify_notification_update(notification, _("A Problem has been Reported"), notify_body, NULL);
                }
                else
                {
                    notify_notification_add_action(notification, A_REPORT_REPORT, _("Report"),
                            NOTIFY_ACTION_CALLBACK(action_report),
                            pi, NULL);

                    notify_notification_update(notification, _("A New Problem has Occurred"), notify_body, NULL);
                }
            }
        }

        GError *err = NULL;
        log_debug("Showing a notification");
        notify_notification_show(notification, &err);
        if (err != NULL)
        {
            error_msg(_("Can't show notification: %s"), err->message);
            g_error_free(err);
        }

next_problem_to_notify:
        free(notify_body);
        notify_body = NULL;
    }

    g_list_free(problems);
}

static void notify_problem(problem_info_t *pi)
{
    GList *problems = g_list_append(NULL, pi);
    notify_problem_list(problems, /* show icon and notify, don't show autoreport and don't use anon report*/ 0);
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
        if (pi->known || !(state->flags & REPORT_UNKNOWN_PROBLEM_IMMEDIATELY))
            notify_problem(pi);
        else
            run_report_from_applet(pi);
    }
    else
    {
        log_debug("fast report failed");
        if (is_networking_enabled())
            notify_problem(pi);
        else
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

static void run_event_async(problem_info_t *pi, const char *event_name, int flags)
{
    if (!problem_info_ensure_writable(pi))
    {
        problem_info_free(pi);
        return;
    }

    export_event_configuration(event_name);

    struct event_processing_state *state = new_event_processing_state();
    state->pi = pi;
    state->flags = flags;

    state->child_pid = spawn_event_handler_child(problem_info_get_dir(state->pi), event_name, &state->child_stdout_fd);

    GIOChannel *channel_event_output = my_io_channel_unix_new(state->child_stdout_fd);
    g_io_add_watch(channel_event_output, G_IO_IN | G_IO_PRI | G_IO_HUP,
                   handle_event_output_cb, state);
}

static void show_problem_list_notification(GList *problems, int flags)
{
    if (is_autoreporting_enabled())
    {
        /* Automatically report only own problems */
        /* and skip foreign problems */
        for (GList *iter = problems; iter;)
        {
            problem_info_t *data = (problem_info_t *)iter->data;
            GList *next = g_list_next(iter);

            if (!data->foreign && !data->incomplete)
            {
                run_event_async((problem_info_t *)iter->data, get_autoreport_event_name(), /* don't automatically report */ 0);
                problems = g_list_delete_link(problems, iter);
            }

            iter = next;
        }

    }

    /* report the rest:
     *  - only foreign if autoreporting is enabled
     *  - the whole list otherwise
     */
    if (problems)
        notify_problem_list(problems, flags | JUST_DETECTED_PROBLEM);
}

static void show_problem_notification(problem_info_t *pi, int flags)
{
    GList *problems = g_list_append(NULL, pi);
    show_problem_list_notification(problems, flags);
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

    /*
     * Can't append dir to the seen list because of directory stealing
     *
     * append_dirlist(dir);
     *
     */

    /* If this problem seems to be repeating, do not annoy user with popup dialog.
     * (The icon in the tray is not suppressed)
     */
    static time_t last_time = 0;
    static char* last_package_name = NULL;
    static char *last_problem_dir = NULL;
    time_t cur_time = time(NULL);
    if (last_package_name && strcmp(last_package_name, package_name) == 0
     && last_problem_dir && strcmp(last_problem_dir, dir) == 0
     && (unsigned)(cur_time - last_time) < 2 * 60 * 60
    ) {
        /* log_msg doesn't show in .xsession_errors */
        error_msg("repeated problem in %s, not showing the notification", package_name);
        return;
    }
    else
    {
        last_time = cur_time;
        free(last_package_name);
        last_package_name = xstrdup(package_name);
        free(last_problem_dir);
        last_problem_dir = xstrdup(dir);
    }


    problem_info_t *pi = problem_info_new(dir);
    if (uuid != NULL && uuid[0] != '\0')
        problem_data_add_text_noteditable(pi->problem_data, FILENAME_UUID, uuid);
    if (duphash != NULL && duphash[0] != '\0')
        problem_data_add_text_noteditable(pi->problem_data, FILENAME_DUPHASH, duphash);
    if (package_name != NULL && package_name[0] != '\0')
        problem_data_add_text_noteditable(pi->problem_data, FILENAME_COMPONENT, package_name);
    pi->foreign = foreign_problem;
    show_problem_notification(pi, 0);
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
    /* If some new dirs appeared since our last run, let user know it */
    GList *new_dirs = NULL;
    GList *notify_list = NULL;
    new_dir_exists(&new_dirs);

#define time_before_ndays(n) (time(NULL) - (n)*24*60*60)

    /* Age limit = now - 3 days */
    const unsigned long min_born_time = (unsigned long)(time_before_ndays(3));

    const bool notify_incomplete = is_notification_of_incomplete_problems_enabled();

    while (new_dirs)
    {
        struct dump_dir *dd = dd_opendir((char *)new_dirs->data, DD_OPEN_READONLY);
        if (dd == NULL)
        {
            log_notice("'%s' is not a dump dir - ignoring\n", (char *)new_dirs->data);
            new_dirs = g_list_next(new_dirs);
            continue;
        }

        if (dd->dd_time < min_born_time)
        {
            log_notice("Ignoring outdated problem '%s'", (char *)new_dirs->data);
            goto next;
        }

        const bool incomplete = !problem_dump_dir_is_complete(dd);
        if (!notify_incomplete && incomplete)
        {
            log_notice("Ignoring incomplete problem '%s'", (char *)new_dirs->data);
            goto next;
        }

        if (!dd_exist(dd, FILENAME_REPORTED_TO))
        {
            problem_info_t *pi = problem_info_new(new_dirs->data);
            const char *elements[] = {FILENAME_UUID, FILENAME_DUPHASH, FILENAME_COMPONENT, FILENAME_NOT_REPORTABLE};

            for (size_t i = 0; i < sizeof(elements)/sizeof(*elements); ++i)
            {
                char * const value = dd_load_text_ext(dd, elements[i],
                        DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
                if (value)
                    problem_data_add_text_noteditable(pi->problem_data, elements[i], value);
                free(value);
            }

            pi->incomplete = incomplete;

            /* Can't be foreign because if the problem is foreign then the
             * dd_opendir() call failed few lines above and the problem is ignored.
             * */
            pi->foreign = false;

            notify_list = g_list_prepend(notify_list, pi);
        }
        else
        {
            log_notice("Ignoring already reported problem '%s'", (char *)new_dirs->data);
        }

next:
        dd_close(dd);

        new_dirs = g_list_next(new_dirs);
    }

    g_ignore_set = ignored_problems_new(concat_path_file(g_get_user_cache_dir(), "abrt/ignored_problems"));

    if (notify_list)
        show_problem_list_notification(notify_list, /* show icon and notify */ 0);

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

    export_abrt_envvars(0);
    msg_prefix = g_progname;

    load_abrt_conf();
    load_event_config_data();
    load_user_settings("abrt-applet");

    const char *default_dirs[] = {
        g_settings_dump_location,
        NULL,
        NULL,
    };
    argv += optind;
    if (!argv[0])
    {
        default_dirs[1] = concat_path_file(g_get_user_cache_dir(), "abrt/spool");
        argv = (char**)default_dirs;
    }
    s_dirs = argv;

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
                                                         "org.freedesktop.problems",
                                                         "Crash",
                                                         "/org/freedesktop/problems",
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

    /* Enter main loop
     */
    gtk_main();

    g_bus_unown_name (name_own_id);

    g_dbus_connection_signal_unsubscribe(system_conn, filter_id);

    ignored_problems_free(g_ignore_set);

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

    free(g_last_notified_problem_id);

    return 0;
}
