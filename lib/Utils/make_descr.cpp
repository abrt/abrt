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
#include "CrashTypes.h"
#include "DebugDump.h" /* FILENAME_ARCHITECTURE etc */
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

static void add_content(bool &was_multiline, string& description, const char *header, const char *content)
{
    /* We separate multiline contents with emply line */
    if (was_multiline)
        description += '\n';

    while (content[0] == '\n')
        content++;

    if (strchr(content, '\n') == NULL)
    {
        if (skip_whitespace(content)[0] == '\0')
        {
            /* empty, dont report at all */
            return;
        }
        /* one string value, like OS release */
        description += header;
        description += ": ";
        description += content;
        description += '\n';
        was_multiline = 0;
    }
    else
    {
        /* multi-string value, like backtrace */
        if (!was_multiline && description.size() != 0) /* if wasn't yet separated */
            description += '\n'; /* do it now */
        description += header;
        description += "\n-----\n";
        description += content;
        if (content[strlen(content) - 1] != '\n')
            description += '\n';
        was_multiline = 1;
    }
}

/* Items we don't want to include */
static const char *const blacklisted_items[] = {
    FILENAME_ANALYZER ,
    FILENAME_COREDUMP ,
    FILENAME_DESCRIPTION, /* package description - basically useless */
    FILENAME_HOSTNAME ,
    CD_UUID           ,
    CD_INFORMALL      ,
    CD_DUPHASH        ,
    CD_DUMPDIR        ,
    CD_COUNT          ,
    CD_REPORTED       ,
    CD_MESSAGE        ,
    NULL
};

string make_description_bz(const map_crash_data_t& pCrashData)
{
    string description;
    string long_description;

    map_crash_data_t::const_iterator it = pCrashData.begin();
    for (; it != pCrashData.end(); it++)
    {
        const string& itemname = it->first;
        const string& type = it->second[CD_TYPE];
        const string& content = it->second[CD_CONTENT];
        if (type == CD_TXT)
        {
            /* Skip items we are not interested in */
            const char *const *bl = blacklisted_items;
            while (*bl)
            {
                if (itemname == *bl)
                    break;
                bl++;
            }
            if (*bl)
                continue; /* blacklisted */
            if (content == "1.\n2.\n3.\n")
                continue; /* user did not change default "How to reproduce" */

            if (content.size() <= CD_TEXT_ATT_SIZE)
            {
                /* Add small (less than few kb) text items inline */
                bool was_multiline = 0;
                string tmp;
                add_content(was_multiline,
                        tmp,
                        /* "reproduce: blah" looks ugly, fixing: */
                        itemname == FILENAME_REPRODUCE ? "How to reproduce" : itemname.c_str(),
                        content.c_str()
                );

                if (was_multiline)
                {
                    /* Not one-liner */
                    if (long_description.size() != 0)
                        long_description += '\n';
                    long_description += tmp;
                }
                else
                {
                    description += tmp;
                }
            } else {
                bool was_multiline = 0;
                add_content(was_multiline, description, "Attached file", itemname.c_str());
            }
        }
    }

    /* One-liners go first, then multi-line items */
    if (description.size() != 0 && long_description.size() != 0)
    {
        description += '\n';
    }
    description += long_description;

    return description;
}

string make_description_logger(const map_crash_data_t& pCrashData)
{
    string description;
    string long_description;

    map_crash_data_t::const_iterator it = pCrashData.begin();
    for (; it != pCrashData.end(); it++)
    {
        const string &filename = it->first;
        const string &type = it->second[CD_TYPE];
        const string &content = it->second[CD_CONTENT];
        if (type == CD_TXT
         || type == CD_BIN
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
            if (content == "1.\n2.\n3.\n")
                continue; /* user did not change default "How to reproduce" */

            bool was_multiline = 0;
            string tmp;
            add_content(was_multiline, tmp, filename.c_str(), content.c_str());

            if (was_multiline)
            {
                if (long_description.size() != 0)
                    long_description += '\n';
                long_description += tmp;
            }
            else
            {
                description += tmp;
            }
        }
    }

    if (description.size() != 0 && long_description.size() != 0)
    {
        description += '\n';
    }
    description += long_description;

    return description;
}

string make_description_reproduce_comment(const map_crash_data_t& pCrashData)
{
    map_crash_data_t::const_iterator end = pCrashData.end();
    map_crash_data_t::const_iterator it;

    string howToReproduce;
    it = pCrashData.find(FILENAME_REPRODUCE);
    if (it != end)
    {
        if ((it->second[CD_CONTENT].size() > 0)
            &&  (it->second[CD_CONTENT] != "1.\n2.\n3.\n"))
        {
            howToReproduce = "\n\nHow to reproduce\n"
                             "-----\n";
            howToReproduce += it->second[CD_CONTENT];
        }
    }
    string comment;
    it = pCrashData.find(FILENAME_COMMENT);
    if (it != end)
    {
        if (it->second[CD_CONTENT].size() > 0)
        {
            comment = "\n\nComment\n"
                     "-----\n";
            comment += it->second[CD_CONTENT];
        }
    }
    return howToReproduce + comment;
}

/* This needs more work to make the result less ugly */
string make_description_catcut(const map_crash_data_t& pCrashData)
{
    map_crash_data_t::const_iterator end = pCrashData.end();
    map_crash_data_t::const_iterator it;

    string howToReproduce;
    it = pCrashData.find(FILENAME_REPRODUCE);
    if (it != end)
    {
        howToReproduce = "\n\nHow to reproduce\n"
                         "-----\n";
        howToReproduce += it->second[CD_CONTENT];
    }
    string comment;
    it = pCrashData.find(FILENAME_COMMENT);
    if (it != end)
    {
        comment = "\n\nComment\n"
                 "-----\n";
        comment += it->second[CD_CONTENT];
    }

    string pDescription = "\nabrt "VERSION" detected a crash.\n";
    pDescription += howToReproduce;
    pDescription += comment;
    pDescription += "\n\nAdditional information\n"
                    "======\n";

    for (it = pCrashData.begin(); it != end; it++)
    {
        const string &filename = it->first;
        const string &type = it->second[CD_TYPE];
        const string &content = it->second[CD_CONTENT];
        if (type == CD_TXT)
        {
            if (content.length() <= CD_TEXT_ATT_SIZE)
            {
                if (filename != CD_DUPHASH
                 && filename != FILENAME_ARCHITECTURE
                 && filename != FILENAME_RELEASE
                 && filename != FILENAME_REPRODUCE
                 && filename != FILENAME_COMMENT
                ) {
                    pDescription += '\n';
                    pDescription += filename;
                    pDescription += "\n-----\n";
                    pDescription += content;
                    pDescription += "\n\n";
                }
            } else {
                pDescription += "\n\nAttached files\n"
                                "----\n";
                pDescription += filename;
                pDescription += '\n';
            }
        }
        else if (type == CD_BIN)
        {
            error_msg(_("Binary file %s will not be reported"), filename.c_str());
        }
    }

    return pDescription;
}
