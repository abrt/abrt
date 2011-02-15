GtkWidget *create_main_window(void);
void add_directory_to_dirlist(const char *dirname);
void sanitize_cursor(GtkTreePath *preferred_path);

void scan_dirs_and_add_to_dirlist();
