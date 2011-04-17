/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <gtk/gtk.h>
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "wizard.h"
#include "libreport-gtk.h"

#define DEFAULT_WIDTH   800
#define DEFAULT_HEIGHT  500


typedef struct event_gui_data_t
{
    char *event_name;
    GtkToggleButton *toggle_button;
} event_gui_data_t;


static GtkAssistant *g_assistant;

static char *g_analyze_event_selected;

static GtkBox *g_box_analyzers;
/* List of event_gui_data's */
static GList *g_list_analyzers;
static GtkLabel *g_lbl_analyze_log;
static GtkTextView *g_tv_analyze_log;

static GtkBox *g_box_reporters;
/* List of event_gui_data's */
static GList *g_list_reporters;
static GtkLabel *g_lbl_report_log;
static GtkTextView *g_tv_report_log;

static GtkContainer *g_container_details1;
static GtkContainer *g_container_details2;

static GtkLabel *g_lbl_cd_reason;
static GtkTextView *g_tv_backtrace;
static GtkTextView *g_tv_comment;
static GtkTreeView *g_tv_details;
static GtkWidget *g_widget_warnings_area;
static GtkBox *g_box_warning_labels;
static GtkToggleButton *g_tb_approve_bt;
static GtkButton *g_btn_refresh;

static GtkLabel *g_lbl_reporters;
static GtkLabel *g_lbl_size;

static GtkCellRenderer *g_tv_details_col2;
static GtkListStore *g_ls_details;
enum
{
    DETAIL_COLUMN_NAME,
    DETAIL_COLUMN_VALUE,
    //COLUMN_PATH,
    DETAIL_NUM_COLUMNS,
};

/* Search in bt */
static guint g_timeout = 0;
static GtkEntry *g_search_entry_bt;

static GtkBuilder *builder;
static PangoFontDescription *monospace_font;


/* THE PAGE FLOW
 * page_5: user comments
 * page_1: analyze action selection
 * page_2: analyze progress
 * page_3: reporter selection
 * page_4: backtrace editor
 * page_6: summary
 * page_7: reporting progress
 */
enum {
    PAGENO_SUMMARY,
    PAGENO_COMMENT,
    PAGENO_ANALYZE_SELECTOR,
    PAGENO_ANALYZE_PROGRESS,
    PAGENO_REPORTER_SELECTOR,
    PAGENO_BACKTRACE_APPROVAL,
    PAGENO_REPORT,
    PAGENO_REPORT_PROGRESS,
};

/* Use of arrays (instead of, say, #defines to C strings)
 * allows cheaper page_obj_t->name == PAGE_FOO comparisons
 * instead of strcmp.
 */
static const gchar PAGE_SUMMARY[]            = "page_0";
static const gchar PAGE_COMMENT[]            = "page_1";
static const gchar PAGE_ANALYZE_SELECTOR[]   = "page_2";
static const gchar PAGE_ANALYZE_PROGRESS[]   = "page_3";
static const gchar PAGE_REPORTER_SELECTOR[]  = "page_4";
static const gchar PAGE_BACKTRACE_APPROVAL[] = "page_5";
static const gchar PAGE_REPORT[]             = "page_6";
static const gchar PAGE_REPORT_PROGRESS[]    = "page_7";

static const gchar *const page_names[] =
{
    PAGE_SUMMARY,
    PAGE_COMMENT,
    PAGE_ANALYZE_SELECTOR,
    PAGE_ANALYZE_PROGRESS,
    PAGE_REPORTER_SELECTOR,
    PAGE_BACKTRACE_APPROVAL,
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
     * INTRO: only [Fwd] button is shown
     * CONTENT: normal page
     * CONFIRM: has [Apply] instead of [Fwd] and emits "apply" signal
     * PROGRESS: skipped on backward navigation
     * SUMMARY: has only [Close] button
     */
    /* glade element name     , on-screen text          , type */
    { PAGE_SUMMARY            , "Problem description"   , GTK_ASSISTANT_PAGE_CONTENT  },
    { PAGE_COMMENT    , "Provide additional information", GTK_ASSISTANT_PAGE_CONTENT  },
    { PAGE_ANALYZE_SELECTOR   , "Select analyzer"       , GTK_ASSISTANT_PAGE_CONFIRM  },
    { PAGE_ANALYZE_PROGRESS   , "Analyzing"             , GTK_ASSISTANT_PAGE_CONTENT  },
    /* Some reporters don't need backtrace, we can skip bt page for them.
     * Therefore we want to know reporters _before_ we go to bt page
     */
    { PAGE_REPORTER_SELECTOR  , "Select reporter"       , GTK_ASSISTANT_PAGE_CONTENT  },
    { PAGE_BACKTRACE_APPROVAL , "Review the backtrace"  , GTK_ASSISTANT_PAGE_CONTENT  },
    { PAGE_REPORT             , "Confirm data to report", GTK_ASSISTANT_PAGE_CONFIRM  },
    /* Was GTK_ASSISTANT_PAGE_PROGRESS */
    { PAGE_REPORT_PROGRESS    , "Reporting"             , GTK_ASSISTANT_PAGE_SUMMARY  },
    { NULL }
};


/* Utility functions */

static void remove_child_widget(GtkWidget *widget, gpointer unused)
{
    /* Destroy will safely remove it and free the memory
     * if there are no refs left
     */
    gtk_widget_destroy(widget);
}

static void save_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    *(gint*)user_data = response_id;
}

struct dump_dir *steal_if_needed(struct dump_dir *dd)
{
    if (!dd)
        xfunc_die(); /* error msg was already logged */

    if (dd->locked)
        return dd;

    dd_close(dd);

