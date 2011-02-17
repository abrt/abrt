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

enum
{
    COLUMN_NAME,
    COLUMN_VALUE,
    COLUMN_PATH,
    COLUMN_COUNT
};

static GtkWidget *assistant;
static GtkListStore *details_ls;
static GtkBuilder *builder;

void on_b_refresh_clicked(GtkButton *button)
{
    g_print("Refresh clicked!\n");
}

/* wizard.glade file as a string WIZARD_GLADE_CONTENTS: */
#include "wizard_glade.c"


GtkTreeView *create_details_treeview()
{
    GtkTreeView *details_tv = GTK_TREE_VIEW(gtk_builder_get_object(builder, "details_tv"));
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Name"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_NAME,
                                                     NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_NAME);
    gtk_tree_view_append_column(details_tv, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Value"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_VALUE,
                                                     NULL);
    gtk_tree_view_append_column(details_tv, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Path"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_PATH,
                                                     NULL);
    gtk_tree_view_append_column(details_tv, column);
    return details_tv;
}

void *append_item_to_details_ls(gpointer name, gpointer value, gpointer data)
{
    crash_item *item = (crash_item*)value;
    GtkTreeIter iter;

    gtk_list_store_append(details_ls, &iter);

    //FIXME: use the vaule representation here
    if(strlen(item->content) < 30)
    {
        gtk_list_store_set(details_ls, &iter,
                              COLUMN_NAME, (char *)name,
                              COLUMN_VALUE, item->content,
                              COLUMN_PATH, xasprintf("%s%s", dump_dir_path, name),
                              -1);
    }
    else
    {
        gtk_list_store_set(details_ls, &iter,
                              COLUMN_NAME, (char *)name,
                              COLUMN_VALUE, _("Content is too long, please use the \"View\" button to display it."),
                              COLUMN_PATH, xasprintf("%s%s", dump_dir_path, name),
                              -1);
    }

    return NULL;
}

void fill_details(GtkTreeView *treeview)
{
    details_ls = gtk_list_store_new(COLUMN_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    g_hash_table_foreach(cd, (GHFunc)append_item_to_details_ls, NULL);
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(details_ls));
}

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

        g_print("added page: %s\n", page_names[i]);
    }

    /* Set pointer to fields we might need to change */
    g_lbl_cd_reason = GTK_LABEL(gtk_builder_get_object(builder, "lbl_cd_reason"));

    g_vb_analyzers = GTK_VBOX(gtk_builder_get_object(builder, "vb_analyzers"));
}

GtkWidget *create_assistant()
{
    assistant = gtk_assistant_new();
    gtk_window_set_default_size(GTK_WINDOW(assistant), DEFAULT_WIDTH, DEFAULT_HEIGHT);

    g_signal_connect(G_OBJECT(assistant), "cancel", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(assistant), "close", G_CALLBACK(gtk_main_quit), NULL);

    builder = gtk_builder_new();
    add_pages();
    GtkTreeView *details_tv = create_details_treeview();
    fill_details(details_tv);
    gtk_builder_connect_signals(builder, NULL);

    return assistant;
}
