/*
    MiddleWare.cpp

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
#include "abrtlib.h"
#include "comm_layer_inner.h"
#include "CommLayerServerDBus.h"
#include "MiddleWare.h"

static problem_data_t *FillCrashInfo(const char *dump_dir_name);


struct logging_state {
    char *last_line;
};

static char *do_log_and_save_line(char *log_line, void *param)
{
    struct logging_state *l_state = (struct logging_state *)param;

    VERB1 log("%s", log_line);
    update_client("%s", log_line);
    free(l_state->last_line);
    l_state->last_line = log_line;
    return NULL;
}


/* We need to share some data between LoadDebugDump and is_crash_a_dup: */
struct cdump_state {
    char *uid;                   /* filled by LoadDebugDump */
    char *uuid;                  /* filled by is_crash_a_dup */
    char *crash_dump_dup_name;   /* filled by is_crash_a_dup */
};

static int is_crash_a_dup(const char *dump_dir_name, void *param)
{
    struct cdump_state *state = (struct cdump_state *)param;

    if (state->uuid)
        return 0; /* we already checked it, don't do it again */

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 0; /* wtf? (error, but will be handled elsewhere later) */
    state->uuid = dd_load_text_ext(dd, FILENAME_UUID,
                DD_FAIL_QUIETLY_ENOENT + DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
    );
    dd_close(dd);
    if (!state->uuid)
    {
        return 0; /* no uuid (yet), "run_event, please continue iterating" */
    }

    /* Scan crash dumps looking for a dup */
//TODO: explain why this is safe wrt concurrent runs
    DIR *dir = opendir(DEBUG_DUMPS_DIR);
    if (dir != NULL)
    {
        struct dirent *dent;
        while ((dent = readdir(dir)) != NULL)
        {
            if (dot_or_dotdot(dent->d_name))
                continue; /* skip "." and ".." */

            int different;
            char *uid, *uuid;
            char *dump_dir_name2 = concat_path_file(DEBUG_DUMPS_DIR, dent->d_name);

            if (strcmp(dump_dir_name, dump_dir_name2) == 0)
                goto next; /* we are never a dup of ourself */

            dd = dd_opendir(dump_dir_name2, /*flags:*/ DD_FAIL_QUIETLY_ENOENT);
            if (!dd)
                goto next;
            uid = dd_load_text(dd, FILENAME_UID);
            uuid = dd_load_text(dd, FILENAME_UUID);
            dd_close(dd);
            different = strcmp(state->uid, uid) || strcmp(state->uuid, uuid);
            free(uid);
            free(uuid);
            if (different)
                goto next;

            state->crash_dump_dup_name = dump_dir_name2;
            /* "run_event, please stop iterating": */
            return 1;

 next:
            free(dump_dir_name2);
        }
        closedir(dir);
    }

    /* No dup found */
    return 0; /* "run_event, please continue iterating" */
}

static char *do_log(char *log_line, void *param)
{
    VERB1 log("%s", log_line);
    //update_client("%s", log_line);
    return log_line;
}