    char *HOME = getenv("HOME");
    if (HOME && HOME[0])
        HOME = concat_path_file(HOME, ".abrt/spool");
    else
//TODO: try to find homedir in password db?
        HOME = xstrdup("/tmp");

    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(g_assistant),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_QUESTION,
                GTK_BUTTONS_OK_CANCEL,
                _("Need writable directory, but '%s' is not writable."
                " Move it to '%s' and operate on the moved copy?"),
                g_dump_dir_name, HOME
    );
    gint response = GTK_RESPONSE_CANCEL;
    g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(save_dialog_response), &response);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response != GTK_RESPONSE_OK)
        return NULL;

    dd = steal_directory(HOME, g_dump_dir_name);
    if (!dd)
        return NULL; /* Stealing failed. Error msg was already logged */

    /* Delete old dir and switch to new one.
     * Don't want to keep new dd open across deletion,
     * therefore it's a bit more complicated.
     */
    char *old_name = g_dump_dir_name;
    g_dump_dir_name = xstrdup(dd->dd_dirname);
    dd_close(dd);

    gtk_window_set_title(GTK_WINDOW(g_assistant), g_dump_dir_name);
    delete_dump_dir_possibly_using_abrtd(old_name); //TODO: if (deletion_failed) error_msg("BAD")?
    free(old_name);

    dd = dd_opendir(g_dump_dir_name, 0);
    if (!dd)
        xfunc_die(); /* error msg was already logged */

    return dd;
}

void show_error_as_msgbox(const char *msg)
{
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(g_assistant),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_CLOSE,
                "%s", msg
    );
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void load_text_to_text_view(GtkTextView *tv, const char *name)
{
    const char *str = g_cd ? get_crash_item_content_or_NULL(g_cd, name) : NULL;
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(tv), (str ? str : ""), -1);
}

static gchar *get_malloced_string_from_text_view(GtkTextView *tv)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(tv);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static void save_text_if_changed(const char *name, const char *new_value)
{
    const char *old_value = g_cd ? get_crash_item_content_or_NULL(g_cd, name) : "";
    if (!old_value)
        old_value = "";
    if (strcmp(new_value, old_value) != 0)
    {
        struct dump_dir *dd = dd_opendir(g_dump_dir_name, DD_OPEN_READONLY);
        dd = steal_if_needed(dd);
        if (dd && dd->locked)
        {
            dd_save_text(dd, name, new_value);
        }
//FIXME: else: what to do with still-unsaved data in the widget??
        dd_close(dd);
        reload_crash_data_from_dump_dir();
        update_gui_state_from_crash_data();
    }
}

static void save_text_from_text_view(GtkTextView *tv, const char *name)
{
    gchar *new_str = get_malloced_string_from_text_view(tv);
    save_text_if_changed(name, new_str);
    free(new_str);
}

static void append_to_textview(GtkTextView *tv, const char *str, int len)
{
    GtkTextBuffer *tb = gtk_text_view_get_buffer(tv);

    /* Ensure we insert text at the end */
    GtkTextIter text_iter;
    gtk_text_buffer_get_iter_at_offset(tb, &text_iter, -1);
    gtk_text_buffer_place_cursor(tb, &text_iter);

    gtk_text_buffer_insert_at_cursor(tb, str, len >= 0 ? len : strlen(str));

    /* Scroll so that the end of the log is visible */
    gtk_text_buffer_get_iter_at_offset(tb, &text_iter, -1);
    gtk_text_view_scroll_to_iter(tv, &text_iter,
                /*within_margin:*/ 0.0, /*use_align:*/ FALSE, /*xalign:*/ 0, /*yalign:*/ 0);
}


/* event_gui_data_t */

static event_gui_data_t *new_event_gui_data_t(void)
{
    return xzalloc(sizeof(event_gui_data_t));
}

static void free_event_gui_data_t(event_gui_data_t *evdata, void *unused)
{
    if (evdata)
    {
        free(evdata->event_name);
        free(evdata);
    }
}


/* tv_details handling */

static void tv_details_row_activated(
                        GtkTreeView       *tree_view,
                        GtkTreePath       *path,
                        GtkTreeViewColumn *column,
                        gpointer           user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(tree_view);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gchar *column_name;
    gtk_tree_model_get(model, &iter, DETAIL_COLUMN_NAME, &column_name, -1);
    struct crash_item *item = get_crash_data_item_or_NULL(g_cd, column_name);
    if (!item || !(item->flags & CD_FLAG_TXT))
        return;
    if (!strchr(item->content, '\n')) /* one line? */
        return;

    gchar *arg[3];
    arg[0] = (char *) "xdg-open";
    arg[1] = concat_path_file(g_dump_dir_name, column_name);
    arg[2] = NULL;
    g_free(column_name);

    g_spawn_sync(NULL, arg, NULL,
                 G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL,
                 NULL, NULL, NULL, NULL, NULL, NULL);

    free(arg[1]);
}

/* static gboolean tv_details_select_cursor_row(
                        GtkTreeView *tree_view,
                        gboolean arg1,
                        gpointer user_data) {...} */

static void tv_details_cursor_changed(
                        GtkTreeView *tree_view,
                        gpointer     user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(tree_view);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gchar *column_name;
    gtk_tree_model_get(model, &iter, DETAIL_COLUMN_NAME, &column_name, -1);
    struct crash_item *item = get_crash_data_item_or_NULL(g_cd, column_name);

    gboolean editable = (item && (item->flags & CD_FLAG_TXT) && !strchr(item->content, '\n'));

    /* Allow user to select the text with mouse.
     * Has undesirable side-effect of allowing user to "edit" the text,
     * but changes aren't saved (the old text reappears as soon as user
     * leaves the field). Need to disable editing somehow.
     */
    g_object_set(G_OBJECT(g_tv_details_col2),
                "editable", editable,
                // "editable-set", editable,
                NULL);
}


