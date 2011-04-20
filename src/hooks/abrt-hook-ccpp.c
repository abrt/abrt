/*
    abrt-hook-ccpp.cpp - the hook for C/C++ crashing program

    Copyright (C) 2009	Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009	RedHat inc.

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
#include "abrtlib.h"
#include <syslog.h>

static char* malloc_readlink(const char *linkname)
{
    char buf[PATH_MAX + 1];
    int len;

    len = readlink(linkname, buf, sizeof(buf)-1);
    if (len >= 0)
    {
        buf[len] = '\0';
        return xstrdup(buf);
    }
    return NULL;
}

/* Custom version of copyfd_xyz,
 * one which is able to write into two descriptors at once.
 */
#define CONFIG_FEATURE_COPYBUF_KB 4
static off_t copyfd_sparse(int src_fd, int dst_fd1, int dst_fd2, off_t size2)
{
	off_t total = 0;
	int last_was_seek = 0;
#if CONFIG_FEATURE_COPYBUF_KB <= 4
	char buffer[CONFIG_FEATURE_COPYBUF_KB * 1024];
	enum { buffer_size = sizeof(buffer) };
#else
	char *buffer;
	int buffer_size;

	/* We want page-aligned buffer, just in case kernel is clever
	 * and can do page-aligned io more efficiently */
	buffer = mmap(NULL, CONFIG_FEATURE_COPYBUF_KB * 1024,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANON,
			/* ignored: */ -1, 0);
	buffer_size = CONFIG_FEATURE_COPYBUF_KB * 1024;
	if (buffer == MAP_FAILED) {
		buffer = alloca(4 * 1024);
		buffer_size = 4 * 1024;
	}
#endif

	while (1) {
		ssize_t rd = safe_read(src_fd, buffer, buffer_size);
		if (!rd) { /* eof */
			if (last_was_seek) {
				if (lseek(dst_fd1, -1, SEEK_CUR) < 0
				 || safe_write(dst_fd1, "", 1) != 1
				 || (dst_fd2 >= 0
				     && (lseek(dst_fd2, -1, SEEK_CUR) < 0
					 || safe_write(dst_fd2, "", 1) != 1
				        )
				    )
				) {
					perror_msg("write error");
					total = -1;
					goto out;
				}
			}
			/* all done */
			goto out;
		}
		if (rd < 0) {
			perror_msg("read error");
			total = -1;
			goto out;
		}

		/* checking sparseness */
		ssize_t cnt = rd;
		while (--cnt >= 0) {
			if (buffer[cnt] != 0) {
				/* not sparse */
				errno = 0;
				ssize_t wr1 = full_write(dst_fd1, buffer, rd);
				ssize_t wr2 = (dst_fd2 >= 0 ? full_write(dst_fd2, buffer, rd) : rd);
				if (wr1 < rd || wr2 < rd) {
					perror_msg("write error");
					total = -1;
					goto out;
				}
				last_was_seek = 0;
				goto adv;
			}
		}
		/* sparse */
		xlseek(dst_fd1, rd, SEEK_CUR);
		if (dst_fd2 >= 0)
			xlseek(dst_fd2, rd, SEEK_CUR);
		last_was_seek = 1;
 adv:
		total += rd;
		size2 -= rd;
		if (size2 < 0)
			dst_fd2 = -1;
	}
 out:

#if CONFIG_FEATURE_COPYBUF_KB > 4
	if (buffer_size != 4 * 1024)
		munmap(buffer, buffer_size);
#endif
	return total;
}

static char* get_executable(pid_t pid, int *fd_p)
{
    char buf[sizeof("/proc/%lu/exe") + sizeof(long)*3];

    sprintf(buf, "/proc/%lu/exe", (long)pid);
    *fd_p = open(buf, O_RDONLY); /* might fail and return -1, it's ok */
    char *executable = malloc_readlink(buf);
    if (!executable)
        return NULL;
    /* find and cut off " (deleted)" from the path */
    char *deleted = executable + strlen(executable) - strlen(" (deleted)");
    if (deleted > executable && strcmp(deleted, " (deleted)") == 0)
    {
        *deleted = '\0';
        log("file %s seems to be deleted", executable);
    }
    /* find and cut off prelink suffixes from the path */
    char *prelink = executable + strlen(executable) - strlen(".#prelink#.XXXXXX");
    if (prelink > executable && strncmp(prelink, ".#prelink#.", strlen(".#prelink#.")) == 0)
    {
        log("file %s seems to be a prelink temporary file", executable);
        *prelink = '\0';
    }
    return executable;
}

