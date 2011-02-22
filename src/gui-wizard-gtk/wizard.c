#include <gtk/gtk.h>
#include "abrtlib.h"
#include "wizard.h"

#define DEFAULT_WIDTH   800
#define DEFAULT_HEIGHT  500

GtkLabel *g_lbl_cd_reason;
GtkLabel *g_lbl_analyze_log;
GtkBox *g_box_analyzers;
GtkBox *g_box_reporters;
GtkTextView *g_analyze_log;

static GtkWidget *assistant;
static GtkListStore *details_ls;
static GtkBuilder *builder;

/* THE PAGE FLOW
 * page_1: analyze action selection
 * page_2: analyze progress
 * page_3: reporter selection
 * page_4: backtrace editor
 * page_5: how to + user comments
 * page_6: summary
 * page_7: reporting progress
 */

/* Use of arrays (instead of, say, #defines to C strings)
 * allows cheaper page_obj_t->name == PAGE_FOO comparisons
 * instead of strcmp.
 */
static const gchar PAGE_ANALYZE_ACTION_SELECTOR[] = "page_1";
static const gchar PAGE_ANALYZE_PROGRESS[]        = "page_2";
static const gchar PAGE_REPORTER_SELECTOR[]       = "page_3";
static const gchar PAGE_BACKTRACE_APPROVAL[]      = "page_4";
static const gchar PAGE_HOWTO[]                   = "page_5";
static const gchar PAGE_SUMMARY[]                 = "page_6";
static const gchar PAGE_REPORT[]                  = "page_7";

static const gchar *const page_names[] =
{
    PAGE_ANALYZE_ACTION_SELECTOR,
    PAGE_ANALYZE_PROGRESS,
    PAGE_REPORTER_SELECTOR,
    PAGE_BACKTRACE_APPROVAL,
    PAGE_HOWTO,
    PAGE_SUMMARY,
    PAGE_REPORT,
    NULL
};

typedef struct
{
    const gchar *name;
    const gchar *title;
    GtkAssistantPageType type;
    GtkWidget *page_widget;
} page_obj_t;

static page_obj_t pages[8] =
{
    /* need this type to get "apply" signal */
    {PAGE_ANALYZE_ACTION_SELECTOR, "Select analyzer", GTK_ASSISTANT_PAGE_CONFIRM, NULL},
    {PAGE_ANALYZE_PROGRESS, "Analyzing reporter", GTK_ASSISTANT_PAGE_PROGRESS, NULL},
    {PAGE_REPORTER_SELECTOR, "Select reporter", GTK_ASSISTANT_PAGE_CONTENT, NULL},
    {PAGE_BACKTRACE_APPROVAL, "Approve the backtrace", GTK_ASSISTANT_PAGE_CONTENT, NULL},
    {PAGE_HOWTO, "Provide additional information", GTK_ASSISTANT_PAGE_CONTENT, NULL},
    {PAGE_SUMMARY, "Confirm and send the report", GTK_ASSISTANT_PAGE_CONFIRM, NULL},
    {PAGE_REPORT, "Approve the backtrace", GTK_ASSISTANT_PAGE_SUMMARY, NULL},
    {NULL}
};

enum
{
    COLUMN_NAME,
    COLUMN_VALUE,
    COLUMN_PATH,
    COLUMN_COUNT
};

void on_b_refresh_clicked(GtkButton *button)
{
    g_print("Refresh clicked!\n");
}

static void fill_backtrace()
{
    crash_item *ci = NULL;
    GtkTextView *backtrace_tev = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "bactrace_tev"));
    GtkTextBuffer *backtrace_buf = gtk_text_buffer_new(NULL);
    ci = g_hash_table_lookup(cd, FILENAME_BACKTRACE);
    if(ci != NULL)
    {
        //g_print(ci->content);
        gtk_text_buffer_set_text(backtrace_buf, ci->content, -1);
        gtk_text_view_set_buffer(backtrace_tev, backtrace_buf);
    }
}

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

    /*
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Path"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_PATH,
                                                     NULL);
    gtk_tree_view_append_column(details_tv, column);
    */
    return details_tv;
}

static void *append_item_to_details_ls(gpointer name, gpointer value, gpointer data)
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
                              COLUMN_PATH, xasprintf("%s%s", g_dump_dir_name, name),
                              -1);
    }
    else
    {
        gtk_list_store_set(details_ls, &iter,
                              COLUMN_NAME, (char *)name,
                              COLUMN_VALUE, _("Content is too long, please use the \"View\" button to display it."),
                              COLUMN_PATH, xasprintf("%s%s", g_dump_dir_name, name),
                              -1);
    }

    return NULL;
}

static void fill_details(GtkTreeView *treeview)
{
    details_ls = gtk_list_store_new(COLUMN_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    g_hash_table_foreach(cd, (GHFunc)append_item_to_details_ls, NULL);
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(details_ls));
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

        pages[i].page_widget = page;

        gtk_assistant_append_page(GTK_ASSISTANT(assistant), page);
        //FIXME: shouldn't be complete until something is selected!
        gtk_assistant_set_page_complete(GTK_ASSISTANT(assistant), page, true);

        gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), page, pages[i].title);
        gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), page, pages[i].type);

        log("added page: %s", page_names[i]);
    }

    /* Set pointer to fields we might need to change */
    g_lbl_cd_reason = GTK_LABEL(gtk_builder_get_object(builder, "lbl_cd_reason"));
    g_lbl_analyze_log = GTK_LABEL(gtk_builder_get_object(builder, "lbl_analyze_log"));
    g_box_analyzers = GTK_BOX(gtk_builder_get_object(builder, "vb_analyzers"));
    g_box_reporters = GTK_BOX(gtk_builder_get_object(builder, "vb_reporters"));
    g_analyze_log = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "analyze_log"));
}