/* update_gui_state_from_crash_data */

static gint find_by_button(gconstpointer a, gconstpointer button)
{
    const event_gui_data_t *evdata = a;
    return (evdata->toggle_button != button);
}

static void analyze_rb_was_toggled(GtkButton *button, gpointer user_data)
{
    GList *found = g_list_find_custom(g_list_analyzers, button, find_by_button);
    if (found)
    {
        event_gui_data_t *evdata = found->data;
        free(g_analyze_event_selected);
        g_analyze_event_selected = xstrdup(evdata->event_name);
    }
}

static void report_tb_was_toggled(GtkButton *button_unused, gpointer user_data_unused)
{
    struct strbuf *reporters_string = strbuf_new();
    GList *li = g_list_reporters;
    for (; li; li = li->next)
    {
        event_gui_data_t *event_gui_data = li->data;
        if (gtk_toggle_button_get_active(event_gui_data->toggle_button) == TRUE)
        {
            strbuf_append_strf(reporters_string,
                            "%s%s",
                            (reporters_string->len != 0 ? ", " : ""),
                            event_gui_data->event_name
            );
            g_validate_event(event_gui_data->event_name);
        }
    }

    gtk_assistant_set_page_complete(g_assistant,
                pages[PAGENO_REPORTER_SELECTOR].page_widget,
                reporters_string->len != 0 /* true if at least one checkbox is active */
    );

    /* Update "list of reporters" label */
    char *str = strbuf_free_nobuf(reporters_string);
    gtk_label_set_text(g_lbl_reporters, str);
    free(str);
}

/* event_name contains "EVENT1\nEVENT2\nEVENT3\n".
 * Add new {radio/check}buttons to GtkBox for each EVENTn (type depends on bool radio).
 * Remember them in GList **p_event_list (list of event_gui_data_t's).
 * Set "toggled" callback on each button to given GCallback if it's not NULL.
 * If prev_selected == EVENTn, set this button as active. In this case return NULL.
 * Else return 1st button created (or NULL if none created).
 */
static event_gui_data_t *add_event_buttons(GtkBox *box,
                GList **p_event_list,
                char *event_name,
                GCallback func,
                bool radio,
                const char *prev_selected)
{
    //VERB2 log("removing all buttons from box %p", box);
    gtk_container_foreach(GTK_CONTAINER(box), &remove_child_widget, NULL);
    g_list_foreach(*p_event_list, (GFunc)free_event_gui_data_t, NULL);
    g_list_free(*p_event_list);
    *p_event_list = NULL;

    bool have_activated_btn = false;
    event_gui_data_t *first_button = NULL;
    while (event_name[0])
    {
        char *event_name_end = strchr(event_name, '\n');
        *event_name_end = '\0';

        /* Form a pretty text representation of event */
        /* By default, use event name, just strip "foo_" prefix if it exists: */
        const char *event_screen_name = strchr(event_name, '_');
        if (event_screen_name)
            event_screen_name++;
        else
            event_screen_name = event_name;
        const char *event_description = NULL;
        event_config_t *cfg = get_event_config(event_name);
        if (cfg)
        {
            /* .xml has (presumably) prettier description, use it: */
            if (cfg->screen_name)
                event_screen_name = cfg->screen_name;
            event_description = cfg->description;
        }

        //VERB2 log("adding button '%s' to box %p", event_name, box);
        char *event_label = xasprintf("%s%s%s",
                        event_screen_name,
                        (event_description ? " - " : ""),
                        event_description ? event_description : ""
        );
        GtkWidget *button = radio
                ? gtk_radio_button_new_with_label_from_widget(
                        (first_button ? GTK_RADIO_BUTTON(first_button->toggle_button) : NULL),
                        event_label
                  )
                : gtk_check_button_new_with_label(event_label);
        free(event_label);
        if (func)
            g_signal_connect(G_OBJECT(button), "toggled", func, NULL);
        if (cfg->long_descr)
            gtk_widget_set_tooltip_text(button, cfg->long_descr);

        event_gui_data_t *event_gui_data = new_event_gui_data_t();
        event_gui_data->event_name = xstrdup(event_name);
        event_gui_data->toggle_button = GTK_TOGGLE_BUTTON(button);
	*p_event_list = g_list_append(*p_event_list, event_gui_data);

        if (!first_button)
            first_button = event_gui_data;

        if (prev_selected && strcmp(prev_selected, event_name) == 0)
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), true);
            have_activated_btn = true;
            prev_selected = NULL;
        }

        *event_name_end = '\n';
        event_name = event_name_end + 1;

        gtk_box_pack_start(box, button, /*expand*/ false, /*fill*/ false, /*padding*/ 0);

    }
    return (have_activated_btn ? NULL : first_button);
}

struct cd_stats {
    off_t filesize;
    unsigned filecount;
};

