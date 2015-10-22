/*
 * Copyright (C) 2015  ABRT team
 * Copyright (C) 2015  RedHat Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _ABRT_XORG_UTILS_H_
#define _ABRT_XORG_UTILS_H_

#include "libabrt.h"

/* How many problem dirs to create at most?
 * Also causes cooldown sleep with -t if exceeded -
 * useful when called from a log watcher.
 */
#define ABRT_OOPS_MAX_DUMPED_COUNT  5

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ABRT_XORG_THROTTLE_CREATION = 1 << 0,
    ABRT_XORG_WORLD_READABLE    = 1 << 1,
    ABRT_XORG_PRINT_STDOUT      = 1 << 2,
};

int g_abrt_xorg_sleep_woke_up_on_signal;
int abrt_xorg_signaled_sleep(int seconds);

/*
 * Information about found xorg crash
 */
struct xorg_crash_info
{
    char *backtrace;
    char *reason;
    char *exe;
};

/*
 * Free xorg crash info data
 */
void xorg_crash_info_free(struct xorg_crash_info *crash_info);

/*
 * Skip log line prefixes
 *
 * Example:
 * "[    28.900] (EE) Foo" -> "Foo"
 * "(EE) Foo"              -> "Foo"
 * " Foo"                  -> "Foo"
 * "Foo"                   -> "Foo"
 *
 * @param str line from log file
 * @returns line without prefixes
 */
char *skip_pfx(char *str);

/*
 * Prints information about found xorg crash
 *
 * @param crash_info extracted xorg crash information
 */
void xorg_crash_info_print_crash(struct xorg_crash_info *crash_info);

/*
 * Get next line from given stream (FILE *)
 * Use as wrapper for reading function
 *
 * @param fd open FILE * as void *
 * @returns malloced and unlimited line without railing \n
 */
char *xorg_get_next_line_from_fd(void *fd);

/*
 * Process Xorg Backtrace
 * This function is called after the key word "Backtrace:" was found in log.
 *
 * @param get_next_line function used for reading lines from data
 * @param data is used as get_nex_line function parameter
 * @returns extracted xorg crash data or NULL in case of error
 */
struct xorg_crash_info *process_xorg_bt(char *(*get_next_line)(void *), void *data);

/*
 * Saves Xorg crash details in the dump directory
 *
 * @param dd The destination directory
 * @param crash_info the crash details
 * @returns non-0 value in case of an error; otherwise 0
 */
int xorg_crash_info_save_in_dump_dir(struct xorg_crash_info *crash_info, struct dump_dir *dd);

/*
 * Create dump dir from given xorg crash info data
 *
 * @param crash_info xorg crash information
 * @param path where the dump dir will be created
 * @param world_readable make the dump dir world readable
 */
void xorg_crash_info_create_dump_dir(struct xorg_crash_info *crash_info, const char *dump_location,
                                     bool world_readable);

#ifdef __cplusplus
}
#endif

#endif /*_ABRT_XORG_UTILS_H_*/
