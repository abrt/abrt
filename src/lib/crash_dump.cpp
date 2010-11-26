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
#include "abrtlib.h"
#include "abrt_crash_dump.h"

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
		vector_string_t& v = pCrashData[pItem]; /* create empty vector */
		v.push_back(pType);
		v.push_back(pEditable);
		v.push_back(pContent);
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

static char* is_text_file(const char *name, ssize_t *sz)
{
    /* We were using magic.h API to check for file being text, but it thinks
     * that file containing just "0" is not text (!!)
     * So, we do it ourself.
     */

    int fd = open(name, O_RDONLY);
    if (fd < 0)
        return NULL; /* it's not text (because it does not exist! :) */

    /* Maybe 64k limit is small. But _some_ limit is necessary:
     * fields declared "text" may end up in editing fields and such.
     * We don't want to accidentally end up with 100meg text in a textbox!
     * So, don't remove this. If you really need to, raise the limit.
     */
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0 || size > 64*1024)
    {
        close(fd);
        return NULL; /* it's not a SMALL text */
    }
    lseek(fd, 0, SEEK_SET);

    char *buf = (char*)xmalloc(*sz);
    ssize_t r = *sz = full_read(fd, buf, *sz);
    close(fd);
    if (r < 0)
    {
        free(buf);
        return NULL; /* it's not text (because we can't read it) */
    }

    /* Some files in our dump directories are known to always be textual */
    const char *base = strrchr(name, '/');
    if (base)
    {
        base++;
        if (strcmp(base, FILENAME_BACKTRACE) == 0
         || strcmp(base, FILENAME_CMDLINE) == 0
        ) {
            return buf;
        }
    }

    /* Every once in a while, even a text file contains a few garbled
     * or unexpected non-ASCII chars. We should not declare it "binary".
     */
    const unsigned RATIO = 50;
    unsigned total_chars = r + RATIO;
    unsigned bad_chars = 1; /* 1 prevents division by 0 later */
    while (--r >= 0)
    {
        if (buf[r] >= 0x7f
         /* among control chars, only '\t','\n' etc are allowed */
         || (buf[r] < ' ' && !isspace(buf[r]))
        ) {
            if (buf[r] == '\0')
            {
                /* We don't like NULs very much. Not text for sure! */
                free(buf);
                return NULL;
            }
            bad_chars++;
        }
    }

    if ((total_chars / bad_chars) >= RATIO)
        return buf; /* looks like text to me */

    free(buf);
    return NULL; /* it's binary */
}

void load_crash_data_from_crash_dump_dir(struct dump_dir *dd, map_crash_data_t& data)
{
    char *short_name;
    char *full_name;

    dd_init_next_file(dd);
    while (dd_get_next_file(dd, &short_name, &full_name))
    {
        ssize_t sz = 4*1024;
        char *text = NULL;
        bool editable = is_editable_file(short_name);

        if (!editable)
        {
            text = is_text_file(full_name, &sz);
            if (!text)
            {
                add_to_crash_data_ext(data,
                        short_name,
                        CD_BIN,
                        CD_ISNOTEDITABLE,
                        full_name
                );

                free(short_name);
                free(full_name);
                continue;
            }
        }

        char *content;
        if (sz < 4*1024) /* is_text_file did read entire file */
            content = xstrndup(text, sz); //TODO: can avoid this copying if is_text_file() adds NUL
        else /* no, need to read it all */
            content = dd_load_text(dd, short_name);
        free(text);

        add_to_crash_data_ext(data,
                short_name,
                CD_TXT,
                editable ? CD_ISEDITABLE : CD_ISNOTEDITABLE,
                content
        );
        free(short_name);
        free(full_name);
        free(content);
    }
}

static const std::string* helper_get_crash_data_item_content(const map_crash_data_t& crash_data, const char *key)
{
	map_crash_data_t::const_iterator it = crash_data.find(key);
	if (it == crash_data.end()) {
		return NULL;
	}
	if (it->second.size() <= CD_CONTENT) {
		return NULL;
	}
	return &it->second[CD_CONTENT];
}

const std::string& get_crash_data_item_content(const map_crash_data_t& crash_data, const char *key)
{
	const std::string* sp = helper_get_crash_data_item_content(crash_data, key);
	if (sp == NULL) {
		if (crash_data.find(key) == crash_data.end())
			error_msg_and_die("Error accessing crash data: no ['%s']", key);
		error_msg_and_die("Error accessing crash data: no ['%s'][%d]", key, CD_CONTENT);
	}
	return *sp;
}

const char *get_crash_data_item_content_or_NULL(const map_crash_data_t& crash_data, const char *key)
{
	const std::string* sp = helper_get_crash_data_item_content(crash_data, key);
	if (!sp) {
		return NULL;
	}
	return sp->c_str();
}

void log_map_crash_data(const map_crash_data_t& data, const char *name)
{
	map_crash_data_t::const_iterator it = data.begin();
	while (it != data.end())
	{
		ssize_t sz = it->second.size();
		log("%s[%s]:%s/%s/'%.20s'",
			name, it->first.c_str(),
			sz > 0 ? it->second[0].c_str() : "<NO [0]>",
			sz > 1 ? it->second[1].c_str() : "<NO [1]>",
			sz > 2 ? it->second[2].c_str() : "<NO [2]>"
		);
		it++;
	}
}
