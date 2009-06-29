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

/**
 * A very important class :-). It manages part of user demands like creating
 * reports, or reporting stuff somewhere etc.
 */
class CMiddleWare
{
    public:
        /**
         * An emun contains all return codes.
         */
        typedef enum { MW_ERROR,            /**< Common error.*/
                       MW_OK,               /**< No error.*/
                       MW_BLACKLISTED,      /**< Package is blacklisted.*/
                       MW_CORRUPTED,        /**< Debugdump directory is corrupted.*/
                       MW_PACKAGE_ERROR,    /**< Cannot determine package name.*/
                       MW_GPG_ERROR,        /**< Package is not signed properly.*/
                       MW_REPORTED,         /**< Crash is already reported.*/
                       MW_OCCURED,          /**< Crash occurred in the past, but it is not reported yet.*/
                       MW_IN_DB,            /**< Debugdump directory is already saved in a database.*/
                       MW_IN_DB_ERROR,      /**< Error while working with a database.*/
                       MW_FILE_ERROR        /**< Error when trying open debugdump directory or
                                                 when trying open file in debug dump directory..*/
                      } mw_result_t;

    private:
        typedef set_strings_t set_blacklist_t;
        typedef set_strings_t set_enabled_plugins_t;
        typedef std::vector<pair_string_string_t> vector_pairt_strings_t;
        typedef vector_pairt_strings_t vector_actions_and_reporters_t;
        typedef std::map<std::string, vector_actions_and_reporters_t> map_analyzer_actions_and_reporters_t;

        /**
         * An instance of CPluginManager. When MiddleWare wants to do something
         * with plugins, it calls the plugin manager.
         * @see PluginManager.h
         */
        CPluginManager* m_pPluginManager;
        /**
         * An instance of CRPM used for package checking.
         * @see RPM.h
         */
        CRPM m_RPM;
        /**
         * A set of blacklisted packages.
         */
        set_blacklist_t m_setBlackList;
        /**
         * A name of database plugin, which is used for metadata.
         */
        std::string m_sDatabase;
        /**
         * A map, which associates particular analyzer to one or more
         * action or reporter plugins. These are activated when a crash, which
         * is maintained by particular analyzer, occurs.
         */
        map_analyzer_actions_and_reporters_t m_mapAnalyzerActionsAndReporters;
        /**
         * A vector of one or more action or reporter plugins. These are
         * activated when any crash occurs.
         */
        vector_actions_and_reporters_t m_vectorActionsAndReporters;
        /**
         * Plugins configuration directory (e.g. /etc/abrt/plugins, ...).
         */
        std::string m_sPluginsConfDir;
        /**
         * Check GPG finger print?
         */
        bool m_bOpenGPGCheck;
        /**
         * A method, which gets a local UUID from particular analyzer plugin.
         * @param pAnalyzer A name of an analyzer plugin.
         * @param pDebugDumpDir A debugdump dir containing all necessary data.
         * @return A local UUID.
         */
        std::string GetLocalUUID(const std::string& pAnalyzer,
                                 const std::string& pDebugDumpDir);
        /**
         * A method, which gets a global UUID from particular analyzer plugin.
         * @param pAnalyzer A name of an analyzer plugin.
         * @param pDebugDumpDir A debugdump dir containing all necessary data.
         * @return A global UUID.
         */
        std::string GetGlobalUUID(const std::string& pAnalyzer,
                                  const std::string& pDebugDumpDir);
        /**
         * A method, which takes care of getting all additional data needed
         * for computing UUIDs and creating a report for particular analyzer
         * plugin. This report could be send somewhere afterwards.
         * @param pAnalyzer A name of an analyzer plugin.
         * @param pDebugDumpPath A debugdump dir containing all necessary data.
         */
        void CreateReport(const std::string& pAnalyzer,
                          const std::string& pDebugDumpDir);
        /**
         * A method, which executes all action plugins, which are associated to
         * particular analyzer plugin.
         * @param pAnalyzer A name of an analyzer plugin.
         * @param pDebugDumpPath A debugdump dir containing all necessary data.
         */
        void RunAnalyzerActions(const std::string& pAnalyzer,
                                const std::string& pDebugDumpDir);
        /**
         * A method, which transforms a debugdump direcortry to inner crash
         * report form. This form is used for later reporting.
         * @param pDebugDumpDir A debugdump dir containing all necessary data.
         * @param pCrashReport A created crash report.
         */
        void DebugDumpToCrashReport(const std::string& pDebugDumpDir,
                                    map_crash_report_t& pCrashReport);
        /**
         * A method, which checks is particular debugdump directory is saved
         * in database. This check is done together with an UID of an user.
         * @param pUID an UID of an user.
         * @param pDebugDumpDir A debugdump dir containing all necessary data.
         * @return It returns true if debugdump dir is already saved, otherwise
         * it returns false.
         */
        bool IsDebugDumpSaved(const std::string& pUID,
                              const std::string& pDebugDumpDir);
        /**
         * A method, which gets a package name from executable name and saves
         * package description to particular debugdump directory of a crash.
         * @param pExecutable A name of crashed application.
         * @param pDebugDumpDir A debugdump dir containing all necessary data.
         * @return It return results of operation. See mw_result_t.
         */
        mw_result_t SavePackageDescriptionToDebugDump(const std::string& pExecutable,
                                                      const std::string& pDebugDumpDir);
        /**
         * A method, which save a debugdump into database. If a saving is
         * successful, then a crash info is filled. Otherwise the crash info is
         * not changed.
         * @param pUUID A local UUID of a crash.
         * @param pUID An UID of an user.
         * @param pTime Time when a crash occurs.
         * @param pDebugDumpPath A debugdump path.
         * @param pCrashInfo A filled crash info.
         * @return It return results of operation. See mw_result_t.
         */
        mw_result_t SaveDebugDumpToDatabase(const std::string& pUUID,
                                            const std::string& pUID,
                                            const std::string& pTime,
                                            const std::string& pDebugDumpDir,
                                            map_crash_info_t& pCrashInfo);

