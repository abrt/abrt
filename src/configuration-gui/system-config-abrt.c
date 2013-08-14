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

#define CLOSE_BUTTON_DATA_NAME_CALLBACK "my-close-callback"
#define CLOSE_BUTTON_DATA_NAME_USER_DATA "my-close-user-data"

static void
system_config_abrt_apply_cb(GtkButton *button, gpointer user_data)
{
    AbrtConfigWidget *config = ABRT_CONFIG_WIDGET(user_data);
    abrt_config_widget_save_chnages(config);
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
}

static void
system_config_abrt_close_cb(GtkButton *button, gpointer user_data)
{
    system_config_abrt_widget_close_callback callback = g_object_get_data(G_OBJECT(button), CLOSE_BUTTON_DATA_NAME_CALLBACK);
    gpointer callback_user_data = g_object_get_data(G_OBJECT(button), CLOSE_BUTTON_DATA_NAME_USER_DATA);

    callback(callback_user_data);
}

static void
system_config_abrt_defaults_cb(GtkButton *button, gpointer user_data)
{
    AbrtConfigWidget *config = ABRT_CONFIG_WIDGET(user_data);
    abrt_config_widget_reset_to_defaults(config);
}

static void
system_config_abrt_changed_cb(AbrtConfigWidget *config, gpointer user_data)
{
    GtkWidget *button = GTK_WIDGET(user_data);
    gtk_widget_set_sensitive(button, abrt_config_widget_get_changed(config));
}

gboolean
system_config_abrt_check_before_close(GtkWidget *config)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(config));
    AbrtConfigWidget *acw = ABRT_CONFIG_WIDGET(children->data);
    g_list_free(children);

    if (abrt_config_widget_get_changed(acw))
    {
        GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Apply setting changes"),
                /*unknown parent window*/NULL,
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                _("_Apply"), GTK_RESPONSE_ACCEPT,
                _("_Discard"), GTK_RESPONSE_REJECT,
                _("_Cancel"), GTK_RESPONSE_CANCEL,
                NULL);

        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, /*spacing*/0);
        gtk_widget_set_margin_top(box, 15);
        gtk_widget_set_margin_bottom(box, 10);
        GtkWidget *image = gtk_image_new_from_icon_name("dialog-warning", GTK_ICON_SIZE_DIALOG);
        gtk_box_pack_start(GTK_BOX(box), image, /*expand*/TRUE, /*fill*/TRUE, /*padding*/10);

        GtkWidget *label = gtk_label_new("The settings have changed.\nDo you want to apply the changes or discard them?");
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_box_pack_start(GTK_BOX(box), label, /*expand*/TRUE, /*fill*/TRUE, /*padding*/10);

        gtk_container_add(GTK_CONTAINER (content_area), box);
        gtk_widget_show_all(dialog);

        const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy (dialog);

        if (response == GTK_RESPONSE_ACCEPT)
            abrt_config_widget_save_chnages(acw);

        return response != GTK_RESPONSE_CANCEL;
    }

    return TRUE;
}

GtkWidget *system_config_abrt_widget_new(void)
{
    return system_config_abrt_widget_new_with_close_button(/*no close button*/NULL,
                                                           /*no user data*/NULL);
}

