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
#include "Settings.h"

#define MAILX_COMMAND "/bin/mailx"
#define MAILX_SUBJECT "\"abrt automated bug report\""

CMailx::CMailx() :
    m_sEmailFrom("user@localhost"),
    m_sEmailTo("root@localhost"),
    m_sParameters(""),
    m_sAttachments(""),
    m_bSendBinaryData(false)
{}


void CMailx::SendEmail(const std::string& pText)
{
    FILE* command;
    std::string mailx_command = MAILX_COMMAND + m_sAttachments +
                                " " + m_sParameters +
                                " -s " + MAILX_SUBJECT +
                                " -r " + m_sEmailFrom + " " + m_sEmailTo;

    command = popen(mailx_command.c_str(), "w");
    if (!command)
    {
        throw std::string("CMailx::SendEmail: Can not execute mailx.");
    }
    if (fputs(pText.c_str(), command) == -1)
    {
        throw std::string("CMailx::SendEmail: Can not send data.");
    }
    pclose(command);
}


void CMailx::Report(const crash_report_t& pCrashReport)
{
    std::stringstream emailBody;
    std::stringstream binaryFiles, commonFiles, additionalFiles, UUIDFile;

    crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        if (it->second.m_sType == TYPE_TXT)
        {
            if (it->first !=  FILENAME_UUID &&
                it->first !=  FILENAME_ARCHITECTURE &&
                it->first !=  FILENAME_KERNEL &&
                it->first !=  FILENAME_PACKAGE)
            {
                additionalFiles << it->first << std::endl;
                additionalFiles << "-----" << std::endl;
                additionalFiles << it->second.m_sContent << std::endl;
            }
            else if (it->first == FILENAME_UUID)
            {
                UUIDFile << it->first << std::endl;
                UUIDFile << "-----" << std::endl;
                UUIDFile << it->second.m_sContent << std::endl;
            }
            else
            {
                commonFiles << it->first << std::endl;
                commonFiles << "-----" << std::endl;
                commonFiles << it->second.m_sContent << std::endl;
            }
        }
        if (it->second.m_sType == TYPE_BIN)
        {
            binaryFiles << " -a " << it->second.m_sContent;
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
    emailBody << "Binary file[s]" << std::endl;
    emailBody << "=====" << std::endl;

    if (m_bSendBinaryData)
    {
        emailBody << "See the attachment[s]" << std::endl;
        m_sAttachments = binaryFiles.str();
    }
    else
    {
        emailBody << "Do not send them." << std::endl;
    }

    SendEmail(emailBody.str());
}

void CMailx::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    load_settings(pPath, settings);

    if (settings.find("EmailFrom")!= settings.end())
    {
        m_sEmailFrom = settings["EmailFrom"];
    }
    if (settings.find("EmailTo")!= settings.end())
    {
        m_sEmailTo = settings["EmailTo"];
    }
    if (settings.find("Parameters")!= settings.end())
    {
        m_sParameters = settings["Parameters"];
    }
    if (settings.find("SendBinaryData")!= settings.end())
    {
        m_bSendBinaryData = settings["SendBinaryData"] == "no";
    }
}
