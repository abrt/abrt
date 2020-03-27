/*
 *  Copyright (C) 2013  Red Hat
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "system-config-abrt.h"

#include <stdlib.h>
#include "libabrt.h"

#define APP_NAME "System Config ABRT"

static void
system_config_abrt_window_close_cb(gpointer user_data)
{
    gtk_widget_destroy(GTK_WIDGET(user_data));
}

static GtkWidget *
system_config_abrt_window_new(GApplication *app)
{
    GtkWidget *wnd = gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_default_size(GTK_WINDOW(wnd), 500, 300);
    gtk_window_set_title(GTK_WINDOW(wnd), _("Problem Reporting Configuration"));

    GtkWidget *sca = system_config_abrt_widget_new_with_close_button(
                                    system_config_abrt_window_close_cb, wnd);

    gtk_container_add(GTK_CONTAINER(wnd), sca);

    return wnd;
}

/* SystemConfigAbrt : GtkApplication */

typedef struct
{
    GtkApplication parent_instance;
} SystemConfigAbrt;

typedef GtkApplicationClass SystemConfigAbrtClass;

G_DEFINE_TYPE (SystemConfigAbrt, system_config_abrt, GTK_TYPE_APPLICATION)

static void
system_config_abrt_finalize (GObject *object)
{
    G_OBJECT_CLASS(system_config_abrt_parent_class)->finalize(object);
}

static void
about_activated (GSimpleAction *action,
        GVariant      *parameter,
        gpointer       user_data)
{
    const gchar *authors[] = {
        "ABRT Team &lt;crash-catcher@lists.fedorahosted.org&gt;",
        NULL
    };

    gtk_show_about_dialog (NULL,
            "program-name", APP_NAME,
            "title", _("About System Config ABRT"),
            "version", VERSION,
            "website", "https://github.com/abrt/abrt/wiki/overview",
            "authors", authors,
            NULL);
}

static void
quit_activated (GSimpleAction *action,
        GVariant      *parameter,
        gpointer       user_data)
{
    GApplication *app = user_data;

    g_application_quit(app);
}

static GActionEntry app_entries[] = {
    { "about", about_activated, NULL, NULL, NULL },
    { "quit", quit_activated, NULL, NULL, NULL },
};

static void
system_config_abrt_startup(GApplication *application)
{
    G_APPLICATION_CLASS(system_config_abrt_parent_class)->startup(application);

    g_action_map_add_action_entries(G_ACTION_MAP(application), app_entries, G_N_ELEMENTS(app_entries), application);

    GMenu *app_menu = g_menu_new();
    g_menu_append(app_menu, _("About"), "app.about");
    g_menu_append(app_menu, _("Quit"), "app.quit");

    gtk_application_set_app_menu(GTK_APPLICATION(application), G_MENU_MODEL(app_menu));
}

static void
system_config_abrt_shutdown(GApplication *application)
{
    G_APPLICATION_CLASS(system_config_abrt_parent_class)->shutdown (application);
}

static void
system_config_abrt_activate(GApplication *application)
{
    GtkWidget *wnd = system_config_abrt_window_new(application);
    gtk_widget_show_all(wnd);
    gtk_application_add_window(GTK_APPLICATION(application), GTK_WINDOW(wnd));
}

static void
system_config_abrt_init (SystemConfigAbrt *app)
{
}

static void
system_config_abrt_class_init (SystemConfigAbrtClass *class)
{
    GApplicationClass *application_class = G_APPLICATION_CLASS(class);
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    application_class->startup = system_config_abrt_startup;
    application_class->shutdown = system_config_abrt_shutdown;
    application_class->activate = system_config_abrt_activate;

    object_class->finalize = system_config_abrt_finalize;
}

SystemConfigAbrt *
system_config_abrt_new (void)
{
    SystemConfigAbrt *system_config_abrt;

    g_set_application_name(APP_NAME);

    system_config_abrt = g_object_new(system_config_abrt_get_type(),
            "application-id", "org.freedesktop.SystemConfigAbrt",
            "flags", G_APPLICATION_HANDLES_OPEN,
            NULL);

    return system_config_abrt;
}

/* End class */

int main(int argc, char *argv[])
{
    libreport_glib_init();

    SystemConfigAbrt *system_config_abrt = system_config_abrt_new();

    const int status = g_application_run(G_APPLICATION(system_config_abrt), argc, argv);

    g_object_unref(system_config_abrt);

    return status;
}
