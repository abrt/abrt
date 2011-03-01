/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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

// caller is responsible for freeing **dsc
static void add_content(bool *was_multiline, char **dsc, const char *header, const char *content)
{
    struct strbuf *buf_description = strbuf_new();
    if (*was_multiline)
        strbuf_append_char(buf_description, '\n');

    while (content[0] == '\n')
        content++;

    if (strchr(content, '\n') == NULL)
    {
        if (skip_whitespace(content)[0] == '\0')
        {
            /* empty, dont report at all */
            *dsc = strbuf_free_nobuf(buf_description);
            return;
        }
        /* one string value, like OS release */
        strbuf_append_strf(buf_description, "%s: %s\n", header, content);
        *was_multiline = 0;
    }
    else
    {
        /* multi-string value, like backtrace */
        if (!*was_multiline && (buf_description->len != 0)) /* if wasn't yet separated */
            strbuf_append_char(buf_description, '\n');

        strbuf_append_strf(buf_description, "%s\n-----\n%s", header, content);
        if (content[strlen(content) - 1] != '\n')
            strbuf_append_char(buf_description, '\n');

        *was_multiline = 1;
    }

    *dsc = strbuf_free_nobuf(buf_description);
}

/* Items we don't want to include */
static const char *const blacklisted_items[] = {
    FILENAME_ANALYZER ,
    FILENAME_COREDUMP ,
    FILENAME_HOSTNAME ,
    FILENAME_DUPHASH  ,
    FILENAME_UUID     ,
    FILENAME_INFORMALL,
    CD_DUMPDIR        ,
    FILENAME_COUNT    ,
    FILENAME_MESSAGE  ,
    NULL
};

char* make_description_mailx(crash_data_t *crash_data)
{
    struct strbuf *buf_dsc = strbuf_new();
    struct strbuf *buf_additional_files = strbuf_new();
    struct strbuf *buf_duphash_file = strbuf_new();
    struct strbuf *buf_common_files = strbuf_new();

    GHashTableIter iter;
    char *name;
    struct crash_item *value;
    g_hash_table_iter_init(&iter, crash_data);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        if (value->flags & CD_FLAG_TXT)
        {
            if ((strcmp(name, FILENAME_DUPHASH) != 0)
             && (strcmp(name, FILENAME_ARCHITECTURE) != 0)
             && (strcmp(name, FILENAME_KERNEL) != 0)
             && (strcmp(name, FILENAME_PACKAGE) != 0)
            ) {
                strbuf_append_strf(buf_additional_files, "%s\n-----\n%s\n\n", name, value->content);
            }
            else if (strcmp(name, FILENAME_DUPHASH) == 0)
                strbuf_append_strf(buf_duphash_file, "%s\n-----\n%s\n\n", name, value->content);
            else
                strbuf_append_strf(buf_common_files, "%s\n-----\n%s\n\n", name, value->content);
        }
    }

    char *common_files = strbuf_free_nobuf(buf_common_files);
    char *duphash_file = strbuf_free_nobuf(buf_duphash_file);
    char *additional_files = strbuf_free_nobuf(buf_additional_files);

    strbuf_append_strf(buf_dsc, "Duplicate check\n=====\n%s\n\n", duphash_file);
    strbuf_append_strf(buf_dsc, "Common information\n=====\n%s\n\n", common_files);
    strbuf_append_strf(buf_dsc, "Additional information\n=====\n%s\n", additional_files);

    free(common_files);
    free(duphash_file);
    free(additional_files);

    return strbuf_free_nobuf(buf_dsc);
}

