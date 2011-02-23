#include <gtk/gtk.h>
#include "abrtlib.h"
#include "wizard.h"

#define DEFAULT_WIDTH   800
#define DEFAULT_HEIGHT  500

GtkAssistant *g_assistant;

GtkBox *g_box_analyzers;
GtkLabel *g_lbl_analyze_log;
GtkTextView *g_tv_analyze_log;
GtkBox *g_box_reporters;
GtkLabel *g_lbl_report_log;
GtkTextView *g_tv_report_log;

GtkLabel *g_lbl_cd_reason;
GtkTextView *g_tv_backtrace;
GtkTreeView *g_tv_details;
GtkListStore *g_ls_details;
GtkWidget *g_widget_warnings_area;
GtkBox *g_box_warning_labels;
GtkToggleButton * g_tb_approve_bt;


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
static const gchar PAGE_SUMMARY[]            = "page_0";
static const gchar PAGE_ANALYZE_SELECTOR[]   = "page_1";
static const gchar PAGE_ANALYZE_PROGRESS[]   = "page_2";
static const gchar PAGE_REPORTER_SELECTOR[]  = "page_3";
static const gchar PAGE_BACKTRACE_APPROVAL[] = "page_4";
static const gchar PAGE_HOWTO[]              = "page_5";
static const gchar PAGE_REPORT[]             = "page_6";
static const gchar PAGE_REPORT_PROGRESS[]    = "page_7";

static const gchar *const page_names[] =
{
    PAGE_SUMMARY,
    PAGE_ANALYZE_SELECTOR,
    PAGE_ANALYZE_PROGRESS,
    PAGE_REPORTER_SELECTOR,
    PAGE_BACKTRACE_APPROVAL,
    PAGE_HOWTO,
    PAGE_REPORT,
    PAGE_REPORT_PROGRESS,
    NULL
};

typedef struct
{
    const gchar *name;
    const gchar *title;
    GtkAssistantPageType type;
    GtkWidget *page_widget;
} page_obj_t;

static page_obj_t pages[] =
{
    /* Page types:
     * CONFIRM: has [Apply] instead of [Fwd] and emits "apply" signal
     * PROGRESS: skipped on backward navigation
     * SUMMARY: has only [Close] button
     */
    /* glade element name     , on-screen text          , type */
    { PAGE_SUMMARY            , "Problem description"   , GTK_ASSISTANT_PAGE_CONTENT  },
    { PAGE_ANALYZE_SELECTOR   , "Select analyzer"       , GTK_ASSISTANT_PAGE_CONFIRM  },
    { PAGE_ANALYZE_PROGRESS   , "Analyzing"             , GTK_ASSISTANT_PAGE_CONTENT  },
    /* Some reporters don't need backtrace, we can skip bt page for them.
     * Therefore we want to know reporters _before_ we go to bt page
     */
    { PAGE_REPORTER_SELECTOR  , "Select reporter"       , GTK_ASSISTANT_PAGE_CONTENT  },
    { PAGE_BACKTRACE_APPROVAL , "Review the backtrace"  , GTK_ASSISTANT_PAGE_CONTENT  },
    { PAGE_HOWTO      , "Provide additional information", GTK_ASSISTANT_PAGE_CONTENT  },
    { PAGE_REPORT             , "Confirm data to report", GTK_ASSISTANT_PAGE_CONFIRM  },
    /* Was GTK_ASSISTANT_PAGE_PROGRESS */
    { PAGE_REPORT_PROGRESS    , "Reporting"             , GTK_ASSISTANT_PAGE_SUMMARY  },
    { NULL }
};


void on_b_refresh_clicked(GtkButton *button)
{
    g_print("Refresh clicked!\n");
}


static gint next_page_no(gint current_page_no, gpointer data)
{
    switch (current_page_no)
    {
    case PAGENO_SUMMARY:
        if (!g_analyze_events[0])
        {
            //TODO: if (!g_reporter_events[0]) /* no reporters available */ then what?
            return PAGENO_REPORTER_SELECTOR; /* skip analyze pages */
        }
        break;

    case PAGENO_REPORTER_SELECTOR:
        if (get_crash_item_content_or_NULL(g_cd, FILENAME_BACKTRACE))
            break;
        current_page_no++; /* no backtrace, skip next page */
        /* fall through */

#if 0
    case PAGENO_BACKTRACE_APPROVAL:
        if (get_crash_item_content_or_NULL(g_cd, FILENAME_COMMENT)
         || get_crash_item_content_or_NULL(g_cd, FILENAME_REPRODUCE)
        ) {
            break;
        }
        current_page_no++; /* no comment, skip next page */
        /* fall through */
#endif

    }

    return current_page_no + 1;
}


