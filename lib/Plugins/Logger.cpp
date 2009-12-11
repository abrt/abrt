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

#include <fstream>
#include <sstream>
#include "abrtlib.h"
#include "Logger.h"
#include "DebugDump.h"
#include "CommLayerInner.h"
#include "ABRTException.h"

CLogger::CLogger() :
    m_sLogPath("/var/log/abrt-logger"),
    m_bAppendLogs(true)
{}

void CLogger::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("LogPath");
    if (it != end)
    {
        m_sLogPath = it->second;
    }
    it = pSettings.find("AppendLogs");
    if (it != end)
    {
        m_bAppendLogs = string_to_bool(it->second.c_str());
    }
}

//ok to delete?
//const map_plugin_settings_t& CLogger::GetSettings()
//{
//    m_pSettings["LogPath"] = m_sLogPath;
//    m_pSettings["AppendLogs"] = m_bAppendLogs ? "yes" : "no";
//
//    return m_pSettings;
//}

std::string CLogger::Report(const map_crash_report_t& pCrashReport,
                const map_plugin_settings_t& pSettings,
                const char *pArgs)
{

    std::string description = make_description_logger(pCrashReport);
    description += "\n\n\n";

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
        update_client(_("Writing report to '%s'"), m_sLogPath.c_str());
        fputs(description.c_str(), fOut);
        fclose(fOut);
        return "file://" + m_sLogPath;
    }

    throw CABRTException(EXCEP_PLUGIN, "Can't open '%s'", m_sLogPath.c_str());
}

PLUGIN_INFO(REPORTER,
            CLogger,
            "Logger",
            "0.0.1",
            "Writes report to a file",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Logger.GTKBuilder");
