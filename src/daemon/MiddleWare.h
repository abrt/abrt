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

/**
 * An enum contains all return codes.
 */
typedef enum {
    MW_OK,               /**< No error.*/
    MW_OCCURRED,         /**< No error, but thus dump is a dup.*/
    MW_ERROR,            /**< Common error.*/
    MW_NOENT_ERROR,
    MW_PERM_ERROR,
    MW_CORRUPTED,        /**< Debugdump directory is corrupted.*/
    MW_GPG_ERROR,        /**< Package is not signed properly.*/
    MW_PLUGIN_ERROR,     /**< plugin wasn't found or error within plugin*/
} mw_result_t;

typedef enum {
    RS_CODE,
    RS_MESSAGE
} report_status_items_t;


/**
 * Reports a crash report to particular receiver. It
 * takes an user uid, tries to find user config file and load it. If it
 * fails, then default config is used. If pUID is emply string, default
 * config is used.
 * ...).
 * @param crash_data
 *  A crash report.
 * @param events
 *  List of events to run.
 * @param caller_uid
 *  An user uid.
 * @return
 *  A report status: which events finished successfully, with messages.
 */
report_status_t Report(crash_data_t *crash_data,
                       const vector_string_t& events,
                       const map_map_string_t& settings,
                       long caller_uid);
/**
 * Detects whether it's a duplicate crash dump.
 * Fills crash info.
 * Note that if it's a dup, loads _first crash_ info, not this one's.
 * @param dump_dir_name A debugdump directory.
 * @param pCrashData A crash info.
 * @return It return results of operation. See mw_result_t.
 */
mw_result_t LoadDebugDump(const char *dump_dir_name, crash_data_t **crash_data);

vector_of_crash_data_t *GetCrashInfos(long caller_uid);
int  CreateReportThread(const char* dump_dir_name, long caller_uid, int force, const char* pSender);
void CreateReport(const char* dump_dir_name, long caller_uid, int force, crash_data_t **crash_data);
int  DeleteDebugDump(const char *dump_dir_name, long caller_uid);

#endif /*MIDDLEWARE_H_*/
