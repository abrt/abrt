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
    m_sEmailFrom(""),
    m_sEmailTo(""),
    m_sParameters(""),
    m_sAttachments("")
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


void CMailx::Report(const std::string& pDebugDumpDir)
{
    std::stringstream ss;
    std::string data;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);

    ss << "Common information" << std::endl;
    ss << "==================" << std::endl << std::endl;
    dd.LoadText(FILENAME_ARCHITECTURE, data);
    ss << "Architecture" << std::endl;
    ss << "------------" << std::endl;
    ss << data << std::endl << std::endl;
    dd.LoadText(FILENAME_KERNEL, data);
    ss << "Kernel version" << std::endl;
    ss << "--------------" << std::endl;
    ss << data << std::endl << std::endl;
    dd.LoadText(FILENAME_PACKAGE, data);
    ss << "Package" << std::endl;
    ss << "-------" << std::endl;
    ss << data << std::endl << std::endl;
    dd.LoadText(FILENAME_EXECUTABLE, data);
    ss << "Executable" << std::endl;
    ss << "----------" << std::endl;
    ss << data << std::endl << std::endl;
    ss << "Created report" << std::endl;
    ss << "==============" << std::endl;
    ss << "Text reports" << std::endl;
    ss << "==============" << std::endl;
    if (dd.Exist(FILENAME_TEXTDATA1))
    {
        dd.LoadText(FILENAME_TEXTDATA1, data);
        ss << "Text Data 1" << std::endl;
        ss << "-----------" << std::endl;
        ss << data << std::endl << std::endl;
    }
    if (dd.Exist(FILENAME_TEXTDATA2))
    {
        dd.LoadText(FILENAME_TEXTDATA2, data);
        ss << "Text Data 2" << std::endl;
        ss << "-----------" << std::endl;
        ss << data << std::endl << std::endl;
    }
    ss << "Binary reports" << std::endl;
    ss << "==============" << std::endl;
    ss << "See the attachment[s]" << std::endl;

    if (dd.Exist(FILENAME_BINARYDATA1))
    {
        m_sAttachments = " -a " + pDebugDumpDir + "/" + FILENAME_BINARYDATA1;
    }
    if (dd.Exist(FILENAME_BINARYDATA2))
    {
        m_sAttachments = " -a " + pDebugDumpDir + "/" + FILENAME_BINARYDATA2;
    }

    SendEmail(ss.str());
}

void CMailx::SetSettings(const map_settings_t& pSettings)
{
    if (pSettings.find("Email_From")!= pSettings.end())
    {
        m_sEmailFrom = pSettings.find("Email_From")->second;
    }
    if (pSettings.find("Email_To")!= pSettings.end())
    {
        m_sEmailTo = pSettings.find("Email_To")->second;
    }
    if (pSettings.find("Parameters")!= pSettings.end())
    {
        m_sParameters = pSettings.find("Parameters")->second;
    }
}
