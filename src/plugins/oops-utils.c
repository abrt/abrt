/*
 * Copyright (C) 2014  ABRT team
 * Copyright (C) 2014  RedHat Inc
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
#include <satyr/stacktrace.h>
#include <satyr/koops/stacktrace.h>
#include <satyr/thread.h>
#include <satyr/koops/frame.h>
#include <satyr/frame.h>
#include <satyr/normalize.h>

#include "oops-utils.h"
#include "libabrt.h"

int abrt_oops_process_list(GList *oops_list, const char *dump_location, const char *analyzer, int flags)
{
    unsigned errors = 0;

    int oops_cnt = g_list_length(oops_list);
    if (oops_cnt != 0)
    {
        log_warning("Found oopses: %d", oops_cnt);
        if ((flags & ABRT_OOPS_PRINT_STDOUT))
        {
            int i = 0;
            while (i < oops_cnt)
            {
                char *kernel_bt = (char*)g_list_nth_data(oops_list, i++);
                char *tainted_short = kernel_tainted_short(kernel_bt);
                if (tainted_short)
                    log_warning("Kernel is tainted '%s'", tainted_short);

                free(tainted_short);
                printf("\nVersion: %s", kernel_bt);
            }
        }
        if (dump_location != NULL)
        {
            log_warning("Creating problem directories");
            errors = abrt_oops_create_dump_dirs(oops_list, dump_location, analyzer, flags);
            if (errors)
                log_warning("%d errors while dumping oopses", errors);
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

    /* If we are run by a log watcher, this delays log rescan
     * (because log watcher waits to us to terminate)
     * and possibly prevents dreaded "abrt storm".
     */
    int unreported_cnt = oops_cnt - ABRT_OOPS_MAX_DUMPED_COUNT;
    if (g_abrt_oops_sleep_woke_up_on_signal <= 0 &&
            (unreported_cnt > 0 && (flags & ABRT_OOPS_THROTTLE_CREATION)))
    {
        /* Quadratic throttle time growth, but careful to not overflow in "n*n" */
        int n = unreported_cnt > 30 ? 30 : unreported_cnt;
        n = n * n;
        if (n > 9)
            log_warning(_("Sleeping for %d seconds"), n);
        abrt_oops_signaled_sleep(n); /* max 15 mins */
    }

    return errors;
}

/* returns number of errors */
unsigned abrt_oops_create_dump_dirs(GList *oops_list, const char *dump_location, const char *analyzer, int flags)
{
    const int oops_cnt = g_list_length(oops_list);
    unsigned countdown = ABRT_OOPS_MAX_DUMPED_COUNT; /* do not report hundreds of oopses */

    log_notice("Saving %u oopses as problem dirs", oops_cnt >= countdown ? countdown : oops_cnt);

    char *cmdline_str = xmalloc_fopen_fgetline_fclose("/proc/cmdline");
    char *fips_enabled = xmalloc_fopen_fgetline_fclose("/proc/sys/crypto/fips_enabled");
    char *proc_modules = xmalloc_open_read_close("/proc/modules", /*maxsize:*/ NULL);
    char *suspend_stats = xmalloc_open_read_close("/sys/kernel/debug/suspend_stats", /*maxsize:*/ NULL);

    time_t t = time(NULL);
    const char *iso_date = iso_date_string(&t);

    pid_t my_pid = getpid();
    unsigned idx = 0;
    unsigned errors = 0;
    while (idx < oops_cnt)
    {
        char base[sizeof("oops-YYYY-MM-DD-hh:mm:ss-%lu-%lu") + 2 * sizeof(long)*3];
        sprintf(base, "oops-%s-%lu-%lu", iso_date, (long)my_pid, (long)idx);
        char *path = concat_path_file(dump_location, base);

        struct dump_dir *dd = dd_create(path, /*fs owner*/0, DEFAULT_DUMP_DIR_MODE);
        if (dd)
        {
            dd_create_basic_files(dd, /*no uid*/(uid_t)-1L, NULL);
            abrt_oops_save_data_in_dump_dir(dd, (char*)g_list_nth_data(oops_list, idx++), proc_modules);
            dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);
            dd_save_text(dd, FILENAME_ANALYZER, "abrt-oops");
            dd_save_text(dd, FILENAME_TYPE, "Kerneloops");
            if (cmdline_str)
                dd_save_text(dd, FILENAME_CMDLINE, cmdline_str);
            if (proc_modules)
                dd_save_text(dd, "proc_modules", proc_modules);
            if (fips_enabled && strcmp(fips_enabled, "0") != 0)
                dd_save_text(dd, "fips_enabled", fips_enabled);
            if (suspend_stats)
                dd_save_text(dd, "suspend_stats", suspend_stats);
            if ((flags & ABRT_OOPS_WORLD_READABLE))
                dd_set_no_owner(dd);
            dd_close(dd);
            notify_new_path(path);
        }
        else
            errors++;

        free(path);

        if (--countdown == 0)
            break;

        if (dd && (flags & ABRT_OOPS_THROTTLE_CREATION))
            if (abrt_oops_signaled_sleep(1) > 0)
                break;
    }

    free(cmdline_str);
    free(proc_modules);
    free(fips_enabled);
    free(suspend_stats);

    return errors;
}

