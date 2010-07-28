/*
    Mailx.cpp

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#include "Mailx.h"
#include "debug_dump.h"
#include "abrt_exception.h"
#include "comm_layer_inner.h"

#define MAILX_COMMAND "/bin/mailx"

CMailx::CMailx() :
    m_sEmailFrom("user@localhost"),
    m_sEmailTo("root@localhost"),
    m_sSubject("[abrt] full crash report"),
    m_bSendBinaryData(false)
{}

static void exec_and_feed_input(uid_t uid, const char* pText, char **pArgs)
{
    int pipein[2];

    pid_t child = fork_execv_on_steroids(
                EXECFLG_INPUT | EXECFLG_QUIET | EXECFLG_SETGUID,
                pArgs,
                pipein,
                /*unsetenv_vec:*/ NULL,
                /*dir:*/ NULL,
                uid);

    full_write(pipein[1], pText, strlen(pText));
    close(pipein[1]);

    waitpid(child, NULL, 0); /* wait for command completion */
}

static char** append_str_to_vector(char **vec, unsigned &size, const char *str)
{
    //log("old vec: %p", vec);
    vec = (char**) xrealloc(vec, (size+2) * sizeof(vec[0]));
    vec[size] = xstrdup(str);
    //log("new vec: %p, added [%d] %p", vec, size, vec[size]);
    size++;
    vec[size] = NULL;
    return vec;
}

std::string CMailx::Report(const map_crash_data_t& pCrashData,
                const map_plugin_settings_t& pSettings,
                const char *pArgs)
{
    SetSettings(pSettings);
    char **args = NULL;
    unsigned arg_size = 0;
    args = append_str_to_vector(args, arg_size, MAILX_COMMAND);

//TODO: move email body generation to make_descr.cpp
    std::string binaryFiles, commonFiles, additionalFiles, DUPHASHFile;
    map_crash_data_t::const_iterator it;
    for (it = pCrashData.begin(); it != pCrashData.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT)
        {
            if (it->first != CD_DUPHASH
             && it->first != FILENAME_ARCHITECTURE
             && it->first != FILENAME_KERNEL
             && it->first != FILENAME_PACKAGE
            ) {
                additionalFiles += it->first;
                additionalFiles += "\n-----\n";
                additionalFiles += it->second[CD_CONTENT];
                additionalFiles += "\n\n";
            }
            else if (it->first == CD_DUPHASH)
            {
                DUPHASHFile += it->first;
                DUPHASHFile += "\n-----\n";
                DUPHASHFile += it->second[CD_CONTENT];
                DUPHASHFile += "\n\n";
            }
            else
            {
                commonFiles += it->first;
                commonFiles += "\n-----\n";
                commonFiles += it->second[CD_CONTENT];
                commonFiles += "\n\n";
            }
        }
        if (it->second[CD_TYPE] == CD_BIN)
        {
            binaryFiles += " -a ";
            binaryFiles += it->second[CD_CONTENT];
            if (m_bSendBinaryData)
            {
                args = append_str_to_vector(args, arg_size, "-a");
                args = append_str_to_vector(args, arg_size, it->second[CD_CONTENT].c_str());
            }
        }
    }

    std::string emailBody = "Duplicate check\n";
    emailBody += "=====\n\n";
    emailBody += DUPHASHFile;
    emailBody += "\nCommon information\n";
    emailBody += "=====\n\n";
    emailBody += commonFiles;
    emailBody += "\nAdditional information\n";
    emailBody += "=====\n\n";
    emailBody += additionalFiles;
    emailBody += '\n';

    args = append_str_to_vector(args, arg_size, "-s");
    args = append_str_to_vector(args, arg_size, (pArgs[0] != '\0' ? pArgs : m_sSubject.c_str()));
    args = append_str_to_vector(args, arg_size, "-r");
    args = append_str_to_vector(args, arg_size, m_sEmailFrom.c_str());
    args = append_str_to_vector(args, arg_size, m_sEmailTo.c_str());

    update_client(_("Sending an email..."));
    const char *uid_str = get_crash_data_item_content(pCrashData, CD_UID).c_str();
    exec_and_feed_input(xatoi_u(uid_str), emailBody.c_str(), args);

    while (*args)
    {
        free(*args++);
    }
    args -= arg_size;
    free(args);

    return "Email was sent to: " + m_sEmailTo;
}

void CMailx::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("Subject");
    if (it != end)
    {
        m_sSubject = it->second;
    }
    it = pSettings.find("EmailFrom");
    if (it != end)
    {
        m_sEmailFrom = it->second;
    }
    it = pSettings.find("EmailTo");
    if (it != end)
    {
        m_sEmailTo = it->second;
    }
    it = pSettings.find("SendBinaryData");
    if (it != end)
    {
        m_bSendBinaryData = string_to_bool(it->second.c_str());
    }
}

//ok to delete?
//const map_plugin_settings_t& CMailx::GetSettings()
//{
//    m_pSettings["Subject"] = m_sSubject;
//    m_pSettings["EmailFrom"] = m_sEmailFrom;
//    m_pSettings["EmailTo"] = m_sEmailTo;
//    m_pSettings["SendBinaryData"] = m_bSendBinaryData ? "yes" : "no";
//
//    return m_pSettings;
//}

PLUGIN_INFO(REPORTER,
            CMailx,
            "Mailx",
            "0.0.2",
            _("Sends an email with a report (via mailx command)"),
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Mailx.glade");
