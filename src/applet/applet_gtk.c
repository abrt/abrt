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
#include "abrtlib.h"
#include "applet_gtk.h"

static gboolean persistent_notification;

#if !defined(NOTIFY_VERSION_MINOR) || (NOTIFY_VERSION_MAJOR == 0 && NOTIFY_VERSION_MINOR >= 6)
static gboolean server_has_persistence (void)
{
    gboolean has;
    GList   *caps;
    GList   *l;

    caps = notify_get_server_caps ();
    if (caps == NULL) {
            fprintf (stderr, "Failed to receive server caps.\n");
            return FALSE;
    }

    l = g_list_find_custom (caps, "persistence", (GCompareFunc)strcmp);
    has = l != NULL;

    g_list_foreach (caps, (GFunc) g_free, NULL);
    g_list_free (caps);
    VERB1 log("notify server %s support pesistence\n", has ? "DOES" : "DOESN'T");
    return has;
}
#endif

static bool load_icons(struct applet *applet)
{
    //FIXME: just a tmp workaround
    return false;
    int stage;
    for (stage = ICON_DEFAULT; stage < ICON_STAGE_LAST; stage++)
    {
        char name[sizeof(ICON_DIR"/abrt%02d.png")];
        GError *error = NULL;
        if (snprintf(name, sizeof(ICON_DIR"/abrt%02d.png"), ICON_DIR"/abrt%02d.png", stage) > 0)
        {
            applet->ap_icon_stages_buff[stage] = gdk_pixbuf_new_from_file(name, &error);
            if (error != NULL)
            {
                error_msg("Can't load pixbuf from %s, animation is disabled", name);
                return false;
            }
        }
    }
    return true;
}

static void stop_animate_icon(struct applet *applet)
{
    /* applet->ap_animator should be 0 if icons are not loaded, so this should be safe */
    if (applet->ap_animator != 0)
    {
        g_source_remove(applet->ap_animator);
        gtk_status_icon_set_from_pixbuf(applet->ap_status_icon,
                                        applet->ap_icon_stages_buff[ICON_DEFAULT]
        );
        applet->ap_animator = 0;
    }
}

//this action should open the reporter dialog directly, without showing the main window
static void action_report(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    struct applet *applet = (struct applet *)user_data;
    if (applet->ap_daemon_running)
    {
        pid_t pid = vfork();
        if (pid < 0)
            perror_msg("vfork");
        if (pid == 0)
        { /* child */
            signal(SIGCHLD, SIG_DFL); /* undo SIG_IGN in abrt-applet */
            execl(BIN_DIR"/bug-reporting-wizard", "bug-reporting-wizard",
                  applet->ap_last_crash_id, (char*) NULL);
            /* Did not find abrt-gui in installation directory. Oh well */
            /* Trying to find it in PATH */
            execlp("bug-reporting-wizard", "bug-reporting-wizard",
                   applet->ap_last_crash_id, (char*) NULL);
            perror_msg_and_die("Can't execute abrt-gui");
        }
        GError *err = NULL;
        notify_notification_close(notification, &err);
        if (err != NULL)
        {
            error_msg("%s", err->message);
            g_error_free(err);
        }
        hide_icon(applet);
        stop_animate_icon(applet);
    }
}

//this action should open the main window
static void action_open_gui(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    struct applet *applet = (struct applet*)user_data;
    if (applet->ap_daemon_running)
    {
        pid_t pid = vfork();
        if (pid < 0)
            perror_msg("vfork");
        if (pid == 0)
        { /* child */
            signal(SIGCHLD, SIG_DFL); /* undo SIG_IGN in abrt-applet */
            execl(BIN_DIR"/abrt-gui", "abrt-gui", (char*) NULL);
            /* Did not find abrt-gui in installation directory. Oh well */
            /* Trying to find it in PATH */
            execlp("abrt-gui", "abrt-gui", (char*) NULL);
            perror_msg_and_die("Can't execute abrt-gui");
        }
        GError *err = NULL;
        notify_notification_close(notification, &err);
        if (err != NULL)
        {
            error_msg("%s", err->message);
            g_error_free(err);
        }
        hide_icon(applet);
        stop_animate_icon(applet);
    }
}