static char *abrt_oops_list_of_tainted_modules(const char *proc_modules)
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

void abrt_oops_save_data_in_dump_dir(struct dump_dir *dd, char *oops, const char *proc_modules)
{
    char *first_line = oops;
    char *second_line = (char*)strchr(first_line, '\n'); /* never NULL */
    *second_line++ = '\0';

    if (first_line[0])
        dd_save_text(dd, FILENAME_KERNEL, first_line);
    dd_save_text(dd, FILENAME_BACKTRACE, second_line);

    /* save crash_function into dumpdir */
    char *error_message = NULL;
    struct sr_stacktrace *stacktrace = sr_stacktrace_parse(SR_REPORT_KERNELOOPS,
                                                           (const char *)second_line, &error_message);

    if (stacktrace)
    {
        sr_normalize_koops_stacktrace((struct sr_koops_stacktrace *)stacktrace);
        /* stacktrace is the same as thread, there is no need to check return value */
        struct sr_thread *thread = sr_stacktrace_find_crash_thread(stacktrace);
        struct sr_koops_frame *frame = (struct sr_koops_frame *)sr_thread_frames(thread);
        if (frame && frame->function_name)
            dd_save_text(dd, FILENAME_CRASH_FUNCTION, frame->function_name);

        sr_stacktrace_free(stacktrace);
    }
    else
    {
        error_msg("Can't parse stacktrace: %s", error_message);
        free(error_message);
    }

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

            struct strbuf *reason = strbuf_new();
            const char *fmt = _("A kernel problem occurred, but your kernel has been "
                    "tainted (flags:%s). Explanation:\n%s"
                    "Kernel maintainers are unable to diagnose tainted reports.");
            strbuf_append_strf(reason, fmt, tainted_short, tnt_long);

            char *modlist = !proc_modules ? NULL : abrt_oops_list_of_tainted_modules(proc_modules);
            if (modlist)
            {
                strbuf_append_strf(reason, _(" Tainted modules: %s."), modlist);
                free(modlist);
            }

            dd_save_text(dd, FILENAME_NOT_REPORTABLE, reason->buf);
            strbuf_free(reason);
            free(tainted_short);
            free(tnt_long);
        }
    }

    // TODO: add "Kernel oops: " prefix, so that all oopses have recognizable FILENAME_REASON?
    // kernel oops 1st line may look quite puzzling otherwise...
    char *reason_pretty = NULL;
    char *error = NULL;
    struct sr_stacktrace *trace = sr_stacktrace_parse(SR_REPORT_KERNELOOPS, second_line, &error);
    if (trace)
    {
        reason_pretty = sr_stacktrace_get_reason(trace);
        sr_stacktrace_free(trace);
    }
    else
        free(error);

    if (reason_pretty)
    {
        dd_save_text(dd, FILENAME_REASON, reason_pretty);
        free(reason_pretty);
    }
    else
        dd_save_text(dd, FILENAME_REASON, second_line);
}

int abrt_oops_signaled_sleep(int seconds)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGHUP);

    struct timespec timeout;
    timeout.tv_sec = seconds;
    timeout.tv_nsec = 0;

    return g_abrt_oops_sleep_woke_up_on_signal = sigtimedwait(&set, NULL, &timeout);
}

char *abrt_oops_string_filter_regex(void)
{
    map_string_t *settings = new_map_string();

    load_abrt_plugin_conf_file("oops.conf", settings);

    int only_fatal_mce = 1;
    try_get_map_string_item_as_bool(settings, "OnlyFatalMCE", &only_fatal_mce);

    free_map_string(settings);

    if (only_fatal_mce)
        return xstrdup("^Machine .*$");

    return NULL;
}