    public:
        /**
         * A constructor.
         * @param pPlugisConfDir A plugins configuration directory.
         * @param pPlugisLibDir A plugins library directory.
         */
        CMiddleWare(const std::string& pPlugisConfDir,
                    const std::string& pPlugisLibDir);
        /**
         * A destructor.
         */
        ~CMiddleWare();
        /**
         * A method, which registers particular plugin.
         * @param pName A plugin name.
         */
        void RegisterPlugin(const std::string& pName);
        /**
         * A method, which unregister particular plugin.
         * @param pName A plugin name.
         */
        void UnRegisterPlugin(const std::string& pName);
        /**
         * A method, which takes care of getting all additional data needed
         * for computing UUIDs and creating a report for particular analyzer
         * plugin. This report could be send somewhere afterwards. If a creation
         * is successful, then  a crash report is filled.
         * @param pAnalyzer A name of an analyzer plugin.
         * @param pDebugDumpPath A debugdump dir containing all necessary data.
         * @param pCrashReport A filled crash report.
         * @return It return results of operation. See mw_result_t.
         */
        mw_result_t CreateCrashReport(const std::string& pUUID,
                                      const std::string& pUID,
                                      map_crash_report_t& pCrashReport);
        /**
         * A method, which activate particular action plugin.
         * @param pActionDir A directory, which is passed as working to a action plugin.
         * @param pPluginName An action plugin name.
         * @param pPluginArgs Action plugin's arguments.
         */
        void RunAction(const std::string& pActionDir,
                       const std::string& pPluginName,
                       const std::string& pPluginArgs);
        /**
         * A method, which activate all action and reporter plugins when any
         * crash occurs.
         * @param pDebugDumpDir A debugdump dir containing all necessary data.
         */
        void RunActionsAndReporters(const std::string& pDebugDumpDir);
        /**
         * A method, which reports a crash report to particular receiver.
         * @param pCrashReport A crash report.
         */
        void Report(const map_crash_report_t& pCrashReport);
        /**
         * A method, which reports a crash report to particular receiver. It
         * takes a path where settings of reporter are stored (e.g. $HOME/.abrt,
         * ...).
         * @param pCrashReport A crash report.
         * @param pSettingsPath A path to setting files.
         */
        void Report(const map_crash_report_t& pCrashReport,
                    const std::string& pSettingsPath);
        /**
         * A method, which deletes particular debugdump directory.
         * @param pDebugDumpDir A debugdump directory.
         */
        void DeleteDebugDumpDir(const std::string& pDebugDumpDir);
        /**
         * A method, which delete a row from database. If a deleting is
         * successfull, it returns a debugdump directort, which is not
         * deleted. Otherwise, it returns empty string.
         * @param pUUID A local UUID of a crash.
         * @param pUID An UID of an user.
         * @return A debugdump directory.
         */
        std::string DeleteCrashInfo(const std::string& pUUID,
                                    const std::string& pUID);
        /**
         * A method, whis saves debugdump into database.
         * @param pDebugDumpDir A debugdump directory.
         * @return It return results of operation. See mw_result_t.
         */
        mw_result_t SaveDebugDump(const std::string& pDebugDumpDir);
        /**
         * A method, whis saves debugdump into database. If saving is sucessful
         * it fills crash info.
         * @param pDebugDumpDir A debugdump directory.
         * @param pCrashInfo A crash info.
         * @return It return results of operation. See mw_result_t.
         */
        mw_result_t SaveDebugDump(const std::string& pDebugDumpDir,
                                  map_crash_info_t& pCrashInfo);
        /**
         * A method, which gets one crash info. If a getting is successful,
         * then a crash info is filled.
         * @param pUUID A local UUID of a crash.
         * @param pUID An UID of an user.
         * @param pCrashInfo A crash info.
         * @return It return results of operation. See mw_result_t.
         */
        mw_result_t GetCrashInfo(const std::string& pUUID,
                                 const std::string& pUID,
                                 map_crash_info_t& pCrashInfo);
        /**
         * A method, which gets all local UUIDs and UIDs of crashes. These crashes
         * occurred when a particular user was logged in.
         * @param pUID an UID of an user.
         * @return A vector of pairs  (local UUID, UID).
         */
        vector_pair_string_string_t GetUUIDsOfCrash(const std::string& pUID);

