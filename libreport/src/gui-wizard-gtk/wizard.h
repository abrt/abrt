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

#include "libreport-gtk.h"

void create_assistant(void);
void update_gui_state_from_problem_data(void);
void show_error_as_msgbox(const char *msg);


extern char *g_glade_file;
extern char *g_dump_dir_name;
extern char *g_analyze_events;
extern char *g_report_events;
extern problem_data_t *g_cd;
extern int g_report_only;
void reload_problem_data_from_dump_dir(void);
