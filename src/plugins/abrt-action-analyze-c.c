/*
    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

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

#include <glib.h>

#include <satyr/thread.h>
#include <satyr/core/stacktrace.h>
#include <satyr/core/thread.h>
#include <satyr/core/frame.h>
#include <satyr/normalize.h>

static void trim_unstrip_output(char *result, const char *unstrip_n_output)
{
    // lines look like this:
    // 0x400000+0x209000 23c77451cf6adff77fc1f5ee2a01d75de6511dda@0x40024c - - [exe]
    // 0x400000+0x209000 ab3c8286aac6c043fd1bb1cc2a0b88ec29517d3e@0x40024c /bin/sleep /usr/lib/debug/bin/sleep.debug [exe]
    // 0x7fff313ff000+0x1000 389c7475e3d5401c55953a425a2042ef62c4c7df@0x7fff313ff2f8 . - linux-vdso.so.1
    //                ^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // we drop everything except the marked part ^

    char *dst = result;
    const char *line = unstrip_n_output;
    while (*line)
    {
        const char *eol = strchrnul(line, '\n');
        const char *plus = (char*)memchr(line, '+', eol - line);
        if (plus)
        {
            while (++plus < eol && *plus != '@')
            {
                if (!isspace(*plus))
                {
                    *dst++ = *plus;
                }
            }
        }
        if (*eol != '\n') break;
        line = eol + 1;
    }
    *dst = '\0';
}

static struct sr_core_thread *
core_thread_from_core_stacktrace(struct sr_core_stacktrace *stacktrace)
{
    struct sr_core_thread *thread = sr_core_stacktrace_find_crash_thread(stacktrace);
    if (!thread)
    {
        log_info("Failed to find crash thread");
        return NULL;
    }

    return thread;
}

static struct sr_core_stacktrace *
core_stacktrace_from_core_json(char *core_backtrace)
{
    char *error = NULL;
    struct sr_core_stacktrace *stacktrace = sr_core_stacktrace_from_json_text(core_backtrace, &error);
    if (!stacktrace)
    {
        if (error)
        {
            log_info("Failed to parse core backtrace: %s", error);
            free(error);
        }
        return NULL;
    }

    return stacktrace;
}

static char *build_ids_from_core_backtrace(const char *dump_dir_name)
{
    char *core_backtrace_path = xasprintf("%s/"FILENAME_CORE_BACKTRACE, dump_dir_name);
    char *json = xmalloc_open_read_close(core_backtrace_path, /*maxsize:*/ NULL);
    free(core_backtrace_path);

    if (!json)
        return NULL;

    struct sr_core_stacktrace *stacktrace = core_stacktrace_from_core_json(json);
    free(json);

    if (!stacktrace)
        return NULL;

    struct sr_core_thread *thread = core_thread_from_core_stacktrace(stacktrace);

    if (!thread)
    {
        sr_core_stacktrace_free(stacktrace);
        return NULL;
    }

    void *build_id_list = NULL;

    struct strbuf *strbuf = strbuf_new();
    for (struct sr_core_frame *frame = thread->frames;
         frame;
         frame = frame->next)
    {
        if (frame->build_id)
            build_id_list = g_list_prepend(build_id_list, frame->build_id);
    }

    build_id_list = g_list_sort(build_id_list, (GCompareFunc)strcmp);
    for (GList *iter = build_id_list; iter; iter = g_list_next(iter))
    {
        GList *next = g_list_next(iter);
        if (next == NULL || 0 != strcmp(iter->data, next->data))
        {
            strbuf = strbuf_append_strf(strbuf, "%s\n", (char *)iter->data);
        }
    }
    g_list_free(build_id_list);
    sr_core_stacktrace_free(stacktrace);

    return strbuf_free_nobuf(strbuf);
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

    const char *dump_dir_name = ".";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] -d DIR\n"
        "\n"
        "Calculates and saves UUID of coredump in problem directory DIR"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Problem directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    char *unstrip_n_output = NULL;
    char *coredump_path = xasprintf("%s/"FILENAME_COREDUMP, dump_dir_name);
    if (access(coredump_path, R_OK) == 0)
        unstrip_n_output = run_unstrip_n(dump_dir_name, /*timeout_sec:*/ 30);

    free(coredump_path);

    if (unstrip_n_output)
    {
        /* Run unstrip -n and trim its output, leaving only sizes and build ids */
        /* modifies unstrip_n_output in-place: */
        trim_unstrip_output(unstrip_n_output, unstrip_n_output);
    }
    else
    {
        /* bad dump_dir_name, can't run unstrip, etc...
         * or maybe missing coredump - try generating it from core_backtrace
         */

        unstrip_n_output = build_ids_from_core_backtrace(dump_dir_name);
    }

    /* Hash package + executable + unstrip_n_output and save it as UUID */

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    /* FILENAME_PACKAGE may be missing if ProcessUnpackaged = yes... */
    char *package = dd_load_text_ext(dd, FILENAME_PACKAGE, DD_FAIL_QUIETLY_ENOENT);
    /* Package variable has "firefox-3.5.6-1.fc11[.1]" format */
    /* Remove distro suffix and maybe least significant version number */
    char *p = package;
    while (*p)
    {
        if (*p == '.' && (p[1] < '0' || p[1] > '9'))
        {
            /* We found "XXXX.nondigitXXXX", trim this part */
            *p = '\0';
            break;
        }
        p++;
    }
    char *first_dot = strchr(package, '.');
    if (first_dot)
    {
        char *last_dot = strrchr(first_dot, '.');
        if (last_dot != first_dot)
        {
            /* There are more than one dot: "1.2.3"
             * Strip last part, we don't want to distinguish crashes
             * in packages which differ only by minor release number.
             */
            *last_dot = '\0';
        }
    }

    char *string_to_hash = xasprintf("%s%s%s", package, executable, unstrip_n_output);
    /*free(package);*/
    /*free(executable);*/
    /*free(unstrip_n_output);*/

    log_debug("String to hash: %s", string_to_hash);

    char hash_str[SHA1_RESULT_LEN*2 + 1];
    str_to_sha1str(hash_str, string_to_hash);

    dd_save_text(dd, FILENAME_UUID, hash_str);

    /* Create crash_function element from core_backtrace */
    char *core_backtrace_json = dd_load_text_ext(dd, FILENAME_CORE_BACKTRACE,
                                                 DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    if (core_backtrace_json)
    {
        struct sr_core_stacktrace *stacktrace = core_stacktrace_from_core_json(core_backtrace_json);
        free(core_backtrace_json);

        if (!stacktrace)
            goto next;

        struct sr_core_thread *thread = core_thread_from_core_stacktrace(stacktrace);

        if (!thread)
            goto next;

        sr_normalize_core_thread(thread);

        struct sr_core_frame *frame = thread->frames;
        if (frame->function_name)
            dd_save_text(dd, FILENAME_CRASH_FUNCTION, frame->function_name);

next:
        /* can be NULL */
        sr_core_stacktrace_free(stacktrace);
    }

    dd_close(dd);

    return 0;
}
