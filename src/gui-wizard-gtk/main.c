#include <gtk/gtk.h>
#include "abrtlib.h"
#include "parse_options.h"
#include "wizard.h"

#define PROGNAME "bug-reporting-wizard"

char *g_glade_file = NULL;
char *dump_dir_path = NULL;

crash_data_t *cd;

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

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

    argv += optind;
    if (!argv[0] || argv[1]) /* zero or >1 arguments */
        show_usage_and_die(program_usage_string, program_options);

    dump_dir_path = argv[0];
    struct dump_dir *dd = dd_opendir(dump_dir_path, 0);
    if (!dd)
        return 1;
    cd = create_crash_data_from_dump_dir(dd);
    char *analyze_events = list_possible_events(dd, /*dump_dir_name*/ NULL, "analyze");
    dd_close(dd);

    GtkWidget *assistant = create_assistant();

    const char *reason = get_crash_item_content_or_NULL(cd, FILENAME_REASON);
    if (reason)
        gtk_label_set_text(g_lbl_cd_reason, reason);

    GtkWidget *first_rb = NULL;
    if (analyze_events[0])
    {
        char *event_name = analyze_events;
        char *event_name_end = strchr(event_name, '\n');
        *event_name_end = '\0';
        GtkWidget *rb = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(first_rb), event_name);
        *event_name_end = '\n';
        event_name = event_name_end + 1;

        if (!first_rb)
            first_rb = rb;
        gtk_box_pack_start(GTK_BOX(g_vb_analyzers), rb, /*expand*/ false, /*fill*/ false, /*padding*/ 0);
    }
    else { /*???*/ }

    gtk_widget_show_all(assistant);

    /* Enter main loop */
    gtk_main();

    return 0;
}
