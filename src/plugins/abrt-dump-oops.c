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
#include <sys/ioctl.h> /* ioctl(FIONREAD) */
#include "abrtlib.h"
#include "parse_options.h"

static bool world_readable_dump = false;
static const char *debug_dumps_dir = ".";

static void queue_oops(GList **vec, const char *data, const char *version)
{
    char *ver_data = xasprintf("%s\n%s", version, data);
    *vec = g_list_append(*vec, ver_data);
}

/*
 * extract_version tries to find the kernel version in given data
 */
static char *extract_version(const char *linepointer)
{
    if (strstr(linepointer, "Pid")
     || strstr(linepointer, "comm")
     || strstr(linepointer, "CPU")
     || strstr(linepointer, "REGS")
     || strstr(linepointer, "EFLAGS")
    ) {
        char* start;
        char* end;

        start = strstr((char*)linepointer, "2.6.");
        if (start)
        {
            end = strchr(start, ')');
            if (!end)
                end = strchrnul(start, ' ');
            return xstrndup(start, end-start);
        }
    }

    return NULL;
}

/*
 * extract_oops tries to find oops signatures in a log
 */
struct line_info {
    char *ptr;
    char level;
};

static void record_oops(GList **oops_list, struct line_info* lines_info, int oopsstart, int oopsend)
{
    int q;
    int len;
    int rv = 1;

    len = 2;
    for (q = oopsstart; q <= oopsend; q++)
        len += strlen(lines_info[q].ptr) + 1;

    /* too short oopses are invalid */
    if (len > 100)
    {
        char *oops = (char*)xzalloc(len);
        char *dst = oops;
        char *version = NULL;
        for (q = oopsstart; q <= oopsend; q++)
        {
            if (!version)
                version = extract_version(lines_info[q].ptr);
            if (lines_info[q].ptr[0])
            {
                dst = stpcpy(dst, lines_info[q].ptr);
                dst = stpcpy(dst, "\n");
            }
        }
        if ((dst - oops) > 100)
            queue_oops(oops_list, oops, version ? version : "undefined");
        else
            /* too short oopses are invalid */
            rv = 0;
        free(oops);
        free(version);
    }

    VERB3 if (rv == 0) log("Dropped oops: too short");
}

