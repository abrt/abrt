#include <gtk/gtk.h>
#include "abrtlib.h"

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
    GTK_ASSISTANT_PAGE_INTRO,
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

static void add_pages()
{
    GError *error = NULL;
    gtk_builder_add_objects_from_file(builder, "wizard.glade", (gchar**)page_names, &error);
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

        g_print("added page: %s\n", page_names[i]);
    }
}

GtkWidget *create_assistant()
{
    assistant = gtk_assistant_new();
    gtk_window_set_default_size(GTK_WINDOW(assistant), DEFAULT_WIDTH, DEFAULT_HEIGHT);
    g_signal_connect(G_OBJECT(assistant), "cancel", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(assistant), "close", G_CALLBACK(gtk_main_quit), NULL);
    builder = gtk_builder_new();
    add_pages();
    gtk_builder_connect_signals(builder, NULL);

    return assistant;
}
