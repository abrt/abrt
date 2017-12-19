/*
 * Copyright (C) 2017  ABRT team
 * Copyright (C) 2017  RedHat Inc
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
#ifndef _ABRT_DOCKERD_UTILS_H_
#define _ABRT_DOCKERD_UTILS_H_

#include "libabrt.h"

/* How many problem dirs to create at most?
 * Also causes cooldown sleep with -t if exceeded -
 * useful when called from a log watcher.
 */
#define ABRT_DOCKERD_MAX_DUMPED_COUNT  5

#ifdef __cplusplus
extern "C" {
#endif

#define DOCKERD_LOG_SEARCH_STRING "{\"ABRT\":"

enum {
    ABRT_DOCKERD_THROTTLE_CREATION = 1 << 0,
    ABRT_DOCKERD_WORLD_READABLE    = 1 << 1,
    ABRT_DOCKERD_PRINT_STDOUT      = 1 << 2,
};

/*
 * Information about crash in container
 */
struct container_crash_info {
    int         pid;
    int         uid;
    const char *executable;
    const char *reason;
    const char *backtrace;
    const char *type;
    /* for purpose of freeing container_crash_info contents */
    void *private_data;
};

int g_abrt_docker_container_sleep_woke_up_on_signal;
int abrt_docker_container_signaled_sleep(int seconds);

/*
 * Prints information about found container crash
 *
 * @param crash_info extracted container crash information
 */
void container_crash_info_print_crash(const struct container_crash_info *crash_info);

/*
 * Free container crash info
 */
void container_crash_info_free(struct container_crash_info *crash_info);

/*
 * Parses crash obtained from docker in form of json string
 *
 * @param line containing json string
 * @returns parsed information or NULL on failure, should be freed by container_crash_info_free
 */
struct container_crash_info *container_crash_info_parse_form_json_str(const char *line);

/*
 * Create dump dir from given json crash data
 *
 * @param crash_info the crash details
 * @param dump_location path where the dump dir will be created
 * @param world_readable make the dump dir world readable
 */
void docker_container_crash_create_dump_dir(struct container_crash_info *crash_info,
                                            const char *dump_location, bool world_readable);

#ifdef __cplusplus
}
#endif

#endif /*_ABRT_DOCKERD_UTILS_H_*/
