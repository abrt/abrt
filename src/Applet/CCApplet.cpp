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
    #include <config.h>
#endif
#if ENABLE_NLS
    #include <libintl.h>
    #define _(S) gettext(S)
#else
    #define _(S) (S)
#endif
#include "abrtlib.h"
#include "CCApplet.h"


static const gchar menu_xml[] =
        "<?xml version=\"1.0\"?>\
<interface>\
  <requires lib=\"gtk+\" version=\"2.16\"/>\
  <!-- interface-naming-policy project-wide -->\
  <object class=\"GtkMenu\" id=\"popup_menu\">\
    <property name=\"visible\">True</property>\
    <child>\
      <object class=\"GtkMenuItem\" id=\"miHide\">\
        <property name=\"visible\">True</property>\
        <property name=\"label\" translatable=\"yes\">Hide</property>\
      </object>\
    </child>\
    <child>\
      <object class=\"GtkImageMenuItem\" id=\"miQuit\">\
        <property name=\"label\">gtk-quit</property>\
        <property name=\"visible\">True</property>\
        <property name=\"use_underline\">True</property>\
        <property name=\"use_stock\">True</property>\
        <property name=\"always_show_image\">True</property>\
      </object>\
    </child>\
    <child>\
      <object class=\"GtkSeparatorMenuItem\" id=\"miSep1\">\
        <property name=\"visible\">True</property>\
      </object>\
    </child>\
    <child>\
      <object class=\"GtkImageMenuItem\" id=\"miAbout\">\
        <property name=\"label\">gtk-about</property>\
        <property name=\"visible\">True</property>\
        <property name=\"use_underline\">True</property>\
        <property name=\"use_stock\">True</property>\
        <property name=\"always_show_image\">True</property>\
      </object>\
    </child>\
  </object>\
  <object class=\"GtkAboutDialog\" id=\"aboutdialog\">\
    <property name=\"border_width\">5</property>\
    <property name=\"type_hint\">normal</property>\
    <property name=\"has_separator\">False</property>\
    <property name=\"program_name\">Automatic Bug Reporting Tool</property>\
    <property name=\"copyright\" translatable=\"yes\">Copyright &#xA9; 2009 Red Hat, Inc</property>\
    <property name=\"website\">https://fedorahosted.org/abrt/</property>\
    <property name=\"website_label\" translatable=\"yes\">Website</property>\
    <property name=\"license\" translatable=\"yes\">This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.\n\
\n\
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License along with this program.  If not, see &lt;http://www.gnu.org/licenses/&gt;.</property>\
    <property name=\"authors\">Jiri Moskovcak  &lt;jmoskovc@redhat.com&gt;</property>\
    <property name=\"wrap_license\">True</property>\
    <child internal-child=\"vbox\">\
      <object class=\"GtkVBox\" id=\"dialog-vbox1\">\
        <property name=\"visible\">True</property>\
        <property name=\"orientation\">vertical</property>\
        <property name=\"spacing\">2</property>\
        <child>\
          <placeholder/>\
        </child>\
        <child internal-child=\"action_area\">\
          <object class=\"GtkHButtonBox\" id=\"dialog-action_area1\">\
            <property name=\"visible\">True</property>\
            <property name=\"layout_style\">end</property>\
          </object>\
          <packing>\
            <property name=\"expand\">False</property>\
            <property name=\"pack_type\">end</property>\
            <property name=\"position\">0</property>\
          </packing>\
        </child>\
      </object>\
    </child>\
  </object>\
</interface>";

