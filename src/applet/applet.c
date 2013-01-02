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
#include <gtk/gtk.h>
#include <libnotify/notify.h>
#include <dbus/dbus-shared.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#include <libreport/internal_abrt_dbus.h>
#include <libreport/event_config.h>
#include <libreport/internal_libreport_gtk.h>
#include "libabrt.h"

/* NetworkManager DBus configuration */
#define NM_DBUS_SERVICE "org.freedesktop.NetworkManager"
#define NM_DBUS_PATH  "/org/freedesktop/NetworkManager"
#define NM_DBUS_INTERFACE "org.freedesktop.NetworkManager"
#define NM_STATE_CONNECTED_LOCAL 50
#define NM_STATE_CONNECTED_SITE 60
#define NM_STATE_CONNECTED_GLOBAL 70

/* libnotify action keys */
#define A_KNOWN_OPEN_GUI "OPEN"
#define A_KNOWN_OPEN_BROWSER "SHOW"
#define A_REPORT_REPORT "REPORT"
#define A_REPORT_AUTOREPORT "AUTOREPORT"

enum
{
    /*
     * with this flag show_problem_notification() shows only the old ABRT icon
     */
    SHOW_ICON_ONLY = 1 << 0,

    /*
     * show autoreport button on notification
     */
    JUST_DETECTED_PROBLEM = 1 << 1,
};


static GDBusConnection *g_system_bus;
static GtkStatusIcon *ap_status_icon;
static GtkWidget *ap_menu;
static char *ap_last_problem_dir;
static char **s_dirs;
static GList *g_deferred_crash_queue;
static guint g_deferred_timeout;

static bool is_autoreporting_enabled(void)
{
    const char *option = get_user_setting("AutoreportingEnabled");

    /* If user configured autoreporting from his scope, don't look at system
     * configuration.
     */
    return    (!option && g_settings_autoreporting)
           || ( option && string_to_bool(option));
}

static const char *get_autoreport_event_name(void)
{
    const char *configured = get_user_setting("AutoreportingEvent");
    return configured ? configured : g_settings_autoreporting_event;
}

static void ask_start_autoreporting()
{
    /* The "Yes" response will be saved even if user don't check the
     * "Don't ask me again" box.
     */
    const int ret = run_ask_yes_no_save_result_dialog("AutoreportingEnabled",
     _("The report which will be sent does not contain any security sensitive data. "
       "Therefore it is not necessary to bother you next time and require any further action by you. "
       "\nDo you want to enable automatically submitted anonymous crash reports?"),
       /*parent wnd */ NULL);

    /* Don't forget:
     *
     * The "Yes" response will be saved even if user don't check the
     * "Don't ask me again" box.
     */
    if (ret != 0)
        set_user_setting("AutoreportingEnabled", "yes");

    /* must be called immediately, otherwise the data could be lost in case of crash */
    save_user_settings();
}

/*
 * Converts a NM state value stored in GVariant to boolean.
 *
 * Returns true if a state means connected.
 *
 * Sinks the args variant.
 */
static bool nm_state_is_connected(GVariant *args)
{
    GVariant *value = g_variant_get_child_value(args, 0);

    if (g_variant_is_of_type(value, G_VARIANT_TYPE_VARIANT))
    {
        GVariant *tmp = g_variant_get_child_value(value, 0);
        g_variant_unref(value);
        value = tmp;
    }

    int state = g_variant_get_uint32 (value);

    g_variant_unref(value);
    g_variant_unref(args);

    return state == NM_STATE_CONNECTED_GLOBAL
           || state == NM_STATE_CONNECTED_LOCAL
           || state == NM_STATE_CONNECTED_SITE;
}

/*
 * The function tries to get network state from NetworkManager over DBus
 * call. If NetworkManager DBus service is not available the function returns
 * true which means that network is enabled and up.
 *
 * Function must return true on any error, otherwise user won't be notified
 * about new problems. Because if network is not enabled, new problems are
 * pushed to the deferred queue. The deferred queue is processed immediately
 * after network becomes enabled. In case where NetworkManager is broken or not
 * available, notification about network state doesn't work thus the deferred
 * queue won't be ever processed.
 */
