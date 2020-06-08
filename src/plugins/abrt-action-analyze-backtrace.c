/*
    Copyright (C) 2010, 2011  Red Hat, Inc.

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
#include <satyr/location.h>
#include <satyr/thread.h>
#include <satyr/gdb/frame.h>
#include <satyr/gdb/stacktrace.h>

#include "libabrt.h"

static const char *dump_dir_name = ".";


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
        "& [options] -d DIR\n"
        "\n"
        "Analyzes C/C++ backtrace, generates duplication hash, backtrace rating,\n"
        "and identifies crash function in problem directory DIR"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&libreport_g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Problem directory")),
        OPT_END()
    };
    /*unsigned opts =*/ libreport_parse_opts(argc, argv, program_options, program_usage_string);

    libreport_export_abrt_envvars(0);

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    g_autofree char *component = dd_load_text(dd, FILENAME_COMPONENT);

    /* Read backtrace */
    g_autofree char *backtrace_str = dd_load_text_ext(dd, FILENAME_BACKTRACE,
                                           DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    if (!backtrace_str)
    {
        dd_close(dd);
        return 1;
    }

    /* Compute backtrace hash */
    struct sr_location location;
    sr_location_init(&location);
    const char *backtrace_str_ptr = backtrace_str;
    struct sr_gdb_stacktrace *backtrace = sr_gdb_stacktrace_parse(&backtrace_str_ptr, &location);

    /* Store backtrace hash */
    if (!backtrace)
    {
        g_autofree char *checksum = NULL;

        /*
         * The parser failed. Compute the duphash from the executable
         * instead of a backtrace.
         * and component only.  This is not supposed to happen often.
         */
        log_warning(_("Backtrace parsing failed for %s"), dump_dir_name);
        log_warning("%d:%d: %s", location.line, location.column, location.message);
        g_autoptr(GString) emptybt = g_string_new(NULL);

        g_autofree char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
        g_string_prepend(emptybt, executable);

        g_string_prepend(emptybt, component);

        log_debug("Generating duphash: %s", emptybt->str);

        checksum = g_compute_checksum_for_string(G_CHECKSUM_SHA1, emptybt->str, -1);

        dd_save_text(dd, FILENAME_DUPHASH, checksum);
        /*
         * Other parts of ABRT assume that if no rating is available,
         * it is ok to allow reporting of the bug. To be sure no bad
         * backtrace is reported, rate the backtrace with the lowest
         * rating.
         */
        dd_save_text(dd, FILENAME_RATING, "0");

        dd_close(dd);

        /* Report success even if the parser failed, as the backtrace
         * has been created and rated. The failure is caused by a flaw
         * in the parser, not in the backtrace.
         */
        return 0;
    }

    uint32_t tid = -1;
    if (dd_load_uint32(dd, FILENAME_TID, &tid) == 0)
        sr_gdb_stacktrace_set_crash_tid(backtrace, tid);

    /* Compute duplication hash. */
    struct sr_thread *crash_thread =
        (struct sr_thread *)sr_gdb_stacktrace_find_crash_thread(backtrace);

    if (crash_thread)
    {
        g_autofree char *hash_str = NULL;

        if (libreport_g_verbose >= 3)
        {
            hash_str = sr_thread_get_duphash(crash_thread, 3, component,
                                             SR_DUPHASH_NOHASH);
            log_warning("Generating duphash: %s", hash_str);
        }

        hash_str = sr_thread_get_duphash(crash_thread, 3, component,
                                         SR_DUPHASH_NORMAL);
        dd_save_text(dd, FILENAME_DUPHASH, hash_str);
    }
    else
        log_warning(_("Crash thread not found"));


    /* Compute the backtrace rating. */
    float quality = sr_gdb_stacktrace_quality_complex(backtrace);
    const char *rating;
    if (quality < 0.6f)
        rating = "0";
    else if (quality < 0.7f)
        rating = "1";
    else if (quality < 0.8f)
        rating = "2";
    else if (quality < 0.9f)
        rating = "3";
    else
        rating = "4";
    dd_save_text(dd, FILENAME_RATING, rating);

    /* Get the function name from the crash frame. */
    struct sr_gdb_frame *crash_frame = sr_gdb_stacktrace_get_crash_frame(backtrace);
    if (crash_frame)
    {
        if (crash_frame->function_name &&
            0 != strcmp(crash_frame->function_name, "??"))
        {
            dd_save_text(dd, FILENAME_CRASH_FUNCTION, crash_frame->function_name);
        }
        sr_gdb_frame_free(crash_frame);
    }
    sr_gdb_stacktrace_free(backtrace);
    dd_close(dd);
    return 0;
}
