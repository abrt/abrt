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

#include <internal_abrt_dbus.h>
#include "libabrt.h"


#define ABRTD_DBUS_NAME  "com.redhat.abrt"
#define ABRTD_DBUS_PATH  "/com/redhat/abrt"
#define ABRTD_DBUS_IFACE "com.redhat.abrt"


static gboolean persistent_notification;
static GtkStatusIcon *ap_status_icon;
static GtkWidget *ap_menu;
static char *ap_last_problem_dir;
static char **s_dirs;
static char *s_home;
#define ap_daemon_running 1

typedef struct problem_info {
    char *problem_dir;
    bool foreign;
} problem_info_t;

static problem_info_t *problem_info_new()
{
    problem_info_t *pi = xmalloc(sizeof(problem_info_t));
    return pi;
}

static void problem_info_free(problem_info_t *pi)
{
    if (pi == NULL)
        return;

    free(pi->problem_dir);
    free(pi);
}

static void call_notify_init(void)
{
    static bool inited = 0;
    if (inited)
        return;
    inited = 1;

    notify_init(_("Problem detected"));
}

#if !defined(NOTIFY_VERSION_MINOR) || (NOTIFY_VERSION_MAJOR == 0 && NOTIFY_VERSION_MINOR >= 6)
static gboolean server_has_persistence(void)
{
    GList *caps;
    GList *l;

    call_notify_init();

    caps = notify_get_server_caps();
    if (caps == NULL)
    {
        error_msg("Failed to receive server caps");
        return FALSE;
    }

    l = g_list_find_custom(caps, "persistence", (GCompareFunc)strcmp);

    list_free_with_free(caps);
    VERB1 log("notify server %s support pesistence", l ? "DOES" : "DOESN'T");
    return (l != NULL);
}
#else
# define server_has_persistence() call_notify_init()
#endif

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

/* Return 1 if a new directory appeared, compared to list saved
 * in ~/.abrt/applet_dirlist. In any case, ~/.abrt/applet_dirlist
 * is updated with updated list.
 */
static GList *new_dir_exists(void)
{
    GList *dirlist = NULL;
    GList *new_dirs = NULL;
    char **pp = s_dirs;
    while (*pp)
    {
        dirlist = add_dirs_to_dirlist(dirlist, *pp);
        pp++;
    }

    char *dirlist_name = concat_path_file(s_home, ".abrt");
    mkdir(dirlist_name, 0777);
    free(dirlist_name);
    dirlist_name = concat_path_file(s_home, ".abrt/applet_dirlist");
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
                new_dirs = g_list_prepend(new_dirs, xstrdup(l1->data));
                VERB1 log("New dir detected: %s", (char *)l1->data);
                l1 = g_list_next(l1);
                continue;
            }
            l2 = g_list_next(l2);
            if (diff == 0)
                l1 = g_list_next(l1);
        }

        different |= (l1 != NULL);
        while(l1)
        {
            new_dirs = g_list_prepend(new_dirs, xstrdup(l1->data));
            VERB1 log("New dir detected: %s", (char *)l1->data);
            l1 = g_list_next(l1);
        }

        if (different || l1 || l2)
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

    return new_dirs;
}

static void fork_exec_gui(void)
{
    pid_t pid = vfork();
    if (pid < 0)
        perror_msg("vfork");
    if (pid == 0)
    {
        /* child */
        signal(SIGCHLD, SIG_DFL); /* undo SIG_IGN in abrt-applet */
//TODO: pass s_dirs[] as DIR param(s) to abrt-gui
        execl(BIN_DIR"/abrt-gui", "abrt-gui", (char*) NULL);
        /* Did not find abrt-gui in installation directory. Oh well */
        /* Trying to find it in PATH */
        execlp("abrt-gui", "abrt-gui", (char*) NULL);
        perror_msg_and_die("Can't execute '%s'", "abrt-gui");
    }
    /* Scan dirs and save new ~/.abrt/applet_dirlist.
     * (Oterwise, after a crash, next time applet is started,
     * it will show alert icon even if we did click on it
     * "in previous life"). We ignore function return value.
     */
    list_free_with_free(new_dir_exists());
}

static void hide_icon(void)
{
    if (ap_status_icon == NULL)
        return;

    gtk_status_icon_set_visible(ap_status_icon, false);
}

