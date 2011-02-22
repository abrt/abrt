enum {
    PAGENO_ANALYZE_ACTION_SELECTOR = 0,
    PAGENO_ANALYZE_PROGRESS,
    PAGENO_REPORTER_SELECTOR,
    PAGENO_BACKTRACE_APPROVAL,
    PAGENO_HOWTO,
    PAGENO_SUMMARY,
    PAGENO_REPORT,
};
extern GtkLabel *g_lbl_cd_reason;
extern GtkLabel *g_lbl_analyze_log;
extern GtkBox *g_box_analyzers;
extern GtkBox *g_box_reporters;
extern GtkTextView *g_backtrace_tv;
enum
{
    COLUMN_NAME,
    COLUMN_VALUE,
    COLUMN_PATH,
    COLUMN_COUNT,
};
extern GtkTreeView *g_details_tv;
extern GtkListStore *g_details_ls;
GtkWidget *create_assistant(void);


extern char *g_glade_file;
extern char *g_dump_dir_name;
extern char *g_analyze_label_selected;
extern char *g_analyze_events;
extern char *g_report_events;
extern crash_data_t *g_cd;
void reload_dump_dir(void);
