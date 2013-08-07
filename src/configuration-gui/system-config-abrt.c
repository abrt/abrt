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
#include "abrt-config-widget.h"

#include <libabrt.h>


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

GtkWidget *system_config_abrt_widget_new(void)
{
    GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, /*spacing*/0));

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

    GtkWidget *btn_apply = gtk_button_new_with_mnemonic(_("_Apply"));
    gtk_widget_set_sensitive(btn_apply, FALSE);
    gtk_widget_set_halign(btn_apply, GTK_ALIGN_END);
    gtk_widget_set_valign(btn_apply, GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(buttons), btn_apply, /*expand*/FALSE, /*fill*/FALSE, /*padding*/0);
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(system_config_abrt_apply_cb), config);
    g_signal_connect(config, "changed", G_CALLBACK(system_config_abrt_changed_cb), btn_apply);

    gtk_widget_show_all(buttons);

    return GTK_WIDGET(box);
}

static void
system_config_abrt_dialog_close(GtkDialog *dialog, gpointer user_data)
{
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static gboolean
system_config_abrt_dialog_delete_event(GtkWidget *dialog, GdkEvent *event, gpointer user_data)
{
    gtk_widget_destroy(GTK_WIDGET(dialog));
    return TRUE; /*do not propagate the event*/
}

void show_system_config_abrt_dialog(GtkWindow *parent)
{
    GtkWidget *dialog = gtk_dialog_new();

    gtk_window_set_title(GTK_WINDOW(dialog), _("Problem Reporting Configuration"));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 480);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    if (parent != NULL)
    {
        gtk_window_set_icon_name(GTK_WINDOW(dialog), gtk_window_get_icon_name(parent));
        gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
        gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    }

    /* Have to handle these signals on our own otherwise users must press close button twice */
    g_signal_connect(dialog, "close", G_CALLBACK(system_config_abrt_dialog_close), /*user_data*/NULL);
    g_signal_connect(dialog, "delete_event", G_CALLBACK(system_config_abrt_dialog_delete_event), /*user_data*/NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *sca = system_config_abrt_widget_new();
    gtk_box_pack_start(GTK_BOX(content), sca, /*expand*/TRUE, /*fill*/TRUE, /*padding*/0);

    gtk_widget_show_all(content);

    gtk_dialog_run(GTK_DIALOG(dialog));
}
