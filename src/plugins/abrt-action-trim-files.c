/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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
#include "parse_options.h"

#define PROGNAME "abrt-action-trim-files"

static double get_dir_size(const char *dirname,
                           char **worst_file,
                           double *worst_file_size)
{
    DIR *dp = opendir(dirname);
    if (!dp)
        return 0;

    /* "now" is used only if caller wants to know worst_file */
    time_t now = worst_file ? time(NULL) : 0;
    struct dirent *dent;
    struct stat stats;
    double size = 0;
    while ((dent = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue;

        char *fullname = concat_path_file(dirname, dent->d_name);
        if (lstat(fullname, &stats) != 0)
        {
            free(fullname);
            continue;
        }

        if (S_ISDIR(stats.st_mode))
        {
            double sz = get_dir_size(fullname, worst_file, worst_file_size);
            size += sz;
        }
        else if (S_ISREG(stats.st_mode))
        {
            double sz = stats.st_size;
            size += sz;

            if (worst_file)
            {
                /* Calculate "weighted" size and age
                 * w = sz_kbytes * age_mins */
                sz /= 1024;
                long age = (now - stats.st_mtime) / 60;
                if (age > 0)
                    sz *= age;

                if (sz > *worst_file_size)
                {
                    *worst_file_size = sz;
                    free(*worst_file);
                    *worst_file = fullname;
                    fullname = NULL;
                }
            }
        }
        free(fullname);
    }
    closedir(dp);

    return size;
}

static const char *parse_size_pfx(double *size, const char *str)
{
    errno = (isdigit(str[0]) ? 0 : ERANGE);
    char *end;
    *size = strtoull(str, &end, 10);

    if (end != str)
    {
        char c = (*end | 0x20); /* lowercasing */
        if (c == 'k')
            end++, *size *= 1024;
        else if (c == 'm')
            end++, *size *= 1024*1024;
        else if (c == 'g')
            end++, *size *= 1024*1024*1024;
        else if (c == 't')
            end++, *size *= 1024.0*1024*1024*1024;
    }

    if (errno || end == str || *end != ':')
        perror_msg_and_die("Bad size prefix in '%s'", str);

    return end + 1;
}

static void delete_dirs(gpointer data, gpointer exclude_path)
{
    double cap_size;
    const char *dir = parse_size_pfx(&cap_size, data);

    trim_debug_dumps(dir, cap_size, exclude_path);
}

static void delete_files(gpointer data, gpointer user_data_unused)
{
    double cap_size;
    const char *dir = parse_size_pfx(&cap_size, data);

    unsigned count = 1000;
    while (--count != 0)
    {
        char *worst_file = NULL;
        double worst_file_size = 0;
        double cur_size = get_dir_size(dir, &worst_file, &worst_file_size);
        if (cur_size <= cap_size)
        {
            VERB2 log("cur_size:%f cap_size:%f, no (more) trimming", cur_size, cap_size);
            free(worst_file);
            break;
        }
        log("%s is %.0f bytes (more than %.0f MB), deleting '%s'",
                dir, cur_size, cap_size / (1024*1024), worst_file);
        if (unlink(worst_file) != 0)
            perror_msg("Can't unlink '%s'", worst_file);
        free(worst_file);
    }
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    GList *dir_list = NULL;
    GList *file_list = NULL;
    char *preserve = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] [-d SIZE:DIR]... [-f SIZE:DIR]... [-p DIR]\n"
        "\n"
        "Deletes dump dirs (-d) or files (-f) in DIRs until they are smaller than SIZE"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_f = 1 << 2,
        OPT_p = 1 << 3,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_LIST(  'd', NULL, &dir_list , "SIZE:DIR", _("Delete dump dirs")),
        OPT_LIST(  'f', NULL, &file_list, "SIZE:DIR", _("Delete files")),
        OPT_STRING('p', NULL, &preserve , "DIR"     , _("Preserve this dump dir")),
        OPT_END()
    };

    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);
    argv += optind;
    if (argv[0] || !(dir_list || file_list))
        show_usage_and_die(program_usage_string, program_options);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    char *pfx = getenv("ABRT_PROG_PREFIX");
    if (pfx && string_to_bool(pfx))
        msg_prefix = PROGNAME;

    g_list_foreach(dir_list, delete_dirs, preserve);
    g_list_foreach(file_list, delete_files, NULL);

    return 0;
}