static bool is_networking_enabled(void)
{
    GError *error = NULL;

    /* Create a D-Bus proxy to get the object properties from the NM Manager
     * object.
     */
    const int flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                      | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS;

    GDBusProxy *props_proxy = g_dbus_proxy_new_sync(g_system_bus,
                                flags,
                                NULL /* GDBusInterfaceInfo */,
                                NM_DBUS_SERVICE,
                                NM_DBUS_PATH,
                                DBUS_INTERFACE_PROPERTIES,
                                NULL /* GCancellable */,
                                &error);

    if (!props_proxy)
    {
        /* The NetworkManager DBus service is not available. */
        error_msg (_("Can't connect to NetworkManager over DBus: %s"), error->message);
        g_error_free (error);

        /* Consider network state as connected. */
        return true;
    }

    /* Get the State property from the NM Manager object */
    GVariant *const value = g_dbus_proxy_call_sync (props_proxy,
                                "Get",
                                 g_variant_new("(ss)", NM_DBUS_INTERFACE, "State"),
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 -1   /* timeout: use proxy default */,
                                 NULL /* GCancellable */,
                                 &error);

    /* Consider network state as connected if any error occurs */
    bool ret = true;

    if (!error)
        /* Convert the state value and sink the variable */
        ret = nm_state_is_connected(value);
    else
    {
        error_msg (_("Can't determine network status via NetworkManager: %s"), error->message);
        g_error_free (error);
    }

    g_object_unref(props_proxy);

    return ret;
}

static void show_problem_list_notification(GList *problems, int flags);

static gboolean process_deferred_queue_timeout_fn(GList *queue)
{
    g_deferred_timeout = 0;
    show_problem_list_notification(queue, /* process these crashes as new crashes */ 0);

    /* Remove this timeout fn from the main loop*/
    return FALSE;
}

static void on_nm_state_changed(GDBusConnection *connection, const gchar *sender_name,
                                const gchar *object_path, const gchar *interface_name,
                                const gchar *signal_name, GVariant *parameters,
                                gpointer user_data)
{
    g_variant_ref(parameters);

    if (nm_state_is_connected(parameters))
    {
        if (g_deferred_timeout)
            g_source_remove(g_deferred_timeout);

        g_deferred_timeout = g_timeout_add(30 * 1000 /* give NM 30s to configure network */,
                                           (GSourceFunc)process_deferred_queue_timeout_fn,
                                           g_deferred_crash_queue);
    }
}

typedef struct problem_info {
    char *problem_dir;
    bool foreign;
    char *message;
    bool known;
} problem_info_t;

static void push_to_deferred_queue(problem_info_t *pi)
{
    g_deferred_crash_queue = g_list_append(g_deferred_crash_queue, pi);
}

static problem_info_t *problem_info_new(void)
{
    problem_info_t *pi = xzalloc(sizeof(*pi));
    return pi;
}

