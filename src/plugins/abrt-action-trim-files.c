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

/* We remember N worst files and their sizes, not just one:
 * if DIR is deep, scanning it takes *a few seconds* even if it's in cache.
 * If we rescan it after each single file deletion, it can be VERY slow.
 * (I observed ~20 min long case).
 */
#define MAX_VICTIM_LIST_SIZE 128

struct name_and_size {
    off_t size;
    double weighted_size_and_age;
    char name[1];
};

/* Updates a list of files sorted by increasing weighted_size_and_age.
 */
static GList *insert_name_and_sizes(GList *list, const char *name, double wsa, off_t sz)
{
    struct name_and_size *ns;
    unsigned list_len = 0;
    GList *cur = list;

    while (cur)
    {
        ns = cur->data;
        if (ns->weighted_size_and_age >= wsa)
            break;
        list_len++;
        cur = cur->next;
    }
    list_len += g_list_length(cur);

    if (cur != list || list_len < MAX_VICTIM_LIST_SIZE)
    {
        ns = xmalloc(sizeof(*ns) + strlen(name));
        ns->weighted_size_and_age = wsa;
        ns->size = sz;
        strcpy(ns->name, name);
        list = g_list_insert_before(list, cur, ns);
        list_len++;
        if (list_len > MAX_VICTIM_LIST_SIZE)
        {
            free(list->data);
            list = g_list_delete_link(list, list);
        }
    }

    return list;
}

static double get_dir_size(const char *dirname,
                GList **pp_worst_file_list,
                GList *preserve_files_list
) {
    DIR *dp = opendir(dirname);
    if (!dp)
        return 0;

    /* "now" is used only if caller wants to know worst_file */
    time_t now = pp_worst_file_list ? time(NULL) : 0;
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
            double sz = get_dir_size(fullname, pp_worst_file_list, preserve_files_list);
            size += sz;
        }
        else if (S_ISREG(stats.st_mode) || S_ISLNK(stats.st_mode))
        {
            double sz = stats.st_size;
            /* Account for filename and inode storage (approximately).
             * This also makes even zero-length files to have nonzero cost.
             */
            sz += strlen(dent->d_name) + sizeof(stats);
            size += sz;

            if (pp_worst_file_list)
            {
                GList *cur = preserve_files_list;
                while (cur)
                {
                    //log_warning("'%s' ? '%s'", fullname, *pp);
                    if (strcmp(fullname, (char*)cur->data) == 0)
                        goto next;
                    cur = cur->next;
                }

                /* Calculate "weighted" size and age
                 * w = sz_kbytes * age_mins */
                sz /= 1024;
                long age = (now - stats.st_mtime) / 60;
                if (age > 1)
                    sz *= age;

                *pp_worst_file_list = insert_name_and_sizes(*pp_worst_file_list, fullname, sz, stats.st_size);
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
    GList *preserve_files_list = void_preserve_list;

    unsigned count = 100;
    while (--count != 0)
    {
        GList *worst_file_list = NULL;
        double cur_size = get_dir_size(dir, &worst_file_list, preserve_files_list);

        if (cur_size <= cap_size || !worst_file_list)
        {
            list_free_with_free(worst_file_list);
            log_info("cur_size:%.0f cap_size:%.0f, no (more) trimming", cur_size, cap_size);
            break;
        }

        /* Invert the list, so that largest/oldest file is first */
        worst_file_list = g_list_reverse(worst_file_list);
        /* And delete (some of) them */
        while (worst_file_list && cur_size > cap_size)
        {
            struct name_and_size *ns = worst_file_list->data;
            log_notice("%s is %.0f bytes (more than %.0f MB), deleting '%s' (%llu bytes)",
                    dir, cur_size, cap_size / (1024*1024), ns->name, (long long)ns->size);
            if (unlink(ns->name) != 0)
                perror_msg("Can't unlink '%s'", ns->name);
            else
                cur_size -= ns->size;
            free(ns);
            worst_file_list = g_list_delete_link(worst_file_list, worst_file_list);
        }
    }
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    GList *dir_list = NULL;
    GList *file_list = NULL;
    char *preserve = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] [-d SIZE:DIR]... [-f SIZE:DIR]... [-p DIR] [FILE]...\n"
        "\n"
        "Deletes problem dirs (-d) or files (-f) in DIRs until they are smaller than SIZE.\n"
        "FILEs are preserved (never deleted)."
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

    /* Preserve not only files specified on command line, but,
     * if they are symlinks, preserve also the real files they point to:
     */
    GList *preserve_files_list = NULL;
    while (*argv)
    {
        char *name = *argv++;
        /* Since we don't bother freeing preserve_files_list on exit,
         * we take a shortcut and insert name instead of xstrdup(name)
         * in the next line:
         */
        preserve_files_list = g_list_prepend(preserve_files_list, name);

        char *rp = realpath(name, NULL);
        if (rp)
        {
            if (strcmp(rp, name) != 0)
                preserve_files_list = g_list_prepend(preserve_files_list, rp);
            else
                free(rp);
        }
    }
    /* Not really necessary, but helps to reduce confusion when debugging */
    preserve_files_list = g_list_reverse(preserve_files_list);

    g_list_foreach(dir_list, delete_dirs, preserve);
    g_list_foreach(file_list, delete_files, preserve_files_list);

    return 0;
}
