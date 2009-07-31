/*
    CCpp.h - header file for C/C++ analyzer plugin
           - it can ger UUID and memory maps from core files

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
#include "Plugin.h"
#include "Analyzer.h"

class CAnalyzerCCpp : public CAnalyzer
{
    private:
        bool m_bMemoryMap;
        pid_t m_Pid;
        std::string m_sOldCorePattern;
        std::string m_sDebugInfo;
        void InstallDebugInfos(const std::string& pPackage);
        void GetBacktrace(const std::string& pDebugDumpDir, std::string& pBacktrace);
        void GetIndependentBacktrace(const std::string& pBacktrace, std::string& pIndependentBacktrace);
        void GetIndependentBuildIdPC(const std::string& pBuildIdPC, std::string& pIndependentBuildIdPC);
        void ExecVP(const char* pCommand, char* const pArgs[], uid_t uid, std::string& pOutput);
        std::string CreateHash(const std::string& pInput);
    public:
        CAnalyzerCCpp();
        virtual ~CAnalyzerCCpp();
        virtual std::string GetLocalUUID(const std::string& pDebugDumpDir);
        virtual std::string GetGlobalUUID(const std::string& pDebugDumpDir);
        virtual void CreateReport(const std::string& pDebugDumpDir);
        virtual void Init();
        virtual void DeInit();
        virtual void LoadSettings(const std::string& pPath);
        virtual void SetSettings(const map_plugin_settings_t& pSettings);
        virtual map_plugin_settings_t GetSettings();
};

#endif /* CCPP */
