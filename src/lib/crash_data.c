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
#include "abrt_crash_data.h"

static void free_crash_item(void *ptr)
{
    if (ptr)
    {
        struct crash_item *item = (struct crash_item *)ptr;
        free(item->content);
        free(item);
    }
}


/* crash_data["name"] = { "content", CD_FLAG_foo_bits } */

crash_data_t *new_crash_data(void)
{
    return g_hash_table_new_full(g_str_hash, g_str_equal,
                 free, free_crash_item);
}

void add_to_crash_data_ext(crash_data_t *crash_data,
                const char *name,
                const char *content,
                unsigned flags)
{
    if (!(flags & (CD_FLAG_SYS|CD_FLAG_BIN|CD_FLAG_TXT)))
        flags |= CD_FLAG_TXT;
    if (!(flags & (CD_FLAG_ISEDITABLE|CD_FLAG_ISNOTEDITABLE)))
        flags |= CD_FLAG_ISNOTEDITABLE;

    struct crash_item *item = (struct crash_item *)xzalloc(sizeof(*item));
    item->content = xstrdup(content);
    item->flags = flags;
    g_hash_table_replace(crash_data, xstrdup(name), item);
}

void add_to_crash_data(crash_data_t *crash_data,
                const char *name,
                const char *content)
{
    add_to_crash_data_ext(crash_data, name, content, CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);
}

const char *get_crash_item_content_or_die(crash_data_t *crash_data, const char *key)
{
    struct crash_item *item = get_crash_data_item_or_NULL(crash_data, key);
    if (!item)
        error_msg_and_die("Error accessing crash data: no ['%s']", key);
    return item->content;
}

const char *get_crash_item_content_or_NULL(crash_data_t *crash_data, const char *key)
{
    struct crash_item *item = get_crash_data_item_or_NULL(crash_data, key);
    if (!item)
        return NULL;
    return item->content;
}


/* crash_data_vector[i] = { "name" = { "content", CD_FLAG_foo_bits } } */

vector_of_crash_data_t *new_vector_of_crash_data(void)
{
    return g_ptr_array_new_with_free_func((void (*)(void*)) &free_crash_data);
}


/* Miscellaneous helpers */

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

crash_data_t *load_crash_data_from_dump_dir(struct dump_dir *dd)
{
    char *short_name;
    char *full_name;
    crash_data_t *crash_data = new_crash_data();

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
                add_to_crash_data_ext(crash_data,
                        short_name,
                        full_name,
                        CD_FLAG_BIN + CD_FLAG_ISNOTEDITABLE
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

        add_to_crash_data_ext(crash_data,
                short_name,
                content,
                (editable ? CD_FLAG_TXT + CD_FLAG_ISEDITABLE : CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE)
        );
        free(short_name);
        free(full_name);
        free(content);
    }
    return crash_data;
}

void log_crash_data(crash_data_t *crash_data, const char *pfx)
{
    GHashTableIter iter;
    char *name;
    struct crash_item *value;
    g_hash_table_iter_init(&iter, crash_data);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        log("%s[%s]:'%s' 0x%x",
                pfx, name,
                value->content,
                value->flags
        );
    }
}
