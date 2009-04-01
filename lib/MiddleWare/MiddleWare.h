/*
    MiddleWare.h - header file for MiddleWare library. It wraps plugins and
                   take case of them.

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


#ifndef MIDDLEWARE_H_
#define MIDDLEWARE_H_

#include "PluginManager.h"
#include "CrashTypes.h"
#include "MiddleWareTypes.h"
#include "RPM.h"

class CMiddleWare
{
    private:
        typedef set_strings_t set_blacklist_t;
        typedef set_strings_t set_enabled_plugins_t;
        typedef set_strings_t set_reporters_t;
        typedef set_strings_t set_opengpg_keys_t;
        typedef std::map<std::string, set_reporters_t> map_plugin2reporters_t;

        CPluginManager* m_pPluginManager;
        CRPM m_RPM;
        set_blacklist_t m_setBlackList;
        set_enabled_plugins_t m_setEnabledPlugins;
        set_opengpg_keys_t m_setOpenGPGKeys;
        std::string m_sDatabase;
        map_plugin2reporters_t m_mapPlugin2Reporters;

        bool m_bOpenGPGCheck;

        std::string GetLocalUUID(const std::string& pAnalyzer,
                                 const std::string& pDebugDumpDir);
        std::string GetGlobalUUID(const std::string& pAnalyzer,
                                  const std::string& pDebugDumpDir);
        void CreateReport(const std::string& pAnalyzer,
                          const std::string& pDebugDumpDir);

        void LoadSettings(const std::string& pPath);

        void DebugDumpToCrashReport(const std::string& pDebugDumpDir,
                                    crash_report_t& pCrashReport);

        bool IsDebugDumpSaved(const std::string& pDebugDumpDir);
        int SavePackageDescriptionToDebugDump(const std::string& pDebugDumpDir);
        int SaveUUIDToDebugDump(const std::string& pDebugDumpDir);
        int SaveDebugDumpToDatabase(const std::string& pDebugDumpDir, crash_info_t& pCrashInfo);
        crash_info_t GetCrashInfo(const std::string& pUUID,
                                  const std::string& pUID);

    public:

        CMiddleWare(const std::string& pPlugisConfDir,
                    const std::string& pPlugisLibDir,
                    const std::string& pMiddleWareConfFile);

        ~CMiddleWare();

        void RegisterPlugin(const std::string& pName);
        void UnRegisterPlugin(const std::string& pName);

        void CreateCrashReport(const std::string& pUUID,
                               const std::string& pUID,
                               crash_report_t& pCrashReport);

        void Report(const crash_report_t& pCrashReport);

        void DeleteCrashInfo(const std::string& pUUID,
                             const std::string& pUID,
                             const bool bWithDebugDump = true);


        int SaveDebugDump(const std::string& pDebugDumpDir);
        int SaveDebugDump(const std::string& pDebugDumpDir, crash_info_t& pCrashInfo);

        vector_crash_infos_t GetCrashInfos(const std::string& pUID);
};

#endif /*MIDDLEWARE_H_*/
