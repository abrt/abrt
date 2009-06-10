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
    public:

        typedef enum { MW_ERROR,
                       MW_OK,
                       MW_BLACKLISTED,
                       MW_CORRUPTED,
                       MW_PACKAGE_ERROR,
                       MW_GPG_ERROR,
                       MW_REPORTED,
                       MW_OCCURED,
                       MW_IN_DB,
                       MW_IN_DB_ERROR,
                       MW_FILE_ERROR } mw_result_t;

    private:
        typedef set_strings_t set_blacklist_t;
        typedef set_strings_t set_enabled_plugins_t;

        typedef std::vector<pair_string_string_t> vector_pairt_strings_t;
        typedef vector_pairt_strings_t vector_actions_and_reporters_t;
        typedef std::map<std::string, vector_actions_and_reporters_t> map_analyzer_actions_and_reporters_t;

        CPluginManager* m_pPluginManager;
        CRPM m_RPM;
        set_blacklist_t m_setBlackList;
        std::string m_sDatabase;
        map_analyzer_actions_and_reporters_t m_mapAnalyzerActionsAndReporters;
        vector_actions_and_reporters_t m_vectorActionsAndReporters;
        std::string m_sPluginsConfDir;
        bool m_bOpenGPGCheck;

        std::string GetLocalUUID(const std::string& pAnalyzer,
                                 const std::string& pDebugDumpDir);
        std::string GetGlobalUUID(const std::string& pAnalyzer,
                                  const std::string& pDebugDumpDir);
        void CreateReport(const std::string& pAnalyzer,
                         const std::string& pDebugDumpDir);
        void RunAnalyzerActions(const std::string& pAnalyzer,
                                const std::string& pDebugDumpDir);
        void DebugDumpToCrashReport(const std::string& pDebugDumpDir,
                                    map_crash_report_t& pCrashReport);
        bool IsDebugDumpSaved(const std::string& pUID,
                              const std::string& pDebugDumpDir);
        mw_result_t SavePackageDescriptionToDebugDump(const std::string& pExecutable,
                                                      const std::string& pDebugDumpDir);
        mw_result_t SaveDebugDumpToDatabase(const std::string& pUUID,
                                            const std::string& pUID,
                                            const std::string& pTime,
                                            const std::string& pDebugDumpDir,
                                            map_crash_info_t& pCrashInfo);

    public:

        CMiddleWare(const std::string& pPlugisConfDir,
                    const std::string& pPlugisLibDir);

        ~CMiddleWare();

        void RegisterPlugin(const std::string& pName);
        void UnRegisterPlugin(const std::string& pName);

        mw_result_t CreateCrashReport(const std::string& pUUID,
                                      const std::string& pUID,
                                      map_crash_report_t& pCrashReport);

        void RunAction(const std::string& pActionDir,
                       const std::string& pPluginName,
                       const std::string& pPluginArgs);
        void RunActionsAndReporters(const std::string& pDebugDumpDir);

        void Report(const map_crash_report_t& pCrashReport);
        void Report(const map_crash_report_t& pCrashReport,
                    const std::string& pSettingsPath);
        void DeleteDebugDumpDir(const std::string& pDebugDumpDir);
        std::string DeleteCrashInfo(const std::string& pUUID,
                                    const std::string& pUID);


        mw_result_t SaveDebugDump(const std::string& pDebugDumpDir);
        mw_result_t SaveDebugDump(const std::string& pDebugDumpDir,
                                  map_crash_info_t& pCrashInfo);

        mw_result_t GetCrashInfo(const std::string& pUUID,
                                 const std::string& pUID,
                                 map_crash_info_t& pCrashInfo);
        vector_strings_t GetUUIDsOfCrash(const std::string& pUID);

        void SetOpenGPGCheck(const bool& pCheck);
        void SetDatabase(const std::string& pDatabase);
        void AddOpenGPGPublicKey(const std::string& pKey);
        void AddBlackListedPackage(const std::string& pPackage);
        void AddAnalyzerActionOrReporter(const std::string& pAnalyzer,
                                         const std::string& pActionOrReporter,
                                         const std::string& pArgs);
        void AddActionOrReporter(const std::string& pActionOrReporter,
                                 const std::string& pArgs);
};

#endif /*MIDDLEWARE_H_*/