/* start_event_run */

struct analyze_event_data {
    struct run_event_state *run_state;
    const char *event_name;
    GtkWidget *page_widget;
    GtkLabel *status_label;
    GtkTextView *tv_log;
    const char *end_msg;
    GIOChannel *channel;
    int fd;
    /*guint event_source_id;*/
};

static gboolean consume_cmd_output(GIOChannel *source, GIOCondition condition, gpointer data)
{
    struct analyze_event_data *evd = data;

    GtkTextBuffer *tb = gtk_text_view_get_buffer(evd->tv_log);

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

    /* Scroll so that the end of the log is visible */
    gtk_text_buffer_get_iter_at_offset(tb, &text_iter, -1);
    gtk_text_view_scroll_to_iter(evd->tv_log, &text_iter,
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

    /* Stop if exit code is not 0, or no more commands */
    if (retval != 0
     || spawn_next_command(evd->run_state, g_dump_dir_name, evd->event_name) < 0
    ) {
        VERB1 log("done running event on '%s': %d", g_dump_dir_name, retval);
        /*g_source_remove(evd->event_source_id);*/
        close(evd->fd);
        free_run_event_state(evd->run_state);
        char *msg = xasprintf(evd->end_msg, retval);
        gtk_label_set_text(evd->status_label, msg);
        free(msg);
        /* Unfreeze assistant */
        gtk_assistant_set_page_complete(g_assistant, evd->page_widget, true);
        free(evd);
        reload_dump_dir();
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

static void start_event_run(const char *event_name,
                GList *more_events, // unused, TODO
                GtkWidget *page,
                GtkTextView *tv_log,
                GtkLabel *status_label,
                const char *start_msg,
                const char *end_msg
) {
    /* Start event asyncronously on the dump dir
     * (syncronous run would freeze GUI until completion)
     */
    struct run_event_state *state = new_run_event_state();

    if (prepare_commands(state, g_dump_dir_name, event_name) == 0
     || spawn_next_command(state, g_dump_dir_name, event_name) < 0
    ) {
        /* No commands needed?! (This is untypical) */
//TODO: better msg?
        char *msg = xasprintf(_("No processing for event '%s' is defined"), event_name);
        gtk_label_set_text(status_label, msg);
        free(msg);
        free_run_event_state(state);
        return;
    }

    /* At least one command is needed, and we started first one.
     * Hook its output fd up to the main loop.
     */
    VERB1 log("running event '%s' on '%s'", event_name, g_dump_dir_name);

    struct analyze_event_data *evd = xzalloc(sizeof(*evd));
    evd->run_state = state;
    evd->event_name = event_name;
    evd->page_widget = page;
    evd->status_label = status_label;
    evd->tv_log = tv_log;
    evd->end_msg = end_msg;
    evd->fd = state->command_out_fd;
    ndelay_on(evd->fd);
    evd->channel = g_io_channel_unix_new(evd->fd);

    /*evd->event_source_id = */ g_io_add_watch(evd->channel,
            G_IO_IN | G_IO_ERR | G_IO_HUP, /* need HUP to detect EOF w/o any data */
            consume_cmd_output,
            evd
    );

    gtk_label_set_text(status_label, start_msg);
    /* Freeze assistant so it can't move away from the page until analyzing is done */
    gtk_assistant_set_page_complete(g_assistant, page, false);
}


/* "Next page" button handler */
static void next_page(GtkAssistant *assistant, gpointer user_data)
{
    /* page_no is actually the previous page, because this
     * function is called before assistant goes to the next_page
     */
    int page_no = gtk_assistant_get_current_page(assistant);
    VERB2 log("page_no:%d", page_no);

    if (page_no == PAGENO_ANALYZE_SELECTOR
     && g_analyze_label_selected != NULL)
    {
        start_event_run(/*event_name:*/ g_analyze_label_selected,
                NULL,
                pages[PAGENO_ANALYZE_PROGRESS].page_widget,
                g_tv_analyze_log,
                g_lbl_analyze_log,
                _("Analyzing..."),
                _("Analyzing finished with exit code %d")
        );
    }

    if (page_no == PAGENO_REPORT)
    {
        GList *reporters = gtk_container_get_children(GTK_CONTAINER(g_box_reporters));
        if (reporters)
        {
            for (GList *li = reporters; li; li = li->next)
            {
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(li->data)) == TRUE)
                    li->data = (gpointer)gtk_button_get_label(GTK_BUTTON(li->data));
                else
                    li->data = NULL;
            }
            reporters = g_list_remove_all(reporters, NULL);
            if (reporters)
            {
                char *first_event_name = reporters->data;
                reporters = g_list_remove(reporters, first_event_name);
                start_event_run(first_event_name,
                        reporters,
                        pages[PAGENO_REPORT_PROGRESS].page_widget,
                        g_tv_report_log,
                        g_lbl_report_log,
                        _("Reporting..."),
                        _("Reporting finished with exit code %d")
                );
                g_list_free(reporters);
            }
        }
    }
}


/* Initialization */

static void create_details_treeview()
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Name"),
                                                     renderer,
                                                     "text",
                                                     DETAIL_COLUMN_NAME,
                                                     NULL);
    gtk_tree_view_column_set_sort_column_id(column, DETAIL_COLUMN_NAME);
    gtk_tree_view_append_column(g_tv_details, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Value"),
                                                     renderer,
                                                     "text",
                                                     DETAIL_COLUMN_VALUE,
                                                     NULL);
    gtk_tree_view_append_column(g_tv_details, column);

    /*
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Path"),
                                                     renderer,
                                                     "text",
                                                     DETAIL_COLUMN_PATH,
                                                     NULL);
    gtk_tree_view_append_column(g_tv_details, column);
    */
}