static void append_item_to_ls_details(gpointer name, gpointer value, gpointer data)
{
    crash_item *item = (crash_item*)value;
    struct cd_stats *stats = data;
    GtkTreeIter iter;

    gtk_list_store_append(g_ls_details, &iter);
    stats->filecount++;

    //FIXME: use the value representation here
    /* If text and not multiline... */
    if (item->flags & CD_FLAG_TXT)
    {
        stats->filesize += strlen(item->content);
        if (!strchr(item->content, '\n'))
        {
            gtk_list_store_set(g_ls_details, &iter,
                              DETAIL_COLUMN_NAME, (char *)name,
                              DETAIL_COLUMN_VALUE, item->content,
                              //DETAIL_COLUMN_PATH, xasprintf("%s%s", g_dump_dir_name, name),
                              -1);
        }
        else
        {
            gtk_list_store_set(g_ls_details, &iter,
                              DETAIL_COLUMN_NAME, (char *)name,
                              DETAIL_COLUMN_VALUE, _("(click here to view/edit)"),
                              //DETAIL_COLUMN_PATH, xasprintf("%s%s", g_dump_dir_name, name),
                              -1);
            //WARNING: will leak xasprintf results above if uncommented
        }
    }
    else if (item->flags & CD_FLAG_BIN)
    {
        struct stat statbuf;
        statbuf.st_size = 0;
        stat(item->content, &statbuf);
        stats->filesize += statbuf.st_size;
        char *msg = xasprintf(_("(binary file, %llu bytes)"), (long long)statbuf.st_size);
        gtk_list_store_set(g_ls_details, &iter,
                              DETAIL_COLUMN_NAME, (char *)name,
                              DETAIL_COLUMN_VALUE, msg,
                              //DETAIL_COLUMN_PATH, xasprintf("%s%s", g_dump_dir_name, name),
                              -1);
        free(msg);
    }
}

void update_gui_state_from_crash_data(void)
{
    gtk_window_set_title(GTK_WINDOW(g_assistant), g_dump_dir_name);

    const char *reason = get_crash_item_content_or_NULL(g_cd, FILENAME_REASON);
    gtk_label_set_text(g_lbl_cd_reason, reason ? reason : _("(no description)"));
///vda    make_label_autowrap_on_resize(g_lbl_cd_reason);

    gtk_list_store_clear(g_ls_details);
    struct cd_stats stats = { 0 };
    g_hash_table_foreach(g_cd, append_item_to_ls_details, &stats);
    char *msg = xasprintf(_("%llu bytes, %u files"), (long long)stats.filesize, stats.filecount);
    gtk_label_set_text(g_lbl_size, msg);
    free(msg);

    load_text_to_text_view(g_tv_backtrace, FILENAME_BACKTRACE);
    load_text_to_text_view(g_tv_comment, FILENAME_COMMENT);

    /* Update analyze radio buttons */
    event_gui_data_t *first_rb = add_event_buttons(g_box_analyzers, &g_list_analyzers,
                g_analyze_events, G_CALLBACK(analyze_rb_was_toggled),
                /*radio:*/ true, /*prev:*/ g_analyze_event_selected
    );
    /* Update the value of currently selected analyzer */
    if (first_rb)
    {
        free(g_analyze_event_selected);
        g_analyze_event_selected = xstrdup(first_rb->event_name);
    }

    /* Update reporter checkboxes */
    /* Remember names of selected reporters */
    GList *old_reporters = NULL;
    GList *li = g_list_reporters;
    for (; li; li = li->next)
    {
        event_gui_data_t *event_gui_data = li->data;
        if (gtk_toggle_button_get_active(event_gui_data->toggle_button) == TRUE)
        {
            /* order isn't important. prepend is faster */
            old_reporters = g_list_prepend(old_reporters, xstrdup(event_gui_data->event_name));
        }
    }
    /* Delete old checkboxes and create new ones */
    add_event_buttons(g_box_reporters, &g_list_reporters,
                g_report_events, /*callback:*/ G_CALLBACK(report_tb_was_toggled),
                /*radio:*/ false, /*prev:*/ NULL
    );
    /* Re-select new reporters which were selected before we deleted them */
    GList *li_new = g_list_reporters;
    for (; li_new; li_new = li_new->next)
    {
        event_gui_data_t *new_gui_data = li_new->data;
        GList *li_old = old_reporters;
        for (; li_old; li_old = li_old->next)
        {
            if (strcmp(new_gui_data->event_name, li_old->data) == 0)
            {
                gtk_toggle_button_set_active(new_gui_data->toggle_button, true);
                break;
            }
        }
    }
    list_free_with_free(old_reporters);

    /* Update readiness state of reporter selector page and "list of reporters" label  */
    report_tb_was_toggled(NULL, NULL);

    /* We can't just do gtk_widget_show_all once in main:
     * We created new widgets (buttons). Need to make them visible.
     */
    gtk_widget_show_all(GTK_WIDGET(g_assistant));

    if (g_reanalyze_events[0])
        gtk_widget_show(GTK_WIDGET(g_btn_refresh));
    else
        gtk_widget_hide(GTK_WIDGET(g_btn_refresh));
}


/* start_event_run */

struct analyze_event_data
{
    struct run_event_state *run_state;
    const char *event_name;
    GList *more_events;
    GList *env_list;
    GtkWidget *page_widget;
    GtkLabel *status_label;
    GtkTextView *tv_log;
    const char *end_msg;
    GIOChannel *channel;
    struct strbuf *event_log;
    int event_log_state;
    int fd;
    /*guint event_source_id;*/
};
enum {
    LOGSTATE_FIRSTLINE = 0,
    LOGSTATE_BEGLINE,
    LOGSTATE_ERRLINE,
    LOGSTATE_MIDLINE,
};

static int spawn_next_command_in_evd(struct analyze_event_data *evd)
{
    evd->env_list = export_event_config(evd->event_name);
    int r = spawn_next_command(evd->run_state, g_dump_dir_name, evd->event_name);
    if (r < 0)
    {
        unexport_event_config(evd->env_list);
        evd->env_list = NULL;
    }
    return r;
}

