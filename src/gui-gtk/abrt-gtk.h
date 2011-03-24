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
GtkWidget *create_main_window(void);
void add_directory_to_dirlist(const char *dirname);
/* May return NULL */
GtkTreePath *get_cursor(void);
void sanitize_cursor(GtkTreePath *preferred_path);
void rescan_dirs_and_add_to_dirlist(void);

void scan_dirs_and_add_to_dirlist(void);