/* wizard.glade file as a string WIZARD_GLADE_CONTENTS: */
#include "wizard_glade.c"

static void add_pages(void)
{
    GError *error = NULL;
    if (!g_glade_file)
    {
        /* Load UI from internal string */
        gtk_builder_add_objects_from_string(builder,
                WIZARD_GLADE_CONTENTS, sizeof(WIZARD_GLADE_CONTENTS) - 1,
                (gchar**)page_names,
                &error);
        if (error != NULL)
            error_msg_and_die("Error loading glade data: %s", error->message);
    }
    else
    {
        /* -g FILE: load IU from it */
        gtk_builder_add_objects_from_file(builder, g_glade_file, (gchar**)page_names, &error);
        if (error != NULL)
            error_msg_and_die("Can't load %s: %s", g_glade_file, error->message);
    }

    for (int i = 0; page_names[i] != NULL; i++)
    {
        GtkWidget *page = GTK_WIDGET(gtk_builder_get_object(builder, page_names[i]));
        if (page == NULL)
            continue;

        pages[i].page_widget = page;

        gtk_assistant_append_page(g_assistant, page);
        /* if we set all pages to complete the wizard thinks there is nothing
        *  to do and shows the button "Last" which allows user to skip all pages
        *  so we need to set them all as incomplete and complete them one by one
        *  on proper place - on_page_prepare() ?
        */
        //gtk_assistant_set_page_complete(g_assistant, page, true);

        gtk_assistant_set_page_title(g_assistant, page, pages[i].title);
        gtk_assistant_set_page_type(g_assistant, page, pages[i].type);

        VERB1 log("added page: %s", page_names[i]);
    }

    /* Set pointers to objects we might need to work with */
    g_lbl_cd_reason = GTK_LABEL(gtk_builder_get_object(builder, "lbl_cd_reason"));
    g_box_analyzers = GTK_BOX(gtk_builder_get_object(builder, "vb_analyzers"));
    g_lbl_analyze_log = GTK_LABEL(gtk_builder_get_object(builder, "lbl_analyze_log"));
    g_tv_analyze_log = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "tv_analyze_log"));
    g_box_reporters = GTK_BOX(gtk_builder_get_object(builder, "vb_reporters"));
    g_lbl_report_log = GTK_LABEL(gtk_builder_get_object(builder, "lbl_report_log"));
    g_tv_report_log = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "tv_report_log"));
    g_tv_backtrace = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "tv_backtrace"));
    g_tv_details = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tv_details"));
    g_box_warning_labels = GTK_BOX(gtk_builder_get_object(builder, "hb_warning_labels"));
    g_tb_approve_bt = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "cb_approve_bt"));
    g_widget_warnings_area = GTK_WIDGET(gtk_builder_get_object(builder, "hb_warnings_area"));
    ///* hide the warnings by default */
    //gtk_widget_hide(g_widget_warnings_area);
}