static void problem_info_free(problem_info_t *pi)
{
    if (pi == NULL)
        return;

    free(pi->message);
    free(pi->problem_dir);
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

static char *build_message(const char *package_name)
{
    if (package_name == NULL || package_name[0] == '\0')
        return xasprintf(_("A problem has been detected"));

    return xasprintf(_("A problem in the %s package has been detected"), package_name);
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
                    VERB1 log("New dir detected: %s", (char *)l1->data);
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
                VERB1 log("New dir detected: %s", (char *)l1->data);
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

static void fork_exec_gui(void)
{
    pid_t pid = fork();
    if (pid < 0)
        perror_msg("fork");
    else if (pid == 0)
    {
        /* child */
        /* double fork to avoid GUI zombies */
        pid_t grand_child = fork();
        if (grand_child != 0)
        {
            if (grand_child < 0) perror_msg("fork");
            exit(0);
        }

        /* grand child */
        //TODO: pass s_dirs[] as DIR param(s) to abrt-gui
        execl(BIN_DIR"/abrt-gui", "abrt-gui", (char*) NULL);
        /* Did not find abrt-gui in installation directory. Oh well */
        /* Trying to find it in PATH */
        execlp("abrt-gui", "abrt-gui", (char*) NULL);
        perror_msg_and_die(_("Can't execute '%s'"), "abrt-gui");
    }
    else
        safe_waitpid(pid, /* status */ NULL, /* options */ 0);

    /* Scan dirs and save new $XDG_CACHE_HOME/abrt/applet_dirlist.
     * (Oterwise, after a crash, next time applet is started,
     * it will show alert icon even if we did click on it
     * "in previous life"). We ignore function return value.
     */
    new_dir_exists(/* new dirs list */ NULL);
}

static void hide_icon(void)
{
    if (ap_status_icon == NULL)
        return;

    gtk_status_icon_set_visible(ap_status_icon, false);
}

static pid_t spawn_event_handler_child(const char *dump_dir_name, const char *event_name, int *fdp)
{
    char *args[6];
    args[0] = (char *) LIBEXEC_DIR"/abrt-handle-event";
    args[1] = (char *) "-e";
    args[2] = (char *) event_name;
    args[3] = (char *) "--";
    args[4] = (char *) dump_dir_name;
    args[5] = NULL;

    int pipeout[2];
    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_QUIET | EXECFLG_ERR2OUT;
    VERB1 flags &= ~EXECFLG_QUIET;

    pid_t child = fork_execv_on_steroids(flags, args, fdp ? pipeout : NULL,
            /*env_vec:*/ NULL, /*dir:*/ NULL,
            /*uid(unused):*/ 0);
    if (fdp)
        *fdp = pipeout[0];

    return child;
}

static void run_report_from_applet(const char *dirname)
{
    /* prevent zombies; double fork() */
    pid_t pid = fork();
    if (pid < 0)
        perror_msg("fork");
    else if (pid == 0)
    {
        spawn_event_handler_child(dirname, "report-gui", NULL);
        exit(0);
    }
    else
        safe_waitpid(pid, /* status */ NULL, /* options */ 0);
}

//this action should open the reporter dialog directly, without showing the main window
static void action_report(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    VERB3 log("Reporting a problem!");
    /* must be closed before ask_yes_no dialog run */
    GError *err = NULL;
    notify_notification_close(notification, &err);
    if (err != NULL)
    {
        error_msg(_("Can't close notification: %s"), err->message);
        g_error_free(err);
    }

    hide_icon();

    problem_info_t *pi = (problem_info_t *)user_data;
    if (pi->problem_dir)
    {
        if (strcmp(A_REPORT_REPORT, action) == 0)
        {
            run_report_from_applet(pi->problem_dir);
            problem_info_free(pi);
        }
        else
        {
            if (pi->foreign == false && strcmp(A_REPORT_AUTOREPORT, action) == 0)
                ask_start_autoreporting();

            run_event_async(pi, get_autoreport_event_name(), REPORT_UNKNOWN_PROBLEM_IMMEDIATELY);
        }

        /* Scan dirs and save new $XDG_CACHE_HOME/abrt/applet_dirlist.
         * (Oterwise, after a crash, next time applet is started,
         * it will show alert icon even if we did click on it
         * "in previous life"). We ignore finction return value.
         */
        new_dir_exists(/* new dirs list */ NULL);
    }
    else
        problem_info_free(pi);
}

static void action_ignore(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    VERB3 log("Ignore a problem!");
    GError *err = NULL;
    notify_notification_close(notification, &err);
    if (err != NULL)
    {
        error_msg("%s", err->message);
        g_error_free(err);
    }
    hide_icon();
}

static void action_known(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    VERB3 log("Handle known action '%s'!", action);
    problem_info_t *pi = (problem_info_t *)user_data;

    if (strcmp(A_KNOWN_OPEN_BROWSER, action) == 0)
    {
        struct dump_dir *dd = dd_opendir(pi->problem_dir, DD_OPEN_READONLY);
        if (dd)
        {
            report_result_t *res = find_in_reported_to(dd, "ABRT Server");
            if (res && res->url)
            {
                GError *error = NULL;
                if (!gtk_show_uri(/* use default screen */ NULL, res->url, GDK_CURRENT_TIME, &error))
                {
                    error_msg(_("Can't open url '%s': %s"), res->url, error->message);
                    g_error_free(error);
                }
            }
            free_report_result(res);
            dd_close(dd);
        }
    }
    else if (strcmp(A_KNOWN_OPEN_GUI, action) == 0)
        /* TODO teach gui to select a problem passed on cmd line */
        fork_exec_gui();
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

    hide_icon();
}

static void on_menu_popup_cb(GtkStatusIcon *status_icon,
                        guint          button,
                        guint          activate_time,
                        gpointer       user_data)
{
    if (ap_menu != NULL)
    {
        gtk_menu_popup(GTK_MENU(ap_menu),
                NULL, NULL,
                gtk_status_icon_position_menu,
                status_icon, button, activate_time);
    }
}

static void on_notify_close(NotifyNotification *notification, gpointer user_data)
{
    VERB3 log("Notify closed!");
    g_object_unref(notification);
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

    return notification;
}

static void on_about_cb(GtkMenuItem *menuitem, gpointer dialog)
{
    if (dialog)
    {
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_hide(GTK_WIDGET(dialog));
    }
}

static GtkWidget *create_about_dialog(void)
{
    const char *copyright_str = "Copyright © 2009 Red Hat, Inc\nCopyright © 2010 Red Hat, Inc";
    const char *license_str = "This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version."
                         "\n\nThis program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details."
                         "\n\nYou should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.";

    const char *website_url = "https://fedorahosted.org/abrt/";
    const char *authors[] = {"Anton Arapov <aarapov@redhat.com>",
                     "Karel Klic <kklic@redhat.com>",
                     "Jiri Moskovcak <jmoskovc@redhat.com>",
                     "Nikola Pajkovsky <npajkovs@redhat.com>",
                     "Zdenek Prikryl <zprikryl@redhat.com>",
                     "Denys Vlasenko <dvlasenk@redhat.com>",
                     NULL};

    const char *artists[] = {"Patrick Connelly <pcon@fedoraproject.org>",
                             "Lapo Calamandrei",
                             "Jakub Steinar <jsteiner@redhat.com>",
                                NULL};

    const char *comments = _("Notification area applet that notifies users about "
                               "issues detected by ABRT");
    GtkWidget *about_d = gtk_about_dialog_new();
    if (about_d)
    {
        gtk_window_set_default_icon_name("abrt");
        gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_d), VERSION);
        gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(about_d), "abrt");
        gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about_d), comments);
        gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about_d), "ABRT");
        gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about_d), copyright_str);
        gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about_d), license_str);
        gtk_about_dialog_set_wrap_license(GTK_ABOUT_DIALOG(about_d),true);
        gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about_d), website_url);
        gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about_d), authors);
        gtk_about_dialog_set_artists(GTK_ABOUT_DIALOG(about_d), artists);
        gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about_d), _("translator-credits"));
    }
    return about_d;
}