/* "Next page" button handler. So far it only starts analyze event run */

struct analyze_event_data {
    struct run_event_state *run_state;
    GIOChannel *channel;
    int fd;
    /*guint event_source_id;*/
};

static gboolean consume_cmd_output(GIOChannel *source, GIOCondition condition, gpointer data)
{
    struct analyze_event_data *evd = data;

    GtkTextBuffer *tb = gtk_text_view_get_buffer(g_analyze_log);

    /* Ensure we insert text at the end */
    GtkTextIter text_iter;
    gtk_text_buffer_get_iter_at_offset(tb, &text_iter, -1);
    gtk_text_buffer_place_cursor(tb, &text_iter);

    /* Read and insert the output into the log pane */
    char buf[128]; /* usually we get one line, no need to have big buf */
    int r;
    while ((r = read(evd->fd, buf, sizeof(buf))) > 0)
    {
        gtk_text_buffer_insert_at_cursor(tb, buf, r);
    }

    /* Scroll so that end of the log is visible */
    gtk_text_buffer_get_iter_at_offset(tb, &text_iter, -1);
    gtk_text_view_scroll_to_iter(g_analyze_log, &text_iter,
                /*within_margin:*/ 0.0, /*use_align:*/ FALSE, /*xalign:*/ 0, /*yalign:*/ 0);

    if (r < 0 && errno == EAGAIN)
        /* We got all data, but fd is still open. Done for now */
        return TRUE; /* "please don't remove this event (yet)" */

    /* EOF/error. Wait for child to actually exit, collect status */
    int status;
    waitpid(evd->run_state->command_pid, &status, 0);
    int retval = WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        retval = WTERMSIG(status) + 128;

    /* Stop if exitcode is not 0, or no more commands */
    if (retval != 0
     || spawn_next_command(evd->run_state, g_dump_dir_name, /*event:*/ g_analyze_label_selected) < 0
    ) {
        log("done running event '%s' on '%s': %d", g_analyze_label_selected, g_dump_dir_name, retval);
        /*g_source_remove(evd->event_source_id);*/
        close(evd->fd);
        free_run_event_state(evd->run_state);
        free(evd);
        char *msg = xasprintf(_("Analyze finished with exitcode %d"), retval);
        gtk_label_set_text(g_lbl_analyze_log, msg);
        free(msg);
        /* Unfreeze assistant */
        gtk_assistant_set_page_complete(GTK_ASSISTANT(assistant),
                        pages[PAGENO_ANALYZE_ACTION_SELECTOR].page_widget, true);
        return FALSE; /* "please remove this event" */
    }

    /* New command was started. Continue waiting for input */

    /* Transplant cmd's output fd onto old one, so that main loop
     * is none the wiser that fd it waits on has changed
     */
    xmove_fd(evd->run_state->command_out_fd, evd->fd);
    evd->run_state->command_out_fd = evd->fd; /* just to keep it consistent */
    ndelay_on(evd->fd);

    return TRUE; /* "please don't remove this event (yet)" */
}

static void next_page(GtkAssistant *assistant, gpointer user_data)
{
    int page_no = gtk_assistant_get_current_page(assistant);
    GtkWidget *cur_page_widget = pages[page_no].page_widget;
    log("page_no:%d", page_no);

    if (page_no == PAGENO_ANALYZE_ACTION_SELECTOR
     && g_analyze_label_selected != NULL)
    {
        /* Start event asyncronously on the dump dir
         * (syncronous run would freeze GUI until completion)
         */
        struct run_event_state *state = new_run_event_state();

        if (prepare_commands(state, g_dump_dir_name, /*event:*/ g_analyze_label_selected) == 0
         || spawn_next_command(state, g_dump_dir_name, /*event:*/ g_analyze_label_selected) < 0
        ) {
            /* No commands needed */
            free_run_event_state(state);
            return;
        }

        /* At least one command is needed, and we started first one.
         * Hook its output fd up to the main loop.
         */
        log("running event '%s' on '%s'", g_analyze_label_selected, g_dump_dir_name);

        struct analyze_event_data *evd = xzalloc(sizeof(*evd));
        evd->run_state = state;
        evd->fd = state->command_out_fd;
        ndelay_on(evd->fd);
        evd->channel = g_io_channel_unix_new(evd->fd);
        /*evd->event_source_id = */ g_io_add_watch(evd->channel,
                G_IO_IN | G_IO_ERR | G_IO_HUP, /* need HUP to detect EOF w/o any data */
                consume_cmd_output,
                evd
        );
        gtk_label_set_text(g_lbl_analyze_log, _("Analyzing..."));
        /* Freeze assistant so it can't move away from the page until analyzing is done */
//doesn't seem to have effect
        gtk_assistant_set_page_complete(GTK_ASSISTANT(assistant), cur_page_widget, false);
    }
}


static void on_page_prepare(GtkAssistant *assistant, GtkWidget *page, gpointer user_data)
{
    page_obj_t *cur_page = pages;
    while (cur_page->page_widget != page)
    {
        if (!cur_page->page_widget)
            return; /* end of pages[] */
        ++cur_page;
    }

    if (cur_page->name == PAGE_BACKTRACE_APPROVAL)
        fill_backtrace();
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
    g_signal_connect(G_OBJECT(assistant), "prepare", G_CALLBACK(on_page_prepare), NULL);

    builder = gtk_builder_new();
    add_pages();
    GtkTreeView *details_tv = create_details_treeview();
    fill_details(details_tv);
    gtk_builder_connect_signals(builder, NULL);

    return assistant;
}
