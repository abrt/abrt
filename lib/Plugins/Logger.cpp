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
#include "ABRTException.h"

CLogger::CLogger() :
    m_sLogPath("/var/log/abrt-logger"),
    m_bAppendLogs(true)
{}

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

std::string CLogger::Report(const map_crash_report_t& pCrashReport, const std::string& pArgs)
{
    update_client(_("Creating a report..."));

    std::string description = make_description_logger(pCrashReport);

    FILE *fOut;
    if (m_bAppendLogs)
    {
        fOut = fopen(m_sLogPath.c_str(), "a");
    }
    else
    {
        fOut = fopen(m_sLogPath.c_str(), "w");
    }

    if (fOut)
    {
	fputs(description.c_str(), fOut);
        fclose(fOut);
        return "file://" + m_sLogPath;
    }

    throw CABRTException(EXCEP_PLUGIN, "CLogger::Report(): Cannot open file: " + m_sLogPath);
}

PLUGIN_INFO(REPORTER,
            CLogger,
            "Logger",
            "0.0.1",
            "Write a report to a specific file",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Logger.GTKBuilder");
