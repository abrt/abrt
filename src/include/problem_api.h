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

int for_each_problem_in_dir(const char *path,
                        uid_t caller_uid,
                        int (*callback)(struct dump_dir *dd, void *arg),
                        void *arg);

/* Retrieves the list of directories currently used as a problem storage
 * The result must be freed by caller
 * @returns List of strings representing the full path to dirs
 */
GList *get_problem_storages(void);
GList *get_problem_dirs_for_uid(uid_t uid, const char *dump_location);
GList *get_problem_dirs_for_element_in_time(uid_t uid,
                                                      const char *element,
                                                      const char *value,
                                                      unsigned long timestamp_from,
                                                      unsigned long timestamp_to,
                                                      const char *dump_location);

/* Counts all problems in given directories
 *
 * @paths[in] list of paths to scan (pass NULL to use the default problem directories)
 * @since[in]
 * @returns   count of problems
 */
unsigned int get_problems_count(GList *paths, unsigned long since);
