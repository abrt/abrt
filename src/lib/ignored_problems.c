/*
    Copyright (C) 2013  ABRT Team
    Copyright (C) 2013  RedHat inc.

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

#include "libabrt.h"

#define IGN_COLUMN_DELIMITER ';'
#define IGN_DD_OPEN_FLAGS (DD_OPEN_READONLY | DD_FAIL_QUIETLY_ENOENT | DD_FAIL_QUIETLY_EACCES)
#define IGN_DD_LOAD_TEXT_FLAGS (DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE | DD_FAIL_QUIETLY_ENOENT | DD_FAIL_QUIETLY_EACCES)

struct ignored_problems
{
    char *ign_set_file_path;
};

ignored_problems_t *ignored_problems_new(char *set_file_path)
{
    ignored_problems_t *set = xmalloc(sizeof(*set));
    set->ign_set_file_path = set_file_path;
    return set;
}

void ignored_problems_free(ignored_problems_t *set)
{
    if (!set)
        return;
    free(set->ign_set_file_path);
    free(set);
}

static bool ignored_problems_eq(ignored_problems_t *set,
        const char *problem_id, const char *uuid, const char *duphash,
        const char *line, unsigned line_num)
{
    const char *ignored = line;
    const char *ignored_end = strchrnul(ignored, IGN_COLUMN_DELIMITER);
    size_t sz = ignored_end - ignored;
    if (sz == strlen(problem_id) && strncmp(problem_id, ignored, sz) == 0)
    {
        VERB1 log("Ignored id matches '%s'", problem_id);
        return true;
    }

    if (ignored_end[0] == '\0')
    {
        VERB1 log("No 2nd column (UUID) at line %d in ignored problems file '%s'",
                line_num, set->ign_set_file_path);
        return false;
    }
    ignored = ignored_end + 1;
    ignored_end = strchrnul(ignored, IGN_COLUMN_DELIMITER);
    sz = ignored_end - ignored;
    if (uuid != NULL && sz == strlen(uuid) && strncmp(uuid, ignored, sz) == 0)
    {
        VERB1 log("Ignored uuid '%s' matches uuid of problem '%s'", ignored, problem_id);
        return true;
    }

    if (ignored_end[0] == '\0')
    {
        VERB1 log("No 3rd column (DUPHASH) at line %d in ignored problems file '%s'",
                line_num, set->ign_set_file_path);
        return false;
    }
    ignored = ignored_end + 1;
    ignored_end = strchrnul(ignored, IGN_COLUMN_DELIMITER);
    sz = ignored_end - ignored;
    if (duphash != NULL && sz == strlen(duphash) && strncmp(duphash, ignored, sz) == 0)
    {
        VERB1 log("Ignored duphash '%s' matches duphash of problem '%s'", ignored, problem_id);
        return true;
    }

    return false;
}

void ignored_problems_add(ignored_problems_t *set, const char *problem_id)
{
#if 1
    /* Hmm, do we really want to open/lock/read/unlock/close dd *twice*?
     * (ignored_problems_contains does that once, then we do it again).
     */
    if (ignored_problems_contains(set, problem_id))
    {
        VERB1 log("Won't add problem '%s' to ignored problems:"
                " it is already there", problem_id);
        return;
    }
#endif

    struct dump_dir *dd = dd_opendir(problem_id, IGN_DD_OPEN_FLAGS);
    if (!dd)
    {
        /* We do not consider this as an error because the directory can be
         * deleted by other programs. This code expects that dd_opendir()
         * already emitted good explanatory message. This message
         * explains what the previous failure causes.
         */
        VERB1 log("Can't add problem '%s' to ignored problems:"
                " can't open the problem", problem_id);
        return;
    }
    char *uuid = dd_load_text_ext(dd, FILENAME_UUID, IGN_DD_LOAD_TEXT_FLAGS);
    char *duphash = dd_load_text_ext(dd, FILENAME_DUPHASH, IGN_DD_LOAD_TEXT_FLAGS);
    dd_close(dd);

    FILE *fp = fopen(set->ign_set_file_path, "a");
    if (!fp)
    {
        /* This is not a fatal problem. We are permissive because we don't want
         * to scare users by strange error messages.
         */
        VERB1 perror_msg("Can't open ignored problems '%s'"
                " for adding problem '%s'",
                set->ign_set_file_path, problem_id);
        goto ret_add_free_hashes;
    }
    /* We can add write error checks here.
     * However, what exactly can we *do* if we detect it?
     */
    fprintf(fp, "%s;%s;%s\n", problem_id, (uuid ? uuid : ""),
                              (duphash ? duphash : ""));
    fclose(fp);

 ret_add_free_hashes:
    free(duphash);
    free(uuid);
}

