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
//#include "crash_types.h"

#define PROGNAME "abrt-handle-crashdump"

static const char *dump_dir_name = ".";
//static const char *conf_filename = CONF_DIR"/abrt_action.conf";
static const char *event;

static char *do_log(char *log_line, void *param)
{
    log("%s", log_line);
    return log_line;
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    const char *program_usage = _(
        PROGNAME" [-vs]" /*" [-c CONFFILE]"*/ " -d DIR -e EVENT\n"
        "\n"
        "Handle crash dump according to rules in abrt_action.conf");
    enum {
        OPT_v = 1 << 0,
        OPT_s = 1 << 1,
        OPT_d = 1 << 2,
        OPT_e = 1 << 3,
//      OPT_c = 1 << 4,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL          ,             _("Log to syslog"       )),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR"     , _("Crash dump directory")),
        OPT_STRING('e', NULL, &event        , "EVENT"   , _("Event"               )),
//      OPT_STRING('c', NULL, &conf_filename, "CONFFILE", _("Configuration file"  )),
        OPT_END()
    };

    unsigned opts = parse_opts(argc, argv, program_options, program_usage);
    if (!(opts & OPT_e))
        parse_usage_and_die(program_usage, program_options);
    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));
    if (opts & OPT_s)
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    struct run_event_state *run_state = new_run_event_state();
    run_state->logging_callback = do_log;
    int r = run_event(run_state, dump_dir_name, event);
    free_run_event_state(run_state);

    return r;
}