static GtkWidget *create_menu(void)
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *b_quit = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
    g_signal_connect(b_quit, "activate", gtk_main_quit, NULL);
    GtkWidget *b_hide = gtk_menu_item_new_with_label(_("Hide"));
    g_signal_connect(b_hide, "activate", G_CALLBACK(hide_icon), NULL);
    GtkWidget *b_about = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
    GtkWidget *about_dialog = create_about_dialog();
    g_signal_connect(b_about, "activate", G_CALLBACK(on_about_cb), about_dialog);
    GtkWidget *separator = gtk_separator_menu_item_new();

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), b_hide);
    gtk_widget_show(b_hide);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), b_about);
    gtk_widget_show(b_about);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    gtk_widget_show(separator);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), b_quit);
    gtk_widget_show(b_quit);

    return menu;
}

static void on_applet_activate_cb(GtkStatusIcon *status_icon, gpointer user_data)
{
    fork_exec_gui();
    hide_icon();
}

static GtkStatusIcon *create_status_icon(void)
{
     GtkStatusIcon *icn = gtk_status_icon_new_from_icon_name("abrt");

     g_signal_connect(G_OBJECT(icn), "activate", G_CALLBACK(on_applet_activate_cb), NULL);
     g_signal_connect(G_OBJECT(icn), "popup_menu", G_CALLBACK(on_menu_popup_cb), NULL);

     return icn;
}

static void show_icon(const char *tooltip)
{
    if (ap_status_icon == NULL)
    {
        ap_status_icon = create_status_icon();
        ap_menu = create_menu();
    }

    gtk_status_icon_set_tooltip_text(ap_status_icon, tooltip);
    gtk_status_icon_set_visible(ap_status_icon, true);
}

