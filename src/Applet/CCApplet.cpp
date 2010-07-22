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
#if HAVE_CONFIG_H
# include <config.h>
#endif
#if ENABLE_NLS
# include <libintl.h>
# define _(S) gettext(S)
#else
# define _(S) (S)
#endif
#include "abrtlib.h"
#include "CCApplet.h"

static void on_notify_close(NotifyNotification *notification, gpointer user_data)
{
    g_object_unref(notification);
}

static NotifyNotification *new_warn_notification()
{
    NotifyNotification *notification;
    notification = notify_notification_new(_("Warning"), NULL, NULL, NULL);
    g_signal_connect(notification, "closed", G_CALLBACK(on_notify_close), NULL);

    GdkPixbuf *pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                GTK_STOCK_DIALOG_WARNING, 48, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);

    if (pixbuf)
        notify_notification_set_icon_from_pixbuf(notification, pixbuf);
    notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(notification, NOTIFY_EXPIRES_DEFAULT);

    return notification;
}


static void on_hide_cb(GtkMenuItem *menuitem, gpointer applet)
{
    if (applet)
        ((CApplet*)applet)->HideIcon();
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

static GtkWidget *create_menu(CApplet *applet)
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

CApplet::CApplet(const char* app_name)
{
    m_bDaemonRunning = true;
    /* set-up icon buffers */
    m_iAnimator = 0;
    m_iAnimationStage = ICON_DEFAULT;
    m_bIconsLoaded = load_icons();
    /* - animation - */
    if (m_bIconsLoaded == true)
    {
        //FIXME: animation is disabled for now
        m_pStatusIcon = gtk_status_icon_new_from_pixbuf(icon_stages_buff[ICON_DEFAULT]);
    }
    else
    {
        m_pStatusIcon = gtk_status_icon_new_from_icon_name("abrt");
    }
    notify_init(app_name);

    gtk_status_icon_set_visible(m_pStatusIcon, FALSE);

    g_signal_connect(G_OBJECT(m_pStatusIcon), "activate", GTK_SIGNAL_FUNC(CApplet::OnAppletActivate_CB), this);
    g_signal_connect(G_OBJECT(m_pStatusIcon), "popup_menu", GTK_SIGNAL_FUNC(CApplet::OnMenuPopup_cb), this);

//    SetIconTooltip(_("Pending events: %i"), m_mapEvents.size());

    m_pMenu = create_menu(this);
}

CApplet::~CApplet()
{
    if (notify_is_initted())
        notify_uninit();
}

void CApplet::SetIconTooltip(const char *format, ...)
{
    va_list args;
    int n;
    char *buf;

    va_start(args, format);
    buf = NULL;
    n = vasprintf(&buf, format, args);
    va_end(args);

    gtk_status_icon_set_tooltip_text(m_pStatusIcon, (n >= 0 && buf) ? buf : "");
    free(buf);
}

void CApplet::action_report(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    CApplet *applet = (CApplet *)user_data;
    if (applet->m_bDaemonRunning)
    {
        pid_t pid = vfork();
        if (pid < 0)
            perror_msg("vfork");
        if (pid == 0)
        { /* child */
            char *buf = xasprintf("--report=%s", applet->m_pLastCrashID);
            signal(SIGCHLD, SIG_DFL); /* undo SIG_IGN in abrt-applet */
            execl(BIN_DIR"/abrt-gui", "abrt-gui", buf, (char*) NULL);
            /* Did not find abrt-gui in installation directory. Oh well */
            /* Trying to find it in PATH */
            execlp("abrt-gui", "abrt-gui", buf, (char*) NULL);
            perror_msg_and_die("Can't execute abrt-gui");
        }
        GError *err = NULL;
        notify_notification_close(notification, &err);
        if (err != NULL)
        {
            error_msg("%s", err->message);
            g_error_free(err);
        }
        gtk_status_icon_set_visible(applet->m_pStatusIcon, false);
        applet->stop_animate_icon();
    }
}

void CApplet::action_open_gui(NotifyNotification *notification, gchar *action, gpointer user_data)
{
    CApplet *applet = (CApplet *)user_data;
    if (applet->m_bDaemonRunning)
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
        gtk_status_icon_set_visible(applet->m_pStatusIcon, false);
        applet->stop_animate_icon();
    }
}

void CApplet::CrashNotify(const char* crash_id, const char *format, ...)
{
    m_pLastCrashID = crash_id;
    va_list args;
    va_start(args, format);
    char *buf = xvasprintf(format, args);
    va_end(args);

    NotifyNotification *notification = new_warn_notification();
    notify_notification_add_action(notification, "REPORT", _("Report"),
                                    NOTIFY_ACTION_CALLBACK(CApplet::action_report),
                                    this, NULL);
    notify_notification_add_action(notification, "OPEN_MAIN_WINDOW", "Open ABRT",
                                    NOTIFY_ACTION_CALLBACK(CApplet::action_open_gui),
                                    this, NULL);

    notify_notification_update(notification, _("Warning"), buf, NULL);
    free(buf);
    GError *err = NULL;
    notify_notification_show(notification, &err);
    if (err != NULL)
    {
        error_msg("%s", err->message);
        g_error_free(err);
    }
}

