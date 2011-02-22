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
GtkWidget *create_assistant(void);


extern char *g_glade_file;
extern crash_data_t *cd;
extern char *g_analyze_label_selected;
extern char *g_dump_dir_name;
