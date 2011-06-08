/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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

#include "libreport.h"
#include "report.h"

void make_label_autowrap_on_resize(GtkLabel *label);
void fix_all_wrapped_labels(GtkWidget *widget);

void show_events_list_dialog(GtkWindow *parent);

void abrt_keyring_save_settings(const char *event_name);
void load_event_config_data_from_keyring();
void g_validate_event(const char* event_name);
extern GtkWindow *g_parent_window;
