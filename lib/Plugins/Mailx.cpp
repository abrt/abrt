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


#include <stdio.h>
#include "abrtlib.h"
#include "Mailx.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

#define MAILX_COMMAND "/bin/mailx"

CMailx::CMailx() :
    m_sEmailFrom("user@localhost"),
    m_sEmailTo("root@localhost"),
    m_sSubject("[abrt] full crash report"),
    m_bSendBinaryData(false),
    m_nArgs(0),
    m_pArgs(NULL)
{}

void CMailx::FreeMailxArgs()
{
    int ii;
    for (ii = 0; ii < m_nArgs; ii++)
    {
        free(m_pArgs[ii]);
    }
    free((void*) m_pArgs);
    m_pArgs = NULL;
    m_nArgs = 0;
}

void CMailx::AddMailxArg(const std::string& pArg)
{
    m_pArgs = (char**) realloc((void*)m_pArgs, (++m_nArgs) * (sizeof(char*)));
    if (pArg == "")
    {
        m_pArgs[m_nArgs - 1] = NULL;
    }
    else
    {
        m_pArgs[m_nArgs - 1] = strdup(pArg.c_str());
    }
}

void CMailx::ExecMailx(uid_t uid, const std::string& pText)
{
    int pipein[2];
    pid_t child;

    struct passwd* pw = getpwuid(uid);
    if (!pw)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string(__func__) + ": cannot get GID for UID.");
    }

    xpipe(pipein);
    child = fork();
    if (child == -1)
    {
        close(pipein[0]);
        close(pipein[1]);
        throw CABRTException(EXCEP_PLUGIN, std::string(__func__) + ": fork failed.");
    }
    if (child == 0)
    {

        close(pipein[1]);
        xmove_fd(pipein[0], STDIN_FILENO);

        setgroups(1, &pw->pw_gid);
        setregid(pw->pw_gid, pw->pw_gid);
        setreuid(uid, uid);
        setsid();

        execvp(MAILX_COMMAND, m_pArgs);
        exit(0);
    }

    close(pipein[0]);
    safe_write(pipein[1], pText.c_str(), pText.length());
    close(pipein[1]);

    wait(NULL); /* why? */
}

void CMailx::SendEmail(const std::string& pSubject, const std::string& pText, const std::string& pUID)
{
    update_client(_("Sending an email..."));

    AddMailxArg("-s");
    AddMailxArg(pSubject);
    AddMailxArg("-r");
    AddMailxArg(m_sEmailFrom);
    AddMailxArg(m_sEmailTo);
    AddMailxArg("");

    ExecMailx(atoi(pUID.c_str()), pText);
}

std::string CMailx::Report(const map_crash_report_t& pCrashReport, 
                           const map_plugin_settings_t& pSettings, const std::string& pArgs)
{
    update_client(_("Creating a report..."));

    std::stringstream emailBody;
    std::stringstream binaryFiles, commonFiles, bigTextFiles, additionalFiles, UUIDFile;

    AddMailxArg(MAILX_COMMAND);

    map_crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT)
        {
            if (it->first !=  CD_UUID &&
                it->first !=  FILENAME_ARCHITECTURE &&
                it->first !=  FILENAME_KERNEL &&
                it->first !=  FILENAME_PACKAGE)
            {
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
            binaryFiles << " -a " << it->second[CD_CONTENT];
            if (m_bSendBinaryData)
            {
                AddMailxArg("-a");
                AddMailxArg(it->second[CD_CONTENT]);
            }
        }
    }

    emailBody << "Duplicity check" << std::endl;
    emailBody << "=====" << std::endl << std::endl;
    emailBody << UUIDFile.str() << std::endl;
    emailBody << "Common information" << std::endl;
    emailBody << "=====" << std::endl << std::endl;
    emailBody << commonFiles.str() << std::endl;
    emailBody << "Additional information" << std::endl;
    emailBody << "=====" << std::endl << std::endl;
    emailBody << additionalFiles.str() << std::endl;
    emailBody << "Other information" << std::endl;
    emailBody << "=====" << std::endl << std::endl;
    emailBody << bigTextFiles.str() << std::endl;

    if (pArgs != "")
    {
        SendEmail(pArgs, emailBody.str(), pCrashReport.find(CD_MWUID)->second[CD_CONTENT]);
    }
    else
    {
        SendEmail(m_sSubject, emailBody.str(), pCrashReport.find(CD_MWUID)->second[CD_CONTENT]);
    }

    FreeMailxArgs();

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
            "Sends an email with a report via mailx command",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Mailx.GTKBuilder");