GtkWidget *system_config_abrt_widget_new_with_close_button(system_config_abrt_widget_close_callback close_cb, gpointer user_data)
{
    GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, /*spacing*/0));

    AbrtConfigWidget *config = abrt_config_widget_new();
    gtk_widget_set_visible(GTK_WIDGET(config), TRUE);
    gtk_box_pack_start(box, GTK_WIDGET(config), /*expand*/TRUE, /*fill*/TRUE, /*padding*/0);

    gtk_box_pack_start(GTK_BOX(box),
            gtk_alignment_new(/*xalign*/.5, /*yalign*/.5, /*xscale*/.5, /*yscale*/.5),
            /*expand*/TRUE, /*fill*/TRUE, /*padding*/0);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, /*spacing*/0);
    gtk_box_pack_start(GTK_BOX(box), buttons, /*expand*/TRUE, /*fill*/FALSE, /*padding*/0);

    gtk_widget_set_margin_left(buttons, 10);
    gtk_widget_set_margin_right(buttons, 10);
    gtk_widget_set_margin_top(buttons, 10);
    gtk_widget_set_margin_bottom(buttons, 10);

    if (close_cb != NULL)
    {
        GtkWidget *btn_close = gtk_button_new_with_mnemonic(_("_Close"));
        gtk_box_pack_start(GTK_BOX(buttons), btn_close, /*expand*/FALSE, /*fill*/FALSE, /*padding*/0);

        g_object_set_data(G_OBJECT(btn_close), CLOSE_BUTTON_DATA_NAME_CALLBACK, close_cb);
        g_object_set_data(G_OBJECT(btn_close), CLOSE_BUTTON_DATA_NAME_USER_DATA, user_data);

        g_signal_connect(btn_close, "clicked", G_CALLBACK(system_config_abrt_close_cb), /*user_data*/NULL);
    }

    GtkWidget *btn_defaults = gtk_button_new_with_mnemonic(_("_Defaults"));
    gtk_box_pack_start(GTK_BOX(buttons), btn_defaults, /*expand*/FALSE, /*fill*/FALSE, /*padding*/0);
    g_signal_connect(btn_defaults, "clicked", G_CALLBACK(system_config_abrt_defaults_cb), config);
    if (close_cb != NULL)
        gtk_widget_set_margin_left(GTK_WIDGET(btn_defaults), 10);

    GtkWidget *align = gtk_alignment_new(/*xalign*/.5, /*yalign*/.5, /*xscale*/1.0, /*yscale*/1.0);
    gtk_box_pack_start(GTK_BOX(buttons), align, /*expand*/TRUE, /*fill*/FALSE, /*padding*/0);

    GtkWidget *btn_apply = gtk_button_new_with_mnemonic(_("_Apply"));
    gtk_widget_set_sensitive(btn_apply, FALSE);
    gtk_box_pack_end(GTK_BOX(buttons), btn_apply, /*expand*/FALSE, /*fill*/FALSE, /*padding*/0);
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(system_config_abrt_apply_cb), config);
    g_signal_connect(config, "changed", G_CALLBACK(system_config_abrt_changed_cb), btn_apply);

    gtk_widget_show_all(buttons);

    return GTK_WIDGET(box);
}

static void
system_config_abrt_dialog_close_cb(gpointer user_data)
{
    GtkWidget *sca = GTK_WIDGET(g_object_get_data(G_OBJECT(user_data), "system-config-abrt"));
    if (system_config_abrt_check_before_close(sca))
        gtk_widget_destroy(GTK_WIDGET(user_data));
}

static void
system_config_abrt_dialog_close(GtkDialog *dialog, gpointer user_data)
{
    system_config_abrt_dialog_close_cb(dialog);
}

static gboolean
system_config_abrt_dialog_delete_event(GtkWidget *dialog, GdkEvent *event, gpointer user_data)
{
    system_config_abrt_dialog_close_cb(dialog);
    return TRUE; /*do not propagate the event*/
}

void show_system_config_abrt_dialog(GtkWindow *parent)
{
    GtkWidget *dialog = gtk_dialog_new();

    gtk_window_set_title(GTK_WINDOW(dialog), _("Problem Reporting Configuration"));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 300);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    if (parent != NULL)
    {
        gtk_window_set_icon_name(GTK_WINDOW(dialog), gtk_window_get_icon_name(parent));
        gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
        gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    }

    /* Have to handle these signals on our own otherwise users must press close button twice */
    g_signal_connect(dialog, "close", G_CALLBACK(system_config_abrt_dialog_close), /*user_data*/NULL);
    g_signal_connect(dialog, "delete-event", G_CALLBACK(system_config_abrt_dialog_delete_event), /*user_data*/NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *sca = system_config_abrt_widget_new_with_close_button(system_config_abrt_dialog_close_cb, dialog);
    g_object_set_data(G_OBJECT(dialog), "system-config-abrt", sca);
    gtk_box_pack_start(GTK_BOX(content), sca, /*expand*/TRUE, /*fill*/TRUE, /*padding*/0);

    gtk_widget_show_all(content);

    gtk_dialog_run(GTK_DIALOG(dialog));
}
