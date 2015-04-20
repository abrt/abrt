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
#include <asm/unistd.h> /* __NR_syslog */
#include <sys/inotify.h>
#include "libabrt.h"

/* How many problem dirs to create at most?
 * Also causes cooldown sleep if exceeded -
 * useful when called from a log watcher.
 */
#define MAX_DUMPED_DD_COUNT  5

static bool world_readable_dump = false;
static const char *debug_dumps_dir = ".";

#define MAX_SCAN_BLOCK  (4*1024*1024)
#define READ_AHEAD          (10*1024)

static void scan_dmesg(GList **oops_list)
{
    VERB1 log("Scanning dmesg");

    /* syslog(3) - read the last len bytes from the log buffer
     * (non-destructively), but dont read more than was written
     * into the buffer since the last "clear ring buffer" cmd.
     * Returns the number of bytes read.
     */
    char *buffer = xzalloc(16*1024);
    syscall(__NR_syslog, 3, buffer, 16*1024 - 1); /* always NUL terminated */
    koops_extract_oopses(oops_list, buffer, strlen(buffer));
    free(buffer);
}

static int scan_syslog_file(GList **oops_list, int fd, struct stat *statbuf, int partial_line_len)
{
    /* fstat(fd, &statbuf) was just done by caller */

    off_t cur_pos = lseek(fd, 0, SEEK_CUR);
    if (statbuf->st_size <= cur_pos)
    {
        /* If file was truncated, treat it as a new file.
         * (changing inode# causes caller to think that file was closed or renamed)
         */
        if (statbuf->st_size < cur_pos)
            statbuf->st_ino++;
        return partial_line_len; /* we are at EOF, nothing to do */
    }

    VERB3 log("File grew by %llu bytes, from %llu to %llu",
        (long long)(statbuf->st_size - cur_pos),
        (long long)(cur_pos),
        (long long)(statbuf->st_size));

    /* Do not try to allocate an absurd amount of memory */
    int sz = MAX_SCAN_BLOCK - READ_AHEAD;
    if (sz > statbuf->st_size - cur_pos)
        sz = statbuf->st_size - cur_pos;

    /* Rewind to the beginning of the current line */
    if (partial_line_len > 0 && lseek(fd, -partial_line_len, SEEK_CUR) != (off_t)-1)
    {
        VERB3 log("Went back %u bytes", partial_line_len);
        cur_pos -= partial_line_len;
        sz += partial_line_len;
    }

    /*
     * In theory we have a race here, since someone can spew
     * to /var/log/messages before we read it in...
     * We try to deal with it by reading READ_AHEAD extra.
     */
    sz += READ_AHEAD;
    char *buffer = xzalloc(sz);

    partial_line_len = 0;
    do {
        int r = full_read(fd, buffer, sz-1);
        if (r <= 0)
            break;
        VERB3 log("Read %u bytes", r);

        /* For future scans, try to find where last (incomplete) line starts */
        partial_line_len = 0;
        char *last_newline = memrchr(buffer, '\n', r) ? : buffer-1;
        partial_line_len = buffer+r - (last_newline+1);
        if (partial_line_len > 500) /* cap it */
            partial_line_len = 500;

        koops_extract_oopses(oops_list, buffer, r);
        cur_pos += r;
    } while (cur_pos < statbuf->st_size);

    free(buffer);

    return partial_line_len;
}

