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

string make_description_bz(const map_crash_report_t& pCrashReport)
{
    string description;

    map_crash_report_t::const_iterator it;
    map_crash_report_t::const_iterator end = pCrashReport.end();

    bool was_multiline = 0;
    it = pCrashReport.find(CD_REPRODUCE);
    if (it != end && it->second[CD_CONTENT] != "1.\n2.\n3.\n")
    {
        add_content(was_multiline, description, "How to reproduce", it->second[CD_CONTENT].c_str());
    }

    it = pCrashReport.find(CD_COMMENT);
    if (it != end)
    {
        add_content(was_multiline, description, "Comment", it->second[CD_CONTENT].c_str());
    }

    it = pCrashReport.begin();
    for (; it != end; it++)
    {
        const string &filename = it->first;
        const string &type = it->second[CD_TYPE];
        const string &content = it->second[CD_CONTENT];
        if (type == CD_TXT)
        {
            if (content.size() <= CD_TEXT_ATT_SIZE)
            {
                if (filename != CD_UUID
                 && filename != FILENAME_ARCHITECTURE
                 && filename != FILENAME_RELEASE
                 && filename != CD_REPRODUCE
                 && filename != CD_COMMENT
                ) {
                    add_content(was_multiline, description, filename.c_str(), content.c_str());
                }
            } else {
                add_content(was_multiline, description, "Attached file", filename.c_str());
            }
        }
    }

    return description;
}

string make_description_logger(const map_crash_report_t& pCrashReport)
{
    string description;
    string long_description;

    map_crash_report_t::const_iterator it = pCrashReport.begin();
    for (; it != pCrashReport.end(); it++)
    {
        const string &filename = it->first;
        const string &type = it->second[CD_TYPE];
        const string &content = it->second[CD_CONTENT];
        if (type == CD_TXT
         || type == CD_BIN
        ) {
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
        description += long_description;
    }

    return description;
}

/* This needs more work to make the result less ugly */
string make_description_catcut(const map_crash_report_t& pCrashReport)
{
    map_crash_report_t::const_iterator end = pCrashReport.end();
    map_crash_report_t::const_iterator it;

    string howToReproduce;
    it = pCrashReport.find(CD_REPRODUCE);
    if (it != end)
    {
        howToReproduce = "\n\nHow to reproduce\n"
                         "-----\n";
        howToReproduce += it->second[CD_CONTENT];
    }
    string comment;
    it = pCrashReport.find(CD_COMMENT);
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

    for (it = pCrashReport.begin(); it != end; it++)
    {
        const string &filename = it->first;
        const string &type = it->second[CD_TYPE];
        const string &content = it->second[CD_CONTENT];
        if (type == CD_TXT)
        {
            if (content.length() <= CD_TEXT_ATT_SIZE)
            {
                if (filename != CD_UUID
                 && filename != FILENAME_ARCHITECTURE
                 && filename != FILENAME_RELEASE
                 && filename != CD_REPRODUCE
                 && filename != CD_COMMENT
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
