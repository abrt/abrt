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

/* How many problem dirs to create at most?
 * Also causes cooldown sleep with -t if exceeded -
 * useful when called from a log watcher.
 */
#define MAX_DUMPED_DD_COUNT  5

static bool world_readable_dump = false;
static bool throttle_dd_creation = false;
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
        log_debug("Read %u bytes", r);
        koops_extract_oopses(oops_list, buffer, r);
//TODO: rewind to last newline?
    }

    free(buffer);
}

static char *list_of_tainted_modules(const char *proc_modules)
{
    struct strbuf *result = strbuf_new();

    const char *p = proc_modules;
    for (;;)
    {
        const char *end = strchrnul(p, '\n');
        const char *paren = strchrnul(p, '(');
        /* We look for a line with this format:
         * "kvm_intel 126289 0 - Live 0xf829e000 (taint_flags)"
         * where taint_flags have letters
         * (flags '+' and '-' indicate (un)loading, we must ignore them).
         */
        while (++paren < end)
        {
            if ((unsigned)(toupper(*paren) - 'A') <= 'Z'-'A')
            {
                strbuf_append_strf(result, result->len == 0 ? "%.*s" : ",%.*s",
                        (int)(strchrnul(p,' ') - p), p
                );
                break;
            }
            if (*paren == ')')
                break;
        }

        if (*end == '\0')
            break;
        p = end + 1;
    }

    if (result->len == 0)
    {
        strbuf_free(result);
        return NULL;
    }
    return strbuf_free_nobuf(result);
}

static void save_oops_data_in_dump_dir(struct dump_dir *dd, char *oops, const char *proc_modules)
{
    char *first_line = oops;
    char *second_line = (char*)strchr(first_line, '\n'); /* never NULL */
    *second_line++ = '\0';

    if (first_line[0])
        dd_save_text(dd, FILENAME_KERNEL, first_line);
    dd_save_text(dd, FILENAME_BACKTRACE, second_line);

    /* check if trace doesn't have line: 'Your BIOS is broken' */
    if (strstr(second_line, "Your BIOS is broken"))
        dd_save_text(dd, FILENAME_NOT_REPORTABLE,
                _("A kernel problem occurred because of broken BIOS. "
                  "Unfortunately, such problems are not fixable by kernel maintainers."));
    /* check if trace doesn't have line: 'Your hardware is unsupported' */
    else if (strstr(second_line, "Your hardware is unsupported"))
        dd_save_text(dd, FILENAME_NOT_REPORTABLE,
                _("A kernel problem occurred, but your hardware is unsupported, "
                  "therefore kernel maintainers are unable to fix this problem."));
    else
    {
        char *tainted_short = kernel_tainted_short(second_line);
        if (tainted_short)
        {
            log_notice("Kernel is tainted '%s'", tainted_short);
            dd_save_text(dd, FILENAME_TAINTED_SHORT, tainted_short);

            char *tnt_long = kernel_tainted_long(tainted_short);
            dd_save_text(dd, FILENAME_TAINTED_LONG, tnt_long);
            free(tnt_long);

            struct strbuf *reason = strbuf_new();
            const char *fmt = _("A kernel problem occurred, but your kernel has been "
                    "tainted (flags:%s). Kernel maintainers are unable to "
                    "diagnose tainted reports.");
            strbuf_append_strf(reason, fmt, tainted_short);

            char *modlist = !proc_modules ? NULL : list_of_tainted_modules(proc_modules);
            if (modlist)
            {
                strbuf_append_strf(reason, _(" Tainted modules: %s."), modlist);
                free(modlist);
            }

            dd_save_text(dd, FILENAME_NOT_REPORTABLE, reason->buf);
            strbuf_free(reason);
            free(tainted_short);
        }
    }

    // TODO: add "Kernel oops: " prefix, so that all oopses have recognizable FILENAME_REASON?
    // kernel oops 1st line may look quite puzzling otherwise...
    strchrnul(second_line, '\n')[0] = '\0';
    dd_save_text(dd, FILENAME_REASON, second_line);
}

