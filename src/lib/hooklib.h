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
#ifndef HOOKLIB_H
#define HOOKLIB_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define check_free_space abrt_check_free_space
void check_free_space(unsigned setting_MaxCrashReportsSize);

#define trim_debug_dumps abrt_trim_debug_dumps
void trim_debug_dumps(const char *dirname, double cap_size, const char *exclude_path);

#ifdef __cplusplus
}
#endif

#endif