//this action should open the reporter dialog directly, without showing the main window
static void action_report(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    problem_info_t *pi = (problem_info_t *)user_data;
    if (ap_daemon_running && ap_last_problem_dir)
    {
        report_problem_in_dir(pi->problem_dir, LIBREPORT_ANALYZE | LIBREPORT_NOWAIT);

        GError *err = NULL;
        notify_notification_close(notification, &err);
        if (err != NULL)
        {
            error_msg("%s", err->message);
            g_error_free(err);
        }

        hide_icon();
        problem_info_free(pi);

        /* Scan dirs and save new ~/.abrt/applet_dirlist.
         * (Oterwise, after a crash, next time applet is started,
         * it will show alert icon even if we did click on it
         * "in previous life"). We ignore finction return value.
         */
        list_free_with_free(new_dir_exists());
    }
}

//this action should open the main window
static void action_open_gui(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    if (ap_daemon_running)
    {
        fork_exec_gui();
        GError *err = NULL;
        notify_notification_close(notification, &err);
        if (err != NULL)
        {
            error_msg("%s", err->message);
            g_error_free(err);
        }
        hide_icon();
    }
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
    g_object_unref(notification);
}

static NotifyNotification *new_warn_notification(void)
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
    notify_notification_set_timeout(notification,
                              server_has_persistence() ? NOTIFY_EXPIRES_NEVER
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
    if (ap_daemon_running)
    {
        fork_exec_gui();
        hide_icon();
    }
}

static void set_icon_tooltip(const char *format, ...)
{
    if (ap_status_icon == NULL)
        return;

    va_list args;
    char *buf;

    va_start(args, format);
    buf = xvasprintf(format, args);
    va_end(args);

    gtk_status_icon_set_tooltip_text(ap_status_icon, buf);
    free(buf);
}

static void show_problem_notification(problem_info_t *pi, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *buf = xvasprintf(format, args);
    va_end(args);

    NotifyNotification *notification = new_warn_notification();
    if (!pi->foreign)
    {
        notify_notification_add_action(notification, "REPORT", _("Report"),
                                    NOTIFY_ACTION_CALLBACK(action_report),
                                    pi, NULL);
    }
    else
    {
        notify_notification_add_action(notification, "SHOW", _("Show"),
                                    NOTIFY_ACTION_CALLBACK(action_open_gui),
                                    NULL, NULL);
    }
    notify_notification_add_action(notification, "default", _("Show"),
                                    NOTIFY_ACTION_CALLBACK(action_open_gui),
                                    NULL, NULL);

    notify_notification_update(notification, _("A Problem has Occurred"), buf, NULL);
    free(buf);

    GError *err = NULL;
    notify_notification_show(notification, &err);
    if (err != NULL)
    {
        error_msg("%s", err->message);
        g_error_free(err);
    }
}

static void show_icon(void)
{
    if (server_has_persistence())
        return;

    gtk_status_icon_set_visible(ap_status_icon, true);
}

static void init_applet(void)
{
    static bool inited = 0;
    if (inited)
        return;
    inited = 1;

    // ap_daemon_running = true;
    persistent_notification = server_has_persistence();

    if (!persistent_notification)
    {
        ap_status_icon = gtk_status_icon_new_from_icon_name("abrt");
        hide_icon();
        g_signal_connect(G_OBJECT(ap_status_icon), "activate", G_CALLBACK(on_applet_activate_cb), NULL);
        g_signal_connect(G_OBJECT(ap_status_icon), "popup_menu", G_CALLBACK(on_menu_popup_cb), NULL);
        ap_menu = create_menu();
    }
}

