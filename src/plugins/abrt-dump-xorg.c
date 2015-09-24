/*
    Copyright (C) 2012  ABRT Team
    Copyright (C) 2012  Red Hat, Inc.

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

/* I want to use -Werror, but gcc-4.4 throws a curveball:
 * "warning: ignoring return value of 'ftruncate', declared with attribute warn_unused_result"
 * and (void) cast is not enough to shut it up! Oh God...
 */
#define IGNORE_RESULT(func_call) do { if (func_call) /* nothing */; } while (0)

enum {
    OPT_v = 1 << 0,
    OPT_s = 1 << 1,
    OPT_o = 1 << 2,
    OPT_d = 1 << 3,
    OPT_D = 1 << 4,
    OPT_x = 1 << 5,
    OPT_m = 1 << 6,
};

/* How many problem dirs to create at most?
 * Also causes cooldown sleep if exceeded -
 * useful when called from a log watcher.
 */
#define MAX_DUMPED_DD_COUNT  5

static unsigned g_bt_count = 0;
static unsigned g_opts;
static const char *debug_dumps_dir = ".";

static char *skip_pfx(char *p)
{
    if (p[0] != '[')
        return p;
    char *q = strchr(p, ']');
    if (!q)
        return p;

    if (q[1] == ' ')
    {
        q += 2;
        /* if there is (EE), ignore it */
        if (strncmp(q, "(EE)", 4) == 0)
        {
            /* if ' ' follows (EE), ignore it too */
            return q + (4 + (q[4] == ' '));
        }

        return q;
    }
    return p;
}

static char *list2lines(GList *list)
{
    struct strbuf *s = strbuf_new();
    while (list)
    {
        strbuf_append_str(s, (char*)list->data);
        strbuf_append_char(s, '\n');
        free(list->data);
        list = g_list_delete_link(list, list);
    }
    return strbuf_free_nobuf(s);
}

static void save_bt_to_dump_dir(const char *bt, const char *exe, const char *reason)
{
    time_t t = time(NULL);
    const char *iso_date = iso_date_string(&t);

    pid_t my_pid = getpid();

    char base[sizeof("xorg-YYYY-MM-DD-hh:mm:ss-%lu-%lu") + 2 * sizeof(long)*3];
    sprintf(base, "xorg-%s-%lu-%u", iso_date, (long)my_pid, g_bt_count);
    char *path = concat_path_file(debug_dumps_dir, base);

    struct dump_dir *dd = dd_create(path, /*fs owner*/0, DEFAULT_DUMP_DIR_MODE);
    if (dd)
    {
        dd_create_basic_files(dd, /*no uid*/(uid_t)-1L, NULL);
        dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);
        dd_save_text(dd, FILENAME_ANALYZER, "abrt-xorg");
        dd_save_text(dd, FILENAME_TYPE, "xorg");
        dd_save_text(dd, FILENAME_REASON, reason);
        dd_save_text(dd, FILENAME_BACKTRACE, bt);
        /*
         * Reporters usually need component name to file a bug.
	 * It is usually derived from executable.
         * We _guess_ X server's executable name as a last resort.
         * Better ideas?
         */
        if (!exe)
        {
            exe = "/usr/bin/X";
            if (access("/usr/bin/Xorg", X_OK) == 0)
                exe = "/usr/bin/Xorg";
        }
        dd_save_text(dd, FILENAME_EXECUTABLE, exe);
        if (!(g_opts & OPT_x))
            dd_set_no_owner(dd);
        dd_close(dd);
        notify_new_path(path);
    }

    free(path);
}

/* Called after "Backtrace:" line was read.
 * Example (yes, stray newline before 'B' is real):
[ 86985.879]<space>
[ 60244.259] (EE) Backtrace:
[ 60244.262] (EE) 0: /usr/libexec/Xorg (OsLookupColor+0x139) [0x59add9]
[ 60244.264] (EE) 1: /lib64/libc.so.6 (__restore_rt+0x0) [0x7f61be425b1f]
[ 60244.266] (EE) 2: /usr/lib64/xorg/modules/drivers/intel_drv.so (_init+0xa9fc) [0x7f61b903116c]
[ 60244.267] (EE) 3: /usr/lib64/xorg/modules/drivers/intel_drv.so (_init+0xbe27) [0x7f61b90339a7]
[ 60244.268] (EE) 4: /usr/lib64/xorg/modules/drivers/intel_drv.so (_init+0x31060) [0x7f61b907db00]
[ 60244.269] (EE) 5: /usr/lib64/xorg/modules/drivers/intel_drv.so (_init+0x3fb73) [0x7f61b909b0c3]
[ 60244.270] (EE) 6: /usr/lib64/xorg/modules/drivers/intel_drv.so (_init+0x3fe1a) [0x7f61b909b77a]
[ 60244.270] (EE) 7: /usr/libexec/Xorg (DamageRegionAppend+0x3783) [0x525003]
[ 60244.270] (EE) 8: /usr/libexec/Xorg (SendGraphicsExpose+0xeb3) [0x4340b3]
[ 60244.270] (EE) 9: /usr/libexec/Xorg (SendErrorToClient+0x2df) [0x43684f]
[ 60244.271] (EE) 10: /usr/libexec/Xorg (remove_fs_handlers+0x453) [0x43a893]
[ 60244.272] (EE) 11: /lib64/libc.so.6 (__libc_start_main+0xf0) [0x7f61be411580]
[ 60244.272] (EE) 12: /usr/libexec/Xorg (_start+0x29) [0x424b79]
[ 60244.273] (EE) 13: ? (?+0x29) [0x29]
[ 60244.273] (EE) 
[ 60244.273] (EE) Segmentation fault at address 0x7f61d93f6160
[ 60244.273] (EE) 
 */