void CApplet::MessageNotify(const char *format, ...)
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
    notify_notification_add_action(notification, "OPEN_MAIN_WINDOW", "Open ABRT",
                                    NOTIFY_ACTION_CALLBACK(CApplet::action_open_gui),
                                    this, NULL);
    notify_notification_update(notification, _("Warning"), buf, NULL);
    free(buf);
    GError *err = NULL;
    notify_notification_show(notification, &err);
    if (err != NULL)
    {
        error_msg("%s", err->message);
        g_error_free(err);
    }
}

void CApplet::OnAppletActivate_CB(GtkStatusIcon *status_icon, gpointer user_data)
{
    CApplet *applet = (CApplet *)user_data;
    if (applet->m_bDaemonRunning)
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
        gtk_status_icon_set_visible(applet->m_pStatusIcon, false);
        applet->stop_animate_icon();
    }
}

void CApplet::OnMenuPopup_cb(GtkStatusIcon *status_icon,
                            guint          button,
                            guint          activate_time,
                            gpointer       user_data)
{
    CApplet *applet = (CApplet *)user_data;
    /* stop the animation */
    applet->stop_animate_icon();

    if (applet->m_pMenu != NULL)
    {
        gtk_menu_popup(GTK_MENU(((CApplet *)user_data)->m_pMenu),
                NULL, NULL,
                gtk_status_icon_position_menu,
                status_icon, button, activate_time);
    }
}

void CApplet::ShowIcon()
{
    gtk_status_icon_set_visible(m_pStatusIcon, true);
    /* only animate if all icons are loaded, use the "gtk-warning" instead */
    if (m_bIconsLoaded)
        animate_icon();
}

void CApplet::HideIcon()
{
    gtk_status_icon_set_visible(m_pStatusIcon, false);
    stop_animate_icon();
}

void CApplet::Disable(const char *reason)
{
    /*
        FIXME: once we have our icon
    */
    m_bDaemonRunning = false;
    GdkPixbuf *gray_scaled;
    GdkPixbuf *pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                GTK_STOCK_DIALOG_WARNING, 24, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
    if (pixbuf)
    {
        gray_scaled = gdk_pixbuf_copy(pixbuf);
        gdk_pixbuf_saturate_and_pixelate(pixbuf, gray_scaled, 0.0, NULL);
        gtk_status_icon_set_from_pixbuf(m_pStatusIcon, gray_scaled);
//do we need to free pixbufs nere?
    }
    else
        error_msg("Can't load icon");
    SetIconTooltip(reason);
    ShowIcon();
}

void CApplet::Enable(const char *reason)
{
    /* restore the original icon */
    m_bDaemonRunning = true;
    SetIconTooltip(reason);
    gtk_status_icon_set_from_stock(m_pStatusIcon, GTK_STOCK_DIALOG_WARNING);
    ShowIcon();
}

gboolean CApplet::update_icon(void *user_data)
{
    CApplet* applet = (CApplet*)user_data;
    if (applet->m_pStatusIcon && applet->m_iAnimationStage < ICON_STAGE_LAST)
    {
        gtk_status_icon_set_from_pixbuf(applet->m_pStatusIcon,
                                        applet->icon_stages_buff[applet->m_iAnimationStage++]);
    }
    if (applet->m_iAnimationStage == ICON_STAGE_LAST)
    {
        applet->m_iAnimationStage = 0;
    }
    if (--applet->m_iAnimCountdown == 0)
    {
        applet->stop_animate_icon();
    }
    return true;
}

void CApplet::animate_icon()
{
    if (m_iAnimator == 0)
    {
        m_iAnimator = g_timeout_add(100, update_icon, this);
        m_iAnimCountdown = 10 * 3; /* 3 sec */
    }
}

void CApplet::stop_animate_icon()
{
    /* animator should be 0 if icons are not loaded, so this should be safe */
    if (m_iAnimator != 0)
    {
        g_source_remove(m_iAnimator);
        gtk_status_icon_set_from_pixbuf(m_pStatusIcon, icon_stages_buff[ICON_DEFAULT]);
        m_iAnimator = 0;
    }
}

bool CApplet::load_icons()
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
            icon_stages_buff[stage] = gdk_pixbuf_new_from_file(name, &error);
            if (error != NULL)
            {
                error_msg("Can't load pixbuf from %s, animation is disabled", name);
                return false;
            }
        }
    }
    return true;
}


//int CApplet::AddEvent(int pUUID, const char *pProgname)
//{
//    m_mapEvents[pUUID] = "pProgname";
//    SetIconTooltip(_("Pending events: %i"), m_mapEvents.size());
//    return 0;
//}
//
//int CApplet::RemoveEvent(int pUUID)
//{
//     m_mapEvents.erase(pUUID);
//     return 0;
//}
