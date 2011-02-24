enum {
    PAGENO_SUMMARY,
    PAGENO_ANALYZE_SELECTOR,
    PAGENO_ANALYZE_PROGRESS,
    PAGENO_REPORTER_SELECTOR,
    PAGENO_BACKTRACE_APPROVAL,
    PAGENO_HOWTO,
    PAGENO_REPORT,
    PAGENO_REPORT_PROGRESS,
};
extern GtkAssistant *g_assistant;
extern GtkLabel *g_lbl_cd_reason;
extern GtkLabel *g_lbl_analyze_log;
extern GtkBox *g_box_analyzers;
extern GtkBox *g_box_reporters;
extern GtkTextView *g_tv_backtrace;
enum
{
    DETAIL_COLUMN_NAME,
    DETAIL_COLUMN_VALUE,
    //COLUMN_PATH,
    DETAIL_NUM_COLUMNS,
};
extern GtkTreeView *g_tv_details;
extern GtkListStore *g_ls_details;
void create_assistant(void);
void update_gui_state_from_crash_data(void);


extern char *g_glade_file;
extern char *g_dump_dir_name;
extern char *g_analyze_label_selected;
extern char *g_analyze_events;
extern char *g_report_events;
extern crash_data_t *g_cd;
void reload_crash_data_from_dump_dir(void);
