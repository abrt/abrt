/*
 * Copyright (C) 2014  ABRT team
 * Copyright (C) 2014  RedHat Inc
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
#ifndef _ABRT_OOPS_UTILS_H_
#define _ABRT_OOPS_UTILS_H_

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
    ABRT_OOPS_THROTTLE_CREATION = 1 << 0,
    ABRT_OOPS_WORLD_READABLE    = 1 << 1,
    ABRT_OOPS_PRINT_STDOUT      = 1 << 2,
};

extern int g_abrt_oops_sleep_woke_up_on_signal;

int abrt_oops_process_list(GList *oops_list, const char *dump_location, const char *analyzer, int flags);
unsigned abrt_oops_create_dump_dirs(GList *oops_list, const char *dump_location, const char *analyzer, int flags);
void abrt_oops_save_data_in_dump_dir(struct dump_dir *dd, char *oops, const char *proc_modules);
int abrt_oops_signaled_sleep(int seconds);
char *abrt_oops_string_filter_regex(void);

#ifdef __cplusplus
}
#endif

#endif /*_ABRT_OOPS_UTILS_H_*/
