#include <gtk/gtk.h>
#include "abrtlib.h"
#include "parse_options.h"
#include "wizard.h"

#define PROGNAME "bug-reporting-wizard"

static crash_data_t *cd;

int main(int argc, char **argv)
{
    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] [DIR]\n\n"
        "GUI tool to run analyze and report ABRT crash in specified DIR\n"
    );
    enum {
        OPT_v = 1 << 0,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_END()
    };

    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    argv += optind;
    if (!argv[0] || argv[1]) /* zero or >1 arguments */
        show_usage_and_die(program_usage_string, program_options);

    struct dump_dir *dd = dd_opendir(argv[0], 0);
    if (!dd)
        return 1;
    cd = create_crash_data_from_dump_dir(dd);
    dd_close(dd);

    gtk_init(&argc, &argv);

    GtkWidget *assistant = create_assistant();
    gtk_widget_show_all(assistant);

    /* Enter main loop */
    gtk_main();

    return 0;
}