static void extract_oopses(GList **oops_list, char *buffer, size_t buflen)
{
    char *c;
    int linecount = 0;
    int lines_info_size = 0;
    struct line_info *lines_info = NULL;

    /* Split buffer into lines */

    if (buflen != 0)
            buffer[buflen - 1] = '\n';  /* the buffer usually ends with \n, but let's make sure */
    c = buffer;
    while (c < buffer + buflen)
    {
        char linelevel;
        char *c9;
        char *colon;

        linecount++;
        c9 = (char*)memchr(c, '\n', buffer + buflen - c); /* a \n will always be found */
        assert(c9);
        *c9 = '\0'; /* turn the \n into a string termination */
        if (c9 == c)
            goto next_line;

        /* Is it a syslog file (/var/log/messages or similar)?
         * Even though _usually_ it looks like "Nov 19 12:34:38 localhost kernel: xxx",
         * some users run syslog in non-C locale:
         * "2010-02-22T09:24:08.156534-08:00 gnu-4 gnome-session[2048]: blah blah"
         *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ !!!
         * We detect it by checking for N:NN:NN pattern in first 15 chars
         * (and this still is not good enough... false positive: "pci 0000:15:00.0: PME# disabled")
         */
        colon = strchr(c, ':');
        if (colon && colon > c && colon < c + 15
         && isdigit(colon[-1]) /* N:... */
         && isdigit(colon[1]) /* ...N:NN:... */
         && isdigit(colon[2])
         && colon[3] == ':'
         && isdigit(colon[4]) /* ...N:NN:NN... */
         && isdigit(colon[5])
        ) {
            /* It's syslog file, not a bare dmesg */

            /* Skip non-kernel lines */
            char *kernel_str = strstr(c, "kernel: ");
            if (!kernel_str)
            {
                /* if we see our own marker:
                 * "hostname abrt: Kerneloops: Reported 1 kernel oopses to Abrt"
                 * we know we submitted everything upto here already */
                if (strstr(c, "kernel oopses to Abrt"))
                {
                    VERB3 log("Found our marker at line %d", linecount);
                    free(lines_info);
                    lines_info = NULL;
                    lines_info_size = 0;
                    list_free_with_free(*oops_list);
                    *oops_list = NULL;
                }
                goto next_line;
            }
            c = kernel_str + sizeof("kernel: ")-1;
        }

        linelevel = 0;
        /* store and remove kernel log level */
        if (*c == '<' && c[1] && c[2] == '>')
        {
            linelevel = c[1];
            c += 3;
        }
        /* remove jiffies time stamp counter if present */
        if (*c == '[')
        {
            char *c2 = strchr(c, '.');
            char *c3 = strchr(c, ']');
            if (c2 && c3 && (c2 < c3) && (c3-c) < 14 && (c2-c) < 8)
            {
                c = c3 + 1;
                if (*c == ' ')
                        c++;
            }
        }
        if ((lines_info_size & 0xfff) == 0)
        {
            lines_info = xrealloc(lines_info, (lines_info_size + 0x1000) * sizeof(lines_info[0]));
        }
        lines_info[lines_info_size].ptr = c;
        lines_info[lines_info_size].level = linelevel;
        lines_info_size++;
next_line:
        c = c9 + 1;
    }

    /* Analyze lines */

    int i;
    char prevlevel = 0;
    int oopsstart = -1;
    int inbacktrace = 0;

    i = 0;
    while (i < lines_info_size)
    {
        char *curline = lines_info[i].ptr;

        if (curline == NULL)
        {
            i++;
            continue;
        }
        while (*curline == ' ')
            curline++;

        if (oopsstart < 0)
        {
            /* Find start-of-oops markers */
            /* In some comparisons, we skip 1st letter, to avoid dealing with
             * changes in capitalization in kernel. For example, I see that
             * current kernel git (at 2011-01-01) has both "kernel BUG at ..."
             * and "Kernel BUG at ..." messages, and I don't want to change
             * the code below whenever kernel is changed to use "K" (or "k")
             * uniformly.
             */
            if (strstr(curline, /*g*/ "eneral protection fault:"))
                oopsstart = i;
            else if (strstr(curline, "BUG:"))
                oopsstart = i;
            else if (strstr(curline, /*k*/ "ernel BUG at"))
                oopsstart = i;
            else if (strstr(curline, "do_IRQ: stack overflow:"))
                oopsstart = i;
            else if (strstr(curline, "RTNL: assertion failed"))
                 oopsstart = i;
            else if (strstr(curline, /*e*/ "eek! page_mapcount(page) went negative!"))
                oopsstart = i;
            else if (strstr(curline, /*n*/ "ear stack overflow (cur:"))
                oopsstart = i;
            else if (strstr(curline, /*d*/ "ouble fault:"))
                oopsstart = i;
            else if (strstr(curline, /*b*/ "adness at"))
                oopsstart = i;
            else if (strstr(curline, "NETDEV WATCHDOG"))
                oopsstart = i;
            else if (strstr(curline, "WARNING: at ")) /* WARN_ON() generated message */
                oopsstart = i;
            else if (strstr(curline, /*u*/ "nable to handle kernel"))
                oopsstart = i;
            else if (strstr(curline, /*s*/ "ysctl table check failed"))
                oopsstart = i;
            else if (strstr(curline, "INFO: possible recursive locking detected"))
                oopsstart = i;
            // Not needed: "--[ cut here ]--" is always followed
            // by "Badness at", "kernel BUG at", or "WARNING: at" string
            //else if (strstr(curline, "------------[ cut here ]------------"))
            //  oopsstart = i;
            else if (strstr(curline, "list_del corruption"))
                oopsstart = i;
            else if (strstr(curline, "list_add corruption"))
                oopsstart = i;

            if (i >= 3 && strstr(curline, "Oops:"))
                oopsstart = i-3;

            if (oopsstart >= 0)
            {
                /* debug information */
                VERB3 {
                    log("Found oops at line %d: '%s'", oopsstart, lines_info[oopsstart].ptr);
                    if (oopsstart != i)
                            log("Trigger line is %d: '%s'", i, c);
                }
                /* try to find the end marker */
                int i2 = i + 1;
                while (i2 < lines_info_size && i2 < (i+50))
                {
                    if (strstr(lines_info[i2].ptr, "---[ end trace"))
                    {
                        inbacktrace = 1;
                        i = i2;
                        break;
                    }
                    i2++;
                }
            }
        }

        /* Are we entering a call trace part? */
        /* a call trace starts with "Call Trace:" or with the " [<.......>] function+0xFF/0xAA" pattern */
        if (oopsstart >= 0 && !inbacktrace)
        {
            if (strstr(curline, "Call Trace:"))
                inbacktrace = 1;
            else
            if (strnlen(curline, 9) > 8
             && curline[0] == '[' && curline[1] == '<'
             && strstr(curline, ">]")
             && strstr(curline, "+0x")
             && strstr(curline, "/0x")
            ) {
                inbacktrace = 1;
            }
        }

        /* Are we at the end of an oops? */
        else if (oopsstart >= 0 && inbacktrace)
        {
            int oopsend = INT_MAX;

            /* line needs to start with " [" or have "] [" if it is still a call trace */
            /* example: "[<ffffffffa006c156>] radeon_get_ring_head+0x16/0x41 [radeon]" */
            if (curline[0] != '['
             && !strstr(curline, "] [")
             && !strstr(curline, "--- Exception")
             && !strstr(curline, "LR =")
             && !strstr(curline, "<#DF>")
             && !strstr(curline, "<IRQ>")
             && !strstr(curline, "<EOI>")
             && !strstr(curline, "<<EOE>>")
             && strncmp(curline, "Code: ", 6) != 0
             && strncmp(curline, "RIP ", 4) != 0
             && strncmp(curline, "RSP ", 4) != 0
            ) {
                oopsend = i-1; /* not a call trace line */
            }
            /* oops lines are always more than 8 chars long */
            else if (strnlen(curline, 8) < 8)
                oopsend = i-1;
            /* single oopses are of the same loglevel */
            else if (lines_info[i].level != prevlevel)
                oopsend = i-1;
            else if (strstr(curline, "Instruction dump:"))
                oopsend = i;
            /* if a new oops starts, this one has ended */
            else if (strstr(curline, "WARNING: at ") && oopsstart != i) /* WARN_ON() generated message */
                oopsend = i-1;
            else if (strstr(curline, "Unable to handle") && oopsstart != i)
                oopsend = i-1;
            /* kernel end-of-oops marker (not including marker itself) */
            else if (strstr(curline, "---[ end trace"))
                oopsend = i-1;

            if (oopsend <= i)
            {
                VERB3 log("End of oops at line %d (%d): '%s'", oopsend, i, lines_info[oopsend].ptr);
                record_oops(oops_list, lines_info, oopsstart, oopsend);
                oopsstart = -1;
                inbacktrace = 0;
            }
        }

        prevlevel = lines_info[i].level;
        i++;

        if (oopsstart >= 0)
        {
            /* Do we have a suspiciously long oops? Cancel it */
            if (i-oopsstart > 60)
            {
                inbacktrace = 0;
                oopsstart = -1;
                VERB3 log("Dropped oops, too long");
                continue;
            }
            if (!inbacktrace && i-oopsstart > 40)
            {
                /*inbacktrace = 0; - already is */
                oopsstart = -1;
                VERB3 log("Dropped oops, too long");
                continue;
            }
        }
    } /* while (i < lines_info_size) */

    /* process last oops if we have one */
    if (oopsstart >= 0 && inbacktrace)
    {
        int oopsend = i-1;
        VERB3 log("End of oops at line %d (end of file): '%s'", oopsend, lines_info[oopsend].ptr);
        record_oops(oops_list, lines_info, oopsstart, oopsend);
    }

    free(lines_info);
}

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
    extract_oopses(oops_list, buffer, strlen(buffer));
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

        extract_oopses(oops_list, buffer, r);
        cur_pos += r;
    } while (cur_pos < statbuf->st_size);

    free(buffer);

    return partial_line_len;
}

