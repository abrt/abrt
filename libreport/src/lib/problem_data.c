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
#include "libreport.h"

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

void add_basics_to_problem_data(problem_data_t *pd)
{
    const char *analyzer = get_problem_item_content_or_NULL(pd, FILENAME_ANALYZER);
    if (analyzer == NULL)
        add_to_problem_data(pd, "analyzer", "libreport");

    /* If application didn't provide dupe hash, we generate it
     * from all components, so we at least eliminate the exact same
     * reports
     */
    if (get_problem_item_content_or_NULL(pd, FILENAME_DUPHASH) == NULL)
    {
        /* start hash */
        sha1_ctx_t sha1ctx;
        sha1_begin(&sha1ctx);

        /*
         * To avoid spurious hash differences, sort keys so that elements are
         * always processed in the same order:
         */
        GList *list = g_hash_table_get_keys(pd);
        list = g_list_sort(list, (GCompareFunc)strcmp);
        GList *l = list;
        while (l)
        {
            const char *key = l->data;
            l = l->next;
            struct problem_item *item = g_hash_table_lookup(pd, key);
            /* do not hash items which are binary (item->flags & CD_FLAG_BIN).
             * Their ->content is full file name, with path. Path is always
             * different and will make hash differ even if files are the same.
             */
            if (item->flags & CD_FLAG_BIN)
                continue;
            sha1_hash(&sha1ctx, item->content, strlen(item->content));
        }
        g_list_free(list);

        /* end hash */
        char hash_bytes[SHA1_RESULT_LEN];
        sha1_end(&sha1ctx, hash_bytes);
        char hash_str[SHA1_RESULT_LEN*2 + 1];
        bin2hex(hash_str, hash_bytes, SHA1_RESULT_LEN)[0] = '\0';

        add_to_problem_data(pd, FILENAME_DUPHASH, hash_str);
    }

    pid_t pid = getpid();
    if (pid > 0)
    {
        char buf[PATH_MAX+1];
        char *exe = xasprintf("/proc/%u/exe", pid);
        ssize_t read = readlink(exe, buf, PATH_MAX);
        if (read > 0)
        {
            buf[read] = 0;
            VERB2 log("reporting initiated from: %s", buf);
            add_to_problem_data(pd, FILENAME_EXECUTABLE, buf);
        }
        free(exe);

//#ifdef WITH_RPM
        /* FIXME: component should be taken from rpm using librpm
         * which means we need to link against it :(
         * or run rpm -qf executable ??
         */
        /* Fedora/RHEL rpm specific piece of code */
        const char *component = get_problem_item_content_or_NULL(pd, FILENAME_COMPONENT);
        //FIXME: this REALLY needs to go away, or every report will be assigned to abrt
        if (component == NULL) // application didn't specify component
            add_to_problem_data(pd, FILENAME_COMPONENT, "abrt");
//#endif
    }
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


/* Miscellaneous helpers */

static const char *const editable_files[] = {
    FILENAME_COMMENT  ,
    FILENAME_BACKTRACE,
    NULL
};
static bool is_editable_file(const char *file_name)
{
    return is_in_string_list(file_name, (char**)editable_files);
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
        if (is_in_string_list(base, (char**)always_text_files))
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

void load_problem_data_from_dump_dir(problem_data_t *problem_data, struct dump_dir *dd, char **excluding)
{
    char *short_name;
    char *full_name;

    dd_init_next_file(dd);
    while (dd_get_next_file(dd, &short_name, &full_name))
    {
        if (excluding && is_in_string_list(short_name, excluding))
        {
            //log("Excluded:'%s'", short_name);
            goto next;
        }

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
                goto next;
            }
        }

        char *content;
        if (sz < 4*1024) /* did is_text_file read entire file? */
        {
            /* yes */
            content = text;
        }
        else
        {
            /* no, need to read it all */
            free(text);
            content = dd_load_text(dd, short_name);
        }
        /* Strip '\n' from one-line elements: */
        char *nl = strchr(content, '\n');
        if (nl && nl[1] == '\0')
            *nl = '\0';

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
        if (is_in_string_list(short_name, (char**)list_files))
            flags |= CD_FLAG_LIST;

        if (strcmp(short_name, FILENAME_TIME) == 0)
            flags |= CD_FLAG_UNIXTIME;

        add_to_problem_data_ext(problem_data,
                short_name,
                content,
                flags
        );
        free(content);
 next:
        free(short_name);
        free(full_name);
    }
}

problem_data_t *create_problem_data_from_dump_dir(struct dump_dir *dd)
{
    problem_data_t *problem_data = new_problem_data();
    load_problem_data_from_dump_dir(problem_data, dd, NULL);
    return problem_data;
}

/*
 * Returns NULL-terminated char *vector[]. Result itself must be freed,
 * but do no free list elements. IOW: do free(result), but never free(result[i])!
 * If comma_separated_list is NULL or "", returns NULL.
 */
static char **build_exclude_vector(const char *comma_separated_list)
{
    char **exclude_items = NULL;
    if (comma_separated_list && comma_separated_list[0])
    {
        /* even w/o commas, we'll need two elements:
         * exclude_items[0] = "name"
         * exclude_items[1] = NULL
         */
        unsigned cnt = 2;

        const char *cp = comma_separated_list;
        while (*cp)
            if (*cp++ == ',')
                cnt++;

        /* We place the string directly after the char *vector[cnt]: */
        exclude_items = xzalloc(cnt * sizeof(exclude_items[0]) + (cp - comma_separated_list) + 1);
        char *p = strcpy((char*)&exclude_items[cnt], comma_separated_list);

        char **pp = exclude_items;
        *pp++ = p;
        while (*p)
        {
            if (*p++ == ',')
            {
                p[-1] = '\0';
                *pp++ = p;
            }
        }
    }

    return exclude_items;
}

problem_data_t *create_problem_data_for_reporting(const char *dump_dir_name)
{
    char **exclude_items = build_exclude_vector(getenv("EXCLUDE_FROM_REPORT"));
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL; /* dd_opendir already emitted error msg */
    problem_data_t *problem_data = new_problem_data();
    load_problem_data_from_dump_dir(problem_data, dd, exclude_items);
    dd_close(dd);
    free(exclude_items);
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
