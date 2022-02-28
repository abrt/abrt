/*
    Copyright (C) 2012  ABRT Team
    Copyright (C) 2012  Red Hat, Inc.

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
#include <sys/inotify.h>
#include <spawn.h>
#include "libabrt.h"

#define MAX_SCAN_BLOCK  (4*1024*1024)
#define READ_AHEAD          (10*1024)

extern char **environ;
static unsigned page_size;

static bool memstr(void *buf, unsigned size, const char *str)
{
    int len = strlen(str);
    while ((int)size >= len)
    {
        //log_warning("LOOKING FOR:'%s'", str);
        char *first = memchr(buf, (unsigned char)str[0], size - len + 1);
        if (!first)
            break;
        //log_warning("FOUND:'%.66s'", first);
        first++;
        if (len <= 1 || strncmp(first, str + 1, len - 1) == 0)
            return true;
        size -= (first - (char*)buf);
        //log_warning("SKIP TO:'%.66s' %d chars", first, (int)(first - (char*)buf));
        buf = first;
    }
    return false;
}

static void run_scanner_prog(int fd, struct stat *statbuf, GList *match_list, char **prog)
{
    pid_t pid;
    int err;
    int attr_set = 0, fd_actions_set = 0;
    posix_spawnattr_t attr;
    posix_spawn_file_actions_t fd_actions;

    /* fstat(fd, &statbuf) was just done by caller */

    off_t cur_pos = lseek(fd, 0, SEEK_CUR);
    if (statbuf->st_size <= cur_pos)
    {
        /* If file was truncated, treat it as a new file.
         * (changing inode# causes caller to think that file was closed or renamed)
         */
        if (statbuf->st_size < cur_pos)
            statbuf->st_ino++;
        return; /* we are at EOF, nothing to do */
    }

    log_info("File grew by %llu bytes, from %llu to %llu",
        (long long)(statbuf->st_size - cur_pos),
        (long long)(cur_pos),
        (long long)(statbuf->st_size));

    if (match_list && (statbuf->st_size - cur_pos) < MAX_SCAN_BLOCK)
    {
        size_t length = statbuf->st_size - cur_pos;

        off_t mapofs = cur_pos & ~(off_t)(page_size - 1);
        size_t maplen = statbuf->st_size - mapofs;
        void *map = mmap(NULL, maplen, PROT_READ, MAP_SHARED, fd, mapofs);

        if (map != MAP_FAILED)
        {
            char *start = (char*)map + (cur_pos & (page_size - 1));
            for (GList *l = match_list; l; l = l->next)
            {
                log_debug("Searching for '%s' in '%.*s'",
                                (char*)l->data,
                                length > 20 ? 20 : (int)length, start
                );
                if (memstr(start, length, (char*)l->data))
                {
                    log_debug("FOUND:'%s'", (char*)l->data);
                    goto found;
                }
            }
            /* None of the strings are found */
            log_debug("NOT FOUND");
            munmap(map, maplen);
            if (lseek(fd, statbuf->st_size, SEEK_SET) < 0)
                perror_msg_and_die("Could not seek to position in log file");
            return;
 found: ;
            munmap(map, maplen);
        }
    }

    fflush(NULL); /* paranoia */

    if ((err = posix_spawn_file_actions_init(&fd_actions)) != 0
#ifdef POSIX_SPAWN_USEVFORK
         || (err = posix_spawnattr_init(&attr)) != 0
         || (attr_set = 1,
             err = posix_spawnattr_setflags(&attr, POSIX_SPAWN_USEVFORK)) != 0
#endif
         || (fd_actions_set = 1,
             err = posix_spawn_file_actions_adddup2(&fd_actions, fd, STDIN_FILENO)) != 0)
    {
        if (attr_set == 1)
            posix_spawnattr_destroy(&attr);

        if (fd_actions_set == 1)
            posix_spawn_file_actions_destroy(&fd_actions);

        perror_msg_and_die("posix_spawn init");
    }

    if ((err = posix_spawnp(&pid, prog[0], &fd_actions, &attr, prog, environ)) != 0)
        perror_msg_and_die(_("Can't execute '%s'"), prog[0]);

    if ((err = posix_spawn_file_actions_destroy(&fd_actions)) != 0
         || ((attr_set == 1) && ((err = posix_spawnattr_destroy(&attr)) != 0)))
         perror_msg_and_die("posix_spawn destroy");

    libreport_safe_waitpid(pid, NULL, 0);

    /* Check fd's position, and move to end if it wasn't advanced.
     * This means that child failed to read its stdin.
     * This is not supposed to happen, so warn about it.
     */
    if (lseek(fd, 0, SEEK_CUR) <= cur_pos)
    {
        log_warning("Warning, '%s' did not process its input", prog[0]);
        lseek(fd, statbuf->st_size, SEEK_SET);
    }
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    page_size = sysconf(_SC_PAGE_SIZE);

    GList *match_list = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-vs] [-F STR]... FILE PROG [ARGS]\n"
        "\n"
        "Watch log file FILE, run PROG when it grows or is replaced"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_s = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&libreport_g_verbose),
        OPT_BOOL('s', NULL, NULL              , _("Log to syslog")),
        OPT_LIST('F', NULL, &match_list, "STR", _("Don't run PROG if STRs aren't found")),
        OPT_END()
    };
    unsigned opts = libreport_parse_opts(argc, argv, program_options, program_usage_string);

    libreport_export_abrt_envvars(0);

    libreport_msg_prefix = libreport_g_progname;
    if ((opts & OPT_s) || getenv("ABRT_SYSLOG"))
    {
        libreport_logmode = LOGMODE_JOURNAL;
    }

    argv += optind;
    if (!argv[0] || !argv[1])
        libreport_show_usage_and_die(program_usage_string, program_options);

    /* We want to support -F "`echo foo; echo bar`" -
     * need to split strings by newline, and be careful about
     * possible last empty string: "foo\nbar\n" = "foo", "bar",
     * NOT "foo", "bar", ""!
     */
    for (GList *l = match_list; l; l = l->next)
    {
        char *eol = strchr((char*)l->data, '\n');
        if (!eol)
            continue;
        *eol++ = '\0';
        if (!*eol)
            continue;
        l = g_list_append(l, eol); /* in fact, always returns unchanged l */
    }

    const char *filename = *argv++;

    int inotify_fd = inotify_init();
    if (inotify_fd == -1)
        perror_msg_and_die("inotify_init failed");
    libreport_close_on_exec_on(inotify_fd);

    struct stat statbuf;
    int file_fd = -1;
    int wd = -1;

    while (1)
    {
        /* If file is already opened, scan it from current pos */
        if (file_fd >= 0)
        {
            memset(&statbuf, 0, sizeof(statbuf));
            if (fstat(file_fd, &statbuf) != 0)
                goto close_fd;
            run_scanner_prog(file_fd, &statbuf, match_list, argv);

            /* Was file deleted or replaced? */
            ino_t fd_ino = statbuf.st_ino;
            if (stat(filename, &statbuf) != 0 || statbuf.st_ino != fd_ino) /* yes */
            {
                log_info("Inode# changed, closing fd");
 close_fd:
                close(file_fd);
                if (wd >= 0)
                    inotify_rm_watch(inotify_fd, wd);
                file_fd = -1;
                wd = -1;
            }
        }

        /* If file isn't opened, try to open it and scan */
        if (file_fd < 0)
        {
            file_fd = open(filename, O_RDONLY);
            if (file_fd >= 0)
            {
                log_info("Opened '%s'", filename);
                /* For -w case, if we don't have inotify watch yet, open one */
                if (wd < 0)
                {
                    wd = inotify_add_watch(inotify_fd, filename, IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF);
                    if (wd < 0)
                        perror_msg("inotify_add_watch failed on '%s'", filename);
                    else
                        log_info("Added inotify watch for '%s'", filename);
                }
                if (fstat(file_fd, &statbuf) == 0)
                {
                    /* If file is large, skip the beginning.
                     * IOW: ignore old log messages because they are unlikely
                     * to have sufficiently recent data to be useful.
                     */
                    if (statbuf.st_size > (MAX_SCAN_BLOCK - READ_AHEAD))
                        lseek(file_fd, statbuf.st_size - (MAX_SCAN_BLOCK - READ_AHEAD), SEEK_SET);
                    /* Note that statbuf is filled by fstat by now,
                     * run_scanner_prog needs that
                     */
                    run_scanner_prog(file_fd, &statbuf, match_list, argv);
                }
            }
        }

        /* Even if log file grows all the time, say, a new line every 5 ms,
         * we don't want to scan it all the time. Sleep a bit and let it grow
         * in bigger increments.
         * Sleep longer if file does not exist.
         */
        sleep(file_fd >= 0 ? 1 : 59);

        /* Now wait for it to change, be moved or deleted */
        if (wd >= 0)
        {
            char buf[4096];
            log_debug("Waiting for '%s' to change", filename);
            /* We block here: */
            int len = read(inotify_fd, buf, sizeof(buf));
            if (len < 0 && errno != EINTR) /* I saw EINTR here on strace attach */
                perror_msg("Error reading inotify fd");
            /* we don't actually check what happened to file -
             * the code will handle all possibilities.
             */
            log_debug("Change in '%s' detected", filename);
            /* Let them finish writing to the log file. otherwise
             * we may end up trying to analyze partial oops.
             */
            sleep(1);
        }

    } /* while (1) */

    return 0;
}