mw_result_t LoadDebugDump(const char *dump_dir_name, problem_data_t **problem_data)
{
    mw_result_t res;

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return MW_ERROR;
    struct cdump_state state;
    state.uid = dd_load_text(dd, FILENAME_UID);
    state.uuid = NULL;
    state.crash_dump_dup_name = NULL;
    char *analyzer = dd_load_text(dd, FILENAME_ANALYZER);
    dd_close(dd);

    res = MW_ERROR;

    /* Run post-create event handler(s) */
    struct run_event_state *run_state = new_run_event_state();
    run_state->post_run_callback = is_crash_a_dup;
    run_state->post_run_param = &state;
    run_state->logging_callback = do_log;
    int r = run_event_on_dir_name(run_state, dump_dir_name, "post-create");
    free_run_event_state(run_state);

//TODO: consider this case:
// new dump is created, post-create detects that it is a dup,
// but then FillCrashInfo(dup_name) *FAILS*.
// In this case, we later delete damaged dup_name (right?)
// but new dump never gets its FILENAME_COUNT set!

    /* Is crash a dup? (In this case, is_crash_a_dup() should have
     * aborted "post-create" event processing as soon as it saw uuid
     * and determined that there is another crash with same uuid.
     * In this case it sets state.crash_dump_dup_name)
     */
    if (!state.crash_dump_dup_name)
    {
        /* No. Was there error on one of processing steps in run_event? */
        if (r != 0)
            goto ret; /* yes */

        /* Was uuid created after all? (In this case, is_crash_a_dup()
         * should have fetched it and created state.uuid)
         */
        if (!state.uuid)
        {
            /* no */
            log("Dump directory '%s' has no UUID element", dump_dir_name);
            goto ret;
        }
    }
    else
    {
        dump_dir_name = state.crash_dump_dup_name;
    }

    /* Loads problem_data (from the *first debugdump dir* if this one is a dup)
     * Returns:
     * MW_OCCURRED: "crash count is != 1" (iow: it is > 1 - dup)
     * MW_OK: "crash count is 1" (iow: this is a new crash, not a dup)
     * else: an error code
     */
    {
        dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (!dd)
        {
            res = MW_ERROR;
            goto ret;
        }

        /* Reset mode/uig/gid to correct values for all files created by event run */
        dd_sanitize_mode_and_owner(dd);

        /* Update count */
        char *count_str = dd_load_text_ext(dd, FILENAME_COUNT, DD_FAIL_QUIETLY_ENOENT);
        unsigned long count = strtoul(count_str, NULL, 10);
        count++;
        char new_count_str[sizeof(long)*3 + 2];
        sprintf(new_count_str, "%lu", count);
        dd_save_text(dd, FILENAME_COUNT, new_count_str);
        dd_close(dd);

        *problem_data = FillCrashInfo(dump_dir_name);
        if (*problem_data != NULL)
        {
            res = MW_OK;
            if (count > 1)
            {
                log("Dump directory is a duplicate of %s", dump_dir_name);
                res = MW_OCCURRED;
            }
        }
    }

 ret:
    free(state.uuid);
    free(state.uid);
    free(state.crash_dump_dup_name);
    free(analyzer);

    return res;
}

static problem_data_t *FillCrashInfo(const char *dump_dir_name)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;

    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
//Not needed anymore?
//    char *events = list_possible_events(dd, NULL, "");
    dd_close(dd);
//
//    add_to_problem_data_ext(problem_data, CD_EVENTS, events,
//                          CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);
//    free(events);

    add_to_problem_data_ext(problem_data, CD_DUMPDIR, dump_dir_name,
                          CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

    return problem_data;
}

/* Remove dump dir */
int DeleteDebugDump(const char *dump_dir_name, long caller_uid)
{
    /* If doesn't start with "DEBUG_DUMPS_DIR/"... */
    if (strncmp(dump_dir_name, DEBUG_DUMPS_DIR"/", strlen(DEBUG_DUMPS_DIR"/")) != 0
    /* or contains "/." anywhere (-> might contain ".." component) */
     || strstr(dump_dir_name + strlen(DEBUG_DUMPS_DIR), "/.")
    ) {
        /* Then refuse to operate on it (someone is attacking us??) */
        error_msg("Bad dump directory name '%s', not deleting", dump_dir_name);
        return MW_ERROR;
    }

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return MW_NOENT_ERROR;

    if (caller_uid != 0) /* not called by root */
    {
        char caller_uid_str[sizeof(long) * 3 + 2];
        sprintf(caller_uid_str, "%ld", caller_uid);

        char *uid = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
        /* we assume that the dump_dir can be handled by everyone if uid == NULL
         * e.g: kerneloops
         */
        if (uid != NULL)
        {
            bool uid_matches = (strcmp(uid, caller_uid_str) == 0);
            free(uid);
            if (!uid_matches)
            {
                dd_close(dd);
                error_msg("Dump directory '%s' can't be accessed by user with uid %ld", dump_dir_name, caller_uid);
                return 1;
            }
        }
    }

    dd_delete(dd);

    return 0; /* success */
}
