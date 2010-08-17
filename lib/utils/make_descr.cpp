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
#include "crash_types.h"
#include "debug_dump.h" /* FILENAME_ARCHITECTURE etc */
#include "strbuf.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if ENABLE_NLS
# include <libintl.h>
# define _(S) gettext(S)
#else
# define _(S) (S)
#endif


using namespace std;

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
    FILENAME_DESCRIPTION, /* package description - basically useless */
    FILENAME_HOSTNAME ,
    FILENAME_GLOBAL_UUID,
    CD_UUID           ,
    CD_INFORMALL      ,
    CD_DUPHASH        ,
    CD_DUMPDIR        ,
    CD_COUNT          ,
    CD_REPORTED       ,
    CD_MESSAGE        ,
    NULL
};

char* make_dsc_mailx(const map_crash_data_t & crash_data)
{
    struct strbuf *buf_dsc = strbuf_new();
    struct strbuf *buf_additional_files = strbuf_new();
    struct strbuf *buf_duphash_file = strbuf_new();
    struct strbuf *buf_common_files = strbuf_new();

    map_crash_data_t::const_iterator it;
    for (it = crash_data.begin(); it != crash_data.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT)
        {
            const char *itemname = it->first.c_str();
            if ((strcmp(itemname, CD_DUPHASH) != 0)
             && (strcmp(itemname, FILENAME_ARCHITECTURE) != 0)
             && (strcmp(itemname, FILENAME_KERNEL) != 0)
             && (strcmp(itemname, FILENAME_PACKAGE) != 0)
            ) {
                strbuf_append_strf(buf_additional_files, "%s\n-----\n%s\n\n", itemname, it->second[CD_CONTENT].c_str());
            }
            else if (strcmp(itemname, CD_DUPHASH) == 0)
                strbuf_append_strf(buf_duphash_file, "%s\n-----\n%s\n\n", itemname, it->second[CD_CONTENT].c_str());
            else
                strbuf_append_strf(buf_common_files, "%s\n-----\n%s\n\n", itemname, it->second[CD_CONTENT].c_str());
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

char* make_description_bz(const map_crash_data_t& pCrashData)
{
    struct strbuf *buf_dsc = strbuf_new();
    struct strbuf *buf_long_dsc = strbuf_new();

    map_crash_data_t::const_iterator it = pCrashData.begin();
    for (; it != pCrashData.end(); it++)
    {
        const char *itemname = it->first.c_str();
        const char *type = it->second[CD_TYPE].c_str();
        const char *content = it->second[CD_CONTENT].c_str();
        if (strcmp(type, CD_TXT) == 0)
        {
            /* Skip items we are not interested in */
            const char *const *bl = blacklisted_items;
            while (*bl)
            {
                if (strcmp(itemname, *bl) == 0)
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
                        (strcmp(itemname, FILENAME_REPRODUCE) == 0) ? "How to reproduce" : itemname,
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
            } else {
                bool was_multiline = 0;
                char *dsc = NULL;
                add_content(&was_multiline, &dsc, "Attached file", itemname);
                strbuf_append_str(buf_dsc, dsc);
                free(dsc);
            }
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

char* make_description_logger(const map_crash_data_t& pCrashData)
{
    struct strbuf *buf_dsc = strbuf_new();
    struct strbuf *buf_long_dsc = strbuf_new();

    map_crash_data_t::const_iterator it = pCrashData.begin();
    for (; it != pCrashData.end(); it++)
    {
        const char *filename = it->first.c_str();
        const char *type = it->second[CD_TYPE].c_str();
        const char *content = it->second[CD_CONTENT].c_str();
        if ((strcmp(type, CD_TXT) == 0)
         || (strcmp(type, CD_BIN) == 0)
        ) {
            /* Skip items we are not interested in */
            const char *const *bl = blacklisted_items;
            while (*bl)
            {
                if (filename == *bl)
                    break;
                bl++;
            }
            if (*bl)
                continue; /* blacklisted */
            if (strcmp(content, "1.\n2.\n3.\n") == 0)
                continue; /* user did not change default "How to reproduce" */

            bool was_multiline = 0;
            char *tmp = NULL;
            add_content(&was_multiline, &tmp, filename, content);

            if (was_multiline)
            {
                if (buf_long_dsc->len != 0)
                    strbuf_append_char(buf_long_dsc,'\n');

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

char* make_description_reproduce_comment(const map_crash_data_t& pCrashData)
{
    char *repro = NULL;
    char *comment = NULL;

    map_crash_data_t::const_iterator end = pCrashData.end();
    map_crash_data_t::const_iterator it;

    it = pCrashData.find(FILENAME_REPRODUCE);
    if (it != end)
    {
        if ((it->second[CD_CONTENT].size() > 0)
            &&  (it->second[CD_CONTENT] != "1.\n2.\n3.\n"))
        {
            repro = xasprintf("\n\nHow to reproduce\n-----\n%s", it->second[CD_CONTENT].c_str());
        }
    }

    it = pCrashData.find(FILENAME_COMMENT);
    if (it != end)
    {
        if (it->second[CD_CONTENT].size() > 0)
            comment = xasprintf("\n\nComment\n-----\n%s", it->second[CD_CONTENT].c_str());
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