static void save_to_event_log(struct analyze_event_data *evd, const char *str)
{
    static const char delim[] = {
        [LOGSTATE_FIRSTLINE] = '>',
        [LOGSTATE_BEGLINE] = ' ',
        [LOGSTATE_ERRLINE] = '*',
    };

    while (str[0])
    {
        char *end = strchrnul(str, '\n');
        char end_char = *end;
        if (end_char == '\n')
            end++;
        switch (evd->event_log_state)
        {
            case LOGSTATE_FIRSTLINE:
            case LOGSTATE_BEGLINE:
            case LOGSTATE_ERRLINE:
                /* skip empty lines */
                if (str[0] == '\n')
                    goto next;
                strbuf_append_strf(evd->event_log, "%s%c %.*s",
                        iso_date_string(NULL),
                        delim[evd->event_log_state],
                        (int)(end - str), str
                );
                break;
            case LOGSTATE_MIDLINE:
                strbuf_append_strf(evd->event_log, "%.*s", (int)(end - str), str);
                break;
        }
        evd->event_log_state = LOGSTATE_MIDLINE;
        if (end_char != '\n')
            break;
        evd->event_log_state = LOGSTATE_BEGLINE;
 next:
        str = end;
    }
}

static void update_event_log_on_disk(const char *str)
{
    /* Load existing log */
    struct dump_dir *dd = dd_opendir(g_dump_dir_name, 0);
    if (!dd)
        return;
    char *event_log = dd_load_text_ext(dd, FILENAME_EVENT_LOG, DD_FAIL_QUIETLY_ENOENT);

    /* Append new log part to existing log */
    unsigned len = strlen(event_log);
    if (len != 0 && event_log[len - 1] != '\n')
        event_log = append_to_malloced_string(event_log, "\n");
    event_log = append_to_malloced_string(event_log, str);

    /* Trim log according to size watermarks */
    len = strlen(event_log);
    char *new_log = event_log;
    if (len > EVENT_LOG_HIGH_WATERMARK)
    {
        new_log += len - EVENT_LOG_LOW_WATERMARK;
        new_log = strchrnul(new_log, '\n');
        if (new_log[0])
            new_log++;
    }

    /* Save */
    dd_save_text(dd, FILENAME_EVENT_LOG, new_log);
    free(event_log);
    dd_close(dd);
}

