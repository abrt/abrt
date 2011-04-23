/*
    On-disk storage of problem data

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
#ifndef DUMP_DIR_H_
#define DUMP_DIR_H_

/* For DIR */
#include <sys/types.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DD_FAIL_QUIETLY_ENOENT = (1 << 0),
    DD_FAIL_QUIETLY_EACCES = (1 << 1),
    DD_OPEN_READONLY = (1 << 2),
};

struct dump_dir {
    char *dd_dirname;
    DIR *next_dir;
    int locked;
    uid_t dd_uid;
    gid_t dd_gid;
    /* mode fo saved files */
    mode_t mode;
};

void dd_close(struct dump_dir *dd);

struct dump_dir *dd_opendir(const char *dir, int flags);
/* Pass uid = (uid_t)-1L to disable chown'ing of newly created files
 * (IOW: if you aren't running under root):
 */
struct dump_dir *dd_create(const char *dir, uid_t uid, mode_t mode);

void dd_create_basic_files(struct dump_dir *dd, uid_t uid);
int dd_exist(struct dump_dir *dd, const char *path);
void dd_sanitize_mode_and_owner(struct dump_dir *dd);

DIR *dd_init_next_file(struct dump_dir *dd);
int dd_get_next_file(struct dump_dir *dd, char **short_name, char **full_name);

enum {
    /* DD_FAIL_QUIETLY_ENOENT bit is valid for dd_load_text_ext too, */
    DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE = (DD_OPEN_READONLY << 1),
};
char* dd_load_text_ext(const struct dump_dir *dd, const char *name, unsigned flags);
char* dd_load_text(const struct dump_dir *dd, const char *name);
void dd_save_text(struct dump_dir *dd, const char *name, const char *data);
void dd_save_binary(struct dump_dir *dd, const char *name, const char *data, unsigned size);
/* Returns 0 if directory is deleted or not found */
int dd_delete(struct dump_dir *dd);

void delete_dump_dir(const char *dirname);

#ifdef __cplusplus
}
#endif

#endif
