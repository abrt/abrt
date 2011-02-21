#include <gtk/gtk.h>
#include "abrtlib.h"
#include "parse_options.h"
#include "wizard.h"

#define PROGNAME "bug-reporting-wizard"

char *g_glade_file = NULL;

char *g_dump_dir_name = NULL;

char *g_analyze_label_selected = NULL;

crash_data_t *cd;

static void analyze_rb_was_toggled(GtkToggleButton *button, gpointer user_data)
{
    const char *label = gtk_button_get_label(GTK_BUTTON(button));
    if (label)
    {
        free(g_analyze_label_selected);
        g_analyze_label_selected = xstrdup(label);
    }
}

static GtkWidget *add_event_buttons(GtkBox *box, char *event_name, GCallback func, bool radio)
{
    GtkWidget *first_button = NULL;
    while (event_name[0])
    {
        char *event_name_end = strchr(event_name, '\n');
        *event_name_end = '\0';

        GtkWidget *button = radio
                ? gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(first_button), event_name)
                : gtk_check_button_new_with_label(event_name);
        if (!first_button)
            first_button = button;

        *event_name_end = '\n';
        event_name = event_name_end + 1;

        gtk_box_pack_start(box, button, /*expand*/ false, /*fill*/ false, /*padding*/ 0);

        if (func)
            g_signal_connect(G_OBJECT(button), "toggled", func, NULL);
    }
    return first_button;
}

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

    g_dump_dir_name = argv[0];

    struct dump_dir *dd = dd_opendir(g_dump_dir_name, 0);
    if (!dd)
        return 1;
    cd = create_crash_data_from_dump_dir(dd);
    char *analyze_events = list_possible_events(dd, g_dump_dir_name, "analyze");
    char *report_events = list_possible_events(dd, g_dump_dir_name, "report");
    dd_close(dd);

    GtkWidget *assistant = create_assistant();

    const char *reason = get_crash_item_content_or_NULL(cd, FILENAME_REASON);
    if (reason)
        gtk_label_set_text(g_lbl_cd_reason, reason);

    if (analyze_events[0])
    {
        GtkWidget *first_rb = add_event_buttons(g_box_analyzers, analyze_events, G_CALLBACK(analyze_rb_was_toggled), /*radio:*/ true);
        if (first_rb)
        {
            const char *label = gtk_button_get_label(GTK_BUTTON(first_rb));
            if (label)
            {
                free(g_analyze_label_selected);
                g_analyze_label_selected = xstrdup(label);
            }
        }
    }
    else
    {
        /* No available analyze events, go to reporter selector page */
//Doesn't work: shows empty page
//        gtk_assistant_set_current_page(GTK_ASSISTANT(assistant), PAGENO_REPORTER_SELECTOR);
    }

    if (report_events[0])
    {
        add_event_buttons(g_box_reporters, report_events, /*callback:*/NULL, /*radio:*/ false);
    }

    gtk_widget_show_all(assistant);

    /* Enter main loop */
    gtk_main();

    return 0;
}
