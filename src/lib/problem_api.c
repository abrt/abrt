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

        struct dump_dir *dd = dd_opendir(full_name,   DD_OPEN_FD_ONLY
                                                    | DD_FAIL_QUIETLY_ENOENT
                                                    | DD_FAIL_QUIETLY_EACCES);
        if (dd == NULL)
        {
            VERB2 perror_msg("can't open problem directory '%s'", full_name);
            free(full_name);
            continue;
        }

        if (caller_uid == -1 || dd_accessible_by_uid(dd, caller_uid))
        {
            /* Silently ignore *any* errors, not only EACCES.
             * We saw "lock file is locked by process PID" error
             * when we raced with wizard.
             */
            int sv_logmode = logmode;
            /* Silently ignore errors only in the silent log level. */
            logmode = g_verbose == 0 ? 0: sv_logmode;
            dd = dd_fdopendir(dd, DD_OPEN_READONLY | DD_DONT_WAIT_FOR_LOCK);
            logmode = sv_logmode;
            if (dd)
                brk = callback ? callback(dd, arg) : 0;
        }

        if (dd)
            dd_close(dd);

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
    if (!dir_has_correct_permissions(dd->dd_dirname, DD_PERM_DAEMONS))
    {
        log_warning("Ignoring '%s': invalid owner, group or mode", dd->dd_dirname);
        /*Do not break*/
        return 0;
    }

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

/* get_problem_dirs_not_accessible_by_uid and its helpers */
struct add_dirname_to_GList_if_not_accessible_args
{
    uid_t uid;
    GList *list;
};

static int add_dirname_to_GList_if_not_accessible(struct dump_dir *dd, void *args)
{
    struct add_dirname_to_GList_if_not_accessible_args *param = (struct add_dirname_to_GList_if_not_accessible_args *)args;
    /* Append if not accessible */
    if (!dump_dir_accessible_by_uid(dd->dd_dirname, param->uid))
        param->list = g_list_prepend(param->list, xstrdup(dd->dd_dirname));

    return 0;
}

GList *get_problem_dirs_not_accessible_by_uid(uid_t uid, const char *dump_location)
{
    struct add_dirname_to_GList_if_not_accessible_args args = {
        .uid = uid,
        .list = NULL,
    };

    for_each_problem_in_dir(dump_location, /*disable default uid check*/-1, add_dirname_to_GList_if_not_accessible, &args);
    return g_list_reverse(args.list);
}


/* get_problem_storages */

GList *get_problem_storages(void)
{
    GList *paths = NULL;
    load_abrt_conf();
    paths = g_list_append(paths, xstrdup(g_settings_dump_location));
    free_abrt_conf_data();

    return paths;
}

int problem_dump_dir_is_complete(struct dump_dir *dd)
{
    return dd_exist(dd, FILENAME_COUNT);
}
