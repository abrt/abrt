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

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * Detects whether it's a duplicate crash dump.
 * Fills crash info.
 * Note that if it's a dup, loads _first crash_ info, not this one's.
 * @param dump_dir_name A debugdump directory.
 * @param pCrashData A crash info.
 * @return It return results of operation. See mw_result_t.
 */
mw_result_t LoadDebugDump(const char *dump_dir_name, crash_data_t **crash_data);

int DeleteDebugDump(const char *dump_dir_name, long caller_uid);

#ifdef __cplusplus
}
#endif

#endif /*MIDDLEWARE_H_*/