char* make_description_bz(crash_data_t *crash_data)
{
    struct strbuf *buf_dsc = strbuf_new();
    struct strbuf *buf_long_dsc = strbuf_new();

    GHashTableIter iter;
    char *name;
    struct crash_item *value;
    g_hash_table_iter_init(&iter, crash_data);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        struct stat statbuf;
        unsigned flags = value->flags;
        const char *content = value->content;
        if (flags & CD_FLAG_TXT)
        {
            /* Skip items we are not interested in */
            const char *const *bl = blacklisted_items;
            while (*bl)
            {
                if (strcmp(name, *bl) == 0)
                    break;
                bl++;
            }
            if (*bl)
                continue; /* blacklisted */
            if (strcmp(content, "1.\n2.\n3.\n") == 0)
                continue; /* user did not change default "How to reproduce" */

            if (strlen(content) <= CD_TEXT_ATT_SIZE)
            {
                /* Add small (less than few kb) text items inline */
                bool was_multiline = 0;
                char *tmp = NULL;
                add_content(&was_multiline,
                        &tmp,
			/* "reproduce: blah" looks ugly, fixing: */
                        (strcmp(name, FILENAME_REPRODUCE) == 0) ? "How to reproduce" : name,
                        content
                );

                if (was_multiline)
                {
                    /* Not one-liner */
                    if (buf_long_dsc->len != 0)
                        strbuf_append_char(buf_long_dsc, '\n');

                    strbuf_append_str(buf_long_dsc, tmp);
                }
                else
                    strbuf_append_str(buf_dsc, tmp);

                free(tmp);
            }
            else
            {
                statbuf.st_size = strlen(content);
                goto add_attachment_info;
            }
        }
        if (flags & CD_FLAG_BIN)
        {
            /* In many cases, it is useful to know how big binary files are
             * (for example, helps with diagnosing bug upload problems)
             */
            if (stat(content, &statbuf) != 0)
                statbuf.st_size = (off_t) -1;

 add_attachment_info: ;
            char *descr;
            if (statbuf.st_size >= 0)
                descr = xasprintf("%s, %llu bytes", name, (long long)statbuf.st_size);
            else
                descr = xstrdup(name);
            bool was_multiline = 0;
            char *tmp = NULL;
            add_content(&was_multiline, &tmp, "Attached file", descr);
            free(descr);
            strbuf_append_str(buf_dsc, tmp);
            free(tmp);
        }
    }

    /* One-liners go first, then multi-line items */
    if (buf_dsc->len != 0 && buf_long_dsc->len != 0)
        strbuf_append_char(buf_dsc, '\n');


    char *long_dsc = strbuf_free_nobuf(buf_long_dsc);
    strbuf_append_str(buf_dsc, long_dsc);
    free(long_dsc);

    return strbuf_free_nobuf(buf_dsc);
}

char* make_description_logger(crash_data_t *crash_data)
{
    struct strbuf *buf_dsc = strbuf_new();
    struct strbuf *buf_long_dsc = strbuf_new();

    GHashTableIter iter;
    char *name;
    struct crash_item *value;
    g_hash_table_iter_init(&iter, crash_data);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        const char *content = value->content;
        if (value->flags & (CD_FLAG_TXT|CD_FLAG_BIN))
        {
            /* Skip items we are not interested in */
            const char *const *bl = blacklisted_items;
            while (*bl)
            {
                if (name == *bl)
                    break;
                bl++;
            }
            if (*bl)
                continue; /* blacklisted */
            if (strcmp(content, "1.\n2.\n3.\n") == 0)
                continue; /* user did not change default "How to reproduce" */

            bool was_multiline = 0;
            char *tmp = NULL;
            add_content(&was_multiline, &tmp, name, content);

            if (was_multiline)
            {
                if (buf_long_dsc->len != 0)
                    strbuf_append_char(buf_long_dsc, '\n');

                strbuf_append_str(buf_long_dsc, tmp);
            }
            else
                strbuf_append_str(buf_dsc, tmp);
        }
    }

    if (buf_dsc->len != 0 && buf_long_dsc->len != 0)
        strbuf_append_char(buf_dsc, '\n');

    char *long_dsc = strbuf_free_nobuf(buf_long_dsc);
    strbuf_append_str(buf_dsc, long_dsc);
    free(long_dsc);

    return strbuf_free_nobuf(buf_dsc);
}

char* make_description_reproduce_comment(crash_data_t *crash_data)
{
    char *repro = NULL;
    char *comment = NULL;
    struct crash_item *value;

    value = get_crash_data_item_or_NULL(crash_data, FILENAME_REPRODUCE);
    if (value)
    {
        if (value->content[0]
         && strcmp(value->content, "1.\n2.\n3.\n") != 0
        ) {
            repro = xasprintf("\n\nHow to reproduce\n-----\n%s", value->content);
        }
    }

    value = get_crash_data_item_or_NULL(crash_data, FILENAME_COMMENT);
    if (value)
    {
        if (value->content[0])
            comment = xasprintf("\n\nComment\n-----\n%s", value->content);
    }

    if (!repro && !comment)
        return NULL;

    struct strbuf *buf_dsc = strbuf_new();

    if (repro)
        strbuf_append_str(buf_dsc, repro);

    if (comment)
        strbuf_append_str(buf_dsc, comment);

    free(repro);
    free(comment);

    return strbuf_free_nobuf(buf_dsc);
}