static char* get_cwd(pid_t pid)
{
    char buf[sizeof("/proc/%lu/cwd") + sizeof(long)*3];

    sprintf(buf, "/proc/%lu/cwd", (long)pid);
    return malloc_readlink(buf);
}

/*
 * %s - signal number
 * %c - ulimit -c value
 * %p - pid
 * %u - uid
 * %g - gid
 * %t - UNIX time of dump
 * %h - hostname
 * %e - executable filename
 * %% - output one "%"
 */
/* Must match CORE_PATTERN order in daemon! */
static const char percent_specifiers[] = "%scpugthe";
static char *core_basename = (char*) "core";

static int open_user_core(const char *user_pwd, uid_t uid, pid_t pid, char **percent_values)
{
    struct passwd* pw = getpwuid(uid);
    gid_t gid = pw ? pw->pw_gid : uid;
    xsetegid(gid);
    xseteuid(uid);

    errno = 0;
    if (user_pwd == NULL
     || chdir(user_pwd) != 0
    ) {
        perror_msg("can't cd to %s", user_pwd);
        return -1;
    }

    if (strcmp(core_basename, "core") == 0)
    {
        /* Mimic "core.PID" if requested */
        char buf[] = "0\n";
        int fd = open("/proc/sys/kernel/core_uses_pid", O_RDONLY);
        if (fd >= 0)
        {
            read(fd, buf, sizeof(buf));
            close(fd);
        }
        if (strcmp(buf, "1\n") == 0)
        {
            core_basename = xasprintf("%s.%lu", core_basename, (long)pid);
        }
    }
    else
    {
        /* Expand old core pattern, put expanded name in core_basename */
        core_basename = xstrdup(core_basename);
        unsigned idx = 0;
        while (1)
        {
            char c = core_basename[idx];
            if (!c)
                break;
            idx++;
            if (c != '%')
                continue;

            /* We just copied %, look at following char and expand %c */
            c = core_basename[idx];
            unsigned specifier_num = strchrnul(percent_specifiers, c) - percent_specifiers;
            if (percent_specifiers[specifier_num] != '\0') /* valid %c (might be %% too) */
            {
                const char *val = "%";
                if (specifier_num > 0) /* not %% */
                    val = percent_values[specifier_num - 1];
                //log("c:'%c'", c);
                //log("val:'%s'", val);

                /* Replace %c at core_basename[idx] by its value */
                idx--;
                char *old = core_basename;
                core_basename = xasprintf("%.*s%s%s", idx, core_basename, val, core_basename + idx + 2);
                //log("pos:'%*s|'", idx, "");
                //log("new:'%s'", core_basename);
                //log("old:'%s'", old);
                free(old);
                idx += strlen(val);
            }
            /* else: invalid %c, % is already copied verbatim,
             * next loop iteration will copy c */
        }
    }

    /* Open (create) compat core file.
     * man core:
     * There are various circumstances in which a core dump file
     * is not produced:
     *
     * [skipped obvious ones]
     * The process does not have permission to write the core file.
     * ...if a file with the same name exists and is not writable
     * or is not a regular file (e.g., it is a directory or a symbolic link).
     *
     * A file with the same name already exists, but there is more
     * than one hard link to that file.
     *
     * The file system where the core dump file would be created is full;
     * or has run out of inodes; or is mounted read-only;
     * or the user has reached their quota for the file system.
     *
     * The RLIMIT_CORE or RLIMIT_FSIZE resource limits for the process
     * are set to zero.
     * [we check RLIMIT_CORE, but how can we check RLIMIT_FSIZE?]
     *
     * The binary being executed by the process does not have
     * read permission enabled. [how we can check it here?]
     *
     * The process is executing a set-user-ID (set-group-ID) program
     * that is owned by a user (group) other than the real
     * user (group) ID of the process. [TODO?]
     * (However, see the description of the prctl(2) PR_SET_DUMPABLE operation,
     * and the description of the /proc/sys/fs/suid_dumpable file in proc(5).)
     */
    /* Do not O_TRUNC: if later checks fail, we do not want to have file already modified here */
    struct stat sb;
    errno = 0;
    int user_core_fd = open(core_basename, O_WRONLY | O_CREAT | O_NOFOLLOW, 0600); /* kernel makes 0600 too */
    xsetegid(0);
    xseteuid(0);
    if (user_core_fd < 0
     || fstat(user_core_fd, &sb) != 0
     || !S_ISREG(sb.st_mode)
     || sb.st_nlink != 1
    /* kernel internal dumper checks this too: if (inode->i_uid != current->fsuid) <fail>, need to mimic? */
    ) {
        perror_msg("%s/%s is not a regular file with link count 1", user_pwd, core_basename);
        return -1;
    }
    if (ftruncate(user_core_fd, 0) != 0) {
        /* perror first, otherwise unlink may trash errno */
        perror_msg("truncate %s/%s", user_pwd, core_basename);
        unlink(core_basename);
        return -1;
    }

    return user_core_fd;
}

