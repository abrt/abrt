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
#ifndef CRASHTYPES_H_
#define CRASHTYPES_H_

#include "abrt_types.h"

// SYS - system value, should not be displayed
// BIN - binary data
// TXT - text data, can be displayed
#define CD_SYS          "s"
#define CD_BIN          "b"
#define CD_TXT          "t"

/* Text bigger than this usually is attached, not added inline */
#define CD_TEXT_ATT_SIZE (2*1024)

#define CD_ISEDITABLE       "y"
#define CD_ISNOTEDITABLE    "n"

#define CD_TYPE         (0)
#define CD_EDITABLE     (1)
#define CD_CONTENT      (2)

#define CD_UUID         "UUID"
#define CD_UID          "UID"
#define CD_COUNT        "Count"
#define CD_EXECUTABLE   "Executable"
#define CD_PACKAGE      "Package"
#define CD_DESCRIPTION  "Description"
#define CD_TIME         "Time"
#define CD_REPORTED     "Reported"
#define CD_MESSAGE      "Message"
#define CD_COMMENT      "Comment"
#define CD_REPRODUCE    "How to reproduce"
#define CD_MWANALYZER   "_MWAnalyzer"
#define CD_MWUID        "_MWUID"
#define CD_MWUUID       "_MWUUID"
#define CD_MWDDD        "_MWDDD"

// currently, vector always has exactly 3 elements -> <type, editable, content>
// <key, data>
typedef map_vector_string_t map_crash_data_t;

typedef std::vector<map_crash_data_t> vector_map_crash_data_t;

void add_to_crash_data(map_crash_data_t& pCrashData,
		const char *pItem,
		const char *pContent);

void add_to_crash_data_ext(map_crash_data_t& pCrashData,
		const char *pItem,
		const char *pType,
		const char *pEditable,
		const char *pContent);

const std::string& get_crash_data_item_content(const map_crash_data_t& crash_data,
		const char *key);

#endif
