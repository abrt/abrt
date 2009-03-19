/*
    Logger.cpp - it simple writes report to specific file

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

#include "Logger.h"
#include <fstream>
#include "Settings.h"

CLogger::CLogger() :
    m_sLogPath("/var/log/abrt-logger"),
    m_bAppendLogs(true)
{}

void CLogger::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    load_settings(pPath, settings);

    if (settings.find("LogPath")!= settings.end())
    {
        m_sLogPath = settings["LogPath"];
    }
    if (settings.find("AppendLogs")!= settings.end())
    {
        m_bAppendLogs = settings["AppendLogs"] == "yes";
    }
}

void CLogger::Report(const crash_report_t& pReport)
{
    std::ofstream fOut;
    if (m_bAppendLogs)
    {
        fOut.open(m_sLogPath.c_str(), std::ios::app);
    }
    else
    {
        fOut.open(m_sLogPath.c_str());
    }
    if (fOut.is_open())
    {
        fOut << "Duplicity check" << std::endl;
        fOut << "==================" << std::endl << std::endl;
        fOut << "UUID" << std::endl;
        fOut << "------------" << std::endl;
        fOut << pReport.m_sUUID << std::endl << std::endl;
        fOut << "Common information" << std::endl;
        fOut << "==================" << std::endl << std::endl;
        fOut << "Architecture" << std::endl;
        fOut << "------------" << std::endl;
        fOut << pReport.m_sArchitecture << std::endl << std::endl;
        fOut << "Kernel version" << std::endl;
        fOut << "--------------" << std::endl;
        fOut << pReport.m_sKernel << std::endl << std::endl;
        fOut << "Package" << std::endl;
        fOut << "-------" << std::endl;
        fOut << pReport.m_sPackage << std::endl << std::endl;
        fOut << "Executable" << std::endl;
        fOut << "----------" << std::endl;
        fOut << pReport.m_sExecutable << std::endl << std::endl;
        fOut << "CmdLine" << std::endl;
        fOut << "----------" << std::endl;
        fOut << pReport.m_sCmdLine << std::endl << std::endl;
        fOut << "Created report" << std::endl;
        fOut << "==============" << std::endl;
        fOut << "Text reports" << std::endl;
        fOut << "==============" << std::endl;
        if (pReport.m_sTextData1 != "")
        {
            fOut << "Text Data 1" << std::endl;
            fOut << "-----------" << std::endl;
            fOut << pReport.m_sTextData1 << std::endl << std::endl;
        }
        if (pReport.m_sTextData2 != "")
        {
            fOut << "Text Data 2" << std::endl;
            fOut << "-----------" << std::endl;
            fOut << pReport.m_sTextData2 << std::endl << std::endl;
        }
        if (pReport.m_sComment != "")
        {
            fOut << "User Comments" << std::endl;
            fOut << "-----------" << std::endl;
            fOut << pReport.m_sComment << std::endl << std::endl;
        }
        fOut << "Binary reports" << std::endl;
        fOut << "==============" << std::endl;
        if (pReport.m_sBinaryData1 != "")
        {
            fOut << "1. " <<  pReport.m_sBinaryData1 << std::endl;
        }
        if (pReport.m_sBinaryData2 != "")
        {
            fOut << "2. " <<  pReport.m_sBinaryData2 << std::endl;
        }
        fOut << std::endl;
        fOut.close();
    }
}