static void add_warning(const char *warning)
{
    char *label_str = xasprintf("• %s", warning);
    GtkWidget *warning_lbl = gtk_label_new(label_str);
    gtk_misc_set_alignment(GTK_MISC(warning_lbl), 0.0, 0.0);
    gtk_label_set_justify(GTK_LABEL(warning_lbl), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(g_box_warning_labels, warning_lbl, false, false, 0);
    gtk_widget_show(warning_lbl);
    /* should be safe to free it, gtk calls strdup() to copy it */
    free(label_str);
}

static void check_backtrace_and_allow_send(void)
{
    bool send = true;

    /* erase all warnings */
    gtk_widget_hide(g_widget_warnings_area);
    gtk_container_foreach(GTK_CONTAINER(g_box_warning_labels), &remove_child_widget, NULL);

    /*
     * FIXME: this should be bind to a reporter not to a compoment
     * but we don't rate oopses, so for now we skip the "kernel" manually
     */
    const char *component = get_crash_item_content_or_NULL(g_cd, FILENAME_COMPONENT);
    if (strcmp(component, "kernel") != 0)
    {
        const char *rating = get_crash_item_content_or_NULL(g_cd, FILENAME_RATING);
        if (rating) switch (*rating)
        {
            case '4': //bt is ok - no warning here
                break;
            case '3': //bt is usable, but not complete, so show a warning
                add_warning(_("The backtrace is incomplete, please make sure you provide the steps to reproduce."));
                send = false;
                break;
            case '2':
            case '1':
                //FIXME: see CreporterAssistant: 394 for ideas
                add_warning(_("Reporting disabled because the backtrace is unusable."));
                send = false;
                break;
        }
    }

    if (!gtk_toggle_button_get_active(g_tb_approve_bt))
    {
        add_warning(_("You should check the backtrace for sensitive data."));
        add_warning(_("You must agree with sending the backtrace."));
        send = false;
    }

    gtk_assistant_set_page_complete(g_assistant,
                                    pages[PAGENO_BACKTRACE_APPROVAL].page_widget,
                                    send);
    if (!send)
        gtk_widget_show(g_widget_warnings_area);
}

static void on_bt_approve_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
    check_backtrace_and_allow_send();
}

static void on_page_prepare(GtkAssistant *assistant, GtkWidget *page, gpointer user_data)
{
    //FIXME: move some of this to the add_pages??
    if (pages[PAGENO_SUMMARY].page_widget == page)
    {
        /* the first page doesn't require any action to be completed,
         * so lets make it complete and enable the "Forward" button
         */
        gtk_assistant_set_page_complete(assistant, page, true);
    }
    if (pages[PAGENO_BACKTRACE_APPROVAL].page_widget == page)
    {
        check_backtrace_and_allow_send();
    }
}

void create_assistant()
{
    g_assistant = GTK_ASSISTANT(gtk_assistant_new());

    gtk_assistant_set_forward_page_func(g_assistant, next_page_no, NULL, NULL);

    GtkWindow *wnd_assistant = GTK_WINDOW(g_assistant);
    gtk_window_set_default_size(wnd_assistant, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    gtk_window_set_title(wnd_assistant, g_dump_dir_name);
    gtk_window_set_icon_name(wnd_assistant, "abrt");

    GObject *obj_assistant = G_OBJECT(g_assistant);
    g_signal_connect(obj_assistant, "cancel", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(obj_assistant, "close", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(obj_assistant, "apply", G_CALLBACK(next_page), NULL);
    g_signal_connect(obj_assistant, "prepare", G_CALLBACK(on_page_prepare), NULL);

    builder = gtk_builder_new();

    add_pages();

    create_details_treeview();
    g_ls_details = gtk_list_store_new(DETAIL_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model(g_tv_details, GTK_TREE_MODEL(g_ls_details));

    gtk_builder_connect_signals(builder, NULL);

    g_signal_connect(g_tb_approve_bt, "toggled", G_CALLBACK(on_bt_approve_toggle), NULL);
    gtk_assistant_set_page_complete(g_assistant,
                pages[PAGENO_BACKTRACE_APPROVAL].page_widget,
                gtk_toggle_button_get_active(g_tb_approve_bt)
    );
}
