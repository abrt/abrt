/*
    Copyright (C) 2012  RedHat inc.

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
#if HAVE_LOCALE_H
# include <locale.h>
#endif
#include "libabrt.h"

static void try_to_move(const char *old, const char *new)
{
    /* We don't bother checking whether old exists.
     * Such check would be racy anyway...
     */
    if (access(new, F_OK) != 0 && (errno == ENOENT || errno == ENOTDIR))
        rename(old, new);
}

void migrate_to_xdg_dirs(void)
{
    char *old = g_build_filename(g_get_home_dir(), ".abrt/applet_dirlist", NULL);
    char *new = g_build_filename(g_get_user_cache_dir(), "abrt/applet_dirlist", NULL);
    char *oslash = strrchr(old, '/');
    char *nslash = strrchr(new, '/');

    /* we don't check success: it may already exists */
    *nslash = '\0';
    g_mkdir_with_parents(new, 0777);

    *nslash = '/';
    try_to_move(old, new);

    strcpy(oslash + 1, "spool");
    strcpy(nslash + 1, "spool");
    try_to_move(old, new);

    strcpy(oslash + 1, "settings");
    free(new);
    new = g_build_filename(g_get_user_config_dir(), "abrt/settings", NULL);
    nslash = strrchr(new, '/');
    *nslash = '\0';
    g_mkdir_with_parents(new, 0777);
    *nslash = '/';
    try_to_move(old, new);
    free(new);

    /* Delete $HOME/.abrt if it is empty */
    *oslash = '\0';
    rmdir(old);

    free(old);
}
