/*
    Copyright (C) 2009  Abrt team.
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
#ifndef RUN_EVENT_H_
#define RUN_EVENT_H_

#include "crash_data.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dump_dir;

struct run_event_state {
    /* Used only for post-create dup detection. TODO: document its API */
    int (*post_run_callback)(const char *dump_dir_name, void *param);
    void *post_run_param;
    /* Can take ownership of log_line, which is malloced. In this case, return NULL.
     * Otherwise should return log_line (it will be freed by caller)
     */
    char* (*logging_callback)(char *log_line, void *param);
    void *logging_param;
};
struct run_event_state *new_run_event_state(void);
void free_run_event_state(struct run_event_state *state);

/* Returns exitcode of first failed action, or first nonzero return value
 * of post_run_callback. If all actions are successful, returns 0.
 * If no actions were run for the event, returns -1.
 */
int run_event_on_dir_name(struct run_event_state *state, const char *dump_dir_name, const char *event);
int run_event_on_crash_data(struct run_event_state *state, crash_data_t *data, const char *event);
char *list_possible_events(struct dump_dir *dd, const char *dump_dir_name, const char *pfx);

#ifdef __cplusplus
}
#endif

#endif
