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
 * Goes through all problems and for problems accessible by caller_uid
 * calls callback. If callback returns non-0, returns that value.
 */
int for_each_problem_in_dir(const char *path,
                        uid_t caller_uid,
                        int (*callback)(struct dump_dir *dd, void *arg),
                        void *arg)
{
    DIR *dp = opendir(path);
    if (!dp)
    {
        /* We don't want to yell if, say, $XDG_CACHE_DIR/abrt/spool doesn't exist */
        //perror_msg("Can't open directory '%s'", path);
        return 0;
    }

    int brk = 0;
    struct dirent *dent;
    while ((dent = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue; /* skip "." and ".." */

        char *full_name = concat_path_file(path, dent->d_name);

        int dir_fd = dd_openfd(full_name);
        if (dir_fd < 0)
        {
            VERB2 perror_msg("can't open problem directory '%s'", full_name);
            free(full_name);
            continue;
        }

        if (dump_dir_accessible_by_uid(full_name, caller_uid))
        {
            /* Silently ignore *any* errors, not only EACCES.
             * We saw "lock file is locked by process PID" error
             * when we raced with wizard.
             */
            int sv_logmode = logmode;
            logmode = 0;
            struct dump_dir *dd = dd_fdopendir(dir_fd, full_name, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES | DD_DONT_WAIT_FOR_LOCK);
            logmode = sv_logmode;
            if (dd)
            {
                brk = callback ? callback(dd, arg) : 0;
                dd_close(dd);
            }
        }
        else
            close(dir_fd);

        free(full_name);
        if (brk)
            break;
    }
    closedir(dp);

    return brk;
}

/* get_problem_dirs_for_uid and its helpers */

static int add_dirname_to_GList(struct dump_dir *dd, void *arg)
{
    GList **list = arg;
    *list = g_list_prepend(*list, xstrdup(dd->dd_dirname));
    return 0;
}

GList *get_problem_dirs_for_uid(uid_t uid, const char *dump_location)
{
    GList *list = NULL;
    for_each_problem_in_dir(dump_location, uid, add_dirname_to_GList, &list);
    /*
     * Why reverse?
     * Because N*prepend+reverse is faster than N*append
     */
    return g_list_reverse(list);
}


/* get_problem_dirs_for_element_in_time and its helpers */

struct field_and_time_range {
    GList *list;
    const char *element;
    const char *value;
    unsigned long timestamp_from;
    unsigned long timestamp_to;
};

static int add_dirname_to_GList_if_matches(struct dump_dir *dd, void *arg)
{
    struct field_and_time_range *me = arg;

    char *field_data;

    if (me->element)
    {
        field_data = dd_load_text(dd, me->element);
        int brk = (strcmp(field_data, me->value) != 0);
        free(field_data);
        if (brk)
            return 0;
    }

    field_data = dd_load_text(dd, FILENAME_LAST_OCCURRENCE);
    long val = atol(field_data);
    free(field_data);
    if (val < me->timestamp_from || val > me->timestamp_to)
        return 0;

    me->list = g_list_prepend(me->list, xstrdup(dd->dd_dirname));
    return 0;
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
    if (timestamp_to == 0)
        timestamp_to = time(NULL);

    struct field_and_time_range me = {
        .list = NULL,
        .element = element,
        .value = value,
        .timestamp_from = timestamp_from,
        .timestamp_to = timestamp_to,
    };

    for_each_problem_in_dir(dump_location, uid, add_dirname_to_GList_if_matches, &me);

    return g_list_reverse(me.list);
}

/* get_problem_storages */

GList *get_problem_storages(void)
{
    GList *pths = NULL;
    load_abrt_conf();
    pths = g_list_append(pths, xstrdup(g_settings_dump_location));
    //not needed, we don't steal directories anymore
    pths = g_list_append(pths, concat_path_file(g_get_user_cache_dir(), "abrt/spool"));
    free_abrt_conf_data();

    return pths;
}

/* get_problems_count and its helpers */

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
        paths = pths = get_problem_storages();
    }

    problem_count_info_t pci;
    pci.problem_count = 0;
    pci.since = since;
    pci.until = time(NULL);

    g_list_foreach(paths, (GFunc)count_problems_in_dir, &pci);

    list_free_with_free(pths);

    return pci.problem_count;
}
