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
#include <sstream>
#include "DebugDump.h"
#include "CommLayerInner.h"

CLogger::CLogger() :
    m_sLogPath("/var/log/abrt-logger"),
    m_bAppendLogs(true)
{}

void CLogger::LoadSettings(const std::string& pPath)
{
    map_plugin_settings_t settings;
    plugin_load_settings(pPath, settings);

    SetSettings(settings);
}

void CLogger::SetSettings(const map_plugin_settings_t& pSettings)
{
    if (pSettings.find("LogPath") != pSettings.end())
    {
        m_sLogPath = pSettings.find("LogPath")->second;
    }
    if (pSettings.find("AppendLogs") != pSettings.end())
    {
        m_bAppendLogs = pSettings.find("AppendLogs")->second == "yes";
    }
}

map_plugin_settings_t CLogger::GetSettings()
{
    map_plugin_settings_t ret;

    ret["LogPath"] = m_sLogPath;
    ret["AppendLogs"] = m_bAppendLogs ? "yes" : "no";

    return ret;
}

void CLogger::Report(const map_crash_report_t& pCrashReport, const std::string& pArgs)
{
    comm_layer_inner_status("Creating a report...");

    std::stringstream binaryFiles, commonFiles, bigTextFiles, additionalFiles, UUIDFile;
    std::ofstream fOut;

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
            binaryFiles << it->first << std::endl;
            binaryFiles << "-----" << std::endl;
            binaryFiles << it->second[CD_CONTENT] << std::endl << std::endl;
        }
    }


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
        fOut << "======" << std::endl << std::endl;
        fOut << UUIDFile.str() << std::endl;
        fOut << "Common information" << std::endl;
        fOut << "======" << std::endl << std::endl;
        fOut << commonFiles.str() << std::endl;
        fOut << "Additional information" << std::endl;
        fOut << "======" << std::endl << std::endl;
        fOut << additionalFiles.str() << std::endl;
        fOut << "Big Text Files" << std::endl;
        fOut << "======" << std::endl;
        fOut << bigTextFiles.str() << std::endl;
        fOut << "Binary files" << std::endl;
        fOut << "======" << std::endl;
        fOut << binaryFiles.str() << std::endl;
        fOut << std::endl;
        fOut.close();
    }
}

PLUGIN_INFO(REPORTER,
            CLogger,
            "Logger",
            "0.0.1",
            "Write a report to a specific file",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Logger.GTKBuilder");
