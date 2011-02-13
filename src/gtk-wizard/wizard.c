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

gchar *pages[] =
        { "page_1",
          "page_2",
          "page_3",
          "page_4",
          "page_5",
          "page_6",
          "page_7",
          NULL
        };

GtkBuilder *builder;
GtkWidget *assistant;

void on_b_refresh_clicked(GtkButton *button)
{
    g_print("Refresh clicked!\n");
}

void add_pages()
{

    GError *error = NULL;
    gtk_builder_add_objects_from_file(builder, "wizard.glade", pages, &error);
    if(error != NULL)
    {
        g_print(error->message);
        g_free(error);
    }

    GtkWidget *page;
    gchar **page_names;
    for(page_names = pages; *page_names != NULL; page_names++)
    {
        page = GTK_WIDGET(gtk_builder_get_object(builder, *page_names));
        if(page == NULL)
            continue;

        gtk_assistant_append_page(GTK_ASSISTANT(assistant), page);
        //FIXME: shouldn't be complete until something is selected!
        gtk_assistant_set_page_complete(GTK_ASSISTANT(assistant), page, true);
        if(strcmp(PAGE_ANALYZE_ACTION_SELECTOR, *page_names) == 0)
        {
            gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), page, "Select analyzer");
            gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), page, GTK_ASSISTANT_PAGE_INTRO);

        }
        if(strcmp(PAGE_ANALYZE_PROGRESS, *page_names) == 0)
        {
            gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), page, "Analyzing problem");
            gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), page, GTK_ASSISTANT_PAGE_PROGRESS);
        }
        if(strcmp(PAGE_REPORTER_SELECTOR, *page_names) == 0)
        {
            gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), page, "Select reporter");
            gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), page, GTK_ASSISTANT_PAGE_CONTENT);
        }
        if(strcmp(PAGE_BACKTRACE_APPROVAL, *page_names) == 0)
        {
            gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), page, "Approve the backtrace");
            gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), page, GTK_ASSISTANT_PAGE_CONTENT);
        }

        if(strcmp(PAGE_HOWTO, *page_names) == 0)
        {
            gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), page, "Provide additional information");
            gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), page, GTK_ASSISTANT_PAGE_CONTENT);
        }

        if(strcmp(PAGE_SUMMARY, *page_names) == 0)
        {
            gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), page, "Confirm and send the report");
            gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), page, GTK_ASSISTANT_PAGE_CONFIRM);
        }

        if(strcmp(PAGE_REPORT, *page_names) == 0)
        {
            gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), page, "Approve the backtrace");
            gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), page, GTK_ASSISTANT_PAGE_SUMMARY);
        }
        g_print("added page: %s\n", *page_names);
    }
}

GtkWidget *create_assistant()
{
    assistant = gtk_assistant_new();
    gtk_window_set_default_size(GTK_WINDOW(assistant), DEFAULT_WIDTH,DEFAULT_HEIGHT);
    g_signal_connect(G_OBJECT(assistant), "cancel", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(assistant), "close", G_CALLBACK(gtk_main_quit), NULL);
    builder = gtk_builder_new();
    add_pages();
    gtk_builder_connect_signals(builder, NULL);


    return assistant;
}