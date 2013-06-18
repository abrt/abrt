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
#include <libabrt.h>


/*
 * Function called for each problem directory in @for_each_problem_in_dir
 *
 * @param dd A dump directory
 * @param arg User's arguments
 * @returns 0 if everything is OK, a non zero value in order to break the iterator
 */
typedef int (* for_each_problem_in_dir_callback)(struct dump_dir *dd, void *arg);

/*
 * Iterates over all dump directories placed in @path and call @callback.
 *
 * @param path Dump directories location
 * @param caller_uid UID for access check. -1 for disabling this check
 * @param callback Called for each applicable dump directory. Non zero
 * value returned from @callback will breaks the iteration.
 * @param arg User's arguments passed to @callback
 * @returns 0 or the first non zero value returned from @callback
 */
int for_each_problem_in_dir(const char *path,
                        uid_t caller_uid,
                        for_each_problem_in_dir_callback callback,
                        void *arg);

/* Retrieves the list of directories currently used as a problem storage
 * The result must be freed by caller
 * @returns List of strings representing the full path to dirs
 */
GList *get_problem_storages(void);
GList *get_problem_dirs_for_uid(uid_t uid, const char *dump_location);

/*
 * Gets list of problem directories not accessible by user
 *
 * @param uid User's uid
 * @param dump_location Dump directories location
 * @returns GList with mallocated absolute paths to dump directories
 */
GList *get_problem_dirs_not_accessible_by_uid(uid_t uid, const char *dump_location);
