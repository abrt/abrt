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

static void free_problem_item(void *ptr)
{
    if (ptr)
    {
        struct problem_item *item = (struct problem_item *)ptr;
        free(item->content);
        free(item);
    }
}

char *format_problem_item(struct problem_item *item)
{
    if (!item)
        return xstrdup("(nullitem)");

    if (item->flags & CD_FLAG_UNIXTIME)
    {
        errno = 0;
        char *end;
        time_t time = strtol(item->content, &end, 10);
        if (!errno && !*end && end != item->content)
        {
            char timeloc[256];
            int success = strftime(timeloc, sizeof(timeloc), "%c", localtime(&time));
            if (success)
                return xstrdup(timeloc);
        }
    }
    return NULL;
}

/* problem_data["name"] = { "content", CD_FLAG_foo_bits } */

problem_data_t *new_problem_data(void)
{
    return g_hash_table_new_full(g_str_hash, g_str_equal,
                 free, free_problem_item);
}

void add_to_problem_data_ext(problem_data_t *problem_data,
                const char *name,
                const char *content,
                unsigned flags)
{
    if (!(flags & CD_FLAG_BIN))
        flags |= CD_FLAG_TXT;
    if (!(flags & CD_FLAG_ISEDITABLE))
        flags |= CD_FLAG_ISNOTEDITABLE;

    struct problem_item *item = (struct problem_item *)xzalloc(sizeof(*item));
    item->content = xstrdup(content);
    item->flags = flags;
    g_hash_table_replace(problem_data, xstrdup(name), item);
}

void add_to_problem_data(problem_data_t *problem_data,
                const char *name,
                const char *content)
{
    add_to_problem_data_ext(problem_data, name, content, CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);
}

const char *get_problem_item_content_or_die(problem_data_t *problem_data, const char *key)
{
    struct problem_item *item = get_problem_data_item_or_NULL(problem_data, key);
    if (!item)
        error_msg_and_die("Error accessing problem data: no ['%s']", key);
    return item->content;
}

const char *get_problem_item_content_or_NULL(problem_data_t *problem_data, const char *key)
{
    struct problem_item *item = get_problem_data_item_or_NULL(problem_data, key);
    if (!item)
        return NULL;
    return item->content;
}


/* problem_data_vector[i] = { "name" = { "content", CD_FLAG_foo_bits } } */

vector_of_problem_data_t *new_vector_of_problem_data(void)
{
    return g_ptr_array_new_with_free_func((void (*)(void*)) &free_problem_data);
}


/* Miscellaneous helpers */

static bool is_in_list(const char *name, const char *const *v)
{
    while (*v)
    {
        if (strcmp(*v, name) == 0)
            return true;
        v++;
    }
    return false;
}

static const char *const editable_files[] = {
    FILENAME_COMMENT  ,
    FILENAME_BACKTRACE,
    NULL
};
static bool is_editable_file(const char *file_name)
{
    return is_in_list(file_name, editable_files);
}

static const char *const always_text_files[] = {
    FILENAME_CMDLINE  ,
    FILENAME_BACKTRACE,
    NULL
};
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
     *
     * Bumped up to 200k: saw 124740 byte /proc/PID/smaps file
     */
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0 || size > 200*1024)
    {
        close(fd);
        return NULL; /* it's not a SMALL text */
    }
    lseek(fd, 0, SEEK_SET);

    char *buf = (char*)xmalloc(*sz);
    ssize_t r = full_read(fd, buf, *sz);
    close(fd);
    if (r < 0)
    {
        free(buf);
        return NULL; /* it's not text (because we can't read it) */
    }
    if (r < *sz)
        buf[r] = '\0';
    *sz = r;

    /* Some files in our dump directories are known to always be textual */
    const char *base = strrchr(name, '/');
    if (base)
    {
        base++;
        if (is_in_list(base, always_text_files))
            return buf;
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

void load_problem_data_from_dump_dir(problem_data_t *problem_data, struct dump_dir *dd)
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
                add_to_problem_data_ext(problem_data,
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
        if (sz < 4*1024) /* did is_text_file read entire file? */
        {
            content = text;
            /* Strip '\n' from one-line elements: */
            char *nl = strchr(content, '\n');
            if (nl && nl[1] == '\0')
                *nl = '\0';
        }
        else
        {
            /* no, need to read it all */
            free(text);
            content = dd_load_text(dd, short_name);
        }

        int flags = 0;

        if (editable)
            flags |= CD_FLAG_TXT | CD_FLAG_ISEDITABLE;
        else
            flags |= CD_FLAG_TXT | CD_FLAG_ISNOTEDITABLE;

        static const char *const list_files[] = {
            FILENAME_UID       ,
            FILENAME_PACKAGE   ,
            FILENAME_EXECUTABLE,
            FILENAME_TIME      ,
            FILENAME_COUNT     ,
            NULL
        };
        if (is_in_list(short_name, list_files))
            flags |= CD_FLAG_LIST;

        if (strcmp(short_name, FILENAME_TIME) == 0)
            flags |= CD_FLAG_UNIXTIME;

        add_to_problem_data_ext(problem_data,
                short_name,
                content,
                flags
        );
        free(short_name);
        free(full_name);
        free(content);
    }
}

problem_data_t *create_problem_data_from_dump_dir(struct dump_dir *dd)
{
    problem_data_t *problem_data = new_problem_data();
    load_problem_data_from_dump_dir(problem_data, dd);
    return problem_data;
}

void log_problem_data(problem_data_t *problem_data, const char *pfx)
{
    GHashTableIter iter;
    char *name;
    struct problem_item *value;
    g_hash_table_iter_init(&iter, problem_data);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        log("%s[%s]:'%s' 0x%x",
                pfx, name,
                value->content,
                value->flags
        );
    }
}
