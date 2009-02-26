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

#define MAILX_COMMAND "/bin/mailx"
#define MAILX_SUBJECT "\"CrashCatcher automated bug report\""

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


void CMailx::Report(const crash_report_t& pReport)
{
    std::stringstream ss;

    ss << "Duplicity check" << std::endl;
    ss << "==================" << std::endl << std::endl;
    ss << "UUID" << std::endl;
    ss << "------------" << std::endl;
    ss << pReport.m_sUUID << std::endl << std::endl;
    ss << "Common information" << std::endl;
    ss << "==================" << std::endl << std::endl;
    ss << "Architecture" << std::endl;
    ss << "------------" << std::endl;
    ss << pReport.m_sArchitecture << std::endl << std::endl;
    ss << "Kernel version" << std::endl;
    ss << "--------------" << std::endl;
    ss << pReport.m_sKernel << std::endl << std::endl;
    ss << "Package" << std::endl;
    ss << "-------" << std::endl;
    ss << pReport.m_sPackage << std::endl << std::endl;
    ss << "Executable" << std::endl;
    ss << "----------" << std::endl;
    ss << pReport.m_sExecutable << std::endl << std::endl;
    ss << "CmdLine" << std::endl;
    ss << "----------" << std::endl;
    ss << pReport.m_sCmdLine << std::endl << std::endl;
    ss << "Created report" << std::endl;
    ss << "==============" << std::endl;
    ss << "Text reports" << std::endl;
    ss << "==============" << std::endl;
    if (pReport.m_sTextData1 != "")
    {
        ss << "Text Data 1" << std::endl;
        ss << "-----------" << std::endl;
        ss << pReport.m_sTextData1 << std::endl << std::endl;
    }
    if (pReport.m_sTextData2 != "")
    {
        ss << "Text Data 2" << std::endl;
        ss << "-----------" << std::endl;
        ss << pReport.m_sTextData2 << std::endl << std::endl;
    }
    ss << "Binary reports" << std::endl;
    ss << "==============" << std::endl;
    ss << "See the attachment[s]" << std::endl;

    if (m_bSendBinaryData)
    {
        if (pReport.m_sBinaryData1 != "")
        {
            m_sAttachments = " -a " + pReport.m_sBinaryData1;
        }
        if (pReport.m_sBinaryData2 != "")
        {
            m_sAttachments = " -a " + pReport.m_sBinaryData2;
        }
    }

    SendEmail(ss.str());
}

void CMailx::SetSettings(const map_settings_t& pSettings)
{
    if (pSettings.find("EmailFrom")!= pSettings.end())
    {
        m_sEmailFrom = pSettings.find("EmailFrom")->second;
    }
    if (pSettings.find("EmailTo")!= pSettings.end())
    {
        m_sEmailTo = pSettings.find("EmailTo")->second;
    }
    if (pSettings.find("Parameters")!= pSettings.end())
    {
        m_sParameters = pSettings.find("Parameters")->second;
    }
    if (pSettings.find("SendBinaryData")!= pSettings.end())
    {
        m_bSendBinaryData = pSettings.find("SendBinaryData")->second == "yes";
    }
}
