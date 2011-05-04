/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat inc.

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
#include "abrtlib.h"
#include "parse_options.h"

static const char *dump_dir_name = NULL;
//static const char *conf_filename = CONF_DIR"/abrt_event.conf";
static const char *event;
static const char *pfx = "";

static char *do_log(char *log_line, void *param)
{
    log("%s", log_line);
    return log_line;
}

int main(int argc, char **argv)
{
    abrt_init(argv);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "\b [-vs]" /*" [-c CONFFILE]"*/ " -d DIR -e EVENT\n"
        "   or: \b [-vs]" /*" [-c CONFFILE]"*/ " [-d DIR] -l[PFX]\n"
        "\n"
        "Handles dump directory DIR according to rules in abrt_event.conf"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_s = 1 << 1,
        OPT_d = 1 << 2,
        OPT_e = 1 << 3,
        OPT_l = 1 << 4,
        OPT_p = 1 << 5,
//      OPT_c = 1 << ?,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(     's', NULL, NULL          ,             _("Log to syslog"       )),
        OPT_STRING(   'd', NULL, &dump_dir_name, "DIR"     , _("Dump directory")),
        OPT_STRING(   'e', NULL, &event        , "EVENT"   , _("Handle EVENT"        )),
        OPT_OPTSTRING('l', NULL, &pfx          , "PFX"     , _("List possible events [which start with PFX]")),
        OPT_BOOL(     'p', NULL, NULL          ,             _("Add program names to log")),
//      OPT_STRING(   'c', NULL, &conf_filename, "CONFFILE", _("Configuration file"  )),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);
    if (!(opts & (OPT_e|OPT_l)))
        show_usage_and_die(program_usage_string, program_options);

    export_abrt_envvars(opts & OPT_p);

    if (opts & OPT_s)
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    if (opts & OPT_l)
    {
        /* Note that dump_dir_name may be NULL here, it means "show all
         * possible events regardless of dir"
         */
        char *events = list_possible_events(NULL, dump_dir_name, pfx);
        if (!events)
            return 1; /* error msg is already logged */
        fputs(events, stdout);
        free(events);
        return 0;
    }

    /* -e EVENT: run event */

    struct run_event_state *run_state = new_run_event_state();
    run_state->logging_callback = do_log;
    int r = run_event_on_dir_name(run_state, dump_dir_name ? dump_dir_name : ".", event);
    if (r == 0 && run_state->children_count == 0)
        error_msg_and_die("No actions are found for event '%s'", event);
    free_run_event_state(run_state);

    return r;
}
