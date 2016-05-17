/*
 * Copyright (C) 2015  ABRT team
 * Copyright (C) 2015  RedHat Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "libabrt.h"
#include "xorg-utils.h"

/* I want to use -Werror, but gcc-4.4 throws a curveball:
 * "warning: ignoring return value of 'ftruncate', declared with attribute warn_unused_result"
 * and (void) cast is not enough to shut it up! Oh God...
 */
#define IGNORE_RESULT(func_call) do { if (func_call) /* nothing */; } while (0)

#define DEFAULT_XORG_CRASH_REASON "Display server crashed"

int abrt_xorg_signaled_sleep(int seconds)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGHUP);

    struct timespec timeout;
    timeout.tv_sec = seconds;
    timeout.tv_nsec = 0;

    return g_abrt_xorg_sleep_woke_up_on_signal = sigtimedwait(&set, NULL, &timeout);
}


void xorg_crash_info_free(struct xorg_crash_info *crash_info)
{
    if (crash_info == NULL)
        return;
    free(crash_info->backtrace);
    free(crash_info->reason);
    free(crash_info->exe);
    free(crash_info);
}

char *skip_pfx(char *str)
{
    if (str[0] == '[')
    {
        char *q = strchr(str, ']');
        if (q)
            str = q + 1;
    }

    if (str[0] == ' ')
        ++str;

    /* if there is (EE), ignore it */
    if (strncmp(str, "(EE)", 4) == 0)
        /* if ' ' follows (EE), ignore it too */
        return str + (4 + (str[4] == ' '));

    return str;
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

void xorg_crash_info_print_crash(struct xorg_crash_info *crash_info)
{
    char *reason = crash_info->reason;
    printf("%s%s%s\n", crash_info->backtrace, reason ? reason : "", reason ? "\n" : "");
}

int xorg_crash_info_save_in_dump_dir(struct xorg_crash_info *crash_info, struct dump_dir *dd)
{
    dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);
    dd_save_text(dd, FILENAME_ANALYZER, "abrt-xorg");
    dd_save_text(dd, FILENAME_TYPE, "xorg");
    dd_save_text(dd, FILENAME_REASON, crash_info->reason);
    dd_save_text(dd, FILENAME_BACKTRACE, crash_info->backtrace);
    /*
     * Reporters usually need component name to file a bug.
     * It is usually derived from executable.
     * We _guess_ X server's executable name as a last resort.
     * Better ideas?
     */
    if (!crash_info->exe)
    {
        if (access("/usr/bin/Xorg", X_OK) == 0)
            crash_info->exe = xstrdup("/usr/bin/Xorg");
        else
            crash_info->exe = xstrdup("/usr/bin/X");
    }
    dd_save_text(dd, FILENAME_EXECUTABLE, crash_info->exe);

    return 0;
}

static
int create_dump_dir_cb(struct dump_dir *dd, void *crash_info)
{
    return xorg_crash_info_save_in_dump_dir((struct xorg_crash_info *)crash_info, dd);
}

void xorg_crash_info_create_dump_dir(struct xorg_crash_info *crash_info, const char *dump_location, bool world_readable)
{
    struct dump_dir *dd = create_dump_dir(dump_location, "xorg", /*fs owner*/0,
                                          create_dump_dir_cb, crash_info);

    if (dd == NULL)
        return;

    if (world_readable)
        dd_set_no_owner(dd);

    char *path = xstrdup(dd->dd_dirname);
    dd_close(dd);
    notify_new_path(path);
    free(path);
}

char *xorg_get_next_line_from_fd(void *fd)
{
    FILE *f = (FILE *)fd;
    return xmalloc_fgetline(f);
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
struct xorg_crash_info *process_xorg_bt(char *(*get_next_line)(void *), void *data)
{
    char *reason = NULL;
    char *exe = NULL;
    GList *list = NULL;
    unsigned cnt = 0;
    char *line = NULL;
    while ((line = get_next_line(data)) != NULL)
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
            /* Here you can place other cases of useful reason string */
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
        struct xorg_crash_info *crash_info = xmalloc(sizeof(struct xorg_crash_info));

        list = g_list_reverse(list);
        crash_info->backtrace = list2lines(list); /* frees list */
        crash_info->reason = (reason ? reason : xstrdup(DEFAULT_XORG_CRASH_REASON));
        crash_info->exe = exe;

        return crash_info;
    }
    return NULL;

}
