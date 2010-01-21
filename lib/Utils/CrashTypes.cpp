/*
    Copyright (C) 2010  Denys Vlasenko (dvlasenk@redhat.com)
    Copyright (C) 2010  RedHat inc.

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
#include "abrt_types.h"
#include "abrtlib.h"
#include "CrashTypes.h"

const char *const must_have_files[] = {
	FILENAME_ARCHITECTURE,
	FILENAME_KERNEL      ,
	FILENAME_PACKAGE     ,
	FILENAME_COMPONENT   ,
	FILENAME_RELEASE     ,
	FILENAME_EXECUTABLE  ,
	NULL
};

static const char *const editable_files[] = {
	FILENAME_DESCRIPTION,
	FILENAME_COMMENT    ,
	FILENAME_REPRODUCE  ,
	FILENAME_BACKTRACE  ,
	NULL
};

static bool is_editable(const char *name, const char *const *v)
{
	while (*v) {
		if (strcmp(*v, name) == 0)
			return true;
		v++;
	}
	return false;
}

bool is_editable_file(const char *file_name)
{
	return is_editable(file_name, editable_files);
}


void add_to_crash_data_ext(map_crash_data_t& pCrashData,
                const char *pItem,
                const char *pType,
                const char *pEditable,
                const char *pContent)
{
	map_crash_data_t::iterator it = pCrashData.find(pItem);
	if (it == pCrashData.end()) {
		pCrashData[pItem].push_back(pType);
		pCrashData[pItem].push_back(pEditable);
		pCrashData[pItem].push_back(pContent);
		return;
	}
	vector_string_t& v = it->second;
	while (v.size() < 3)
		v.push_back("");
	v[CD_TYPE]     = pType;
	v[CD_EDITABLE] = pEditable;
	v[CD_CONTENT]  = pContent;
}

void add_to_crash_data(map_crash_data_t& pCrashData,
                const char *pItem,
                const char *pContent)
{
	add_to_crash_data_ext(pCrashData, pItem, CD_TXT, CD_ISNOTEDITABLE, pContent);
}

const std::string& get_crash_data_item_content(const map_crash_data_t& crash_data, const char *key)
{
	map_crash_data_t::const_iterator it = crash_data.find(key);
	if (it == crash_data.end()) {
		error_msg_and_die("Error accessing crash data: no ['%s']", key);
	}
	if (it->second.size() <= CD_CONTENT) {
		error_msg_and_die("Error accessing crash data: no ['%s'][%d]", key, CD_CONTENT);
	}
	return it->second[CD_CONTENT];
}

void log_map_crash_data(const map_crash_data_t& data, const char *name)
{
	map_crash_data_t::const_iterator itc = data.begin();
	while (itc != data.end())
	{
		log("%s[%s]:%s/%s/'%.20s'",
			name, itc->first.c_str(),
			itc->second[0].c_str(), itc->second[1].c_str(),
			itc->second[2].c_str()
		);
		itc++;
	}
}
