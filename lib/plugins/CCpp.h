/*
    CCpp.h - header file for C/C++ analyzer plugin
           - it can get UUID and memory maps from core files

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
#ifndef CCPP_H_
#define CCPP_H_

#include <string>
#include "plugin.h"
#include "analyzer.h"

class CAnalyzerCCpp : public CAnalyzer
{
    private:
        bool m_bBacktrace;
        bool m_bBacktraceRemotes;
        bool m_bMemoryMap;
        bool m_bInstallDebugInfo;
        unsigned m_nDebugInfoCacheMB;
        unsigned m_nGdbTimeoutSec;
        std::string m_sOldCorePattern;
        std::string m_sDebugInfo;
        std::string m_sDebugInfoDirs;

    public:
        CAnalyzerCCpp();
        virtual std::string GetLocalUUID(const char *pDebugDumpDir);
        virtual std::string GetGlobalUUID(const char *pDebugDumpDir);
        virtual void CreateReport(const char *pDebugDumpDir, int force);
        virtual void Init();
        virtual void DeInit();
        virtual void SetSettings(const map_plugin_settings_t& pSettings);
//ok to delete?
//      virtual const map_plugin_settings_t& GetSettings();
};

#endif /* CCPP */
