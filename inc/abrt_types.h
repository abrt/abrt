/*
    Copyright (C) 2009  Denys Vlasenko (dvlasenk@redhat.com)
    Copyright (C) 2009  RedHat inc.

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

#ifndef ABRT_TYPES_H_
#define ABRT_TYPES_H_

#include <map>
#include <set>
#include <vector>
#include <string>

typedef std::vector<std::string> vector_string_t;
typedef std::set<std::string> set_string_t;
typedef std::pair<std::string, std::string> pair_string_string_t;
typedef std::map<std::string, std::string> map_string_t;

typedef std::vector<pair_string_string_t> vector_pair_string_string_t;
typedef std::vector<map_string_t> vector_map_string_t;
typedef std::map<std::string, map_string_t> map_map_string_t;
typedef std::map<std::string, vector_string_t> map_vector_string_t;
typedef std::map<std::string, vector_pair_string_string_t> map_vector_pair_string_string_t;

/* Report() method return type */
typedef map_vector_string_t report_status_t;
/* map_vector_string_t's vector element meaning: */
#define REPORT_STATUS_IDX_FLAG 0
#define REPORT_STATUS_IDX_MSG  1
/* Holds result of .conf file section parsing: map["name"] = "value" */
typedef map_string_t map_plugin_settings_t;

#endif
