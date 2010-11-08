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

#include "abrt_types.h"
#include "PluginManager.h"

/**
 * An enum contains all return codes.
 */
typedef enum {
    MW_OK,               /**< No error.*/
    MW_OCCURRED,         /**< A not-yet-reported dup.*/
    MW_REPORTED,         /**< A reported dup.*/
    MW_ERROR,            /**< Common error.*/
    MW_CORRUPTED,        /**< Debugdump directory is corrupted.*/
    MW_GPG_ERROR,        /**< Package is not signed properly.*/
    MW_IN_DB,            /**< Debugdump directory is already saved in a database.*/
    MW_IN_DB_ERROR,      /**< Error while working with a database.*/
    MW_PLUGIN_ERROR,     /**< plugin wasn't found or error within plugin*/
} mw_result_t;

typedef enum {
    RS_CODE,
    RS_MESSAGE
} report_status_items_t;


/**
 * Takes care of getting all additional data needed
 * for computing UUIDs and creating a report for particular analyzer
 * plugin. This report could be send somewhere afterwards. If a creation
 * is successful, then  a crash report is filled.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpPath A debugdump dir containing all necessary data.
 * @param pCrashData A filled crash report.
 * @return It return results of operation. See mw_result_t.
 */
mw_result_t CreateCrashReport(const char *crash_id,
                              long caller_uid,
                              int force,
                              map_crash_data_t& pCrashData);
/**
 * Activates particular action plugin.
 * @param pActionDir A directory, which is passed as working to a action plugin.
 * @param pPluginName An action plugin name.
 * @param pPluginArgs Action plugin's arguments.
 */
void RunAction(const char *pActionDir,
               const char *pPluginName,
               const char *pPluginArgs);
/**
 * Reports a crash report to particular receiver. It
 * takes an user uid, tries to find user config file and load it. If it
 * fails, then default config is used. If pUID is emply string, default
 * config is used.
 * ...).
 * @param crash_data
 *  A crash report.
 * @param reporters
 *  List of allowed reporters. Which reporters will be used depends
 *  on the analyzer of the crash_data. Reporters missing from this list
 *  will not be used.
 * @param caller_uid
 *  An user uid.
 * @return
 *  A report status, which reporters ends successfuly with messages.
 */
report_status_t Report(const map_crash_data_t& crash_data,
                       const vector_string_t& reporters,
                       const map_map_string_t& settings,
                       long caller_uid);
/**
 * Adds package name and description to debugdump dir.
 * Saves debugdump into database.
 * Detects whether it's a duplicate crash.
 * Fills crash info.
 * Note that if it's a dup, loads _first crash_ info, not this one's.
 * @param pDebugDumpDir A debugdump directory.
 * @param pCrashData A crash info.
 * @return It return results of operation. See mw_result_t.
 */
mw_result_t SaveDebugDump(const char *pDebugDumpDir,
                        map_crash_data_t& pCrashData);
/**
 * Get one crash info. If getting is successful,
 * then crash info is filled.
 * @param pUUID A local UUID of a crash.
 * @param pUID An UID of an user.
 * @param pCrashData A crash info.
 * @return It return results of operation. See mw_result_t.
 */
mw_result_t FillCrashInfo(const char *crash_id,
                        map_crash_data_t& pCrashData);
/**
 * Gets all local UUIDs and UIDs of crashes. These crashes
 * occurred when a particular user was logged in.
 * @param pUID an UID of an user.
 * @return A vector of pairs  (local UUID, UID).
 */
void GetUUIDsOfCrash(long caller_uid, vector_string_t &result);
/**
 * Adds one association among alanyzer plugin and its
 * action and reporter plugins.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pActionOrReporter A name of an action or reporter plugin.
 * @param pArgs An arguments for action or reporter plugin.
 */
void AddAnalyzerActionOrReporter(const char *pAnalyzer,
                                 const char *pActionOrReporter,
                                 const char *pArgs);

bool analyzer_has_InformAllUsers(const char *analyzer_name);
#endif /*MIDDLEWARE_H_*/