static void on_menu_popup_cb(GtkStatusIcon *status_icon,
                        guint          button,
                        guint          activate_time,
                        gpointer       user_data)
{
    struct applet *applet = (struct applet*)user_data;
    /* stop the animation */
    stop_animate_icon(applet);

    if (applet->ap_menu != NULL)
    {
        gtk_menu_popup(GTK_MENU(applet->ap_menu),
                NULL, NULL,
                gtk_status_icon_position_menu,
                status_icon, button, activate_time);
    }
}

// why it is not named with suffix _cb when it is callback for g_timeout_add?
static gboolean update_icon(void *user_data)
{
    struct applet *applet = (struct applet*)user_data;
    if (applet->ap_status_icon && applet->ap_animation_stage < ICON_STAGE_LAST)
    {
        gtk_status_icon_set_from_pixbuf(applet->ap_status_icon,
                                        applet->ap_icon_stages_buff[applet->ap_animation_stage++]);
    }
    if (applet->ap_animation_stage == ICON_STAGE_LAST)
    {
        applet->ap_animation_stage = 0;
    }
    if (--applet->ap_anim_countdown == 0)
    {
        stop_animate_icon(applet);
    }
    return true;
}

static void animate_icon(struct applet* applet)
{
    if (applet->ap_animator == 0)
    {
        applet->ap_animator = g_timeout_add(100, update_icon, applet);
        applet->ap_anim_countdown = 10 * 3; /* 3 sec */
    }
}

static void on_notify_close(NotifyNotification *notification, gpointer user_data)
{
    g_object_unref(notification);
}

static NotifyNotification *new_warn_notification()
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
                GTK_STOCK_DIALOG_WARNING, 48, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);

    if (pixbuf)
        notify_notification_set_icon_from_pixbuf(notification, pixbuf);
    notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(notification,
                              persistent_notification ? NOTIFY_EXPIRES_NEVER
                                                      : NOTIFY_EXPIRES_DEFAULT);

    return notification;
}


static void on_hide_cb(GtkMenuItem *menuitem, gpointer applet)
{
    if (applet)
        hide_icon((struct applet*)applet);
}

static void on_about_cb(GtkMenuItem *menuitem, gpointer dialog)
{
    if (dialog)
    {
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_hide(GTK_WIDGET(dialog));
    }
}

static GtkWidget *create_about_dialog()
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

static GtkWidget *create_menu(struct applet *applet)
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *b_quit = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
    g_signal_connect(b_quit, "activate", gtk_main_quit, NULL);
    GtkWidget *b_hide = gtk_menu_item_new_with_label(_("Hide"));
    g_signal_connect(b_hide, "activate", G_CALLBACK(on_hide_cb), applet);
    GtkWidget *b_about = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
    GtkWidget *about_dialog = create_about_dialog();
    g_signal_connect(b_about, "activate", G_CALLBACK(on_about_cb), about_dialog);
    GtkWidget *separator = gtk_separator_menu_item_new();
    if (menu)
    {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),b_hide);
        gtk_widget_show(b_hide);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),b_about);
        gtk_widget_show(b_about);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),separator);
        gtk_widget_show(separator);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),b_quit);
        gtk_widget_show(b_quit);
    }
    return menu;
}

static void on_applet_activate_cb(GtkStatusIcon *status_icon, gpointer user_data)
{
    struct applet *applet = (struct applet*)user_data;
    if (applet->ap_daemon_running)
    {
        pid_t pid = vfork();
        if (pid < 0)
            perror_msg("vfork");
        if (pid == 0)
        { /* child */
            signal(SIGCHLD, SIG_DFL); /* undo SIG_IGN in abrt-applet */
            execl(BIN_DIR"/abrt-gui", "abrt-gui", (char*) NULL);
            /* Did not find abrt-gui in installation directory. Oh well */
            /* Trying to find it in PATH */
            execlp("abrt-gui", "abrt-gui", (char*) NULL);
            perror_msg_and_die("Can't execute abrt-gui");
        }
        hide_icon(applet);
        stop_animate_icon(applet);
    }
}

