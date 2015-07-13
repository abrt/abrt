/*
    Copyright (C) 2011,2014  ABRT team
    Copyright (C) 2011,2014  RedHat Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Authors:
       Anton Arapov <anton@redhat.com>
       Arjan van de Ven <arjan@linux.intel.com>
 */
#include <syslog.h>
#include "libabrt.h"
#include "oops-utils.h"

#define MAX_SCAN_BLOCK  (4*1024*1024)
#define READ_AHEAD          (10*1024)
#define ABRT_DUMP_OOPS_ANALYZER "abrt-oops"

static void scan_syslog_file(GList **oops_list, int fd)
{
    struct stat st;
    struct stat *statbuf = &st;

    /* Try to not allocate an absurd amount of memory */
    int sz = MAX_SCAN_BLOCK - READ_AHEAD;
    /* If it's a real file, estimate size after cur pos */
    off_t cur_pos = lseek(fd, 0, SEEK_CUR);
    if (cur_pos >= 0 && fstat(fd, statbuf) == 0 && S_ISREG(statbuf->st_mode))
    {
        off_t size_to_read = statbuf->st_size - cur_pos;
        if (size_to_read >= 0 && sz > size_to_read)
            sz = size_to_read;
    }

    /*
     * In theory we have a race here, since someone can spew
     * to /var/log/messages before we read it in...
     * We try to deal with it by reading READ_AHEAD extra.
     */
    sz += READ_AHEAD;
    char *buffer = xzalloc(sz);

    for (;;)
    {
        int r = full_read(fd, buffer, sz-1);
        if (r <= 0)
            break;
        log_debug("Read %u bytes", r);
        koops_extract_oopses(oops_list, buffer, r);
//TODO: rewind to last newline?
    }

    free(buffer);
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

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-vusoxm] [-d DIR]/[-D] [FILE]\n"
        "\n"
        "Extract oops from FILE (or standard input)"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_s = 1 << 1,
        OPT_o = 1 << 2,
        OPT_d = 1 << 3,
        OPT_D = 1 << 4,
        OPT_u = 1 << 5,
        OPT_x = 1 << 6,
        OPT_t = 1 << 7,
        OPT_m = 1 << 8,
    };
    char *problem_dir = NULL;
    char *dump_location = NULL;
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found oopses on standard output")),
        /* oopses don't contain any sensitive info, and even
         * the old koops app was showing the oopses to all users
         */
        OPT_STRING('d', NULL, &dump_location, "DIR", _("Create new problem directory in DIR for every oops found")),
        OPT_BOOL(  'D', NULL, NULL, _("Same as -d DumpLocation, DumpLocation is specified in abrt.conf")),
        OPT_STRING('u', NULL, &problem_dir, "PROBLEM", _("Save the extracted information in PROBLEM")),
        OPT_BOOL(  'x', NULL, NULL, _("Make the problem directory world readable")),
        OPT_BOOL(  't', NULL, NULL, _("Throttle problem directory creation to 1 per second")),
        OPT_BOOL(  'm', NULL, NULL, _("Print search string(s) to stdout and exit")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    msg_prefix = g_progname;
    if ((opts & OPT_s) || getenv("ABRT_SYSLOG"))
    {
        logmode = LOGMODE_JOURNAL;
    }

    if (opts & OPT_m)
    {
        char *oops_string_filter_regex = abrt_oops_string_filter_regex();
        if (oops_string_filter_regex)
        {
            regex_t filter_re;
            if (regcomp(&filter_re, oops_string_filter_regex, REG_NOSUB) != 0)
                perror_msg_and_die(_("Failed to compile regex"));

            const regex_t *filter[] = { &filter_re, NULL };

            koops_print_suspicious_strings_filtered(filter);

            regfree(&filter_re);
            free(oops_string_filter_regex);
        }
        else
            koops_print_suspicious_strings();

        return 1;
    }

    if (opts & OPT_D)
    {
        if (opts & OPT_d)
            show_usage_and_die(program_usage_string, program_options);
        load_abrt_conf();
        dump_location = g_settings_dump_location;
        g_settings_dump_location = NULL;
        free_abrt_conf_data();
    }

    int oops_utils_flags = 0;
    if ((opts & OPT_x))
        oops_utils_flags |= ABRT_OOPS_WORLD_READABLE;

    if ((opts & OPT_t))
        oops_utils_flags |= ABRT_OOPS_THROTTLE_CREATION;

    if ((opts & OPT_o))
        oops_utils_flags |= ABRT_OOPS_PRINT_STDOUT;

    argv += optind;
    if (argv[0])
        xmove_fd(xopen(argv[0], O_RDONLY), STDIN_FILENO);

    GList *oops_list = NULL;
    scan_syslog_file(&oops_list, STDIN_FILENO);

    unsigned errors = 0;
    if (opts & OPT_u)
    {
        log("Updating problem directory");
        switch (g_list_length(oops_list))
        {
            case 0:
                {
                    error_msg(_("Can't update the problem: no oops found"));
                    errors = 1;
                    break;
                }
            default:
                {
                    log_notice(_("More oopses found: process only the first one"));
                }
                /* falls trought */
            case 1:
                {
                    struct dump_dir *dd = dd_opendir(problem_dir, /*open for writing*/0);
                    if (dd)
                    {
                        abrt_oops_save_data_in_dump_dir(dd, (char *)oops_list->data, /*no proc modules*/NULL);
                        dd_close(dd);
                    }
                }
        }
    }
    else
        errors = abrt_oops_process_list(oops_list, dump_location,
                                        ABRT_DUMP_OOPS_ANALYZER, oops_utils_flags);

    list_free_with_free(oops_list);
    //oops_list = NULL;

    return errors;
}