static void process_xorg_bt(void)
{
    char *reason = NULL;
    char *exe = NULL;
    GList *list = NULL;
    unsigned cnt = 0;
    char *line;
    while ((line = xmalloc_fgetline(stdin)) != NULL)
    {
        char *p = skip_pfx(line);

        /* ignore empty lines
         * [ 60244.273] (EE) 13: ? (?+0x29) [0x29]
         * [ 60244.273] (EE) <---
         * [ 60244.273] (EE) Segmentation fault at address 0x7f61d93f6160
         */
        if (*p == '\0')
            continue;

        /* xorg-server-1.12.0/os/osinit.c:
         * if (sip->si_code == SI_USER) {
         *     ErrorF("Recieved signal %d sent by process %ld, uid %ld\n",
         *             ^^^^^^^^ yes, typo here! Can't grep for this word! :(
         *            signo, (long) sip->si_pid, (long) sip->si_uid);
         * } else {
         *     switch (signo) {
         *         case SIGSEGV:
         *         case SIGBUS:
         *         case SIGILL:
         *         case SIGFPE:
         *             ErrorF("%s at address %p\n", strsignal(signo), sip->si_addr);
         */
        if (*p < '0' || *p > '9')
        {
            if (strstr(p, " at address ") || strstr(p, " sent by process "))
            {
                overlapping_strcpy(line, p);
                reason = line;
                line = NULL;
            }
            /* TODO: Other cases when we have useful reason string? */
            break;
        }

        errno = 0;
        char *end;
        IGNORE_RESULT(strtoul(p, &end, 10));
        if (errno || end == p || *end != ':')
            break;

        /* This looks like bt line */

        /* Guess Xorg server's executable name from it */
        if (!exe)
        {
            char *filename = skip_whitespace(end + 1);
            char *filename_end = skip_non_whitespace(filename);
            char sv = *filename_end;
            *filename_end = '\0';
            /* Does it look like "[/usr]/[s]bin/Xfoo"? */
            if (strstr(filename, "bin/X"))
                exe = xstrdup(filename);
            *filename_end = sv;
        }

        /* Save it to list */
        overlapping_strcpy(line, p);
        list = g_list_prepend(list, line);
        line = NULL;
        if (++cnt > 255) /* prevent ridiculously large bts */
            break;
    }
    free(line);

    if (list)
    {
        list = g_list_reverse(list);
        char *bt = list2lines(list); /* frees list */
        if (g_opts & OPT_o)
            printf("%s%s%s\n", bt, reason ? reason : "", reason ? "\n" : "");
        if (g_opts & (OPT_d|OPT_D))
            if (g_bt_count <= MAX_DUMPED_DD_COUNT)
                save_bt_to_dump_dir(bt, exe, reason ? reason : "Xorg server crashed");
        free(bt);
    }
    free(reason);
    free(exe);
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
        "Extract Xorg crash from FILE (or standard input)"
    );
    /* Keep OPT_z enums and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found crash data on standard output")),
        OPT_STRING('d', NULL, &debug_dumps_dir, "DIR", _("Create problem directory in DIR for every crash found")),
        OPT_BOOL(  'D', NULL, NULL, _("Same as -d DumpLocation, DumpLocation is specified in abrt.conf")),
        OPT_BOOL(  'x', NULL, NULL, _("Make the problem directory world readable")),
        OPT_BOOL(  'm', NULL, NULL, _("Print search string(s) to stdout and exit")),
        OPT_END()
    };
    unsigned opts = g_opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    msg_prefix = g_progname;
    if ((opts & OPT_s) || getenv("ABRT_SYSLOG"))
    {
        logmode = LOGMODE_JOURNAL;
    }

    if (opts & OPT_m)
    {
        puts("Backtrace");
        return 0;
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

    char *line;
    while ((line = xmalloc_fgetline(stdin)) != NULL)
    {
        char *p = skip_pfx(line);
        if (strcmp(p, "Backtrace:") == 0)
        {
            free(line);
            g_bt_count++;
            process_xorg_bt();
            continue;
        }
        free(line);
    }

    /* If we are run by a log watcher, this delays log rescan
     * (because log watcher waits to us to terminate)
     * and possibly prevents dreaded "abrt storm".
     */
    if (opts & (OPT_d|OPT_D))
    {
        if (g_bt_count > MAX_DUMPED_DD_COUNT)
            sleep(g_bt_count - MAX_DUMPED_DD_COUNT);
    }

    return 0;
}