static void Crash(DBusMessage* signal)
{
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

    if (uid_str != NULL)
    {
        char *end;
        errno = 0;
        unsigned long uid_num = strtoul(uid_str, &end, 10);
        if (errno || *end != '\0' || uid_num != getuid())
        {
            return;
        }
    }

    const int foreign = !dump_dir_accessible_by_uid(dir, getuid());
    const char* message;

    if (foreign)
    {
        if (package_name[0] == '\0')
            message = _("Use a privileged user account to report the problem");
        else
            message = _("Use a privileged user account to report the problem in the %s package");
    }
    else
    {
        if (package_name[0] == '\0')
            message = _("A problem has been detected");
        else
            message = _("A problem in the %s package has been detected");
    }

    if (!server_has_persistence())
    {
        error_msg("notifyd doesn't have persistence - showing old style status icon");
        init_applet();
        set_icon_tooltip(message, package_name);
        show_icon();
    }

    /* If this problem seems to be repeating, do not annoy user with popup dialog.
     * (The icon in the tray is not suppressed)
     */
    static time_t last_time = 0;
    static char* last_package_name = NULL;
    time_t cur_time = time(NULL);
    if (last_package_name && strcmp(last_package_name, package_name) == 0
     && ap_last_problem_dir && strcmp(ap_last_problem_dir, dir) == 0
     && (unsigned)(cur_time - last_time) < 2 * 60 * 60
    ) {
        log_msg("repeated problem in %s, not showing the notification", package_name);
        return;
    }
    last_time = cur_time;
    free(last_package_name);
    last_package_name = xstrdup(package_name);
    free(ap_last_problem_dir);
    ap_last_problem_dir = xstrdup(dir);

    problem_info_t *pi = problem_info_new();
    pi->problem_dir = xstrdup(dir);
    pi->foreign = foreign;
    show_problem_notification(pi, message, package_name);
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

    //if (strcmp(member, "NameOwnerChanged") == 0)
    //    NameOwnerChanged(msg);
    else if (strcmp(member, "Crash") == 0)
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
     - threading is now always enabled in GLib
     - support for custom thread implementations (including our own internal
     - support for errorcheck mutexes) has been removed
  */
#if (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 31)
    //can't use log(), because g_verbose is not set yet
    /* Need to be thread safe */
    g_thread_init(NULL);
    gdk_threads_init();
    gdk_threads_enter();
#endif

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

    export_abrt_envvars(0);
    msg_prefix = g_progname;

    s_home = getenv("HOME");

    load_abrt_conf();
    const char *default_dirs[] = {
        g_settings_dump_location,
        NULL,
        NULL,
    };
    argv += optind;
    if (!argv[0])
    {
        if (s_home)
            default_dirs[1] = concat_path_file(s_home, ".abrt/spool");
        argv = (char**)default_dirs;
    }
    s_dirs = argv;

    /* Prevent zombies when we spawn abrt-gui */
    signal(SIGCHLD, SIG_IGN);

    /* Initialize our (dbus_abrt) machinery: hook _system_ dbus to glib main loop.
     * (session bus is left to be handled by libnotify, see below) */
    DBusError err;
    dbus_error_init(&err);
    DBusConnection* system_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    die_if_dbus_error(system_conn == NULL, &err, "Can't connect to system dbus");
    attach_dbus_conn_to_glib_main_loop(system_conn, NULL, NULL);
    if (!dbus_connection_add_filter(system_conn, handle_message, NULL, NULL))
        error_msg_and_die("Can't add dbus filter");
    /* which messages do we want to be fed to handle_message()? */
    //signal sender=org.freedesktop.DBus -> path=/org/freedesktop/DBus; interface=org.freedesktop.DBus; member=NameOwnerChanged
    //   string "com.redhat.abrt"
    //   string ""
    //   string ":1.70"
    dbus_bus_add_match(system_conn, "type='signal',member='NameOwnerChanged'", &err);
    die_if_dbus_error(false, &err, "Can't add dbus match");
    //signal sender=:1.73 -> path=/com/redhat/abrt; interface=com.redhat.abrt; member=Crash
    //   string "coreutils-7.2-3.fc11"
    //   string "0"
    dbus_bus_add_match(system_conn, "type='signal',path='/com/redhat/abrt'", &err);
    die_if_dbus_error(false, &err, "Can't add dbus match");

    /* dbus_abrt cannot handle more than one bus, and we don't really need to.
     * The only thing we want to do is to announce ourself on session dbus */
    DBusConnection* session_conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    die_if_dbus_error(session_conn == NULL, &err, "Can't connect to session dbus");
    int r = dbus_bus_request_name(session_conn,
        "com.redhat.abrt.applet",
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
    GList *new_dirs = new_dir_exists();
    while (new_dirs)
    {
        struct dump_dir *dd = dd_opendir((char *)new_dirs->data, DD_OPEN_READONLY);
        if (dd == NULL)
        {
            VERB1 log("'%s' is not a dump dir - ignoring\n", (char *)new_dirs->data);
            new_dirs = g_list_next(new_dirs);
            continue;
        }

        const char *package_name = dd_load_text(dd, FILENAME_COMPONENT);
        const char *message = _("A problem in the %s package has been detected");
        if (package_name[0] == '\0')
            message = _("A problem has been detected");

        init_applet();
        set_icon_tooltip(message, package_name);
        show_icon();

        problem_info_t *pi = problem_info_new();
        pi->problem_dir = xstrdup((char *)new_dirs->data);
        pi->foreign = false;
        show_problem_notification(pi, message, package_name);

        dd_close(dd);

        new_dirs = g_list_next(new_dirs);
    }
    list_free_with_free(new_dirs);

    /* Enter main loop */
    gtk_main();
#if (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 31)
    gdk_threads_leave();
#endif

    if (notify_is_initted())
        notify_uninit();

    return 0;
}
