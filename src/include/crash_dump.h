/*
    Copyright (C) 2009  Abrt team.
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
#ifndef CRASH_DUMP_H_
#define CRASH_DUMP_H_


// Crash data is a map of 3-element vectors of strings: type, editable, content
#define CD_TYPE         0
#define CD_EDITABLE     1
#define CD_CONTENT      2

// SYS - system value, should not be displayed
// BIN - binary data
// TXT - text data, can be displayed
#define CD_SYS          "s"
#define CD_BIN          "b"
#define CD_TXT          "t"

#define CD_ISEDITABLE    "y"
#define CD_ISNOTEDITABLE "n"

struct dump_dir;

#ifdef __cplusplus

#include <map>
#include <vector>
#include <string>

/* In-memory crash data structure and accessors */

typedef std::map<std::string, std::vector<std::string> > map_crash_data_t;

void add_to_crash_data_ext(map_crash_data_t& pCrashData,
                const char *pItem,
                const char *pType,
                const char *pEditable,
                const char *pContent);
/* Uses type:CD_TXT, editable:CD_ISNOTEDITABLE */
void add_to_crash_data(map_crash_data_t& pCrashData,
                const char *pItem,
                const char *pContent);

const char        *get_crash_data_item_content_or_NULL(const map_crash_data_t& crash_data, const char *key);
/* Aborts if key is not found: */
const std::string& get_crash_data_item_content(const map_crash_data_t& crash_data, const char *key);


/* Conversions between in-memory and on-disk formats */

void load_crash_data_from_crash_dump_dir(struct dump_dir *dd, map_crash_data_t& data);
struct dump_dir *create_crash_dump_dir(const map_crash_data_t& crash_data);

#endif

#endif
