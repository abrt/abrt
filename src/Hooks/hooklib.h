/*
    Copyright (C) 2009	RedHat inc.

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

void parse_conf(const char *additional_conf, unsigned *setting_MaxCrashReportsSize, bool *setting_MakeCompatCore);
void check_free_space(unsigned setting_MaxCrashReportsSize);
void trim_debug_dumps(unsigned setting_MaxCrashReportsSize, const char *exclude_path);
