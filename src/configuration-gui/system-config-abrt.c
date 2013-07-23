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

#include "abrt-config-widget.h"
#include "libabrt.h"

#include <stdlib.h>
#include <gtk/gtk.h>

#define APP_NAME "System Config ABRT"

static void
system_config_abrt_apply_cb(GtkButton *button, gpointer user_data)
{
    AbrtConfigWidget *config = ABRT_CONFIG_WIDGET(user_data);
    abrt_config_widget_save_chnages(config);
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
}

static void
system_config_abrt_changed_cb(AbrtConfigWidget *config, gpointer user_data)
{
    GtkWidget *button = GTK_WIDGET(user_data);
    gtk_widget_set_sensitive(button, TRUE);
}

static GtkWidget *
system_config_abrt_window_new(GApplication *app)
{
    GtkWidget *wnd = gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_default_size(GTK_WINDOW(wnd), 640, 480);
    gtk_window_set_title(GTK_WINDOW(wnd), _("Problem Reporting Configuration"));

    GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, /*spacing*/0));
    gtk_container_add(GTK_CONTAINER(wnd), GTK_WIDGET(box));

    AbrtConfigWidget *config = abrt_config_widget_new();
    gtk_widget_set_visible(GTK_WIDGET(config), TRUE);
    gtk_box_pack_start(box, GTK_WIDGET(config), /*expand*/TRUE, /*fill*/TRUE, /*padding*/0);

    gtk_box_pack_start(GTK_BOX(box),
            gtk_alignment_new(/*xalign*/.5, /*yalign*/.5, /*xscale*/.5, /*yscale*/.5),
            /*expand*/TRUE, /*fill*/TRUE, /*padding*/0);

    GtkWidget *buttons = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(box), buttons, /*expand*/FALSE, /*fill*/FALSE, /*padding*/0);

    gtk_widget_set_margin_left(buttons, 10);
    gtk_widget_set_margin_right(buttons, 10);
    gtk_widget_set_margin_top(buttons, 10);
    gtk_widget_set_margin_bottom(buttons, 10);

    gtk_box_pack_start(GTK_BOX(buttons),
            gtk_alignment_new(/*xalign*/.5, /*yalign*/.5, /*xscale*/.5, /*yscale*/.5),
            /*expand*/TRUE, /*fill*/TRUE, /*padding*/0);

    GtkWidget *btn_apply = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    gtk_widget_set_sensitive(btn_apply, FALSE);
    gtk_widget_set_halign(btn_apply, GTK_ALIGN_END);
    gtk_widget_set_valign(btn_apply, GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(buttons), btn_apply, /*expand*/FALSE, /*fill*/FALSE, /*padding*/0);
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(system_config_abrt_apply_cb), config);
    g_signal_connect(config, "changed", G_CALLBACK(system_config_abrt_changed_cb), btn_apply);

    gtk_widget_show_all(buttons);

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
    glib_init();

    SystemConfigAbrt *system_config_abrt = system_config_abrt_new();

    const int status = g_application_run(G_APPLICATION(system_config_abrt), argc, argv);

    g_object_unref(system_config_abrt);

    return status;
}
