void create_assistant(void);
void update_gui_state_from_crash_data(void);
void show_error_as_msgbox(const char *msg);


extern char *g_glade_file;
extern char *g_dump_dir_name;
extern char *g_analyze_events;
extern char *g_reanalyze_events;
extern char *g_report_events;
extern crash_data_t *g_cd;
void reload_crash_data_from_dump_dir(void);
