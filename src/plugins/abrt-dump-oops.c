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

    Authors:
       Anton Arapov <anton@redhat.com>
       Arjan van de Ven <arjan@linux.intel.com>
 */
#include <syslog.h>
#include "libabrt.h"


static bool world_readable_dump = false;
static const char *debug_dumps_dir = ".";

#define MAX_SCAN_BLOCK  (4*1024*1024)
#define READ_AHEAD          (10*1024)

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
        VERB3 log("Read %u bytes", r);
        koops_extract_oopses(oops_list, buffer, r);
//TODO: rewind to last newline?
    }

    free(buffer);
}

/* returns number of errors */
static unsigned save_oops_to_dump_dir(GList *oops_list, unsigned oops_cnt)
{
    unsigned countdown = 16; /* do not report hundreds of oopses */
    unsigned idx = 0; //oops_cnt;

    VERB1 log("Saving %u oopses as dump dirs", oops_cnt >= countdown ? countdown-1 : oops_cnt);

    char *cmdline_str = NULL;
    FILE *cmdline_fp = fopen("/proc/cmdline", "r");
    if (cmdline_fp)
    {
        cmdline_str = xmalloc_fgetline(cmdline_fp);
        fclose(cmdline_fp);
    }

    time_t t = time(NULL);
    const char *iso_date = iso_date_string(&t);
    /* dump should be readable by all if we're run with -x */
    uid_t my_euid = (uid_t)-1L;
    mode_t mode = 0644;
    /* and readable only for the owner otherwise */
    if (!world_readable_dump)
    {
        mode = 0640;
        my_euid = geteuid();
    }

    pid_t my_pid = getpid();
    unsigned errors = 0;
    while (idx < oops_cnt && --countdown != 0)
    {
        char *first_line = (char*)g_list_nth_data(oops_list, idx++);
        char *second_line = (char*)strchr(first_line, '\n'); /* never NULL */
        *second_line++ = '\0';

        struct dump_dir *dd;
        {
            char base[sizeof("oops-YYYY-MM-DD-hh:mm:ss-%lu-%lu") + 2 * sizeof(long)*3];
            sprintf(base, "oops-%s-%lu-%lu", iso_date, (long)my_pid, (long)idx);
            char *path = concat_path_file(debug_dumps_dir, base);
            dd = dd_create(path, /*uid:*/ my_euid, mode);
            free(path);
        }

        if (dd)
        {
            dd_create_basic_files(dd, /*uid:*/ my_euid);
            dd_save_text(dd, "abrt_version", VERSION);
            dd_save_text(dd, FILENAME_ANALYZER, "Kerneloops");
            dd_save_text(dd, FILENAME_KERNEL, first_line);
            if (cmdline_str)
                dd_save_text(dd, FILENAME_CMDLINE, cmdline_str);
            dd_save_text(dd, FILENAME_BACKTRACE, second_line);

            /* check if trace doesn't have line: 'Your BIOS is broken' */
            char *broken_bios = strstr(second_line, "Your BIOS is broken");
            if (broken_bios)
                dd_save_text(dd, FILENAME_NOT_REPORTABLE, "Your BIOS is broken");

            char *tainted_short = kernel_tainted_short(second_line);
            if (tainted_short && !broken_bios)
            {
                VERB1 log("Kernel is tainted '%s'", tainted_short);
                dd_save_text(dd, FILENAME_TAINTED_SHORT, tainted_short);
                const char *fmt = _("A kernel problem occurred, but your kernel has been "
                             "tainted (flags:%s). Kernel maintainers are unable to "
                             "diagnose tainted reports.");

                char *reason = xasprintf(fmt, tainted_short);

                dd_save_text(dd, FILENAME_NOT_REPORTABLE, reason);
                free(reason);
            }
// TODO: add "Kernel oops: " prefix, so that all oopses have recognizable FILENAME_REASON?
// kernel oops 1st line may look quite puzzling otherwise...
            strchrnul(second_line, '\n')[0] = '\0';
            dd_save_text(dd, FILENAME_REASON, second_line);

/*
            GList *tainted_long = kernel_tainted_long(tainted);

            struct strbuf *tnt_long = strbuf_new();
            for (GList *li = tainted_long; li; li = li->next)
                strbuf_append_strf(tnt_long, "%s\n", (char*) li->data);

            dd_save_text(dd, FILENAME_TAINTED, tainted_str);
            dd_save_text(dd, FILENAME_TAINTED_SHORT, tainted_short);
            dd_save_text(dd, FILENAME_TAINTED_LONG, tnt_long->buf);
            strbuf_free(tnt_long);
            list_free_with_free(tainted_long);
*/
            dd_close(dd);
        }
        else
            errors++;
    }

    free(cmdline_str);

    return errors;
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
        "& [-vsoxm] [-d DIR]/[-D] [FILE]\n"
        "\n"
        "Extract oops from FILE (or standard input)"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_s = 1 << 1,
        OPT_o = 1 << 2,
        OPT_d = 1 << 3,
        OPT_D = 1 << 4,
        OPT_x = 1 << 5,
        OPT_m = 1 << 6,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found oopses on standard output")),
        /* oopses don't contain any sensitive info, and even
         * the old koops app was showing the oopses to all users
         */
        OPT_STRING('d', NULL, &debug_dumps_dir, "DIR", _("Create ABRT dump in DIR for every oops found")),
        OPT_BOOL(  'D', NULL, NULL, _("Same as -d DumpLocation, DumpLocation is specified in abrt.conf")),
        OPT_BOOL(  'x', NULL, NULL, _("Make the problem directory world readable")),
        OPT_BOOL(  'm', NULL, NULL, _("Print search string(s) to stdout and exit")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    msg_prefix = g_progname;
    if ((opts & OPT_s) || getenv("ABRT_SYSLOG"))
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    if (opts & OPT_m)
    {
        koops_print_suspicious_strings();
        return 1;
    }

    if (opts & OPT_D)
    {
        if (opts & OPT_d)
            show_usage_and_die(program_usage_string, program_options);
        load_abrt_conf();
        debug_dumps_dir = g_settings_dump_location;
        g_settings_dump_location = NULL;
        free_abrt_conf_data();
    }

    argv += optind;
    if (argv[0])
        xmove_fd(xopen(argv[0], O_RDONLY), STDIN_FILENO);

    world_readable_dump = (opts & OPT_x);
    unsigned errors = 0;
    GList *oops_list = NULL;
    scan_syslog_file(&oops_list, STDIN_FILENO);

    int oops_cnt = g_list_length(oops_list);
    if (oops_cnt != 0)
    {
        log("Found oopses: %d", oops_cnt);
        if (opts & OPT_o)
        {
            int i = 0;
            while (i < oops_cnt)
            {
                char *kernel_bt = (char*)g_list_nth_data(oops_list, i++);
                char *tainted_short = kernel_tainted_short(kernel_bt);
                if (tainted_short)
                    log("Kernel is tainted '%s'", tainted_short);

                free(tainted_short);
                printf("\nVersion: %s", kernel_bt);
            }
        }
        if (opts & (OPT_d|OPT_D))
        {
            if (opts & OPT_D)
            {
                load_abrt_conf();
                debug_dumps_dir = g_settings_dump_location;
            }

            log("Creating dump directories");
            errors = save_oops_to_dump_dir(oops_list, oops_cnt);
            if (errors)
                log("%d errors while dumping oopses", errors);
            /*
             * This marker in syslog file prevents us from
             * re-parsing old oopses. The only problem is that we
             * can't be sure here that the file we are watching
             * is the same file where syslog(xxx) stuff ends up.
             */
            syslog(LOG_WARNING,
                    "Reported %u kernel oopses to Abrt",
                    oops_cnt
            );
        }
    }
    //list_free_with_free(oops_list);
    //oops_list = NULL;

    return errors;
}
