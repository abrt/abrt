#include <gtk/gtk.h>
#include "abrtlib.h"
#include "abrt-gtk.h"

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");

    gtk_init(&argc, &argv);

    GtkWidget *main_window = main_window_create();

    const char *default_dirs[] = {
        "/var/spool/abrt",
        NULL,
        NULL,
    };
    argv++;
    if (!argv[0])
    {
        char *home = getenv("HOME");
        if (home)
            default_dirs[1] = concat_path_file(home, ".abrt/spool");
        argv = (char**)default_dirs;
    }
    while (*argv)
	dump_list_hydrate(*argv++);

    gtk_widget_show_all(main_window);

    /* Prevent zombies when we spawn wizard */
    signal(SIGCHLD, SIG_IGN);

    /* Enter main loop */
    gtk_main();

    return 0;
}
