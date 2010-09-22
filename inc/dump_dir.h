/*
    DebugDump.h - header file for the library caring of writing new reports
                  to the specific directory

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
#ifndef DEBUGDUMP_H_
#define DEBUGDUMP_H_

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DD_CLOSE_ON_OPEN_ERR    = (1 << 0),
    DD_FAIL_QUIETLY         = (1 << 1),
};

struct dump_dir {
    char *dd_dir;
    DIR *next_dir;
    int locked;
    uid_t dd_uid;
    gid_t dd_gid;
};

struct dump_dir *dd_init(void);
void dd_close(struct dump_dir *dd);

int dd_opendir(struct dump_dir *dd, const char *dir, int flags);
int dd_exist(struct dump_dir *dd, const char *path);
int dd_create(struct dump_dir *dd, const char *dir, uid_t uid);
DIR *dd_init_next_file(struct dump_dir *dd);
int dd_get_next_file(struct dump_dir *dd, char **short_name, char **full_name);

char* dd_load_text(const struct dump_dir *dd, const char* name);
void dd_save_text(struct dump_dir *dd, const char *name, const char *data);
void dd_save_binary(struct dump_dir* dd, const char* name, const char* data, unsigned size);
void dd_delete(struct dump_dir *dd);

void delete_debug_dump_dir(const char *dd_dir);

#ifdef __cplusplus
}
#endif

#endif
