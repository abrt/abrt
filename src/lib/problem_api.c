/*
    Copyright (C) ABRT Team
    Copyright (C) RedHat inc.

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

#include <glib.h>
#include <sys/time.h>
#include "problem_api.h"

/*
 * Evaluates a NULL-terminated list of problem conditions as a logical conjunction
 */
static bool problem_condition_evaluate_and(struct dump_dir *dd,
                                           const struct problem_condition *const *condition)
{
    /* We stop on the first FALSE condition */
    while (condition && *condition != NULL)
    {
        const struct problem_condition *c = *condition;
        char *field_data = dd_load_text(dd, c->field_name);
        bool value = c->evaluate(field_data, c->args);
        free(field_data);
        if (!value)
            return false;
        ++condition;
    }

    return true;
}

/*
 * Goes through all problems and selects only problems accessible by caller_uid and
 * problems for which an and_filter gets TRUE
 *
 * @param condition a NULL-terminated list of problem conditions evaluated
 * as conjunction, can be NULL (means always TRUE)
 */
static GList* scan_directory(const char *path,
                             uid_t caller_uid,
                             const struct problem_condition *const *condition)
{
    GList *list = NULL;

    DIR *dp = opendir(path);
    if (!dp)
    {
        /* We don't want to yell if, say, $XDG_CACHE_DIR/abrt/spool doesn't exist */
        //perror_msg("Can't open directory '%s'", path);
        return list;
    }

    struct dirent *dent;
    while ((dent = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue; /* skip "." and ".." */

        char *full_name = concat_path_file(path, dent->d_name);
        if (dump_dir_accessible_by_uid(full_name, caller_uid))
        {
            /* Silently ignore *any* errors, not only EACCES.
             * We saw "lock file is locked by process PID" error
             * when we raced with wizard.
             */
            int sv_logmode = logmode;
            logmode = 0;
            struct dump_dir *dd = dd_opendir(full_name, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES | DD_DONT_WAIT_FOR_LOCK);
            logmode = sv_logmode;
            /* or we could just setuid?
             - but it would require locking, because we want to setuid back before we server another request..
            */
            if (dd)
            {
                if (!condition || problem_condition_evaluate_and(dd, condition))
                {
                    list = g_list_prepend(list, full_name);
                    full_name = NULL;
                }
                dd_close(dd); //doesn't fail even if dd == NULL
            }
        }
        free(full_name);
    }
    closedir(dp);

    /* Why reverse?
     * Because N*prepend+reverse is faster than N*append
     */
    return g_list_reverse(list);
}

/* Self explaining time interval structure */
struct time_interval
{
    unsigned long from;
    unsigned long to;
};

/*
 * A problem condition evaluate function for checking of the TIME field against
 * an allowed interval
 *
 * @param field_data a content from the PID field
 * @param args a pointer to an instance of struct time_interval
 * @return TRUE if a field value is in a specified interval; otherwise FALSE
 */
static bool time_interval_problem_condition(const char *field_data, const void *args)
{
    const struct time_interval *const interval = (const struct time_interval *)args;
    const time_t timestamp = atol(field_data);

    return interval->from <= timestamp && timestamp <= interval->to;
}

/*
 * A problem condition evaluate function passed if strings are equal
 *
 * @param field_data a content of a field
 * @param args a checked string
 * @return TRUE if both strings are equal; otherwise FALSE
 */
static bool equal_string_problem_condition(const char *field_data, const void *args)
{
    return !strcmp(field_data, (const char *)args);
}

GList *get_problem_dirs_for_uid(uid_t uid, const char *dump_location)
{
    GList *dirs = scan_directory(dump_location, uid, NULL);
    return dirs;
}

/*
 * Finds problems which were created in the interval
 */
GList *get_problem_dirs_for_element_in_time(uid_t uid,
                                                      const char *element,
                                                      const char *value,
                                                      unsigned long timestamp_from,
                                                      unsigned long timestamp_to,
                                                      const char *dump_location)
{
    struct timeval tv;
    /* use the current time if timestamp_to is 0 */
    if (timestamp_to == 0) {
        gettimeofday(&tv, NULL);
        timestamp_to = tv.tv_sec;
    }


    const struct problem_condition elementc = {
        .field_name = element,
        .args = value,
        .evaluate = equal_string_problem_condition,
    };

    const struct time_interval interval = {
        .from = timestamp_from,
        .to = timestamp_to,
    };

    const struct problem_condition timec = {
        .field_name = FILENAME_TIME,
        .args = &interval,
        .evaluate = time_interval_problem_condition
    };

    const struct problem_condition *const condition[] = {
        &timec,
        element ? &elementc : NULL,
        NULL
    };
    GList *dirs = scan_directory(dump_location, uid, condition);
    return dirs;
}

GList *get_problem_storages()
{
    GList *pths = NULL;
    load_abrt_conf();
    pths = g_list_append(pths, xstrdup(g_settings_dump_location));
    //no needed, we don't steal directories anymore
    pths = g_list_append(pths, concat_path_file(g_get_user_cache_dir(), "abrt/spool"));
    free_abrt_conf_data();

    return pths;
}


typedef struct problem_count_info {
    int problem_count;
    unsigned long since;
    unsigned long until;

} problem_count_info_t;

static void count_problems_in_dir(char *path, gpointer problem_counter)
{
    problem_count_info_t *pci = (problem_count_info_t *)problem_counter;
    VERB2 log("scanning '%s' for problems since %lu", path, pci->since);
    GList *problems = get_problem_dirs_for_element_in_time(getuid(), NULL /*don't filter by element*/, NULL, pci->since, pci->until, path);
    pci->problem_count += g_list_length(problems);
    list_free_with_free(problems);
}

unsigned int get_problems_count(GList *paths, unsigned long since)
{
    GList *pths = NULL;

    if (paths == NULL)
    {
        pths = get_problem_storages();
    }

    problem_count_info_t pci;

    pci.problem_count = 0;
    pci.since = since;
    pci.until = 0;

    g_list_foreach(paths ? paths : pths, (GFunc)count_problems_in_dir, &pci);

    list_free_with_free(pths);

    return pci.problem_count;
}