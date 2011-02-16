#include <gtk/gtk.h>
#include "abrtlib.h"
#include "parse_options.h"
#include "abrt-gtk.h"

#define PROGNAME "abrt-gtk"

static char **s_dirs;

static void scan_directory_and_add_to_dirlist(const char *path)
{
    DIR *dp = opendir(path);
    if (!dp)
    {
        /* We don't want to yell if, say, $HOME/.abrt/spool doesn't exist */
        //perror_msg("Can't open directory '%s'", path);
        return;
    }

    struct dirent *dent;
    while ((dent = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue; /* skip "." and ".." */

        char *full_name = concat_path_file(path, dent->d_name);
        struct stat statbuf;
        if (stat(full_name, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
            add_directory_to_dirlist(full_name);
        free(full_name);
    }
    closedir(dp);
}

void scan_dirs_and_add_to_dirlist(void)
{
    char **argv = s_dirs;
    while (*argv)
	scan_directory_and_add_to_dirlist(*argv++);
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] [DIR]...\n\n"
        "Shows list of ABRT dump directories in specified DIR(s)\n"
        "(default DIRs: "DEBUG_DUMPS_DIR" $HOME/.abrt/spool)"
    );
    enum {
        OPT_v = 1 << 0,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    gtk_init(&argc, &argv);

    GtkWidget *main_window = create_main_window();

    const char *default_dirs[] = {
        "/var/spool/abrt",
        NULL,
        NULL,
    };
    argv += optind;
    if (!argv[0])
    {
        char *home = getenv("HOME");
        if (home)
            default_dirs[1] = concat_path_file(home, ".abrt/spool");
        argv = (char**)default_dirs;
    }
    s_dirs = argv;

    scan_dirs_and_add_to_dirlist();

    gtk_widget_show_all(main_window);

    sanitize_cursor(NULL);

    /* Prevent zombies when we spawn wizard */
    signal(SIGCHLD, SIG_IGN);

    /* Enter main loop */
    gtk_main();

    return 0;
}
