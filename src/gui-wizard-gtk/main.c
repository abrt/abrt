#include <gtk/gtk.h>
#include "abrtlib.h"
#include "parse_options.h"
#include "wizard.h"

#define PROGNAME "bug-reporting-wizard"

char *g_glade_file = NULL;
char *g_dump_dir_name = NULL;
char *g_analyze_label_selected = NULL;
char *g_analyze_events = NULL;
char *g_report_events = NULL;
crash_data_t *g_cd;


static void analyze_rb_was_toggled(GtkToggleButton *button, gpointer user_data)
{
    const char *label = gtk_button_get_label(GTK_BUTTON(button));
    if (label)
    {
        free(g_analyze_label_selected);
        g_analyze_label_selected = xstrdup(label);
    }
}

static void remove_child_widget(GtkWidget *widget, gpointer container)
{
    gtk_container_remove(container, widget);
}

static GtkWidget *add_event_buttons(GtkBox *box, char *event_name, GCallback func, bool radio)
{
    gtk_container_foreach(GTK_CONTAINER(box), &remove_child_widget, box);

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

static void append_item_to_details_ls(gpointer name, gpointer value, gpointer data)
{
    crash_item *item = (crash_item*)value;
    GtkTreeIter iter;

    gtk_list_store_append(g_details_ls, &iter);

    //FIXME: use the value representation here
    /* If text and not multiline... */
    if ((item->flags & CD_FLAG_TXT) && !strchr(item->content, '\n'))
    {
        gtk_list_store_set(g_details_ls, &iter,
                              DETAIL_COLUMN_NAME, (char *)name,
                              DETAIL_COLUMN_VALUE, item->content,
                              //DETAIL_COLUMN_PATH, xasprintf("%s%s", g_dump_dir_name, name),
                              -1);
    }
    else
    {
        gtk_list_store_set(g_details_ls, &iter,
                              DETAIL_COLUMN_NAME, (char *)name,
                              DETAIL_COLUMN_VALUE, _("Content is too long, please use the \"View\" button to display it."),
                              //DETAIL_COLUMN_PATH, xasprintf("%s%s", g_dump_dir_name, name),
                              -1);
        //WARNING: will leak xasprintf results above if uncommented
    }
}

void reload_dump_dir(void)
{
    free_crash_data(g_cd);
    free(g_analyze_events);
    free(g_report_events);

    struct dump_dir *dd = dd_opendir(g_dump_dir_name, 0);
    if (!dd)
        xfunc_die(); /* dd_opendir already logged error msg */
    g_cd = create_crash_data_from_dump_dir(dd);
    g_analyze_events = list_possible_events(dd, g_dump_dir_name, "analyze");
    g_report_events = list_possible_events(dd, g_dump_dir_name, "report");
    dd_close(dd);

    const char *reason = get_crash_item_content_or_NULL(g_cd, FILENAME_REASON);
    gtk_label_set_text(g_lbl_cd_reason, reason ? reason : _("(no description)"));

    gtk_list_store_clear(g_details_ls);
    g_hash_table_foreach(g_cd, append_item_to_details_ls, NULL);

    GtkTextBuffer *backtrace_buf = gtk_text_view_get_buffer(g_backtrace_tv);
    const char *bt = g_cd ? get_crash_item_content_or_NULL(g_cd, FILENAME_BACKTRACE) : NULL;
    gtk_text_buffer_set_text(backtrace_buf, bt ? bt : "", -1);

//Doesn't work: shows empty page
//    if (!g_analyze_events[0])
//    {
//        /* No available analyze events, go to reporter selector page */
//        gtk_assistant_set_current_page(GTK_ASSISTANT(assistant), PAGENO_REPORTER_SELECTOR);
//    }

    GtkWidget *first_rb = add_event_buttons(g_box_analyzers, g_analyze_events, G_CALLBACK(analyze_rb_was_toggled), /*radio:*/ true);
    if (first_rb)
    {
        const char *label = gtk_button_get_label(GTK_BUTTON(first_rb));
        if (label)
        {
            free(g_analyze_label_selected);
            g_analyze_label_selected = xstrdup(label);
        }
    }

    add_event_buttons(g_box_reporters, g_report_events, /*callback:*/ NULL, /*radio:*/ false);
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

    g_dump_dir_name = argv[0];

    GtkWidget *assistant = create_assistant();

    reload_dump_dir();

    gtk_widget_show_all(assistant);

    /* Enter main loop */
    gtk_main();

    return 0;
}
