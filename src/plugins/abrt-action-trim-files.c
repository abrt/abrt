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
#include "libabrt.h"

/* TODO: remember N worst files and their sizes, not just one:
 * if DIR is deep, scanning it takes *a few seconds*.
 * If we rescan it after each single file deletion, it can be VERY slow.
 * (I observed ~20 min long case).
 */

static double get_dir_size(const char *dirname,
                char **worst_file,
                double *worst_file_size,
                char **preserve_list
) {
    DIR *dp = opendir(dirname);
    if (!dp)
        return 0;

    /* "now" is used only if caller wants to know worst_file */
    time_t now = worst_file ? time(NULL) : 0;
    struct dirent *dent;
    double size = 0;
    while ((dent = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue;

        char *fullname = concat_path_file(dirname, dent->d_name);
        struct stat stats;
        if (lstat(fullname, &stats) != 0)
            goto next;

        if (S_ISDIR(stats.st_mode))
        {
            double sz = get_dir_size(fullname, worst_file, worst_file_size, preserve_list);
            size += sz;
        }
        else if (S_ISREG(stats.st_mode))
        {
            double sz = stats.st_size;
            /* Account for filename and inode storage (approximately).
             * This also makes even zero-length files to have nonzero cost
             */
            sz += strlen(dent->d_name) + sizeof(stats);
            size += sz;

            if (worst_file)
            {
                if (preserve_list)
                {
                    char **pp = preserve_list;
                    while (*pp)
                    {
                        //log("'%s' ? '%s'", fullname, *pp);
                        if (strcmp(fullname, *pp) == 0)
                            goto next;
                        pp++;
                    }
                }

                /* Calculate "weighted" size and age
                 * w = sz_kbytes * age_mins */
                sz /= 1024;
                long age = (now - stats.st_mtime) / 60;
                if (age > 1)
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
 next:
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

    trim_problem_dirs(dir, cap_size, exclude_path);
}

static void delete_files(gpointer data, gpointer void_preserve_list)
{
    double cap_size;
    const char *dir = parse_size_pfx(&cap_size, data);
    char **preserve_list = void_preserve_list;

    unsigned count = 1000;
    while (--count != 0)
    {
        char *worst_file = NULL;
        double worst_file_size = 0;
        double cur_size = get_dir_size(dir, &worst_file, &worst_file_size, preserve_list);
        if (cur_size <= cap_size || !worst_file)
        {
            VERB2 log("cur_size:%.0f cap_size:%.0f, no (more) trimming", cur_size, cap_size);
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
    abrt_init(argv);

    GList *dir_list = NULL;
    GList *file_list = NULL;
    char *preserve = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] [-d SIZE:DIR]... [-f SIZE:DIR]... [-p DIR] [FILE]...\n"
        "\n"
        "Deletes problem dirs (-d) or files (-f) in DIRs until they are smaller than SIZE"
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
        OPT_LIST('d'  , NULL, &dir_list , "SIZE:DIR", _("Delete whole problem directories")),
        OPT_LIST('f'  , NULL, &file_list, "SIZE:DIR", _("Delete files inside this directory")),
        OPT_STRING('p', NULL, &preserve,  "DIR"     , _("Preserve this directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);
    argv += optind;
    if ((argv[0] && !file_list)
     || !(dir_list || file_list)
    ) {
        show_usage_and_die(program_usage_string, program_options);
    }

    /* We don't have children, so this is not needed: */
    //export_abrt_envvars(/*set_pfx:*/ 0);

    g_list_foreach(dir_list, delete_dirs, preserve);
    g_list_foreach(file_list, delete_files, argv);

    return 0;
}
