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
#include <sys/inotify.h>
#include "abrtlib.h"
#include "parse_options.h"
#include "abrt-gtk.h"
#if HAVE_LOCALE_H
# include <locale.h>
#endif

#define PROGNAME "abrt-gui"

static int inotify_fd = -1;
static GIOChannel *channel_inotify;
static int channel_inotify_event_id = -1;
static char **s_dirs;
static int s_signal_pipe[2];


static void rescan_and_refresh(void)
{
    GtkTreePath *old_path = get_cursor();
    //log("old_path1:'%s'", gtk_tree_path_to_string(old_path)); //leak

    rescan_dirs_and_add_to_dirlist();

    /* Try to restore cursor position */
    //log("old_path2:'%s'", gtk_tree_path_to_string(old_path)); //leak
    sanitize_cursor(old_path);
    if (old_path)
        gtk_tree_path_free(old_path);
}


static void handle_signal(int signo)
{
    int save_errno = errno;

    // Enable for debugging only, malloc/printf are unsafe in signal handlers
    //VERB3 log("Got signal %d", signo);

    uint8_t sig_caught = signo;
    if (write(s_signal_pipe[1], &sig_caught, 1))
        /* we ignore result, if() shuts up stupid compiler */;

    errno = save_errno;
}

static gboolean handle_signal_pipe(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused)
{
    /* It can only be SIGCHLD. Therefore we do not check the result,
     * and eat more than one byte at once: why refresh twice?
     */
    gchar buf[16];
    gsize bytes_read;
    g_io_channel_read(gio, buf, sizeof(buf), &bytes_read);

    /* Destroy zombies */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        continue;

    rescan_and_refresh();

    return TRUE; /* "please don't remove this event" */
}


static gboolean handle_inotify_cb(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused);

static void init_notify(void)
{
    VERB1 log("Initializing inotify");
    errno = 0;
    inotify_fd = inotify_init();
    if (inotify_fd < 0)
        perror_msg("inotify_init failed");
    else
    {
        close_on_exec_on(inotify_fd);
        VERB1 log("Adding inotify watch to glib main loop");
        channel_inotify = g_io_channel_unix_new(inotify_fd);
        channel_inotify_event_id = g_io_add_watch(channel_inotify,
                                                  G_IO_IN,
                                                  handle_inotify_cb,
                                                  NULL);
    }
}

static void close_notify(void)
{
    if (inotify_fd >= 0)
    {
        //VERB1 log("g_source_remove:");
        g_source_remove(channel_inotify_event_id);
        //VERB1 log("g_io_channel_unref:");
        g_io_channel_unref(channel_inotify);
        //VERB1 log("close(inotify_fd):");
        close(inotify_fd);
        inotify_fd = -1;
        //VERB1 log("Done");
    }
}

/* Inotify handler */
static gboolean handle_inotify_cb(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused)
{
    /* We don't bother reading inotify fd. We simply close and reopen it.
     * This happens rarely enough to not bother making it efficient.
     */
    close_notify();
    init_notify();

    rescan_and_refresh();

    return TRUE; /* "please don't remove this event" */
}

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

    if (inotify_fd >= 0 && inotify_add_watch(inotify_fd, path, 0
    //      | IN_ATTRIB         // Metadata changed
    //      | IN_CLOSE_WRITE    // File opened for writing was closed
            | IN_CREATE         // File/directory created in watched directory
            | IN_DELETE         // File/directory deleted from watched directory
            | IN_DELETE_SELF    // Watched file/directory was itself deleted
            | IN_MODIFY         // File was modified
            | IN_MOVE_SELF      // Watched file/directory was itself moved
            | IN_MOVED_FROM     // File moved out of watched directory
            | IN_MOVED_TO       // File moved into watched directory
    ) < 0)
    {
        perror_msg("inotify_add_watch failed on '%s'", path);
    }
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
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    /* without this the name is set to argv[0] which confuses
     * desktops which uses the name to find the corresponding .desktop file
     * trac#180
     */
    g_set_prgname("abrt");
    gtk_init(&argc, &argv);

    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-vp] [DIR]...\n"
        "\n"
        "Shows list of ABRT dump directories in specified DIR(s)\n"
        "(default DIRs: "DEBUG_DUMPS_DIR" $HOME/.abrt/spool)"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_p = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(   'p', NULL, NULL      , _("Add program names to log")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));
    if (opts & OPT_p)
    {
        msg_prefix = PROGNAME;
        putenv((char*)"ABRT_PROG_PREFIX=1");
    }

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

    init_notify();

    scan_dirs_and_add_to_dirlist();

    gtk_widget_show_all(main_window);

    sanitize_cursor(NULL);

    /* Set up signal pipe */
    xpipe(s_signal_pipe);
    close_on_exec_on(s_signal_pipe[0]);
    close_on_exec_on(s_signal_pipe[1]);
    ndelay_off(s_signal_pipe[0]);
    ndelay_off(s_signal_pipe[1]);
    signal(SIGCHLD, handle_signal);
    g_io_add_watch(g_io_channel_unix_new(s_signal_pipe[0]),
                G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
                handle_signal_pipe,
                NULL);

    /* Enter main loop */
    gtk_main();

    return 0;
}