CApplet::CApplet()
{
    m_bDaemonRunning = true;
    /* set-up icon buffers */
    animator = 0;
    animation_stage = ICON_DEFAULT;
    load_icons();
    /* - animation - */
    m_pStatusIcon = gtk_status_icon_new_from_pixbuf(icon_stages_buff[ICON_DEFAULT]);
    notify_init("ABRT");
    m_pNotification = notify_notification_new_with_status_icon("Warning", NULL, NULL, m_pStatusIcon);
    notify_notification_set_urgency(m_pNotification, NOTIFY_URGENCY_CRITICAL);
    notify_notification_set_timeout(m_pNotification, 5000);

    gtk_status_icon_set_visible(m_pStatusIcon, FALSE);

    g_signal_connect(G_OBJECT(m_pStatusIcon), "activate", GTK_SIGNAL_FUNC(CApplet::OnAppletActivate_CB), this);
    g_signal_connect(G_OBJECT(m_pStatusIcon), "popup_menu", GTK_SIGNAL_FUNC(CApplet::OnMenuPopup_cb), this);

//    SetIconTooltip(_("Pending events: %i"), m_mapEvents.size());

    m_pBuilder = gtk_builder_new();
    if (!gtk_builder_add_from_string(m_pBuilder, menu_xml, sizeof(menu_xml)-1, NULL))
    //if (!gtk_builder_add_from_file(m_pBuilder, "popup.GtkBuilder", NULL))
    {
        error_msg("Can't create menu from the description, popup won't be available");
        return;
    }

    m_pMenu = gtk_builder_get_object(m_pBuilder, "popup_menu");
    //gtk_menu_attach_to_widget(GTK_MENU(m_pMenu), GTK_WIDGET(m_pStatusIcon), NULL);
    m_pmiHide = gtk_builder_get_object(m_pBuilder, "miHide");
    if (m_pmiHide != NULL)
    {
        g_signal_connect(m_pmiHide, "activate", G_CALLBACK(CApplet::onHide_cb), this);
    }
    m_pmiQuit = gtk_builder_get_object(m_pBuilder, "miQuit");
    if (m_pmiQuit != NULL)
    {
        g_signal_connect(m_pmiQuit, "activate", G_CALLBACK(gtk_main_quit), NULL);
    }
    m_pAboutDialog = gtk_builder_get_object(m_pBuilder, "aboutdialog");
    m_pmiAbout = gtk_builder_get_object(m_pBuilder, "miAbout");
    if (m_pmiAbout != NULL)
    {
        g_signal_connect(m_pmiAbout, "activate", G_CALLBACK(CApplet::onAbout_cb), m_pAboutDialog);
    }
}

CApplet::~CApplet()
{
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

void CApplet::CrashNotify(const char *format, ...)
{
    va_list args;
    char *buf;
    int n;
    GError *err = NULL;

    va_start(args, format);
    buf = NULL;
    n = vasprintf(&buf, format, args);
    va_end(args);

    notify_notification_update(m_pNotification, _("Warning"), buf, NULL);
    if (gtk_status_icon_is_embedded(m_pStatusIcon))
        notify_notification_show(m_pNotification, &err);
    if (err != NULL)
        g_print(err->message);
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
            perror_msg_and_die("Can't exec abrt-gui");
        }
        gtk_status_icon_set_visible(applet->m_pStatusIcon, false);
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
    gtk_status_icon_set_from_pixbuf(applet->m_pStatusIcon, applet->icon_stages_buff[ICON_DEFAULT]);
    
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
    animate_icon();
    //gtk_status_icon_set_visible(m_pStatusIcon, true);
    //Active wait for icon to be REALLY visible in status area
    //while(!gtk_status_icon_is_embedded(m_pStatusIcon)) continue;
}

void CApplet::onHide_cb(GtkMenuItem *menuitem, gpointer applet)
{
    gtk_status_icon_set_visible(((CApplet*)applet)->m_pStatusIcon, false);
}

void CApplet::onAbout_cb(GtkMenuItem *menuitem, gpointer dialog)
{
    if (dialog)
        gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(GTK_WIDGET(dialog));
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

gboolean CApplet::update_icon(void *applet)
{
    if(((CApplet*)applet)->m_pStatusIcon && ((CApplet*)applet)->animation_stage < ICON_STAGE_LAST){
        gtk_status_icon_set_from_pixbuf(((CApplet*)applet)->m_pStatusIcon,
                                        ((CApplet*)applet)->icon_stages_buff[((CApplet*)applet)->animation_stage++]);
    }
    else
        error_msg("icon is null");
    if(((CApplet*)applet)->animation_stage == ICON_STAGE_LAST){
        ((CApplet*)applet)->animation_stage = 0;
    }
    return true;
}

void CApplet::animate_icon()
{
    if(animator == 0)
    {
        animator = g_timeout_add(100, update_icon, this);
    }
}

void CApplet::stop_animate_icon()
{
    if(animator != 0){
        g_source_remove(animator);
        animator = 0;
    }
}

void CApplet::load_icons()
{
    int stage = ICON_DEFAULT;
    for(stage = ICON_DEFAULT; stage < ICON_STAGE_LAST; stage++)
    {
        char *name;
        GError *error = NULL;
        name = g_strdup_printf(ICON_DIR"/abrt%02d.png", stage);
        icon_stages_buff[stage] = gdk_pixbuf_new_from_file(name, &error);
        if(error != NULL)
            error_msg("Can't load pixbuf from %s\n", name);
        g_free(name);
    }
}


//int CApplet::AddEvent(int pUUID, const std::string& pProgname)
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
//void CApplet::BlinkIcon(bool pBlink)
//{
//    gtk_status_icon_set_blinking(m_pStatusIcon, pBlink);
//}