static gboolean consume_cmd_output(GIOChannel *source, GIOCondition condition, gpointer data)
{
    struct analyze_event_data *evd = data;

    /* Read and insert the output into the log pane */
    char buf[257]; /* usually we get one line, no need to have big buf */
    int r;
    while ((r = read(evd->fd, buf, sizeof(buf)-1)) > 0)
    {
        buf[r] = '\0';
        append_to_textview(evd->tv_log, buf, r);
        save_to_event_log(evd, buf);
    }

    if (r < 0 && errno == EAGAIN)
        /* We got all buffered data, but fd is still open. Done for now */
        return TRUE; /* "please don't remove this event (yet)" */

    /* EOF/error */

    unexport_event_config(evd->env_list);
    evd->env_list = NULL;

    /* Wait for child to actually exit, collect status */
    int status;
    waitpid(evd->run_state->command_pid, &status, 0);
    int retval = WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        retval = WTERMSIG(status) + 128;

    /* Write a final message to the log */
    if (evd->event_log->len != 0 && evd->event_log->buf[evd->event_log->len - 1] != '\n')
        save_to_event_log(evd, "\n");
    /* If program failed, or if it finished successfully without saying anything... */
    if (retval != 0 || evd->event_log_state == LOGSTATE_FIRSTLINE)
    {
        if (retval != 0) /* If program failed, emit error line */
            evd->event_log_state = LOGSTATE_ERRLINE;
        char *msg;
        if (WIFSIGNALED(status))
            msg = xasprintf("(killed by signal %d)\n", WTERMSIG(status));
        else
            msg = xasprintf("(exited with %d)\n", retval);
        save_to_event_log(evd, msg);
        free(msg);
    }

    /* Append log to FILENAME_EVENT_LOG */
    update_event_log_on_disk(evd->event_log->buf);
    strbuf_clear(evd->event_log);
    evd->event_log_state = LOGSTATE_FIRSTLINE;

    if (geteuid() == 0)
    {
        /* Reset mode/uig/gid to correct values for all files created by event run */
        struct dump_dir *dd = dd_opendir(g_dump_dir_name, 0);
        if (dd)
        {
            dd_sanitize_mode_and_owner(dd);
            dd_close(dd);
        }
    }

    /* Inform abrt-gui that it is a good idea to rescan the directory */
    kill(getppid(), SIGCHLD);

    /* Stop if exit code is not 0, or no more commands */
    if (retval != 0
     || spawn_next_command_in_evd(evd) < 0
    ) {
        VERB1 log("done running event on '%s': %d", g_dump_dir_name, retval);
//append_to_textview(evd->tv_log, msg);

        for (;;)
        {
            if (!evd->more_events)
            {
                char *msg = xasprintf(evd->end_msg, retval);
                gtk_label_set_text(evd->status_label, msg);
                free(msg);
                /* Unfreeze assistant
                 * we can't allow user to continue if analyze action fails
                 * i.e: if gdb fails to generate backtrace
                */
                if (retval == 0 || (strncmp(evd->event_name, "analyze", strlen("analyze")) != 0))
                    gtk_assistant_set_page_complete(g_assistant, evd->page_widget, true);

                /*g_source_remove(evd->event_source_id);*/
                close(evd->fd);
                free_run_event_state(evd->run_state);
                strbuf_free(evd->event_log);
                free(evd);

                reload_crash_data_from_dump_dir();
                update_gui_state_from_crash_data();

                return FALSE; /* "please remove this event" */
            }

            evd->event_name = evd->more_events->data;
            evd->more_events = g_list_remove(evd->more_events, evd->more_events->data);

            if (prepare_commands(evd->run_state, g_dump_dir_name, evd->event_name) != 0
             && spawn_next_command_in_evd(evd) >= 0
            ) {
                VERB1 log("running event '%s' on '%s'", evd->event_name, g_dump_dir_name);
                break;
            }
            /* No commands needed?! (This is untypical) */
//TODO: msg?
//append_to_textview(evd->tv_log, msg);
        }
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
                GList *more_events,
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

    if (prepare_commands(state, g_dump_dir_name, event_name) == 0)
    {
 no_cmds:
        /* No commands needed?! (This is untypical) */
        free_run_event_state(state);
//TODO: better msg?
        char *msg = xasprintf(_("No processing for event '%s' is defined"), event_name);
        gtk_label_set_text(status_label, msg);
        free(msg);
        return;
    }

    struct dump_dir *dd = dd_opendir(g_dump_dir_name, DD_OPEN_READONLY);
    dd = steal_if_needed(dd);
    int locked = (dd && dd->locked);
    dd_close(dd);
    if (!locked)
        return; /* user refused to steal, or write error, etc... */

    GList *env_list = export_event_config(event_name);
    if (spawn_next_command(state, g_dump_dir_name, event_name) < 0)
    {
        unexport_event_config(env_list);
        goto no_cmds;
    }

    VERB1 log("running event '%s' on '%s'", event_name, g_dump_dir_name);

    /* At least one command is needed, and we started first one.
     * Hook its output fd to the main loop.
     */
    struct analyze_event_data *evd = xzalloc(sizeof(*evd));
    evd->run_state = state;
    evd->event_name = event_name;
    evd->more_events = more_events;
    evd->env_list = env_list;
    evd->page_widget = page;
    evd->status_label = status_label;
    evd->tv_log = tv_log;
    evd->end_msg = end_msg;
    evd->event_log = strbuf_new();
    evd->fd = state->command_out_fd;
    ndelay_on(evd->fd);
    evd->channel = g_io_channel_unix_new(evd->fd);
    /*evd->event_source_id = */ g_io_add_watch(evd->channel,
            G_IO_IN | G_IO_ERR | G_IO_HUP, /* need HUP to detect EOF w/o any data */
            consume_cmd_output,
            evd
    );

    gtk_label_set_text(status_label, start_msg);
//TODO: save_to_event_log(evd, "message that we run event foo")?

    /* Freeze assistant so it can't move away from the page until event run is done */
    gtk_assistant_set_page_complete(g_assistant, page, false);
}


/* Backtrace checkbox handling */

static void add_warning(const char *warning)
{
    char *label_str = xasprintf("• %s", warning);
    GtkWidget *warning_lbl = gtk_label_new(label_str);
    /* should be safe to free it, gtk calls strdup() to copy it */
    free(label_str);

    gtk_misc_set_alignment(GTK_MISC(warning_lbl), 0.0, 0.0);
    gtk_label_set_justify(GTK_LABEL(warning_lbl), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(g_box_warning_labels, warning_lbl, false, false, 0);
    gtk_widget_show(warning_lbl);
}

static void check_backtrace_and_allow_send(void) //TODO: rename, this checks rating, not backtrace
{
    bool send = true;
    bool warn = false;

    /* erase all warnings */
    gtk_widget_hide(g_widget_warnings_area);
    gtk_container_foreach(GTK_CONTAINER(g_box_warning_labels), &remove_child_widget, NULL);

    /*
     * FIXME: this should be bind to a reporter not to a compoment
     * but so far only oopses doesn't have rating, so for now we
     * skip the "kernel" manually
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
                warn = true;
                break;
            case '2':
            case '1':
                //FIXME: see CreporterAssistant: 394 for ideas
                add_warning(_("Reporting disabled because the backtrace is unusable."));
                send = false;
                warn = true;
                break;
        }
    }

    if (!gtk_toggle_button_get_active(g_tb_approve_bt))
    {
        add_warning(_("You should check the backtrace for sensitive data."));
        add_warning(_("You must agree with sending the backtrace."));
        send = false;
        warn = true;
    }

    gtk_assistant_set_page_complete(g_assistant,
                                    pages[PAGENO_BACKTRACE_APPROVAL].page_widget,
                                    send);
    if (warn)
        gtk_widget_show(g_widget_warnings_area);
}

static void on_bt_approve_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
    check_backtrace_and_allow_send();
}


/* Refresh button handling */

static void on_btn_refresh_clicked(GtkButton *button)
{
    if (g_reanalyze_events[0])
    {
        /* Save backtrace text if changed */
        save_text_from_text_view(g_tv_backtrace, FILENAME_BACKTRACE);

        g_analyze_events = append_to_malloced_string(g_analyze_events, g_reanalyze_events);
        g_reanalyze_events[0] = '\0';
        /* Refresh GUI so that we see new analyze+reanalyze buttons */
        update_gui_state_from_crash_data();

        /* Change page to analyzer selector - let user play with them */
        gtk_assistant_set_current_page(g_assistant, PAGENO_ANALYZE_SELECTOR);
    }
}


/* Page navigation handlers */

static void next_page(GtkAssistant *assistant, gpointer user_data)
{
    /* page_no is actually the previous page, because this
     * function is called before assistant goes to the next_page
     */
    int page_no = gtk_assistant_get_current_page(assistant);
    VERB2 log("page_no:%d", page_no);

    if (page_no == PAGENO_ANALYZE_SELECTOR
     && g_analyze_event_selected != NULL
    ) {
        start_event_run(g_analyze_event_selected,
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
        GList *reporters = NULL;
        GList *li = g_list_reporters;
        for (; li; li = li->next)
        {
            event_gui_data_t *event_gui_data = li->data;
            if (gtk_toggle_button_get_active(event_gui_data->toggle_button) == TRUE)
            {
                reporters = g_list_append(reporters, event_gui_data->event_name);
            }
        }
        if (reporters)
        {
            char *first_event_name = reporters->data;
            reporters = g_list_remove(reporters, reporters->data);
            start_event_run(first_event_name,
                    reporters,
                    pages[PAGENO_REPORT_PROGRESS].page_widget,
                    g_tv_report_log,
                    g_lbl_report_log,
                    _("Reporting..."),
                    _("Reporting finished with exit code %d")
            );
        }
    }
}

static void on_show_event_list_cb(GtkWidget *button, gpointer user_data)
{
    show_events_list_dialog(GTK_WINDOW(g_assistant));
}

static void on_page_prepare(GtkAssistant *assistant, GtkWidget *page, gpointer user_data)
{
    if (pages[PAGENO_BACKTRACE_APPROVAL].page_widget == page)
    {
        check_backtrace_and_allow_send();
    }

    /* Save text fields if changed */
    save_text_from_text_view(g_tv_backtrace, FILENAME_BACKTRACE);
    save_text_from_text_view(g_tv_comment, FILENAME_COMMENT);

    if (pages[PAGENO_SUMMARY].page_widget == page
     || pages[PAGENO_REPORT].page_widget == page
    ) {
        GtkWidget *w = GTK_WIDGET(g_tv_details);
        GtkContainer *c = GTK_CONTAINER(gtk_widget_get_parent(w));
        if (c)
            gtk_container_remove(c, w);
        gtk_container_add(pages[PAGENO_SUMMARY].page_widget == page ?
                        g_container_details1 : g_container_details2,
                w
        );
    }
}

static gint next_page_no(gint current_page_no, gpointer data)
{
 again:
    current_page_no++;
    switch (current_page_no)
    {
#if 0
    case PAGENO_COMMENT:
        if (get_crash_item_content_or_NULL(g_cd, FILENAME_COMMENT))
            break;
        goto again; /* no comment, skip next page */
#endif

    case PAGENO_BACKTRACE_APPROVAL:
        if (get_crash_item_content_or_NULL(g_cd, FILENAME_BACKTRACE))
            break;
        goto again; /* no backtrace, skip next page */

    case PAGENO_ANALYZE_SELECTOR:
        if (!g_analyze_events[0])
        {
            //TODO: if (!g_reporter_events[0]) /* no reporters available */ then what?
            return PAGENO_REPORTER_SELECTOR; /* skip analyze pages */
        }
        break;
    }

    VERB2 log("next page_no:%d", current_page_no);
    return current_page_no;
}


static gboolean highlight_search(gpointer user_data)
{
    GtkEntry *entry = GTK_ENTRY(user_data);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(g_tv_backtrace);
    GtkTextIter start_find;
    GtkTextIter end_find;
    GtkTextIter start_match;
    GtkTextIter end_match;
    int offset = 0;

    gtk_text_buffer_get_start_iter(buffer, &start_find);
    gtk_text_buffer_get_end_iter(buffer, &end_find);
    gtk_text_buffer_remove_tag_by_name(buffer, "search_result_bg", &start_find, &end_find);
    VERB1 log("searching: %s", gtk_entry_get_text(entry));
    while (gtk_text_iter_forward_search(&start_find, gtk_entry_get_text(entry),
                                 GTK_TEXT_SEARCH_TEXT_ONLY, &start_match,
                                 &end_match, NULL))
    {
        gtk_text_buffer_apply_tag_by_name(buffer, "search_result_bg",
                                          &start_match, &end_match);
        offset = gtk_text_iter_get_offset(&end_match);
        gtk_text_buffer_get_iter_at_offset(buffer, &start_find, offset);
    }

    //returning false will make glib to remove this event
    return false;
}

static void search_timeout(GtkEntry *entry)
{
    /* this little hack makes the search start searching after 500 milisec after
     * user stops writing into entry box
     * if this part is removed, then the search will be started on every
     * change of the search entry
     */
    if (g_timeout != 0)
        g_source_remove(g_timeout);
    g_timeout = g_timeout_add(500, &highlight_search, (gpointer)entry);
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

    g_tv_details_col2 = renderer = gtk_cell_renderer_text_new();
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

    int i;
    for (i = 0; page_names[i] != NULL; i++)
    {
        GtkWidget *page = GTK_WIDGET(gtk_builder_get_object(builder, page_names[i]));
        if (page == NULL)
            continue;

        pages[i].page_widget = page;

        gtk_assistant_append_page(g_assistant, page);
        /* If we set all pages to complete the wizard thinks there is nothing
         * to do and shows the button "Last" which allows user to skip all pages
         * so we need to set them all as incomplete and complete them one by one
         * on proper place - on_page_prepare() ?
         */
        gtk_assistant_set_page_complete(g_assistant, page, true);

        gtk_assistant_set_page_title(g_assistant, page, pages[i].title);
        gtk_assistant_set_page_type(g_assistant, page, pages[i].type);

        VERB1 log("added page: %s", page_names[i]);
    }
    /* Set pointers to objects we might need to work with */
    g_lbl_cd_reason        = GTK_LABEL(        gtk_builder_get_object(builder, "lbl_cd_reason"));
    g_box_analyzers        = GTK_BOX(          gtk_builder_get_object(builder, "vb_analyzers"));
    g_lbl_analyze_log      = GTK_LABEL(        gtk_builder_get_object(builder, "lbl_analyze_log"));
    g_tv_analyze_log       = GTK_TEXT_VIEW(    gtk_builder_get_object(builder, "tv_analyze_log"));
    g_box_reporters        = GTK_BOX(          gtk_builder_get_object(builder, "vb_reporters"));
    g_lbl_report_log       = GTK_LABEL(        gtk_builder_get_object(builder, "lbl_report_log"));
    g_tv_report_log        = GTK_TEXT_VIEW(    gtk_builder_get_object(builder, "tv_report_log"));
    g_tv_backtrace         = GTK_TEXT_VIEW(    gtk_builder_get_object(builder, "tv_backtrace"));
    g_tv_comment           = GTK_TEXT_VIEW(    gtk_builder_get_object(builder, "tv_comment"));
    g_tv_details           = GTK_TREE_VIEW(    gtk_builder_get_object(builder, "tv_details"));
    g_box_warning_labels   = GTK_BOX(          gtk_builder_get_object(builder, "box_warning_labels"));
    g_tb_approve_bt        = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "cb_approve_bt"));
    g_widget_warnings_area = GTK_WIDGET(       gtk_builder_get_object(builder, "box_warning_area"));
    g_btn_refresh          = GTK_BUTTON(       gtk_builder_get_object(builder, "btn_refresh"));
    g_search_entry_bt      = GTK_ENTRY(        gtk_builder_get_object(builder, "entry_search_bt"));
    g_container_details1   = GTK_CONTAINER(    gtk_builder_get_object(builder, "container_details1"));
    g_container_details2   = GTK_CONTAINER(    gtk_builder_get_object(builder, "container_details2"));
    g_lbl_reporters        = GTK_LABEL(        gtk_builder_get_object(builder, "lbl_reporters"));
    g_lbl_size             = GTK_LABEL(        gtk_builder_get_object(builder, "lbl_size"));

    make_label_autowrap_on_resize(g_lbl_cd_reason);
    gtk_widget_modify_font(GTK_WIDGET(g_tv_analyze_log), monospace_font);
    gtk_widget_modify_font(GTK_WIDGET(g_tv_report_log), monospace_font);
    gtk_widget_modify_font(GTK_WIDGET(g_tv_backtrace), monospace_font);

    ///* hide the warnings by default */
    //gtk_widget_hide(g_widget_warnings_area);

    //gtk_assistant_set_page_complete(g_assistant, pages[PAGENO_REPORTER_SELECTOR].page_widget, false);
    gtk_assistant_set_page_complete(g_assistant, pages[PAGENO_BACKTRACE_APPROVAL].page_widget,
                gtk_toggle_button_get_active(g_tb_approve_bt));

    /* configure btn on select analyzers page */
    GtkWidget *config_btn = GTK_WIDGET(gtk_builder_get_object(builder, "button_cfg1"));
    if (config_btn)
        g_signal_connect(G_OBJECT(config_btn), "clicked", G_CALLBACK(on_show_event_list_cb), NULL);

    /* configure btn on select reporters page */
    config_btn = GTK_WIDGET(gtk_builder_get_object(builder, "button_cfg2"));
    if (config_btn)
        g_signal_connect(G_OBJECT(config_btn), "clicked", G_CALLBACK(on_show_event_list_cb), NULL);
}

void create_assistant()
{
    monospace_font = pango_font_description_from_string("monospace");

    builder = gtk_builder_new();

    g_assistant = GTK_ASSISTANT(gtk_assistant_new());

    gtk_assistant_set_forward_page_func(g_assistant, next_page_no, NULL, NULL);

    GtkWindow *wnd_assistant = GTK_WINDOW(g_assistant);
    g_parent_window = wnd_assistant;
    gtk_window_set_default_size(wnd_assistant, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    /* set_default sets icon for every windows used in this app, so we don't
       have to set the icon for those windows manually
    */
    gtk_window_set_default_icon_name("abrt");

    GObject *obj_assistant = G_OBJECT(g_assistant);
    g_signal_connect(obj_assistant, "cancel", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(obj_assistant, "close", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(obj_assistant, "apply", G_CALLBACK(next_page), NULL);
    g_signal_connect(obj_assistant, "prepare", G_CALLBACK(on_page_prepare), NULL);

    add_pages();

    create_details_treeview();
    g_ls_details = gtk_list_store_new(DETAIL_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model(g_tv_details, GTK_TREE_MODEL(g_ls_details));

//    gtk_builder_connect_signals(builder, NULL);

    g_signal_connect(g_tb_approve_bt, "toggled", G_CALLBACK(on_bt_approve_toggle), NULL);
    g_signal_connect(g_btn_refresh, "clicked", G_CALLBACK(on_btn_refresh_clicked), NULL);
    g_signal_connect(g_tv_details, "row-activated", G_CALLBACK(tv_details_row_activated), NULL);
    /* [Enter] on a row: g_signal_connect(g_tv_details, "select-cursor-row", G_CALLBACK(tv_details_select_cursor_row), NULL); */
    g_signal_connect(g_tv_details, "cursor-changed", G_CALLBACK(tv_details_cursor_changed), NULL);

    /* init searching */
    GtkTextBuffer *backtrace_buf = gtk_text_view_get_buffer(g_tv_backtrace);
    /* found items background */
    gtk_text_buffer_create_tag(backtrace_buf, "search_result_bg", "background", "red", NULL);
    g_signal_connect(g_search_entry_bt, "changed", G_CALLBACK(search_timeout), NULL);

    gtk_assistant_set_page_complete(g_assistant,
                pages[PAGENO_BACKTRACE_APPROVAL].page_widget,
                gtk_toggle_button_get_active(g_tb_approve_bt)
    );
}
