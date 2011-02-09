#include <gtk/gtk.h>

#include "abrtlib.h"
#include "abrt-gtk.h"



int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
    char *path = ".";

    //int optflags = 0;
    int opt;
    while ((opt = getopt(argc, argv, "c:d:v")) != -1)
    {
        switch (opt)
        {
        case 'c':
            VERB1 log("Loading settings from '%s'", optarg);
            //load_conf_file(optarg, settings, /*skip key w/o values:*/ true);
            VERB3 log("Loaded '%s'", optarg);
            break;
        case 'd':
            path = optarg;
            break;
        case 'v':
            g_verbose++;
            break;
        default:
            /* Careful: the string below contains tabs, dont replace with spaces */
            error_msg_and_die(
                "Usage: abrt-gtk -c CONFFILE -d DIR [-v]"
                "\n"
                "\nReport a crash to Bugzilla"
                "\n"
                "\nOptions:"
                "\n	-c FILE	Configuration file (may be given many times)"
                "\n	-d DIR	Crash dump directory"
                "\n	-v	Verbose"
                "\n	-s	Log to syslog"
            );
        }
    }

    if (argc > 1)
        path = argv[1];

    gtk_init(&argc, &argv);

    /* Prevent zombies when we spawn wizard */
    signal(SIGCHLD, SIG_IGN);

    GtkWidget *main_window = main_window_create();
    dump_list_hydrate(path);
    gtk_widget_show_all(main_window);

    /* Enter main loop */
    gtk_main();

    return 0;
}
