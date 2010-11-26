/*
    Copyright (C) 2009	RedHat inc.

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
#include "abrtlib.h"
#include "hooklib.h"
#include <sys/statvfs.h>

void parse_conf(const char *additional_conf, unsigned *setting_MaxCrashReportsSize, bool *setting_MakeCompatCore, bool *setting_SaveBinaryImage)
{
    FILE *fp = fopen(CONF_DIR"/abrt.conf", "r");
    if (!fp)
        return;

    while (1)
    {
        char *line = xmalloc_fgetline(fp);
        if (!line)
        {
            fclose(fp);
            if (additional_conf)
            {
                /* Next .conf file plz */
                fp = fopen(additional_conf, "r");
                if (fp)
                {
                    additional_conf = NULL;
                    continue;
                }
            }
            break;
        }

        const char *p = skip_whitespace(line);
#undef DIRECTIVE
#define DIRECTIVE "MaxCrashReportsSize"
        if (setting_MaxCrashReportsSize && strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)
        {
            p = skip_whitespace(p + sizeof(DIRECTIVE)-1);
            if (*p != '=')
                goto free_line;
            p = skip_whitespace(p + 1);
            if (isdigit(*p))
            {
                /* x1.25: go a bit up, so that usual in-daemon trimming
                 * kicks in first, and we don't "fight" with it. */
                *setting_MaxCrashReportsSize = (unsigned long)xatou(p) * 5 / 4;
            }
            goto free_line;
        }
#undef DIRECTIVE
#define DIRECTIVE "MakeCompatCore"
        if (setting_MakeCompatCore && strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)
        {
            p = skip_whitespace(p + sizeof(DIRECTIVE)-1);
            if (*p != '=')
                goto free_line;
            p = skip_whitespace(p + 1);
            *setting_MakeCompatCore = string_to_bool(p);
            goto free_line;
        }
#undef DIRECTIVE
#define DIRECTIVE "SaveBinaryImage"
        if (setting_SaveBinaryImage && strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)
        {
            p = skip_whitespace(p + sizeof(DIRECTIVE)-1);
            if (*p != '=')
                goto free_line;
            p = skip_whitespace(p + 1);
            *setting_SaveBinaryImage = string_to_bool(p);
            goto free_line;
        }
#undef DIRECTIVE
        /* add more 'if (strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)' here... */

 free_line:
        free(line);
    }
}

void check_free_space(unsigned setting_MaxCrashReportsSize)
{
    struct statvfs vfs;
    if (statvfs(DEBUG_DUMPS_DIR, &vfs) != 0)
    {
        perror_msg_and_die("statvfs('%s')", DEBUG_DUMPS_DIR);
    }

    /* Check that at least MaxCrashReportsSize/4 MBs are free */

    /* fs_free_mb_x4 ~= vfs.f_bfree * vfs.f_bsize * 4, expressed in MBytes.
     * Need to neither overflow nor round f_bfree down too much. */
    unsigned long fs_free_mb_x4 = ((unsigned long long)vfs.f_bfree / (1024/4)) * vfs.f_bsize / 1024;
    if (fs_free_mb_x4 < setting_MaxCrashReportsSize)
    {
        error_msg_and_die("aborting dump: only %luMiB is available on %s", fs_free_mb_x4 / 4, DEBUG_DUMPS_DIR);
    }
}

/* rhbz#539551: "abrt going crazy when crashing process is respawned".
 * Check total size of dump dir, if it overflows,
 * delete oldest/biggest dumps.
 */
void trim_debug_dumps(unsigned setting_MaxCrashReportsSize, const char *exclude_path)
{
    int count = 10;
    while (--count >= 0)
    {
        const char *base_dirname = strrchr(exclude_path, '/') + 1; /* never NULL */
        /* We exclude our own dump from candidates for deletion (3rd param): */
        char *worst_dir = NULL;
        double dirsize = get_dirsize_find_largest_dir(DEBUG_DUMPS_DIR, &worst_dir, base_dirname);
        if (dirsize / (1024*1024) < setting_MaxCrashReportsSize || !worst_dir)
        {
            free(worst_dir);
            break;
        }
        log("size of '%s' >= %u MB, deleting '%s'", DEBUG_DUMPS_DIR, setting_MaxCrashReportsSize, worst_dir);
        char *d = concat_path_file(DEBUG_DUMPS_DIR, worst_dir);
        free(worst_dir);
        worst_dir = NULL;
        delete_crash_dump_dir(d);
        free(d);
    }
}
