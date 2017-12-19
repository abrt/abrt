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
#include <json.h>

#include "libabrt.h"
#include "dockerd-utils.h"

int abrt_docker_container_signaled_sleep(int seconds)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGHUP);

    struct timespec timeout;
    timeout.tv_sec = seconds;
    timeout.tv_nsec = 0;

    return g_abrt_docker_container_sleep_woke_up_on_signal = sigtimedwait(&set, NULL, &timeout);
}

void container_crash_info_print_crash(const struct container_crash_info *crash_info)
{
    printf("type: %s\n", crash_info->type);
    printf("%s\n", crash_info->backtrace);
    if (crash_info->reason)
        printf("reason: %s\n", crash_info->reason);
}

void container_crash_info_free(struct container_crash_info *crash_info)
{
    if (crash_info == NULL)
        return;
    if (crash_info->private_data)
        json_object_put((json_object *) crash_info->private_data);
    free(crash_info);
}

struct container_crash_info *container_crash_info_parse_form_json_str(const char *line)
{
    struct container_crash_info *crash_info = xmalloc(sizeof(struct container_crash_info));
    json_object *pid, *executable, *reason, *backtrace;

    json_object *json = json_tokener_parse(line);
    if (is_error(json))
    {
        log_warning("Failed to parse docker container crash json from journal");
        goto error;
    }
    crash_info->private_data = json;

    if (!json_object_object_get_ex(json, "ABRT", &json))
    {
        log_warning("Failed to parse mandatory json object \"ABRT\"");
        goto error;
    }

    if (!json_object_object_get_ex(json, "reason", &reason))
    {
        log_warning("Failed to parse mandatory json object \"reason\"");
        goto error;
    }
    crash_info->reason     = json_object_get_string(reason);

    if (!json_object_object_get_ex(json, "backtrace", &backtrace))
    {
        log_warning("Failed to parse mandatory json object \"backtrace\"");
        goto error;
    }
    crash_info->backtrace = json_object_get_string(backtrace);

    crash_info->type = xstrdup("placeholder"); /// temp TODO replace when json will contain a type
    crash_info->uid = 9; // placeholder

    json_object_object_get_ex(json, "pid", &pid);
    crash_info->pid        = json_object_get_int(pid);

    json_object_object_get_ex(json, "executable", &executable);
    crash_info->executable = json_object_get_string(executable);

    return crash_info;

error:
    container_crash_info_free(crash_info);
    return NULL;
}

int docker_container_crash_info_save_in_dump_dir(struct dump_dir *dd, struct container_crash_info *crash_info)
{
    char *pid = xasprintf("%d", crash_info->pid);

    dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);
    dd_save_text(dd, FILENAME_PID, pid);
    dd_save_text(dd, FILENAME_TYPE, crash_info->type);
    dd_save_text(dd, FILENAME_EXECUTABLE, crash_info->executable);
    dd_save_text(dd, FILENAME_REASON, crash_info->reason);
    dd_save_text(dd, FILENAME_BACKTRACE, crash_info->backtrace);

    free(pid);
    return 0;
}

void docker_container_crash_create_dump_dir(struct container_crash_info *crash_info,
                                            const char *dump_location, bool world_readable)
{
    struct dump_dir *dd = create_dump_dir(dump_location, crash_info->type, /*fs owner*/0,
                                          (save_data_call_back)docker_container_crash_info_save_in_dump_dir,
                                          crash_info);

    if (dd == NULL)
        return;

    if (world_readable)
        dd_set_no_owner(dd);

    char *path = xstrdup(dd->dd_dirname);
    dd_close(dd);
    notify_new_path(path);
    free(path);
}