int main(int argc, char** argv)
{
    struct stat sb;

    if (argc < 10) /* no argv[9]? */
    {
        /* percent specifier:                %s    %c              %p  %u  %g  %t   %h       %e */
        /* argv:                 [0] [1]     [2]   [3]             [4] [5] [6] [7]  [8]      [9]         [10] */
      // [OLD_PATTERN] is deprecated, so removing it from help:
      //error_msg_and_die("Usage: %s DUMPDIR SIGNO CORE_SIZE_LIMIT PID UID GID TIME HOSTNAME BINARY_NAME [OLD_PATTERN]", argv[0]);
        error_msg_and_die("Usage: %s DUMPDIR SIGNO CORE_SIZE_LIMIT PID UID GID TIME HOSTNAME BINARY_NAME", argv[0]);
    }

    /* Not needed on 2.6.30.
     * At least 2.6.18 has a bug where
     * argv[1] = "DUMPDIR SIGNO CORE_SIZE_LIMIT ..."
     * argv[2] = "SIGNO CORE_SIZE_LIMIT ..."
     * and so on. Fixing it:
     */
    if (strchr(argv[1], ' '))
    {
        int i;
        for (i = 1; argv[i]; i++)
        {
            strchrnul(argv[i], ' ')[0] = '\0';
        }
    }

    openlog("abrt", LOG_PID, LOG_DAEMON);
    logmode = LOGMODE_SYSLOG;

    errno = 0;
    const char* dddir = argv[1];
    const char* signal_str = argv[2];
    int signal_no = xatoi_positive(signal_str);
    off_t ulimit_c = strtoull(argv[3], NULL, 10);
    if (ulimit_c < 0) /* unlimited? */
    {
        /* set to max possible >0 value */
        ulimit_c = ~((off_t)1 << (sizeof(off_t)*8-1));
    }
    pid_t pid = xatoi_positive(argv[4]);
    uid_t uid = xatoi_positive(argv[5]);
    if (errno || pid <= 0)
    {
        perror_msg_and_die("pid '%s' or limit '%s' is bogus", argv[4], argv[3]);
    }

    FILE *saved_core_pattern = fopen(VAR_RUN"/abrt/saved_core_pattern", "r");
    if (saved_core_pattern)
    {
        char *s = xmalloc_fgetline(saved_core_pattern);
        fclose(saved_core_pattern);
        /* If we have a saved pattern and it's not a "|PROG ARGS" thing... */
        if (s && s[0] != '|')
        {
            core_basename = s;
            argv[10] = NULL; /* don't use old way to pass OLD_PATTERN */
        }
    }
    if (argv[10]) /* OLD_PATTERN (deprecated) */
    {
        char *buf = (char*) xzalloc(strlen(argv[10]) / 2 + 2);
        char *end = hex2bin(buf, argv[10], strlen(argv[10]));
        if (end && end > buf && end[-1] == '\0')
        {
            core_basename = buf;
            //log("core_basename:'%s'", core_basename);
        }
        else
        {
            /* Until recently, kernels were truncating expanded core pattern.
             * In this case, we end up here...
             */
            error_msg("bad old pattern '%s', ignoring and using 'core'", argv[10]);
            /* core_basename = "core"; - already is */
            free(buf);
        }
    }

    int src_fd_binary;
    char *executable = get_executable(pid, &src_fd_binary);
    if (executable && strstr(executable, "/abrt-hook-ccpp"))
    {
        error_msg_and_die("pid %lu is '%s', not dumping it to avoid recursion",
                        (long)pid, executable);
    }

    char *user_pwd = get_cwd(pid); /* may be NULL on error */

    /* Parse abrt.conf */
    load_abrt_conf();
    free_abrt_conf_data(); /* can do this because we need only g_settings_nMaxCrashReportsSize */
    /* x1.25: go a bit up, so that usual in-daemon trimming
     * kicks in first, and we don't "fight" with it:
     */
    g_settings_nMaxCrashReportsSize += g_settings_nMaxCrashReportsSize / 4;
    /* ... and plugins/CCpp.conf */
    bool setting_MakeCompatCore;
    bool setting_SaveBinaryImage;
    {
        map_string_h *settings = new_map_string();
        load_conf_file(PLUGINS_CONF_DIR"/CCpp.conf", settings, /*skip key w/o values:*/ false);
        char *value;
        value = g_hash_table_lookup(settings, "MakeCompatCore");
        setting_MakeCompatCore = value && string_to_bool(value);
        value = g_hash_table_lookup(settings, "SaveBinaryImage");
        setting_SaveBinaryImage = value && string_to_bool(value);
        free_map_string(settings);
    }

    if (!setting_SaveBinaryImage && src_fd_binary >= 0)
    {
        close(src_fd_binary);
        src_fd_binary = -1;
    }

    /* Open a fd to compat coredump, if requested and is possible */
    int user_core_fd = -1;
    if (setting_MakeCompatCore && ulimit_c != 0)
        /* note: checks "user_pwd == NULL" inside; updates core_basename */
        user_core_fd = open_user_core(user_pwd, uid, pid, &argv[2]);

    if (executable == NULL)
    {
        /* readlink on /proc/$PID/exe failed, don't create abrt dump dir */
        error_msg("can't read /proc/%lu/exe link", (long)pid);
        goto create_user_core;
    }

    const char *signame = NULL;
    switch (signal_no)
    {
        case SIGILL : signame = "ILL" ; break;
        case SIGFPE : signame = "FPE" ; break;
        case SIGSEGV: signame = "SEGV"; break;
        case SIGBUS : signame = "BUS" ; break; //Bus error (bad memory access)
        case SIGABRT: signame = "ABRT"; break; //usually when abort() was called
    // We have real-world reports from users who see buggy programs
    // dying with SIGTRAP, uncommented it too:
        case SIGTRAP: signame = "TRAP"; break; //Trace/breakpoint trap
    // These usually aren't caused by bugs:
      //case SIGQUIT: signame = "QUIT"; break; //Quit from keyboard
      //case SIGSYS : signame = "SYS" ; break; //Bad argument to routine (SVr4)
      //case SIGXCPU: signame = "XCPU"; break; //CPU time limit exceeded (4.2BSD)
      //case SIGXFSZ: signame = "XFSZ"; break; //File size limit exceeded (4.2BSD)
        default: goto create_user_core; // not a signal we care about
    }

    if (!daemon_is_ok())
    {
        /* not an error, exit with exit code 0 */
        log("abrt daemon is not running. If it crashed, "
            "/proc/sys/kernel/core_pattern contains a stale value, "
            "consider resetting it to 'core'"
        );
        goto create_user_core;
    }

    if (g_settings_nMaxCrashReportsSize > 0)
    {
        g_settings_nMaxCrashReportsSize += g_settings_nMaxCrashReportsSize / 4;
        check_free_space(g_settings_nMaxCrashReportsSize);
    }

    char path[PATH_MAX];

    /* Check /var/spool/abrt/last-ccpp marker, do not dump repeated crashes
     * if they happen too often. Else, write new marker value.
     */
    snprintf(path, sizeof(path), "%s/last-ccpp", dddir);
    int fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd >= 0)
    {
        int sz;
        fstat(fd, &sb); /* !paranoia. this can't fail. */

        if (sb.st_size != 0 /* if it wasn't created by us just now... */
         && (unsigned)(time(NULL) - sb.st_mtime) < 20 /* and is relatively new [is 20 sec ok?] */
        ) {
            sz = read(fd, path, sizeof(path)-1); /* (ab)using path as scratch buf */
            if (sz > 0)
            {
                path[sz] = '\0';
                if (strcmp(executable, path) == 0)
                {
                    error_msg("not dumping repeating crash in '%s'", executable);
                    if (setting_MakeCompatCore)
                        goto create_user_core;
                    return 1;
                }
            }
            lseek(fd, 0, SEEK_SET);
        }
        sz = write(fd, executable, strlen(executable));
        if (sz >= 0)
            ftruncate(fd, sz);
        close(fd);
    }

    const char *last_slash = strrchr(executable, '/');
    if (last_slash && strncmp(++last_slash, "abrt", 4) == 0)
    {
        /* If abrtd/abrt-foo crashes, we don't want to create a _directory_,
         * since that can make new copy of abrtd to process it,
         * and maybe crash again...
         * Unlike dirs, mere files are ignored by abrtd.
         */
        snprintf(path, sizeof(path), "%s/%s-coredump", dddir, last_slash);
        int abrt_core_fd = xopen3(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        off_t core_size = copyfd_eof(STDIN_FILENO, abrt_core_fd, COPYFD_SPARSE);
        if (core_size < 0 || fsync(abrt_core_fd) != 0)
        {
            unlink(path);
            /* copyfd_eof logs the error including errno string,
             * but it does not log file name */
            error_msg_and_die("error saving coredump to %s", path);
        }
        log("saved core dump of pid %lu (%s) to %s (%llu bytes)", (long)pid, executable, path, (long long)core_size);
        return 0;
    }

    unsigned path_len = snprintf(path, sizeof(path), "%s/ccpp-%s-%lu.new",
            dddir, iso_date_string(NULL), (long)pid);
    if (path_len >= (sizeof(path) - sizeof("/"FILENAME_COREDUMP)))
        return 1;

    struct dump_dir *dd = dd_create(path, uid, 0640);
    if (dd)
    {
        dd_create_basic_files(dd, uid);

        char source_filename[sizeof("/proc/%lu/smaps") + sizeof(long)*3];
        int source_base_ofs = sprintf(source_filename, "/proc/%lu/smaps", (long)pid);
        source_base_ofs -= strlen("smaps");
        char *dest_filename = concat_path_file(dd->dd_dirname, FILENAME_SMAPS);
        char *dest_base = strrchr(dest_filename, '/') + 1;

        // Disabled for now: /proc/PID/smaps tends to be BIG,
        // and not much more informative than /proc/PID/maps:
        //copy_file(source_filename, dest_filename, 0640);
        //chown(dest_filename, dd->dd_uid, dd->dd_gid);

        strcpy(source_filename + source_base_ofs, "maps");
        strcpy(dest_base, FILENAME_MAPS);
        copy_file(source_filename, dest_filename, 0640);
        chown(dest_filename, dd->dd_uid, dd->dd_gid);

        free(dest_filename);

        dd_save_text(dd, FILENAME_ANALYZER, "CCpp");
        dd_save_text(dd, FILENAME_EXECUTABLE, executable);

        char *reason = xasprintf("Process %s was killed by signal %s (SIG%s)",
                                 executable, signal_str, signame ? signame : signal_str);
        dd_save_text(dd, FILENAME_REASON, reason);
        free(reason);

        char *cmdline = get_cmdline(pid);
        dd_save_text(dd, FILENAME_CMDLINE, cmdline ? : "");
        free(cmdline);

        char *environ = get_environ(pid);
        dd_save_text(dd, FILENAME_ENVIRON, environ ? : "");
        free(environ);

        if (src_fd_binary > 0)
        {
            strcpy(path + path_len, "/"FILENAME_BINARY);
            int dst_fd_binary = xopen3(path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            fchown(dst_fd_binary, dd->dd_uid, dd->dd_gid);
            off_t sz = copyfd_eof(src_fd_binary, dst_fd_binary, COPYFD_SPARSE);
            if (sz < 0 || fsync(dst_fd_binary) != 0)
            {
                unlink(path);
                error_msg_and_die("error saving binary image to %s", path);
            }
            close(dst_fd_binary);
            close(src_fd_binary);
        }

        strcpy(path + path_len, "/"FILENAME_COREDUMP);
        int abrt_core_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (abrt_core_fd < 0)
        {
            int sv_errno = errno;
            dd_delete(dd);
            if (user_core_fd >= 0)
            {
                xchdir(user_pwd);
                unlink(core_basename);
            }
            errno = sv_errno;
            perror_msg_and_die("can't open '%s'", path);
        }
        fchown(abrt_core_fd, dd->dd_uid, dd->dd_gid);

        /* We write both coredumps at once.
         * We can't write user coredump first, since it might be truncated
         * and thus can't be copied and used as abrt coredump;
         * and if we write abrt coredump first and then copy it as user one,
         * then we have a race when process exits but coredump does not exist yet:
         * $ echo -e '#include<signal.h>\nmain(){raise(SIGSEGV);}' | gcc -o test -x c -
         * $ rm -f core*; ulimit -c unlimited; ./test; ls -l core*
         * 21631 Segmentation fault (core dumped) ./test
         * ls: cannot access core*: No such file or directory <=== BAD
         */
//TODO: fchown abrt_core_fd to uid:abrt?
//Currently it is owned by 0:0 but is readable by anyone, so the owner
//of the crashed binary still can access it, as he has
//r-x access to the dump dir.
        off_t core_size = copyfd_sparse(STDIN_FILENO, abrt_core_fd, user_core_fd, ulimit_c);
        if (core_size < 0 || fsync(abrt_core_fd) != 0)
        {
            unlink(path);
            dd_delete(dd);
            if (user_core_fd >= 0)
            {
                xchdir(user_pwd);
                unlink(core_basename);
            }
            /* copyfd_sparse logs the error including errno string,
             * but it does not log file name */
            error_msg_and_die("error writing %s", path);
        }
        log("saved core dump of pid %lu (%s) to %s (%llu bytes)", (long)pid, executable, path, (long long)core_size);
        if (user_core_fd >= 0 && core_size >= ulimit_c)
        {
            /* user coredump is too big, nuke it */
            xchdir(user_pwd);
            unlink(core_basename);
        }

        /* We close dumpdir before we start catering for crash storm case.
         * Otherwise, delete_dump_dir's from other concurrent
         * CCpp's won't be able to delete our dump (their delete_dump_dir
         * will wait for us), and we won't be able to delete their dumps.
         * Classic deadlock.
         */
        dd_close(dd);
        path[path_len] = '\0'; /* path now contains only directory name */
        char *newpath = xstrndup(path, path_len - (sizeof(".new")-1));
        if (rename(path, newpath) == 0)
            strcpy(path, newpath);
        free(newpath);

        /* rhbz#539551: "abrt going crazy when crashing process is respawned" */
        if (g_settings_nMaxCrashReportsSize > 0)
        {
            trim_debug_dumps(DEBUG_DUMPS_DIR, g_settings_nMaxCrashReportsSize * (double)(1024*1024), path);
        }

        return 0;
    }

    /* We didn't create abrt dump, but may need to create compat coredump */
 create_user_core:
    if (user_core_fd < 0)
        return 0;

    off_t core_size = copyfd_size(STDIN_FILENO, user_core_fd, ulimit_c, COPYFD_SPARSE);
    if (core_size < 0 || fsync(user_core_fd) != 0) {
        /* perror first, otherwise unlink may trash errno */
        perror_msg("error writing %s/%s", user_pwd, core_basename);
        xchdir(user_pwd);
        unlink(core_basename);
        return 1;
    }
    if (core_size >= ulimit_c)
    {
        xchdir(user_pwd);
        unlink(core_basename);
        return 1;
    }
    log("saved core dump of pid %lu to %s/%s (%llu bytes)", (long)pid, user_pwd, core_basename, (long long)core_size);

    return 0;
}
