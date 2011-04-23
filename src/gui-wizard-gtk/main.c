/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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
#include <gtk/gtk.h>
#include "abrtlib.h"
#include "parse_options.h"
#include "wizard.h"
#include "libreport-gtk.h"
#if HAVE_LOCALE_H
# include <locale.h>
#endif

#define PROGNAME "bug-reporting-wizard"

char *g_glade_file = NULL;
char *g_dump_dir_name = NULL;
char *g_analyze_events = NULL;
char *g_reanalyze_events = NULL;
char *g_report_events = NULL;
problem_data_t *g_cd;


void reload_problem_data_from_dump_dir(void)
{
    free_problem_data(g_cd);
    free(g_analyze_events);
    free(g_reanalyze_events);
    free(g_report_events);

    struct dump_dir *dd = dd_opendir(g_dump_dir_name, DD_OPEN_READONLY);
    if (!dd)
        xfunc_die(); /* dd_opendir already logged error msg */

    g_cd = create_problem_data_from_dump_dir(dd);
    add_to_problem_data_ext(g_cd, CD_DUMPDIR, g_dump_dir_name, CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

    g_analyze_events = list_possible_events(dd, NULL, "analyze");
    g_reanalyze_events = list_possible_events(dd, NULL, "reanalyze");
    g_report_events = list_possible_events(dd, NULL, "report");
    dd_close(dd);

    /* Load /etc/abrt/events/foo.{conf,xml} stuff */
    load_event_config_data();
    load_event_config_data_from_keyring();
//TODO: Load ~/.abrt/events/foo.conf?
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    g_set_prgname("abrt");
    gtk_init(&argc, &argv);

    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-vp] [-g GUI_FILE] DIR\n"
        "\n"
        "GUI tool to analyze and report problem saved in specified DIR"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_g = 1 << 1,
        OPT_p = 1 << 2,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('g', NULL, &g_glade_file, "FILE" , _("Alternate GUI file")),
        OPT_BOOL(  'p', NULL, NULL                  , _("Add program names to log")),
        OPT_END()
    };

    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));
    if (opts & OPT_p)
    {
        msg_prefix = PROGNAME;
        putenv((char*)"ABRT_PROG_PREFIX=1");
    }

    argv += optind;
    if (!argv[0] || argv[1]) /* zero or >1 arguments */
        show_usage_and_die(program_usage_string, program_options);

    g_dump_dir_name = xstrdup(argv[0]);

    /* load /etc/abrt/events/foo.{conf,xml} stuff
       and ~/.abrt/events/foo.conf */
    load_event_config_data();
    load_event_config_data_from_keyring();

    create_assistant();

    g_custom_logger = &show_error_as_msgbox;

    reload_problem_data_from_dump_dir();

    update_gui_state_from_problem_data();

    /* Enter main loop */
    gtk_main();

    return 0;
}