void ignored_problems_remove(ignored_problems_t *set, const char *problem_id)
{
#if 1
    /* Hmm, do we really want to open/lock/read/unlock/close dd *twice*?
     * (ignored_problems_contains does that once, then we do it again).
     */
    if (!ignored_problems_contains(set, problem_id))
    {
        VERB1 log("Won't remove problem '%s' from ignored problems:"
                  " it is already removed", problem_id);
        return;
    }
#endif

    VERB1 log("Removing problem '%s' from ignored problems", problem_id);

    char *uuid = NULL;
    char *duphash = NULL;
    struct dump_dir *dd = dd_opendir(problem_id, IGN_DD_OPEN_FLAGS);
    if (dd)
    {
        uuid = dd_load_text_ext(dd, FILENAME_UUID, IGN_DD_LOAD_TEXT_FLAGS);
        duphash = dd_load_text_ext(dd, FILENAME_DUPHASH, IGN_DD_LOAD_TEXT_FLAGS);
        dd_close(dd);
    }
    else
    {
        /* We do not consider this as an error because the directory can be
         * deleted by other programs. This code expects that dd_opendir()
         * already emitted good explanatory message. This message
         * explains what the previous failure causes.
         */
        VERB1 error_msg("Can't get UUID/DUPHASH from"
                " '%s' to remove it from the ignored problems:"
                " can't open the problem", problem_id);
    }

    FILE *orig_fp = fopen(set->ign_set_file_path, "r");
    if (!orig_fp)
    {
        /* This is not a fatal problem. We are permissive because we don't want
         * to scare users by strange error messages.
         */
        VERB1 perror_msg("Can't open '%s' for removal of problem '%s'",
                set->ign_set_file_path, problem_id);
        goto ret_free_hashes;
    }

    char *new_tempfile_name = xasprintf("%s.XXXXXX", set->ign_set_file_path);
    int new_tempfile_fd = mkstemp(new_tempfile_name);
    if (new_tempfile_fd < 0)
    {
        perror_msg(_("Can't create temporary file '%s'"), set->ign_set_file_path);
        goto ret_close_files;
    }

    unsigned line_num = 0;
    char *line;
    while ((line = xmalloc_fgetline(orig_fp)) != NULL)
    {
        ++line_num;
        if (!ignored_problems_eq(set, problem_id, uuid, duphash, line, line_num))
        {
            ssize_t len = strlen(line);
            line[len] = '\n';
            if (full_write(new_tempfile_fd, line, len + 1) < 0)
            {
                /* Probably out of space */
                line[len] = '\0';
                perror_msg(_("Can't write to '%s'."
                        " Problem '%s' will not be removed from the ignored"
                        " problems '%s'"),
                        new_tempfile_name, problem_id, set->ign_set_file_path);
                free(line);
                goto ret_unlink_new;
            }
        }
        free(line);
    }

    if (rename(new_tempfile_name, set->ign_set_file_path) < 0)
    {
        /* Something nefarious happened */
        perror_msg(_("Can't rename '%s' to '%s'. Failed to remove problem '%s'"),
                set->ign_set_file_path, new_tempfile_name, problem_id);
 ret_unlink_new:
        unlink(new_tempfile_name);
    }

 ret_close_files:
    fclose(orig_fp);
    if (new_tempfile_fd >= 0)
        close(new_tempfile_fd);
    free(new_tempfile_name);

 ret_free_hashes:
    free(duphash);
    free(uuid);
}

bool ignored_problems_contains(ignored_problems_t *set, const char *problem_id)
{
    struct dump_dir *dd = dd_opendir(problem_id, IGN_DD_OPEN_FLAGS);
    if (!dd)
    {
        /* We do not consider this as an error because the directory can be
         * deleted by other programs. This code expects that dd_opendir()
         * already emitted good and explanatory message. This message attempts
         * to explain what the previous failure causes.
         */
        VERB1 error_msg("Can't open '%s'."
                " Won't try to check whether it belongs to ignored problems",
                problem_id);
        return false;
    }
    char *uuid = dd_load_text_ext(dd, FILENAME_UUID, IGN_DD_LOAD_TEXT_FLAGS);
    char *duphash = dd_load_text_ext(dd, FILENAME_DUPHASH, IGN_DD_LOAD_TEXT_FLAGS);
    dd_close(dd);

    bool found = false;
    FILE *fp = fopen(set->ign_set_file_path, "r");
    if (!fp)
    {
        /* This is not a fatal problem. We are permissive because we don't want
         * to scare users by strange error messages.
         */
        VERB1 perror_msg("Can't open '%s' and determine"
                   " whether problem '%s' belongs to it",
                   set->ign_set_file_path, problem_id);
        goto ret_contains_free_hashes;
    }

    unsigned line_num = 0;
    while (!found)
    {
        char *line = xmalloc_fgetline(fp);
        if (!line)
            break;
        ++line_num;
        found = ignored_problems_eq(set, problem_id, uuid, duphash, line, line_num);
        free(line);
    }

    fclose(fp);

 ret_contains_free_hashes:
    free(duphash);
    free(uuid);

    return found;
}
