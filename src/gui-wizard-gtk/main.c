#include <gtk/gtk.h>
#include "abrtlib.h"
#include "parse_options.h"
#include "wizard.h"

#define PROGNAME "bug-reporting-wizard"

char *g_glade_file = NULL;
char *g_dump_dir_name = NULL;
char *g_analyze_events = NULL;
char *g_reanalyze_events = NULL;
char *g_report_events = NULL;
crash_data_t *g_cd;


void reload_crash_data_from_dump_dir(void)
{
    free_crash_data(g_cd);
    free(g_analyze_events);
    free(g_reanalyze_events);
    free(g_report_events);

    struct dump_dir *dd = dd_opendir(g_dump_dir_name, DD_OPEN_READONLY);
    if (!dd)
        xfunc_die(); /* dd_opendir already logged error msg */

    g_cd = create_crash_data_from_dump_dir(dd);
    add_to_crash_data_ext(g_cd, CD_DUMPDIR, g_dump_dir_name, CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

    g_analyze_events = list_possible_events(dd, NULL, "analyze");
    g_reanalyze_events = list_possible_events(dd, NULL, "reanalyze");
    g_report_events = list_possible_events(dd, NULL, "report");
    dd_close(dd);
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] [-g GUI_FILE] DIR\n\n"
        "GUI tool to analyze and report ABRT crash in specified DIR"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_g = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('g', NULL, &g_glade_file, "FILE" , _("Alternate GUI file")),
        OPT_END()
    };

    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    argv += optind;
    if (!argv[0] || argv[1]) /* zero or >1 arguments */
        show_usage_and_die(program_usage_string, program_options);

    g_dump_dir_name = xstrdup(argv[0]);

    create_assistant();

    g_custom_logger = &show_error_as_msgbox;

    reload_crash_data_from_dump_dir();

    update_gui_state_from_crash_data();

    /* Enter main loop */
    gtk_main();

    return 0;
}
