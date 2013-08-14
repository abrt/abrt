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
#ifndef _SYSTEM_CONFIG_ABRT_H
#define _SYSTEM_CONFIG_ABRT_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Creates a new System Config ABRT widget without close button
 *
 * @returns the new System Config ABRT widget
 */
GtkWidget *system_config_abrt_widget_new(void);

/*
 * A callback function for Close button on System Config ABRT widget
 *
 * @param user_data user data set when System Config ABRT was created
 */
typedef void (* system_config_abrt_widget_close_callback)(gpointer user_data);

/*
 * Creates a new System Config ABRT widget with close button
 *
 * @param close_cb Close button click handler
 * @param user_data User data passed to @close_cb as the first argument
 */
GtkWidget *system_config_abrt_widget_new_with_close_button(system_config_abrt_widget_close_callback close_cb, gpointer user_data);

/*
 * If changes were not applied, asks user for action via dialog.
 *
 * @returns TRUE if application can exit, otherwise FALSE
 */
gboolean system_config_abrt_check_before_close(GtkWidget *config);

/*
 * Shows the System Config ABRT dialog
 *
 * @param parent A window for which the dialog is modal
 */
void show_system_config_abrt_dialog(GtkWindow *parent);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SYSTEM_CONFIG_ABRT_H */