/* returns number of errors */
static unsigned save_oops_to_dump_dir(GList *oops_list, unsigned oops_cnt)
{
    unsigned countdown = MAX_DUMPED_DD_COUNT + 1; /* do not report hundreds of oopses */
    unsigned idx = 0;

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
    if (g_settings_privatereports)
    {
        if (world_readable_dump)
            log("Not going to make dump directories world readable because PrivateReports is on");

        mode = 0640;
        my_euid = 0;
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
            dd_create_basic_files(dd, /*uid:*/ my_euid, NULL);
            dd_save_text(dd, "abrt_version", VERSION);
            dd_save_text(dd, FILENAME_ANALYZER, "Kerneloops");
            dd_save_text(dd, FILENAME_TYPE, "Kerneloops");
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

    /* This delays log rescan
     * and possibly prevents dreaded "abrt storm".
     */
    if (oops_cnt > MAX_DUMPED_DD_COUNT)
    {
        sleep(oops_cnt - MAX_DUMPED_DD_COUNT);
    }

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
        "& [-vsrowx] [-d DIR] FILE\n"
        "or: & [-vsrowx] -D FILE\n"
        "\n"
        "Extract oops from syslog/dmesg file"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_s = 1 << 1,
        OPT_r = 1 << 2,
        OPT_o = 1 << 3,
        OPT_w = 1 << 4,
        OPT_d = 1 << 5,
        OPT_x = 1 << 6,
        OPT_D = 1 << 7,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_BOOL(  'r', NULL, NULL, _("Parse kernel's message buffer before parsing FILE")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found oopses on standard output")),
        OPT_BOOL(  'w', NULL, NULL, _("Do not exit, watch the file for new oopses")),
        /* oopses don't contain any sensitive info, and even
         * the old koops app was showing the oopses to all users
         */
        OPT_STRING('d', NULL, &debug_dumps_dir, "DIR", _("Create ABRT dump in DIR for every oops found")),
        OPT_BOOL(  'x', NULL, NULL, _("Make the problem directory world readable")),
        OPT_BOOL(  'D', NULL, NULL, _("Same as -d DumpLocation, DumpLocation is specified in abrt.conf")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    if ((opts & OPT_d) && (opts & OPT_D))
        show_usage_and_die(program_usage_string, program_options);

    export_abrt_envvars(0);

    msg_prefix = g_progname;
    if ((opts & OPT_s) || getenv("ABRT_SYSLOG"))
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    argv += optind;
    if (!argv[0])
        show_usage_and_die(program_usage_string, program_options);
    const char *filename = argv[0];

    int inotify_fd = -1;
    if (opts & OPT_w)
    {
        inotify_fd = inotify_init();
        if (inotify_fd == -1)
            perror_msg_and_die("inotify_init failed");
        //close_on_exec_on(inotify_fd);
    }

    GList *oops_list = NULL;
    if (opts & OPT_r)
        /* Scan dmesg (only once even with -w) */
        scan_dmesg(&oops_list);

    world_readable_dump = (opts & OPT_x);

    int partial_line_len = 0;
    struct stat statbuf;
    int file_fd = -1;
    int wd = -1;
    unsigned errors = 0;

    while (1) /* loops only if -w */
    {
        /* If file is already opened, parse oopses from current pos */
        if (file_fd >= 0)
        {
            memset(&statbuf, 0, sizeof(statbuf));
            fstat(file_fd, &statbuf);
            partial_line_len = scan_syslog_file(&oops_list, file_fd, &statbuf, partial_line_len);

            /* Was file deleted or replaced? */
            ino_t fd_ino = statbuf.st_ino;
            if (stat(filename, &statbuf) != 0 || statbuf.st_ino != fd_ino) /* yes */
            {
                VERB2 log("Inode# changed, closing fd");
                close(file_fd);
                if (wd >= 0)
                    inotify_rm_watch(inotify_fd, wd);
                file_fd = -1;
                wd = -1;
                partial_line_len = 0;
            }
        }

        /* If file isn't opened, try to open it and parse oopses */
        if (file_fd < 0)
        {
            file_fd = open(filename, O_RDONLY);
            if (file_fd < 0)
            {
                if (!(opts & OPT_w))
                    perror_msg_and_die("Can't open '%s'", filename);
                /* with -w, we ignore open errors */
            }
            else
            {
                VERB2 log("Opened '%s'", filename);
                /* For -w case, if we don't have inotify watch yet, open one */
                if ((opts & OPT_w) && wd < 0)
                {
                    wd = inotify_add_watch(inotify_fd, filename, IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF);
                    if (wd < 0)
                        perror_msg("inotify_add_watch failed on '%s'", filename);
                    else
                        VERB2 log("Added inotify watch for '%s'", filename);
                }
                if (fstat(file_fd, &statbuf) == 0)
                {
                    /* If file is large, skip the beginning.
                     * IOW: ignore old log messages because they are unlikely
                     * to have sufficiently recent data to be useful.
                     */
                    if (statbuf.st_size > (MAX_SCAN_BLOCK - READ_AHEAD))
                        lseek(file_fd, statbuf.st_size - (MAX_SCAN_BLOCK - READ_AHEAD), SEEK_SET);
                    /* Note that statbuf is filled by fstat by now,
                     * scan_syslog_file needs that
                     */
                    partial_line_len = scan_syslog_file(&oops_list, file_fd, &statbuf, partial_line_len);
                }
            }
        }

        /* Print and/or save oopses */
        int oops_cnt = g_list_length(oops_list);
        if (!(opts & OPT_w) || oops_cnt != 0)
            log("Found oopses: %d", oops_cnt);
        if (oops_cnt != 0)
        {
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
            if ((opts & OPT_d) || (opts & OPT_D))
            {
                if (opts & OPT_D)
                {
                    load_abrt_conf();
                    debug_dumps_dir = g_settings_dump_location;
                }

                log("Creating dump directories");
                errors += save_oops_to_dump_dir(oops_list, oops_cnt);
                if (errors > 0)
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

                if (opts & OPT_D)
                    free_abrt_conf_data();
            }
        }
        list_free_with_free(oops_list);
        oops_list = NULL;

        /* Done if no -w */
        if (!(opts & OPT_w))
            break;

        /* Even if log file grows all the time, say, a new line every 5 ms,
         * we don't want to scan it all the time. Sleep a bit and let it grow
         * in bigger increments.
         * Sleep longer if file does not exist.
         */
        sleep(file_fd >= 0 ? 1 : 59);

        /* Now wait for it to change, be moved or deleted */
        if (wd >= 0)
        {
            char buf[4096];
            VERB3 log("Waiting for '%s' to change", filename);
            /* We block here: */
            int len = read(inotify_fd, buf, sizeof(buf));
            if (len < 0 && errno != EINTR) /* I saw EINTR here on strace attach */
                perror_msg("Error reading inotify fd");
            /* we don't actually check what happened to file -
             * the code will handle all possibilities.
             */
            VERB3 log("Change in '%s' detected", filename);
            /* Let them finish writing to the log file. otherwise
             * we may end up trying to analyze partial oops.
             */
            sleep(1);
        }

    } /* while (1) */

    return errors;
}