static gboolean server_has_persistence(void)
{
#if !defined(NOTIFY_VERSION_MINOR) || (NOTIFY_VERSION_MAJOR == 0 && NOTIFY_VERSION_MINOR >= 6)
    GList *caps = notify_get_server_caps();
    if (caps == NULL)
    {
        error_msg("Failed to receive server caps");
        return FALSE;
    }

    GList *l = g_list_find_custom(caps, "persistence", (GCompareFunc)strcmp);

    list_free_with_free(caps);
    VERB1 log("notify server %s support pesistence", l ? "DOES" : "DOESN'T");
    return (l != NULL);
#else
    return FALSE;
#endif
}

static void notify_problem_list(GList *problems, int flags)
{
    bool persistence = false;
    if (notify_is_initted() || notify_init(_("Problem detected")))
        persistence = server_has_persistence();
    else
        /* show icon and don't try to show notify if initialization of libnotify failed */
        flags |= SHOW_ICON_ONLY;

    if (!persistence || flags & SHOW_ICON_ONLY)
    {
        /* Use a message of the last one */
        GList *last = g_list_last(problems);

        if (last)
        {
            problem_info_t *pi = (problem_info_t *)last->data;
            show_icon(pi->message);
        }
    }

    if (flags & SHOW_ICON_ONLY)
    {
        g_list_free_full(problems, (GDestroyNotify)problem_info_free);
        return;
    }

    for (GList *iter = problems; iter; iter = g_list_next(iter))
    {
        problem_info_t *pi = iter->data;
        NotifyNotification *notification = new_warn_notification(persistence);
        notify_notification_add_action(notification, "IGNORE", _("Ignore"),
                NOTIFY_ACTION_CALLBACK(action_ignore),
                pi, NULL);

        if (pi->known)
        {
            notify_notification_add_action(notification, A_KNOWN_OPEN_GUI, _("Open"),
                    NOTIFY_ACTION_CALLBACK(action_known),
                    pi, NULL);

            notify_notification_add_action(notification, A_KNOWN_OPEN_BROWSER, _("Show"),
                    NOTIFY_ACTION_CALLBACK(action_known),
                    pi, NULL);

            notify_notification_update(notification, _("A Known Problem has Occurred"), pi->message, NULL);
        }
        else
        {
            if (flags & JUST_DETECTED_PROBLEM)
            {
                notify_notification_add_action(notification, A_REPORT_AUTOREPORT, _("Report"),
                        NOTIFY_ACTION_CALLBACK(action_report),
                        pi, NULL);

                notify_notification_update(notification, _("A Problem has Occurred"), pi->message, NULL);
            }
            else
            {
                notify_notification_add_action(notification, A_REPORT_REPORT, _("Report"),
                        NOTIFY_ACTION_CALLBACK(action_report),
                        pi, NULL);

                notify_notification_update(notification, _("A New Problem has Occurred"), pi->message, NULL);
            }
        }

        GError *err = NULL;
        VERB3 log("Showing a notification");
        notify_notification_show(notification, &err);
        if (err != NULL)
        {
            error_msg(_("Can't show notification: %s"), err->message);
            g_error_free(err);
        }
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

            VERB3 log("%s", msg);
            pi->known |= (prefixcmp(msg, "THANKYOU") == 0);

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

    if (status == 0)
    {
        VERB3 log("fast report finished successfully");
        if (pi->known || !(state->flags & REPORT_UNKNOWN_PROBLEM_IMMEDIATELY))
            notify_problem(pi);
        else
            run_report_from_applet(pi->problem_dir);
    }
    else
    {
        VERB3 log("fast report failed");
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
    if (pi->foreign)
    {
        int res = chown_dir_over_dbus(pi->problem_dir);
        if (res != 0)
        {
            error_msg(_("Can't take ownership of '%s'"), pi->problem_dir);
            problem_info_free(pi);
            return;
        }
        pi->foreign = false;
    }

    struct dump_dir *dd = open_directory_for_writing(pi->problem_dir, /* don't ask */ NULL);
    if (!dd)
    {
        error_msg(_("Can't open directory for writing '%s'"), pi->problem_dir);
        problem_info_free(pi);
        return;
    }

    free(pi->problem_dir);
    pi->problem_dir = xstrdup(dd->dd_dirname);
    dd_close(dd);

    export_event_configuration(event_name);

    struct event_processing_state *state = new_event_processing_state();
    state->pi = pi;
    state->flags = flags;

    state->child_pid = spawn_event_handler_child(state->pi->problem_dir, event_name, &state->child_stdout_fd);

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

            if (!data->foreign)
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

static void Crash(DBusMessage* signal)
{
    VERB3 log("Crash recorded");
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(signal, &in_iter);

    /* 1st param: package */
    const char* package_name = NULL;
    r = load_charp(&in_iter, &package_name);

    /* 2nd param: dir */
//dir parameter is not used for now, use is planned in the future
    if (r != ABRT_DBUS_MORE_FIELDS)
    {
        error_msg("dbus signal %s: parameter type mismatch", __func__);
        return;
    }
    const char* dir = NULL;
    r = load_charp(&in_iter, &dir);

    /* Optional 3rd param: uid */
    const char* uid_str = NULL;
    if (r == ABRT_DBUS_MORE_FIELDS)
    {
        r = load_charp(&in_iter, &uid_str);
    }
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus signal %s: parameter type mismatch", __func__);
        return;
    }

    bool foreign_problem = false;
    if (uid_str != NULL)
    {
        char *end;
        errno = 0;
        unsigned long uid_num = strtoul(uid_str, &end, 10);
        if (errno || *end != '\0' || uid_num != getuid())
        {
            foreign_problem = true;
            VERB1 log("foreign problem %i", foreign_problem);
        }
    }

    /* If this problem seems to be repeating, do not annoy user with popup dialog.
     * (The icon in the tray is not suppressed)
     */
    static time_t last_time = 0;
    static char* last_package_name = NULL;
    time_t cur_time = time(NULL);
    int flags = 0;
    if (last_package_name && strcmp(last_package_name, package_name) == 0
     && ap_last_problem_dir && strcmp(ap_last_problem_dir, dir) == 0
     && (unsigned)(cur_time - last_time) < 2 * 60 * 60
    ) {
        /* log_msg doesn't show in .xsession_errors */
        error_msg("repeated problem in %s, not showing the notification", package_name);
        flags |= SHOW_ICON_ONLY;
    }
    else
    {
        last_time = cur_time;
        free(last_package_name);
        last_package_name = xstrdup(package_name);
        free(ap_last_problem_dir);
        ap_last_problem_dir = xstrdup(dir);
    }

    problem_info_t *pi = problem_info_new();
    pi->problem_dir = xstrdup(dir);
    pi->foreign = foreign_problem;
    pi->message = build_message(package_name);
    show_problem_notification(pi, flags);
}

static DBusHandlerResult handle_message(DBusConnection* conn, DBusMessage* msg, void* user_data)
{
    const char* member = dbus_message_get_member(msg);

    VERB1 log("%s(member:'%s')", __func__, member);

    int type = dbus_message_get_type(msg);
    if (type != DBUS_MESSAGE_TYPE_SIGNAL)
    {
        log("The message is not a signal. ignoring");
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(member, "Crash") == 0)
        Crash(msg);

    return DBUS_HANDLER_RESULT_HANDLED;
}

//TODO: move to abrt_dbus.cpp
static void die_if_dbus_error(bool error_flag, DBusError* err, const char* msg)
{
    if (dbus_error_is_set(err))
    {
        error_msg("dbus error: %s", err->message);
        /*dbus_error_free(&err); - why bother, we will exit in a microsecond */
        error_flag = true;
    }
    if (!error_flag)
        return;
    error_msg_and_die("%s", msg);
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
    /* Glib 2.31:
     * Major changes to threading and synchronisation
     * - threading is now always enabled in GLib
     * - support for custom thread implementations (including our own internal
     * - support for errorcheck mutexes) has been removed
     */
#if (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 31)
    //can't use log(), because g_verbose is not set yet
    /* Need to be thread safe */
    g_thread_init(NULL);
    gdk_threads_init();
    gdk_threads_enter();
#endif

#if (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 35)
    /* Initialize GType system */
    g_type_init();
#endif

    /* Monitor 'StateChanged' signal on 'org.freedesktop.NetworkManager' interface */
    GError *error = NULL;
    g_system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);

    if (g_system_bus == NULL)
    {
        error_msg("Error creating D-Bus proxy: %s\n", error->message);
        g_error_free(error);
        return -1;
    }

    const guint signal_ret = g_dbus_connection_signal_subscribe(g_system_bus,
                                                        NM_DBUS_SERVICE,
                                                        NM_DBUS_INTERFACE,
                                                        "StateChanged",
                                                        NM_DBUS_PATH,
                                                        /* arg0 */ NULL,
                                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                                        on_nm_state_changed,
                                                        /* user_data */ NULL,
                                                        /* user_data_free_func */ NULL);

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

    /* Initialize our (dbus_abrt) machinery: hook _system_ dbus to glib main loop.
     * (session bus is left to be handled by libnotify, see below) */
    DBusError err;
    dbus_error_init(&err);
    DBusConnection* system_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    die_if_dbus_error(system_conn == NULL, &err, "Can't connect to system dbus");
    attach_dbus_conn_to_glib_main_loop(system_conn, NULL, NULL);
    if (!dbus_connection_add_filter(system_conn, handle_message, NULL, NULL))
        error_msg_and_die("Can't add dbus filter");
    //signal sender=:1.73 -> path=/org/freedesktop/problems; interface=org.freedesktop.problems; member=Crash
    //   string "coreutils-7.2-3.fc11"
    //   string "0"
    dbus_bus_add_match(system_conn, "type='signal',path='"ABRT_DBUS_OBJECT"'", &err);
    die_if_dbus_error(false, &err, "Can't add dbus match");

    /* dbus_abrt cannot handle more than one bus, and we don't really need to.
     * The only thing we want to do is to announce ourself on session dbus */
    DBusConnection* session_conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    die_if_dbus_error(session_conn == NULL, &err, "Can't connect to session dbus");
    int r = dbus_bus_request_name(session_conn,
        ABRT_DBUS_NAME".applet",
        /* flags */ DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    die_if_dbus_error(r != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER, &err,
        "Problem connecting to dbus, or applet is already running");

    /* dbus_bus_request_name can already read some data. Thus while dbus fd hasn't
     * any data anymore, dbus library can buffer a message or two.
     * If we don't do this, the data won't be processed until next dbus data arrives.
     */
    int cnt = 10;
    while (dbus_connection_dispatch(system_conn) != DBUS_DISPATCH_COMPLETE && --cnt)
        continue;

    /* If some new dirs appeared since our last run, let user know it */
    GList *new_dirs = NULL;
    GList *notify_list = NULL;
    new_dir_exists(&new_dirs);
    while (new_dirs)
    {
        struct dump_dir *dd = dd_opendir((char *)new_dirs->data, DD_OPEN_READONLY);
        if (dd == NULL)
        {
            VERB1 log("'%s' is not a dump dir - ignoring\n", (char *)new_dirs->data);
            new_dirs = g_list_next(new_dirs);
            continue;
        }

        char *reported_to = dd_load_text_ext(dd, FILENAME_REPORTED_TO,
                                DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
        if (reported_to == NULL)
        {
            problem_info_t *pi = problem_info_new();
            pi->problem_dir = xstrdup((char *)new_dirs->data);

            char *component = dd_load_text_ext(dd, FILENAME_COMPONENT,
                                DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
            pi->message = build_message(component);
            free(component);

            /* Can't be foreign because if the problem is foreign then the
             * dd_opendir() call failed few lines above and the problem is ignored.
             * */
            pi->foreign = false;

            notify_list = g_list_prepend(notify_list, pi);
        }

        free(reported_to);
        dd_close(dd);

        new_dirs = g_list_next(new_dirs);
    }

    if (notify_list)
        show_problem_list_notification(notify_list, /* show icon and notify */ 0);

    list_free_with_free(new_dirs);

    /* Enter main loop */
    gtk_main();
#if (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 31)
    gdk_threads_leave();
#endif

    if (notify_is_initted())
        notify_uninit();

    save_user_settings();

    g_dbus_connection_signal_unsubscribe(g_system_bus, signal_ret);
    g_object_unref(g_system_bus);

    return 0;
}
