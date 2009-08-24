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
 * An enum contains all return codes.
 */
typedef enum {
    MW_ERROR,            /**< Common error.*/
    MW_OK,               /**< No error.*/
    MW_BLACKLISTED,      /**< Package is blacklisted.*/
    MW_CORRUPTED,        /**< Debugdump directory is corrupted.*/
    MW_PACKAGE_ERROR,    /**< Cannot determine package name.*/
    MW_GPG_ERROR,        /**< Package is not signed properly.*/
    MW_REPORTED,         /**< Crash is already reported.*/
    MW_OCCURED,          /**< Crash occurred in the past, but it is not reported yet.*/
    MW_IN_DB,            /**< Debugdump directory is already saved in a database.*/
    MW_IN_DB_ERROR,      /**< Error while working with a database.*/
    MW_PLUGIN_ERROR,     /**< plugin wasn't found or error within plugin*/
    MW_FILE_ERROR        /**< Error when trying open debugdump directory or
                              when trying open file in debug dump directory..*/
} mw_result_t;

typedef enum {
    RS_CODE,
    RS_MESSAGE
} report_status_items_t;

typedef std::map<std::string, vector_strings_t> report_status_t;
typedef std::map<std::string, vector_pair_string_string_t> map_analyzer_actions_and_reporters_t;


extern CPluginManager* g_pPluginManager;


void CMiddleWare(const std::string& pPluginsConfDir,
            const std::string& pPluginsLibDir);
void CMiddleWare_deinit();
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
 * A method, which reports a crash report to particular receiver. It
 * takes an user uid, tries to find user config file and load it. If it
 * fails, then default config is used. If pUID is emply string, default
 * config is used.
 * ...).
 * @param pCrashReport A crash report.
 * @param pUID An user uid
 * @return A report status, which reporters ends sucessfuly with messages.
 */
report_status_t Report(const map_crash_report_t& pCrashReport,
                       const std::string& pUID);
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
 * Get one crash info. If getting is successful,
 * then crash info is filled.
 * @param pUUID A local UUID of a crash.
 * @param pUID An UID of an user.
 * @param pCrashInfo A crash info.
 * @return It return results of operation. See mw_result_t.
 */
mw_result_t GetCrashInfo(const std::string& pUUID,
                         const std::string& pUID,
                         map_crash_info_t& pCrashInfo);
/**
 * Gets all local UUIDs and UIDs of crashes. These crashes
 * occurred when a particular user was logged in.
 * @param pUID an UID of an user.
 * @return A vector of pairs  (local UUID, UID).
 */
vector_pair_string_string_t GetUUIDsOfCrash(const std::string& pUID);
/**
 * Sets a GPG finger print check.
 * @param pCheck Is it enabled?
 */
void SetOpenGPGCheck(bool pCheck);
/**
 * Sets a name of database.
 * @param pDatabase A database name.
 */
void SetDatabase(const std::string& pDatabase);
/**
 * Adds one path to a GPG public key into MW's set.
 * @param pKey A path to a GPG public key.
 */
void AddOpenGPGPublicKey(const std::string& pKey);
/**
 * Adds one blacklisted package.
 */
void AddBlackListedPackage(const std::string& pPackage);
/**
 * Adds one association among alanyzer plugin and its
 * action and reporter plugins.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pActionOrReporter A name of an action or reporter plugin.
 * @param pArgs An arguments for action or reporter plugin.
 */
void AddAnalyzerActionOrReporter(const std::string& pAnalyzer,
                                 const std::string& pActionOrReporter,
                                 const std::string& pArgs);
/**
 * Add action and reporter plugins, which are activated
 * when any crash occurs.
 * @param pActionOrReporter A name of an action or reporter plugin.
 * @param pArgs An arguments for action or reporter plugin.
 */
void AddActionOrReporter(const std::string& pActionOrReporter,
                         const std::string& pArgs);


#endif /*MIDDLEWARE_H_*/
