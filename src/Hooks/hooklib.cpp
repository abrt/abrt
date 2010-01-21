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
#include "DebugDump.h"
#include <sys/statvfs.h>

using namespace std;

void parse_conf(const char *additional_conf, unsigned *setting_MaxCrashReportsSize, bool *setting_MakeCompatCore)
{
    FILE *fp = fopen(CONF_DIR"/abrt.conf", "r");
    if (!fp)
        return;

    char line[256];
    while (1)
    {
        if (fgets(line, sizeof(line), fp) == NULL)
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

        strchrnul(line, '\n')[0] = '\0';
        const char *p = skip_whitespace(line);
#undef DIRECTIVE
#define DIRECTIVE "MaxCrashReportsSize"
        if (setting_MaxCrashReportsSize && strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)
        {
            p = skip_whitespace(p + sizeof(DIRECTIVE)-1);
            if (*p != '=')
                continue;
            p = skip_whitespace(p + 1);
            if (isdigit(*p))
            {
                /* x1.25: go a bit up, so that usual in-daemon trimming
                 * kicks in first, and we don't "fight" with it. */
                *setting_MaxCrashReportsSize = (unsigned long)xatou(p) * 5 / 4;
            }
            continue;
        }
#undef DIRECTIVE
#define DIRECTIVE "MakeCompatCore"
        if (setting_MakeCompatCore && strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)
        {
            p = skip_whitespace(p + sizeof(DIRECTIVE)-1);
            if (*p != '=')
                continue;
            p = skip_whitespace(p + 1);
            *setting_MakeCompatCore = string_to_bool(p);
            continue;
        }
#undef DIRECTIVE
        /* add more 'if (strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)' here... */
    }
}

void check_free_space(unsigned setting_MaxCrashReportsSize)
{
    /* Check that at least MaxCrashReportsSize/4 MBs are free. */
    struct statvfs vfs;
    if (statvfs(DEBUG_DUMPS_DIR, &vfs) != 0
     || (vfs.f_bfree / (1024*1024 / 4)) * vfs.f_bsize < setting_MaxCrashReportsSize
    ) {
        error_msg_and_die("Low free disk space detected, aborting dump");
    }
}

/* rhbz#539551: "abrt going crazy when crashing process is respawned".
 * Check total size of dump dir, if it overflows,
 * delete oldest/biggest dumps.
 */
void trim_debug_dumps(unsigned setting_MaxCrashReportsSize, const char *exclude_path)
{
    int count = 10;
    string worst_dir;
    while (--count >= 0)
    {
        const char *base_dirname = strrchr(exclude_path, '/') + 1; /* never NULL */
        /* We exclude our own dump from candidates for deletion (3rd param): */
        double dirsize = get_dirsize_find_largest_dir(DEBUG_DUMPS_DIR, &worst_dir, base_dirname);
        if (dirsize / (1024*1024) < setting_MaxCrashReportsSize || worst_dir == "")
            break;
        log("size of '%s' >= %u MB, deleting '%s'", DEBUG_DUMPS_DIR, setting_MaxCrashReportsSize, worst_dir.c_str());
        delete_debug_dump_dir(concat_path_file(DEBUG_DUMPS_DIR, worst_dir.c_str()).c_str());
        worst_dir = "";
    }
}