/* returns number of errors */
static unsigned save_oops_to_dump_dir(GList *oops_list, unsigned oops_cnt)
{
    unsigned countdown = 16; /* do not report hundreds of oopses */
    unsigned idx = oops_cnt;

    VERB1 log("Saving %u oopses as dump dirs", idx >= countdown ? countdown-1 : idx);

    char *tainted_str = NULL;
    FILE *tainted_fp = fopen("/proc/sys/kernel/tainted", "r");
    if (tainted_fp)
    {
        tainted_str = xmalloc_fgetline(tainted_fp);
        fclose(tainted_fp);
    }
    else
        perror_msg("Can't open '%s'", "/proc/sys/kernel/tainted");

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
    while (idx != 0 && --countdown != 0)
    {
        char *first_line = (char*)g_list_nth_data(oops_list, --idx);
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
            dd_save_text(dd, FILENAME_ANALYZER, "Kerneloops");
            dd_save_text(dd, FILENAME_KERNEL, first_line);
            if (cmdline_str)
                dd_save_text(dd, FILENAME_CMDLINE, cmdline_str);
            dd_save_text(dd, FILENAME_BACKTRACE, second_line);
// TODO: add "Kernel oops: " prefix, so that all oopses have recognizable FILENAME_REASON?
// kernel oops 1st line may look quite puzzling otherwise...
            strchrnul(second_line, '\n')[0] = '\0';
            dd_save_text(dd, FILENAME_REASON, second_line);

            if (tainted_str && tainted_str[0] != '0')
                dd_save_text(dd, FILENAME_TAINTED, tainted_str);

            dd_close(dd);
        }
        else
            errors++;
    }

    free(tainted_str);
    free(cmdline_str);

    return errors;
}

int main(int argc, char **argv)
{
    abrt_init(argv);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "\b [-vsrowx] [-d DIR] FILE\n"
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
        OPT_BOOL(  'x', NULL, NULL, _("Make the dump directory world readable")),
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
                    printf("\nVersion: %s", (char*)g_list_nth_data(oops_list, i++));
            }
            if (opts & OPT_d)
            {
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
