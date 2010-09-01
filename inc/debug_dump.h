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

typedef struct dump_dir {
    char *dd_dir;
    DIR *next_dir;
    int opened;
    int locked;
    uid_t uid;
    gid_t gid;
} dump_dir_t;

dump_dir_t* dd_init(void);
void dd_close(dump_dir_t *dd);

int dd_opendir(dump_dir_t *dd, const char *dir);
int dd_exist(dump_dir_t *dd, const char *path);
int dd_create(dump_dir_t *dd, const char *dir, uid_t uid);
int dd_init_next_file(dump_dir_t *dd);
int dd_get_next_file(dump_dir_t *dd, char **short_name, char **full_name);

char* dd_load_text(const dump_dir_t *dd, const char* name);
void dd_save_text(dump_dir_t *dd, const char *name, const char *data);
void dd_save_binary(dump_dir_t* dd, const char* name, const char* data, unsigned size);
void dd_delete(dump_dir_t *dd);

void delete_debug_dump_dir(const char *dd_dir);

#ifdef __cplusplus
}
#endif

#endif
