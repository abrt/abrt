#include <gtk/gtk.h>
#include "abrtlib.h"
#include "wizard.h"

/* THE PAGE FLOW
 * page_1: analyze action selection
 * page_2: analyze progress
 * page_3: reporter selection
 * page_4: backtrace editor
 * page_5: how to + user comments
 * page_6: summary
 * page_7: reporting progress
 */

#define PAGE_ANALYZE_ACTION_SELECTOR "page_1"
#define PAGE_ANALYZE_PROGRESS        "page_2"
#define PAGE_REPORTER_SELECTOR       "page_3"
#define PAGE_BACKTRACE_APPROVAL      "page_4"
#define PAGE_HOWTO                   "page_5"
#define PAGE_SUMMARY                 "page_6"
#define PAGE_REPORT                  "page_7"

#define DEFAULT_WIDTH   800
#define DEFAULT_HEIGHT  500

GtkLabel *g_lbl_cd_reason;
GtkVBox *g_vb_analyzers;
GtkTextView *g_analyze_log;

static const gchar *const page_names[] =
{
    "page_1",
    "page_2",
    "page_3",
    "page_4",
    "page_5",
    "page_6",
    "page_7",
    NULL
};

static const gchar *const page_titles[] =
{
    "Select analyzer",
    "Analyzing problem",
    "Select reporter",
    "Approve the backtrace",
    "Provide additional information",
    "Confirm and send the report",
    "Approve the backtrace",
};

static const GtkAssistantPageType page_types[] =
{
    GTK_ASSISTANT_PAGE_CONFIRM, /* need this type to get "apply" signal */
    GTK_ASSISTANT_PAGE_PROGRESS,
    GTK_ASSISTANT_PAGE_CONTENT,
    GTK_ASSISTANT_PAGE_CONTENT,
    GTK_ASSISTANT_PAGE_CONTENT,
    GTK_ASSISTANT_PAGE_CONFIRM,
    GTK_ASSISTANT_PAGE_SUMMARY,
};

static GtkBuilder *builder;
static GtkWidget *assistant;

void on_b_refresh_clicked(GtkButton *button)
{
    g_print("Refresh clicked!\n");
}

/* wizard.glade file as a string WIZARD_GLADE_CONTENTS: */
#include "wizard_glade.c"

static void add_pages()
{
    GError *error = NULL;
    if (!g_glade_file)
        /* Load UI from internal string */
        gtk_builder_add_objects_from_string(builder,
                WIZARD_GLADE_CONTENTS, sizeof(WIZARD_GLADE_CONTENTS) - 1,
                (gchar**)page_names,
                &error);
    else
        /* -g FILE: load IU from it */
        gtk_builder_add_objects_from_file(builder, g_glade_file, (gchar**)page_names, &error);
    if (error != NULL)
    {
        error_msg_and_die("can't load %s: %s", "wizard.glade", error->message);
    }

    for (int i = 0; page_names[i] != NULL; i++)
    {
        GtkWidget *page = GTK_WIDGET(gtk_builder_get_object(builder, page_names[i]));
        if (page == NULL)
            continue;

        gtk_assistant_append_page(GTK_ASSISTANT(assistant), page);
        //FIXME: shouldn't be complete until something is selected!
        gtk_assistant_set_page_complete(GTK_ASSISTANT(assistant), page, true);

        gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), page, page_titles[i]);
        gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), page, page_types[i]);

        log("added page: %s", page_names[i]);
    }

    /* Set pointer to fields we might need to change */
    g_lbl_cd_reason = GTK_LABEL(gtk_builder_get_object(builder, "lbl_cd_reason"));
    g_vb_analyzers = GTK_VBOX(gtk_builder_get_object(builder, "vb_analyzers"));
    g_analyze_log = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "analyze_log"));
}

static char *add_log_to_analyze_log(char *log_line, void *param)
{
    GtkTextBuffer *tb = gtk_text_view_get_buffer(g_analyze_log);

    gtk_text_buffer_insert_at_cursor(tb, log_line, -1);
    gtk_text_buffer_insert_at_cursor(tb, "\n", 1);

    return log_line;
}

static void next_page(GtkAssistant *assistant, gpointer user_data)
{
    int page_no = gtk_assistant_get_current_page(assistant);
    log("page_no:%d", page_no);

    if (g_analyze_label_selected != NULL)
    {
        struct run_event_state *run_state = new_run_event_state();
        run_state->logging_callback = add_log_to_analyze_log;
// Need async version of run_event_on_dir_name() here! This one will freeze GUI until completion:
        log("running event '%s' on '%s'", g_analyze_label_selected, g_dump_dir_name);
        int res = run_event_on_dir_name(run_state, g_dump_dir_name, g_analyze_label_selected);
        free_run_event_state(run_state);
        log("done running event '%s' on '%s': %d", g_analyze_label_selected, g_dump_dir_name, res);
    }
}

GtkWidget *create_assistant()
{
    assistant = gtk_assistant_new();
    gtk_window_set_default_size(GTK_WINDOW(assistant), DEFAULT_WIDTH, DEFAULT_HEIGHT);
    gtk_window_set_title(GTK_WINDOW(assistant), g_dump_dir_name);
    gtk_window_set_icon_name(GTK_WINDOW(assistant), "abrt");

    g_signal_connect(G_OBJECT(assistant), "cancel", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(assistant), "close", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(assistant), "apply", G_CALLBACK(next_page), NULL);

    builder = gtk_builder_new();
    add_pages();
    gtk_builder_connect_signals(builder, NULL);

    return assistant;
}
