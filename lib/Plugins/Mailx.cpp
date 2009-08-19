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

#include "Mailx.h"
#include <stdio.h>
#include <sstream>
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

#define MAILX_COMMAND "/bin/mailx"

CMailx::CMailx() :
    m_sEmailFrom("user@localhost"),
    m_sEmailTo("root@localhost"),
    m_sAttachments(""),
    m_sSubject("[abrt] full crash report"),
    m_bSendBinaryData(false)
{}


void CMailx::SendEmail(const std::string& pSubject, const std::string& pText)
{
    comm_layer_inner_status("Sending an email...");

    FILE* command;
    std::string mailx_command = MAILX_COMMAND + m_sAttachments +
                                " -s " + pSubject +
                                " -r " + m_sEmailFrom + " " + m_sEmailTo;

    command = popen(mailx_command.c_str(), "w");
    if (!command)
    {
        throw CABRTException(EXCEP_PLUGIN, "CMailx::SendEmail(): Can not execute mailx.");
    }
    if (fputs(pText.c_str(), command) == -1)
    {
        throw CABRTException(EXCEP_PLUGIN, "CMailx::SendEmail(): Can not send data.");
    }
    pclose(command);
}


std::string CMailx::Report(const map_crash_report_t& pCrashReport, const std::string& pArgs)
{
    comm_layer_inner_status("Creating a report...");

    std::stringstream emailBody;
    std::stringstream binaryFiles, commonFiles, bigTextFiles, additionalFiles, UUIDFile;

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
    emailBody << bigTextFiles << std::endl;

    if (m_bSendBinaryData)
    {
        m_sAttachments += binaryFiles.str();
    }

    if (pArgs != "")
    {
        SendEmail(pArgs, emailBody.str());
    }
    else
    {
        SendEmail(m_sSubject, emailBody.str());
    }
    return "Email was sent to: " + m_sEmailTo;
}

void CMailx::SetSettings(const map_plugin_settings_t& pSettings)
{
    if (pSettings.find("Subject") != pSettings.end())
    {
        m_sSubject = pSettings.find("Subject")->second;
    }
    if (pSettings.find("EmailFrom") != pSettings.end())
    {
        m_sEmailFrom = pSettings.find("EmailFrom")->second;
    }
    if (pSettings.find("EmailTo") != pSettings.end())
    {
        m_sEmailTo = pSettings.find("EmailTo")->second;
    }
    if (pSettings.find("SendBinaryData") != pSettings.end())
    {
        m_bSendBinaryData = pSettings.find("SendBinaryData")->second == "yes";
    }
}

map_plugin_settings_t CMailx::GetSettings()
{
    map_plugin_settings_t ret;

    ret["Subject"] = m_sSubject;
    ret["EmailFrom"] = m_sEmailFrom;
    ret["EmailTo"] = m_sEmailTo;
    ret["SendBinaryData"] = m_bSendBinaryData ? "yes" : "no";

    return ret;
}

PLUGIN_INFO(REPORTER,
            CMailx,
            "Mailx",
            "0.0.2",
            "Sends an email with a report via mailx command",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Mailx.GTKBuilder");