        /**
         * A method, which set a GPG finger print check.
         * @param pCheck Is it enabled?
         */
        void SetOpenGPGCheck(const bool& pCheck);
        /**
         * A method, which sets a name of database.
         * @param pDatabase A database name.
         */
        void SetDatabase(const std::string& pDatabase);
        /**
         * A method, which adds one path to a GPG public key into MW's set.
         * @param pKey A path to a GPG public key.
         */
        void AddOpenGPGPublicKey(const std::string& pKey);
        /**
         * A method, which adds one blacklisted package.
         */
        void AddBlackListedPackage(const std::string& pPackage);
        /**
         * A method, which adds one association among alanyzer plugin and its
         * action and reporter plugins.
         * @param pAnalyzer A name of an analyzer plugin.
         * @param pActionOrReporter A name of an action or reporter plugin.
         * @param pArgs An arguments for action or reporter plugin.
         */
        void AddAnalyzerActionOrReporter(const std::string& pAnalyzer,
                                         const std::string& pActionOrReporter,
                                         const std::string& pArgs);
        /**
         * A method, which adds action and reporter plugins, which are activated
         * when any crash occurs.
         * @param pActionOrReporter A name of an action or reporter plugin.
         * @param pArgs An arguments for action or reporter plugin.
         */
        void AddActionOrReporter(const std::string& pActionOrReporter,
                                 const std::string& pArgs);
};

#endif /*MIDDLEWARE_H_*/
