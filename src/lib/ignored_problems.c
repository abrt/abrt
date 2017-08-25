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

#include "internal_libabrt.h"

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
    if (strncmp(problem_id, ignored, sz) == 0 && problem_id[sz] == '\0')
    {
        log_notice("Ignored id matches '%s'", problem_id);
        return true;
    }

    if (ignored_end[0] == '\0')
    {
        log_notice("No 2nd column (UUID) at line %d in ignored problems file '%s'",
                line_num, set->ign_set_file_path);
        return false;
    }
    ignored = ignored_end + 1;
    ignored_end = strchrnul(ignored, IGN_COLUMN_DELIMITER);
    sz = ignored_end - ignored;
    if (uuid != NULL && strncmp(uuid, ignored, sz) == 0 && uuid[sz] == '\0')
    {
        log_notice("Ignored uuid '%s' matches uuid of problem '%s'", ignored, problem_id);
        return true;
    }

    if (ignored_end[0] == '\0')
    {
        log_notice("No 3rd column (DUPHASH) at line %d in ignored problems file '%s'",
                line_num, set->ign_set_file_path);
        return false;
    }
    ignored = ignored_end + 1;
    ignored_end = strchrnul(ignored, IGN_COLUMN_DELIMITER);
    sz = ignored_end - ignored;
    if (duphash != NULL && strncmp(duphash, ignored, sz) == 0 && duphash[sz] == '\0')
    {
        log_notice("Ignored duphash '%s' matches duphash of problem '%s'", ignored, problem_id);
        return true;
    }

    return false;
}

static bool ignored_problems_file_contains(ignored_problems_t *set,
        const char *problem_id, const char *uuid, const char *duphash,
        FILE **out_fp, const char *mode)
{
    bool found = false;
    FILE *fp = fopen(set->ign_set_file_path, mode);
    if (!fp)
    {
        if (errno != ENOENT)
            pwarn_msg("Can't open ignored problems '%s' in mode '%s'", set->ign_set_file_path, mode);
        goto ret_contains_end;
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

 ret_contains_end:
    if (out_fp)
        *out_fp = fp;
    else if (fp)
        fclose(fp);

    return found;
}

static void ignored_problems_add_row(ignored_problems_t *set, const char *problem_id,
        const char *uuid, const char *duphash)
{
    log_notice("Going to add problem '%s' to ignored problems", problem_id);

    FILE *fp;
    if (!ignored_problems_file_contains(set, problem_id, uuid, duphash, &fp, "a+"))
    {
        if (fp)
        {
            /* We can add write error checks here.
             * However, what exactly can we *do* if we detect it?
             */
            fprintf(fp, "%s;%s;%s\n", problem_id, (uuid ? uuid : ""),
                                      (duphash ? duphash : ""));
        }
        else
        {
            /* This is not a fatal problem. We are permissive because we don't want
             * to scare users by strange error messages.
             */
            log_notice("Can't add problem '%s' to ignored problems:"
                      " can't open the list", problem_id);
        }
    }
    else
    {
        log_notice("Won't add problem '%s' to ignored problems:"
                " it is already there", problem_id);
    }

    if (fp)
        fclose(fp);
}

void ignored_problems_add_problem_data(ignored_problems_t *set, problem_data_t *pd)
{
    ignored_problems_add_row(set,
            problem_data_get_content_or_NULL(pd, CD_DUMPDIR),
            problem_data_get_content_or_NULL(pd, FILENAME_UUID),
            problem_data_get_content_or_NULL(pd, FILENAME_DUPHASH)
            );
}

void ignored_problems_add(ignored_problems_t *set, const char *problem_id)
{
    struct dump_dir *dd = dd_opendir(problem_id, IGN_DD_OPEN_FLAGS);
    if (!dd)
    {
        /* We do not consider this as an error because the directory can be
         * deleted by other programs. This code expects that dd_opendir()
         * already emitted good explanatory message. This message
         * explains what the previous failure causes.
         */
        VERB1 log_warning("Can't add problem '%s' to ignored problems:"
                " can't open the problem", problem_id);
        return;
    }
    char *uuid = dd_load_text_ext(dd, FILENAME_UUID, IGN_DD_LOAD_TEXT_FLAGS);
    char *duphash = dd_load_text_ext(dd, FILENAME_DUPHASH, IGN_DD_LOAD_TEXT_FLAGS);
    dd_close(dd);

    ignored_problems_add_row(set, problem_id, uuid, duphash);

    free(duphash);
    free(uuid);
}

void ignored_problems_remove_row(ignored_problems_t *set, const char *problem_id,
        const char *uuid, const char *duphash)
{
    INITIALIZE_LIBABRT();

    VERB1 log_warning("Going to remove problem '%s' from ignored problems", problem_id);

    FILE *orig_fp;
    if (!ignored_problems_file_contains(set, problem_id, uuid, duphash, &orig_fp, "r"))
    {
        if (orig_fp)
        {
            log_notice("Won't remove problem '%s' from ignored problems:"
                      " it is already removed", problem_id);
            /* Close orig_fp here because it looks like much simpler than
             * exetendig the set of goto labels at the end of this function */
            fclose(orig_fp);
        }
        else
        {
            /* This is not a fatal problem. We are permissive because we don't want
             * to scare users by strange error messages.
             */
            log_notice("Can't remove problem '%s' from ignored problems:"
                      " can't open the list", problem_id);
        }
        return;
    }

    /* orig_fp must be valid here because if ignored_problems_file_contains()
     * returned TRUE the function ensures that orig_fp is set to a valid FILE*.
     *
     * But the function moved the file position indicator.
     */
    rewind(orig_fp);

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

}

void ignored_problems_remove_problem_data(ignored_problems_t *set, problem_data_t *pd)
{
    ignored_problems_remove_row(set,
            problem_data_get_content_or_NULL(pd, CD_DUMPDIR),
            problem_data_get_content_or_NULL(pd, FILENAME_UUID),
            problem_data_get_content_or_NULL(pd, FILENAME_DUPHASH)
            );
}

void ignored_problems_remove(ignored_problems_t *set, const char *problem_id)
{
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

    ignored_problems_remove_row(set, problem_id, uuid, duphash);

    free(duphash);
    free(uuid);
}

bool ignored_problems_contains_problem_data(ignored_problems_t *set, problem_data_t *pd)
{
    return ignored_problems_file_contains(set,
            problem_data_get_content_or_NULL(pd, CD_DUMPDIR),
            problem_data_get_content_or_NULL(pd, FILENAME_UUID),
            problem_data_get_content_or_NULL(pd, FILENAME_DUPHASH),
            /* (FILE **) */NULL, "r"
            );
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

    log_notice("Going to check if problem '%s' is in ignored problems '%s'",
            problem_id, set->ign_set_file_path);

    bool found = ignored_problems_file_contains(set, problem_id, uuid, duphash,
                    /* (FILE **) */NULL, "r");

    free(duphash);
    free(uuid);

    return found;
}