struct applet *applet_new(const char* app_name)
{
    struct applet *applet = (struct applet*)xzalloc(sizeof(struct applet));
    applet->ap_daemon_running = true;
#if !defined(NOTIFY_VERSION_MINOR) || (NOTIFY_VERSION_MAJOR == 0 && NOTIFY_VERSION_MINOR >= 6)
    persistent_notification = server_has_persistence();
#endif

    applet->ap_status_icon = NULL;
    if (!persistent_notification)
    {
        /* set-up icon buffers */
        if (ICON_DEFAULT != 0)
            applet->ap_animation_stage = ICON_DEFAULT;
        applet->ap_icons_loaded = load_icons(applet);
        /* - animation - */
        if (applet->ap_icons_loaded == true)
        {
            //FIXME: animation is disabled for now
            applet->ap_status_icon = gtk_status_icon_new_from_pixbuf(applet->ap_icon_stages_buff[ICON_DEFAULT]);
        }
        else
        {
            applet->ap_status_icon = gtk_status_icon_new_from_icon_name("abrt");
        }
        hide_icon(applet);
        g_signal_connect(G_OBJECT(applet->ap_status_icon), "activate", GTK_SIGNAL_FUNC(on_applet_activate_cb), applet);
        g_signal_connect(G_OBJECT(applet->ap_status_icon), "popup_menu", GTK_SIGNAL_FUNC(on_menu_popup_cb), applet);
        applet->ap_menu = create_menu(applet);
    }

    notify_init("ABRT");
    return applet;
}

void applet_destroy(struct applet *applet)
{
    if (notify_is_initted())
        notify_uninit();

    free(applet);
}

void set_icon_tooltip(struct applet *applet, const char *format, ...)
{
    if (applet->ap_status_icon == NULL)
        return;

    va_list args;
    int n;
    char *buf;

    // xvasprintf?
    va_start(args, format);
    buf = NULL;
    n = vasprintf(&buf, format, args);
    va_end(args);

    gtk_status_icon_set_tooltip_text(applet->ap_status_icon, (n >= 0 && buf) ? buf : "");
    free(buf);
}

void show_crash_notification(struct applet *applet, const char* crash_dir, const char *format, ...)
{
    applet->ap_last_crash_id = crash_dir;
    va_list args;
    va_start(args, format);
    char *buf = xvasprintf(format, args);
    va_end(args);

    NotifyNotification *notification = new_warn_notification();
    notify_notification_add_action(notification, "REPORT", _("Report"),
                                    NOTIFY_ACTION_CALLBACK(action_report),
                                    applet, NULL);
    notify_notification_add_action(notification, "default", _("Show"),
                                    NOTIFY_ACTION_CALLBACK(action_open_gui),
                                    applet, NULL);

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

void show_msg_notification(struct applet *applet, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    char *buf = xvasprintf(format, args);
    va_end(args);

    /* we don't want to show any buttons now,
       maybe later we can add action binded to message
       like >>Clear old dumps<< for quota exceeded
   */
    NotifyNotification *notification = new_warn_notification();
    notify_notification_add_action(notification, "OPEN_MAIN_WINDOW", _("Open ABRT"),
                                    NOTIFY_ACTION_CALLBACK(action_open_gui),
                                    applet, NULL);
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

void show_icon(struct applet *applet)
{
    if (applet->ap_status_icon == NULL)
        return;

    gtk_status_icon_set_visible(applet->ap_status_icon, true);
    /* only animate if all icons are loaded, use the "gtk-warning" instead */
    if (applet->ap_icons_loaded)
        animate_icon(applet);
}

void hide_icon(struct applet *applet)
{
    if (applet->ap_status_icon == NULL)
        return;

    gtk_status_icon_set_visible(applet->ap_status_icon, false);
    stop_animate_icon(applet);
}