/* returns number of errors */
static unsigned create_oops_dump_dirs(GList *oops_list, unsigned oops_cnt)
{
    unsigned countdown = MAX_DUMPED_DD_COUNT; /* do not report hundreds of oopses */

    log_notice("Saving %u oopses as problem dirs", oops_cnt >= countdown ? countdown : oops_cnt);

    char *cmdline_str = xmalloc_fopen_fgetline_fclose("/proc/cmdline");
    char *fips_enabled = xmalloc_fopen_fgetline_fclose("/proc/sys/crypto/fips_enabled");
    char *proc_modules = xmalloc_open_read_close("/proc/modules", /*maxsize:*/ NULL);
    char *suspend_stats = xmalloc_open_read_close("/sys/kernel/debug/suspend_stats", /*maxsize:*/ NULL);

    time_t t = time(NULL);
    const char *iso_date = iso_date_string(&t);
    /* dump should be readable by all if we're run with -x */
    uid_t my_euid = (uid_t)-1L;
    mode_t mode = DEFAULT_DUMP_DIR_MODE | S_IROTH;
    /* and readable only for the owner otherwise */
    if (!world_readable_dump)
    {
        mode = DEFAULT_DUMP_DIR_MODE;
        my_euid = geteuid();
    }

    pid_t my_pid = getpid();
    unsigned idx = 0;
    unsigned errors = 0;
    while (idx < oops_cnt)
    {
        char base[sizeof("oops-YYYY-MM-DD-hh:mm:ss-%lu-%lu") + 2 * sizeof(long)*3];
        sprintf(base, "oops-%s-%lu-%lu", iso_date, (long)my_pid, (long)idx);
        char *path = concat_path_file(debug_dumps_dir, base);

        struct dump_dir *dd = dd_create(path, /*uid:*/ my_euid, mode);
        if (dd)
        {
            dd_create_basic_files(dd, /*uid:*/ my_euid, NULL);
            save_oops_data_in_dump_dir(dd, (char*)g_list_nth_data(oops_list, idx++), proc_modules);
            dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);
            dd_save_text(dd, FILENAME_ANALYZER, "Kerneloops");
            dd_save_text(dd, FILENAME_TYPE, "Kerneloops");
            if (cmdline_str)
                dd_save_text(dd, FILENAME_CMDLINE, cmdline_str);
            if (proc_modules)
                dd_save_text(dd, "proc_modules", proc_modules);
            if (fips_enabled && strcmp(fips_enabled, "0") != 0)
                dd_save_text(dd, "fips_enabled", fips_enabled);
            if (suspend_stats)
                dd_save_text(dd, "suspend_stats", suspend_stats);
            dd_close(dd);
            notify_new_path(path);
        }
        else
            errors++;

        free(path);

        if (--countdown == 0)
            break;

        if (dd && throttle_dd_creation)
            sleep(1);
    }

    free(cmdline_str);
    free(proc_modules);
    free(fips_enabled);
    free(suspend_stats);

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
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found oopses on standard output")),
        /* oopses don't contain any sensitive info, and even
         * the old koops app was showing the oopses to all users
         */
        OPT_STRING('d', NULL, &debug_dumps_dir, "DIR", _("Create new problem directory in DIR for every oops found")),
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
        map_string_t *settings = new_map_string();

        load_abrt_plugin_conf_file("oops.conf", settings);

        int only_fatal_mce = 1;
        try_get_map_string_item_as_bool(settings, "OnlyFatalMCE", &only_fatal_mce);

        free_map_string(settings);

        if (only_fatal_mce)
        {
            regex_t mce_re;
            if (regcomp(&mce_re, "^Machine .*$", REG_NOSUB) != 0)
                perror_msg_and_die(_("Failed to compile regex"));

            const regex_t *filter[] = { &mce_re, NULL };

            koops_print_suspicious_strings_filtered(filter);

            regfree(&mce_re);
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
        debug_dumps_dir = g_settings_dump_location;
        g_settings_dump_location = NULL;
        free_abrt_conf_data();
    }

    argv += optind;
    if (argv[0])
        xmove_fd(xopen(argv[0], O_RDONLY), STDIN_FILENO);

    world_readable_dump = (opts & OPT_x);
    throttle_dd_creation = (opts & OPT_t);
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

            log("Creating problem directories");
            errors = create_oops_dump_dirs(oops_list, oops_cnt);
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
        if (opts & OPT_u)
        {
            log("Updating problem directory");
            switch (oops_cnt)
            {
                case 1:
                    {
                        struct dump_dir *dd = dd_opendir(problem_dir, /*open for writing*/0);
                        if (dd)
                        {
                            save_oops_data_in_dump_dir(dd, (char *)oops_list->data, /*no proc modules*/NULL);
                            dd_close(dd);
                        }
                    }
                    break;
                default:
                    error_msg(_("Can't update the problem: more than one oops found"));
                    break;
            }
        }
    }
    list_free_with_free(oops_list);
    //oops_list = NULL;

    /* If we are run by a log watcher, this delays log rescan
     * (because log watcher waits to us to terminate)
     * and possibly prevents dreaded "abrt storm".
     */
    int unreported_cnt = oops_cnt - MAX_DUMPED_DD_COUNT;
    if (unreported_cnt > 0 && throttle_dd_creation)
    {
        /* Quadratic throttle time growth, but careful to not overflow in "n*n" */
        int n = unreported_cnt > 30 ? 30 : unreported_cnt;
        n = n * n;
        if (n > 9)
            log(_("Sleeping for %d seconds"), n);
        sleep(n); /* max 15 mins */
    }

    return errors;
}
