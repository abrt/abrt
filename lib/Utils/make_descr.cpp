#include "abrtlib.h"
//#include "abrt_types.h"
#include "CrashTypes.h"
#include "DebugDump.h" /* FILENAME_ARCHITECTURE etc */

using namespace std;

static void add_content(bool &was_multiline, string& description, const char *header, const char *content)
{
    /* We separate multiline contents with emply line */
    if (was_multiline)
        description += '\n';

    description += header;

    while (content[0] == '\n')
        content++;

    if (strchr(content, '\n') == NULL)
    {
        /* one string value, like OS release */
        description += ": ";
        description += content;
        description += '\n';
        was_multiline = 0;
    }
    else
    {
        /* multi-string value, like backtrace */
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
            if (filename != CD_UUID
             && filename != FILENAME_ARCHITECTURE
             && filename != FILENAME_RELEASE
             && filename != CD_REPRODUCE
             && filename != CD_COMMENT
	    ) {
                add_content(was_multiline, description, filename.c_str(), content.c_str());
            }
        }
        else if (type == CD_ATT)
        {
            add_content(was_multiline, description, "Attached file", filename.c_str());
        }
        //else if (type == CD_BIN)
        //{
        //    string msg = ssprintf(_("Binary file %s is not reported"), filename.c_str());
        //    warn_client(msg);
        //}
    }

    return description;
}

string make_description_logger(const map_crash_report_t& pCrashReport)
{
//    string description;
    stringstream binaryFiles, commonFiles, bigTextFiles, additionalFiles, UUIDFile;

    map_crash_report_t::const_iterator it = pCrashReport.begin();
    for (; it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT)
        {
            if (it->first != CD_UUID
             && it->first != FILENAME_ARCHITECTURE
             && it->first != FILENAME_KERNEL
             && it->first != FILENAME_PACKAGE
            ) {
                additionalFiles << it->first << std::endl;
                additionalFiles << "-----" << std::endl;
                additionalFiles << it->second[CD_CONTENT] << std::endl << std::endl;
            }
            else if (it->first == CD_UUID)
            {
                UUIDFile << it->first << std::endl;
                UUIDFile << "-----" << std::endl;
                UUIDFile << it->second[CD_CONTENT] << std::endl << std::endl;
            }
            else
            {
                commonFiles << it->first << std::endl;
                commonFiles << "-----" << std::endl;
                commonFiles << it->second[CD_CONTENT] << std::endl << std::endl;
            }
        }
        if (it->second[CD_TYPE] == CD_ATT)
        {
            bigTextFiles << it->first << std::endl;
            bigTextFiles << "-----" << std::endl;
            bigTextFiles << it->second[CD_CONTENT] << std::endl << std::endl;
        }
        if (it->second[CD_TYPE] == CD_BIN)
        {
            binaryFiles << it->first << std::endl;
            binaryFiles << "-----" << std::endl;
            binaryFiles << it->second[CD_CONTENT] << std::endl << std::endl;
        }
    }

    string description = "Duplicity check\n======\n\n";
    description += UUIDFile.str();
    description += '\n';
    description += "Common information\n======\n\n";
    description += commonFiles.str();
    description += '\n';
    description += "Additional information\n======\n\n";
    description += additionalFiles.str();
    description += '\n';
    description += "Big Text Files\n======\n\n";
    description += bigTextFiles.str();
    description += '\n';
    description += "Binary files\n======\n";
    description += binaryFiles.str();
    description += "\n\n";

    return description;
}
