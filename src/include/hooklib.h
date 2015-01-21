/*
    Copyright (C) 2009 RedHat inc.

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

/** @file hooklib.h */

/**
 @brief Saves the problem data

 Creates the problem_dir in the system problem directory where it's
 picked by ABRT

 @param[in] pd Filled problem data structure to be saved
 @return Unique identifier for the saved problem (usually full path to the
 stored data, but it's not guaranteed)
 */
char *problem_data_save(problem_data_t *pd);

#define  DUMP_SUID_UNSAFE 1
#define  DUMP_SUID_SAFE 2

int dump_suid_policy();
int signal_is_fatal(int signal_no, const char **name);
