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
#include <sys/utsname.h>
#include "libabrt.h"

#ifdef ENABLE_DUMP_TIME_UNWIND
#include <satyr/abrt.h>
#include <satyr/utils.h>
#endif /* ENABLE_DUMP_TIME_UNWIND */


/* I want to use -Werror, but gcc-4.4 throws a curveball:
 * "warning: ignoring return value of 'ftruncate', declared with attribute warn_unused_result"
 * and (void) cast is not enough to shut it up! Oh God...
 */
#define IGNORE_RESULT(func_call) do { if (func_call) /* nothing */; } while (0)

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
					perror_msg("Write error");
					total = -1;
					goto out;
				}
			}
			/* all done */
			goto out;
		}
		if (rd < 0) {
			perror_msg("Read error");
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
					perror_msg("Write error");
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
//TODO: truncate to 0 or even delete the second file
//(currently we delete the file later)
	}
 out:

#if CONFIG_FEATURE_COPYBUF_KB > 4
	if (buffer_size != 4 * 1024)
		munmap(buffer, buffer_size);
#endif
	return total;
}


/* Global data */
static char *user_pwd;
static struct dump_dir *dd;

/*
 * %s - signal number
 * %c - ulimit -c value
 * %p - pid
 * %u - uid
 * %g - gid
 * %t - UNIX time of dump
 * %e - executable filename
 * %i - crash thread tid
 * %% - output one "%"
 */
/* Hook must be installed with exactly the same sequence of %c specifiers.
 * Last one, %h, may be omitted (we can find it out).
 */
static const char percent_specifiers[] = "%scpugtei";
static char *core_basename = (char*) "core";
/*
 * Used for error messages only.
 * It is either the same as core_basename if it is absolute,
 * or $PWD/core_basename.
 */
static char *full_core_basename;

static int open_user_core(uid_t uid, uid_t fsuid, pid_t pid, char **percent_values)
{
    errno = 0;
    if (user_pwd == NULL
     || chdir(user_pwd) != 0
    ) {
        perror_msg("Can't cd to '%s'", user_pwd);
        return -1;
    }

    struct passwd* pw = getpwuid(uid);
    gid_t gid = pw ? pw->pw_gid : uid;
    //log("setting uid: %i gid: %i", uid, gid);
    xsetegid(gid);
    xseteuid(fsuid);

    if (strcmp(core_basename, "core") == 0)
    {
        /* Mimic "core.PID" if requested */
        char buf[] = "0\n";
        int fd = open("/proc/sys/kernel/core_uses_pid", O_RDONLY);
        if (fd >= 0)
        {
            IGNORE_RESULT(read(fd, buf, sizeof(buf)));
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

    full_core_basename = core_basename;
    if (core_basename[0] != '/')
        core_basename = concat_path_file(user_pwd, core_basename);

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
    struct stat sb;
    errno = 0;
    /* Do not O_TRUNC: if later checks fail, we do not want to have file already modified here */
    int user_core_fd = open(core_basename, O_WRONLY | O_CREAT | O_NOFOLLOW, 0600); /* kernel makes 0600 too */
    xsetegid(0);
    xseteuid(0);
    if (user_core_fd < 0
     || fstat(user_core_fd, &sb) != 0
     || !S_ISREG(sb.st_mode)
     || sb.st_nlink != 1
    /* kernel internal dumper checks this too: if (inode->i_uid != current->fsuid) <fail>, need to mimic? */
    ) {
        if (user_core_fd < 0)
            perror_msg("Can't open '%s'", full_core_basename);
        else
            perror_msg("'%s' is not a regular file with link count 1", full_core_basename);
        return -1;
    }
    if (ftruncate(user_core_fd, 0) != 0) {
        /* perror first, otherwise unlink may trash errno */
        perror_msg("Can't truncate '%s' to size 0", full_core_basename);
        unlink(core_basename);
        return -1;
    }

    return user_core_fd;
}

/* Like xopen, but on error, unlocks and deletes dd and user core */
static int create_or_die(const char *filename, int user_core_fd)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, DEFAULT_DUMP_DIR_MODE);
    if (fd >= 0)
    {
        IGNORE_RESULT(fchown(fd, dd->dd_uid, dd->dd_gid));
        return fd;
    }

    int sv_errno = errno;
    if (dd)
        dd_delete(dd);
    if (user_core_fd >= 0)
    {
        xchdir(user_pwd);
        unlink(core_basename);
    }
    errno = sv_errno;
    perror_msg_and_die("Can't open '%s'", filename);
}

static void create_core_backtrace(pid_t tid, const char *executable, int signal_no, const char *dd_path)
{
#ifdef ENABLE_DUMP_TIME_UNWIND
    if (g_verbose > 1)
        sr_debug_parser = true;

    char *error_message = NULL;
    bool success = sr_abrt_create_core_stacktrace_from_core_hook(dd_path, tid, executable,
                                                                 signal_no, &error_message);

    if (!success)
    {
        log("Failed to create core_backtrace: %s", error_message);
        free(error_message);
    }
#endif /* ENABLE_DUMP_TIME_UNWIND */
}

static int create_user_core(int user_core_fd, pid_t pid, off_t ulimit_c)
{
    if (user_core_fd >= 0)
    {
        off_t core_size = copyfd_size(STDIN_FILENO, user_core_fd, ulimit_c, COPYFD_SPARSE);
        if (fsync(user_core_fd) != 0 || close(user_core_fd) != 0 || core_size < 0)
        {
            /* perror first, otherwise unlink may trash errno */
            perror_msg("Error writing '%s'", full_core_basename);
            xchdir(user_pwd);
            unlink(core_basename);
            return 1;
        }
        if (ulimit_c == 0 || core_size > ulimit_c)
        {
            xchdir(user_pwd);
            unlink(core_basename);
            return 1;
        }
        log_notice("Saved core dump of pid %lu to %s (%llu bytes)", (long)pid, full_core_basename, (long long)core_size);
    }

    return 0;
}

static int test_configuration(bool setting_SaveFullCore, bool setting_CreateCoreBacktrace)
{
    if (!setting_SaveFullCore && !setting_CreateCoreBacktrace)
    {
        fprintf(stderr, "Both SaveFullCore and CreateCoreBacktrace are disabled - "
                        "at least one of them is needed for useful report.\n");
        return 1;
    }

    return 0;
}

int save_crashing_binary(pid_t pid, const char *dest_path, uid_t uid, gid_t gid)
{
    char buf[sizeof("/proc/%lu/exe") + sizeof(long)*3];

    sprintf(buf, "/proc/%lu/exe", (long)pid);
    int src_fd_binary = open(buf, O_RDONLY); /* might fail and return -1, it's ok */
    if (src_fd_binary < 0)
    {
        log_notice("Failed to open an image of crashing binary");
        return 0;
    }

    int dst_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, DEFAULT_DUMP_DIR_MODE);
    if (dst_fd < 0)
    {
        log_notice("Failed to create file '%s'", dest_path);
        close(src_fd_binary);
        return -1;
    }

    IGNORE_RESULT(fchown(dst_fd, uid, gid));

    off_t sz = copyfd_eof(src_fd_binary, dst_fd, COPYFD_SPARSE);
    close(src_fd_binary);

    return fsync(dst_fd) != 0 || close(dst_fd) != 0 || sz < 0;
}

int main(int argc, char** argv)
{
    /* Kernel starts us with all fd's closed.
     * But it's dangerous:
     * fprintf(stderr) can dump messages into random fds, etc.
     * Ensure that if any of fd 0,1,2 is closed, we open it to /dev/null.
     */
    int fd = xopen("/dev/null", O_RDWR);
    while (fd < 2)
        fd = xdup(fd);
    if (fd > 2)
        close(fd);

    logmode = LOGMODE_JOURNAL;

    /* Parse abrt.conf */
    load_abrt_conf();
    /* ... and plugins/CCpp.conf */
    bool setting_MakeCompatCore;
    bool setting_SaveBinaryImage;
    bool setting_SaveFullCore;
    bool setting_CreateCoreBacktrace;
    {
        map_string_t *settings = new_map_string();
        load_abrt_plugin_conf_file("CCpp.conf", settings);
        const char *value;
        value = get_map_string_item_or_NULL(settings, "MakeCompatCore");
        setting_MakeCompatCore = value && string_to_bool(value);
        value = get_map_string_item_or_NULL(settings, "SaveBinaryImage");
        setting_SaveBinaryImage = value && string_to_bool(value);
        value = get_map_string_item_or_NULL(settings, "SaveFullCore");
        setting_SaveFullCore = value ? string_to_bool(value) : true;
        value = get_map_string_item_or_NULL(settings, "CreateCoreBacktrace");
        setting_CreateCoreBacktrace = value ? string_to_bool(value) : true;
        value = get_map_string_item_or_NULL(settings, "VerboseLog");
        if (value)
            g_verbose = xatoi_positive(value);
        free_map_string(settings);
    }

    if (argc == 2 && strcmp(argv[1], "--config-test"))
        return test_configuration(setting_SaveFullCore, setting_CreateCoreBacktrace);

    if (argc < 8)
    {
        /* percent specifier:         %s   %c              %p  %u  %g  %t   %e          %i */
        /* argv:                  [0] [1]  [2]             [3] [4] [5] [6]  [7]         [8]*/
        error_msg_and_die("Usage: %s SIGNO CORE_SIZE_LIMIT PID UID GID TIME BINARY_NAME [TID]", argv[0]);
    }

    /* Not needed on 2.6.30.
     * At least 2.6.18 has a bug where
     * argv[1] = "SIGNO CORE_SIZE_LIMIT PID ..."
     * argv[2] = "CORE_SIZE_LIMIT PID ..."
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

    errno = 0;
    const char* signal_str = argv[1];
    int signal_no = xatoi_positive(signal_str);
    off_t ulimit_c = strtoull(argv[2], NULL, 10);
    if (ulimit_c < 0) /* unlimited? */
    {
        /* set to max possible >0 value */
        ulimit_c = ~((off_t)1 << (sizeof(off_t)*8-1));
    }
    const char *pid_str = argv[3];
    pid_t pid = xatoi_positive(argv[3]);
    uid_t uid = xatoi_positive(argv[4]);
    if (errno || pid <= 0)
    {
        perror_msg_and_die("PID '%s' or limit '%s' is bogus", argv[3], argv[2]);
    }

    {
        char *s = xmalloc_fopen_fgetline_fclose(VAR_RUN"/abrt/saved_core_pattern");
        /* If we have a saved pattern and it's not a "|PROG ARGS" thing... */
        if (s && s[0] != '|')
            core_basename = s;
        else
            free(s);
    }

    pid_t tid = 0;
    if (argv[8])
    {
        tid = xatoi_positive(argv[8]);
    }

    char path[PATH_MAX];

    char *executable = get_executable(pid);
    if (executable && strstr(executable, "/abrt-hook-ccpp"))
    {
        error_msg_and_die("PID %lu is '%s', not dumping it to avoid recursion",
                        (long)pid, executable);
    }

    user_pwd = get_cwd(pid); /* may be NULL on error */
    log_notice("user_pwd:'%s'", user_pwd);

    sprintf(path, "/proc/%lu/status", (long)pid);
    char *proc_pid_status = xmalloc_xopen_read_close(path, /*maxsz:*/ NULL);

    uid_t fsuid = uid;
    uid_t tmp_fsuid = get_fsuid(proc_pid_status);
    if (tmp_fsuid < 0)
        perror_msg_and_die("Can't parse 'Uid: line' in /proc/%lu/status", (long)pid);

    int suid_policy = dump_suid_policy();
    if (tmp_fsuid != uid)
    {
        /* use root for suided apps unless it's explicitly set to UNSAFE */
        fsuid = 0;
        if (suid_policy == DUMP_SUID_UNSAFE)
        {
            fsuid = tmp_fsuid;
        }
    }

    /* Open a fd to compat coredump, if requested and is possible */
    int user_core_fd = -1;
    if (setting_MakeCompatCore && ulimit_c != 0)
        /* note: checks "user_pwd == NULL" inside; updates core_basename */
        user_core_fd = open_user_core(uid, fsuid, pid, &argv[1]);

    if (executable == NULL)
    {
        /* readlink on /proc/$PID/exe failed, don't create abrt dump dir */
        error_msg("Can't read /proc/%lu/exe link", (long)pid);
        return create_user_core(user_core_fd, pid, ulimit_c);
    }

    const char *signame = NULL;
    if (!signal_is_fatal(signal_no, &signame))
        return create_user_core(user_core_fd, pid, ulimit_c); // not a signal we care about

    if (!daemon_is_ok())
    {
        /* not an error, exit with exit code 0 */
        log("abrtd is not running. If it crashed, "
            "/proc/sys/kernel/core_pattern contains a stale value, "
            "consider resetting it to 'core'"
        );
        return create_user_core(user_core_fd, pid, ulimit_c);
    }

    if (g_settings_nMaxCrashReportsSize > 0)
    {
        /* If free space is less than 1/4 of MaxCrashReportsSize... */
        if (low_free_space(g_settings_nMaxCrashReportsSize, g_settings_dump_location))
            return create_user_core(user_core_fd, pid, ulimit_c);
    }

    /* Check /var/tmp/abrt/last-ccpp marker, do not dump repeated crashes
     * if they happen too often. Else, write new marker value.
     */
    snprintf(path, sizeof(path), "%s/last-ccpp", g_settings_dump_location);
    if (check_recent_crash_file(path, executable))
    {
        /* It is a repeating crash */
        return create_user_core(user_core_fd, pid, ulimit_c);
    }

    const char *last_slash = strrchr(executable, '/');
    if (last_slash && strncmp(++last_slash, "abrt", 4) == 0)
    {
        /* If abrtd/abrt-foo crashes, we don't want to create a _directory_,
         * since that can make new copy of abrtd to process it,
         * and maybe crash again...
         * Unlike dirs, mere files are ignored by abrtd.
         */
        snprintf(path, sizeof(path), "%s/%s-coredump", g_settings_dump_location, last_slash);
        int abrt_core_fd = xopen3(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        off_t core_size = copyfd_eof(STDIN_FILENO, abrt_core_fd, COPYFD_SPARSE);
        if (core_size < 0 || fsync(abrt_core_fd) != 0)
        {
            unlink(path);
            /* copyfd_eof logs the error including errno string,
             * but it does not log file name */
            error_msg_and_die("Error saving '%s'", path);
        }
        log_notice("Saved core dump of pid %lu (%s) to %s (%llu bytes)", (long)pid, executable, path, (long long)core_size);
        return 0;
    }

    unsigned path_len = snprintf(path, sizeof(path), "%s/ccpp-%s-%lu.new",
            g_settings_dump_location, iso_date_string(NULL), (long)pid);
    if (path_len >= (sizeof(path) - sizeof("/"FILENAME_COREDUMP)))
    {
        return create_user_core(user_core_fd, pid, ulimit_c);
    }

    /* use fsuid instead of uid, so we don't expose any sensitive
     * information of suided app in /var/tmp/abrt
     */
    dd = dd_create(path, fsuid, DEFAULT_DUMP_DIR_MODE);
    if (dd)
    {
        char *rootdir = get_rootdir(pid);

        dd_create_basic_files(dd, fsuid, (rootdir && strcmp(rootdir, "/") != 0) ? rootdir : NULL);

        char source_filename[sizeof("/proc/%lu/somewhat_long_name") + sizeof(long)*3];
        int source_base_ofs = sprintf(source_filename, "/proc/%lu/smaps", (long)pid);
        source_base_ofs -= strlen("smaps");
        char *dest_filename = concat_path_file(dd->dd_dirname, "also_somewhat_longish_name");
        char *dest_base = strrchr(dest_filename, '/') + 1;

        // Disabled for now: /proc/PID/smaps tends to be BIG,
        // and not much more informative than /proc/PID/maps:
        //copy_file(source_filename, dest_filename, 0640);
        //chown(dest_filename, dd->dd_uid, dd->dd_gid);

        strcpy(source_filename + source_base_ofs, "maps");
        strcpy(dest_base, FILENAME_MAPS);
        copy_file(source_filename, dest_filename, DEFAULT_DUMP_DIR_MODE);
        IGNORE_RESULT(chown(dest_filename, dd->dd_uid, dd->dd_gid));

        strcpy(source_filename + source_base_ofs, "limits");
        strcpy(dest_base, FILENAME_LIMITS);
        copy_file(source_filename, dest_filename, DEFAULT_DUMP_DIR_MODE);
        IGNORE_RESULT(chown(dest_filename, dd->dd_uid, dd->dd_gid));

        strcpy(source_filename + source_base_ofs, "cgroup");
        strcpy(dest_base, FILENAME_CGROUP);
        copy_file(source_filename, dest_filename, DEFAULT_DUMP_DIR_MODE);
        IGNORE_RESULT(chown(dest_filename, dd->dd_uid, dd->dd_gid));

        strcpy(dest_base, FILENAME_OPEN_FDS);
        if (dump_fd_info(dest_filename, source_filename, source_base_ofs))
            IGNORE_RESULT(chown(dest_filename, dd->dd_uid, dd->dd_gid));

        free(dest_filename);

        dd_save_text(dd, FILENAME_ANALYZER, "CCpp");
        dd_save_text(dd, FILENAME_TYPE, "CCpp");
        dd_save_text(dd, FILENAME_EXECUTABLE, executable);
        dd_save_text(dd, FILENAME_PID, pid_str);
        dd_save_text(dd, FILENAME_PROC_PID_STATUS, proc_pid_status);
        if (user_pwd)
            dd_save_text(dd, FILENAME_PWD, user_pwd);
        if (rootdir)
        {
            if (strcmp(rootdir, "/") != 0)
                dd_save_text(dd, FILENAME_ROOTDIR, rootdir);
        }

        char *reason = xasprintf("%s killed by SIG%s",
                                 last_slash, signame ? signame : signal_str);
        dd_save_text(dd, FILENAME_REASON, reason);
        free(reason);

        char *cmdline = get_cmdline(pid);
        dd_save_text(dd, FILENAME_CMDLINE, cmdline ? : "");
        free(cmdline);

        char *environ = get_environ(pid);
        dd_save_text(dd, FILENAME_ENVIRON, environ ? : "");
        free(environ);

        char *fips_enabled = xmalloc_fopen_fgetline_fclose("/proc/sys/crypto/fips_enabled");
        if (fips_enabled)
        {
            if (strcmp(fips_enabled, "0") != 0)
                dd_save_text(dd, "fips_enabled", fips_enabled);
            free(fips_enabled);
        }

        dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);

        if (setting_SaveBinaryImage)
        {
            strcpy(path + path_len, "/"FILENAME_BINARY);

            if (save_crashing_binary(pid, path, dd->dd_uid, dd->dd_gid))
            {
                error_msg("Error saving '%s'", path);

                goto error_exit;
            }
        }

        off_t core_size = 0;
        if (setting_SaveFullCore)
        {
            strcpy(path + path_len, "/"FILENAME_COREDUMP);
            int abrt_core_fd = create_or_die(path, user_core_fd);

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
            core_size = copyfd_sparse(STDIN_FILENO, abrt_core_fd, user_core_fd, ulimit_c);
            if (fsync(abrt_core_fd) != 0 || close(abrt_core_fd) != 0 || core_size < 0)
            {
                unlink(path);

                /* copyfd_sparse logs the error including errno string,
                 * but it does not log file name */
                error_msg("Error writing '%s'", path);

                goto error_exit;
            }
            if (user_core_fd >= 0
                /* error writing user coredump? */
             && (fsync(user_core_fd) != 0 || close(user_core_fd) != 0
                /* user coredump is too big? */
                || (ulimit_c == 0 /* paranoia */ || core_size > ulimit_c)
                )
            ) {
                /* nuke it (silently) */
                xchdir(user_pwd);
                unlink(core_basename);
            }
        }
        else
        {
            /* User core is created even if WriteFullCore is off. */
            create_user_core(user_core_fd, pid, ulimit_c);
        }

        /* User core is either written or closed */
        user_core_fd = -1;

        /*
         * ! No other errors should cause removal of the user core !
         */

        /* Save JVM crash log if it exists. (JVM's coredump per se
         * is nearly useless for JVM developers)
         */
        {
            char *java_log = xasprintf("/tmp/jvm-%lu/hs_error.log", (long)pid);
            int src_fd = open(java_log, O_RDONLY);
            free(java_log);

            /* If we couldn't open the error log in /tmp directory we can try to
             * read the log from the current directory. It may produce AVC, it
             * may produce some error log but all these are expected.
             */
            if (src_fd < 0)
            {
                java_log = xasprintf("%s/hs_err_pid%lu.log", user_pwd, (long)pid);
                src_fd = open(java_log, O_RDONLY);
                free(java_log);
            }

            if (src_fd >= 0)
            {
                strcpy(path + path_len, "/hs_err.log");
                int dst_fd = create_or_die(path, user_core_fd);
                off_t sz = copyfd_eof(src_fd, dst_fd, COPYFD_SPARSE);
                if (close(dst_fd) != 0 || sz < 0)
                {
                    error_msg("Error saving '%s'", path);

                    goto error_exit;
                }
                close(src_fd);
            }
        }

        /* Perform crash-time unwind of the guilty thread. */
        if (tid > 0 && setting_CreateCoreBacktrace)
            create_core_backtrace(tid, executable, signal_no, dd->dd_dirname);

        /* We close dumpdir before we start catering for crash storm case.
         * Otherwise, delete_dump_dir's from other concurrent
         * CCpp's won't be able to delete our dump (their delete_dump_dir
         * will wait for us), and we won't be able to delete their dumps.
         * Classic deadlock.
         */
        dd_close(dd);
        dd = NULL;

        path[path_len] = '\0'; /* path now contains only directory name */
        char *newpath = xstrndup(path, path_len - (sizeof(".new")-1));
        if (rename(path, newpath) == 0)
            strcpy(path, newpath);
        free(newpath);

        if (core_size > 0)
            log_notice("Saved core dump of pid %lu (%s) to %s (%llu bytes)",
                       (long)pid, executable, path, (long long)core_size);

        notify_new_path(path);

        /* rhbz#539551: "abrt going crazy when crashing process is respawned" */
        if (g_settings_nMaxCrashReportsSize > 0)
        {
            /* x1.25 and round up to 64m: go a bit up, so that usual in-daemon trimming
             * kicks in first, and we don't "fight" with it:
             */
            unsigned maxsize = g_settings_nMaxCrashReportsSize + g_settings_nMaxCrashReportsSize / 4;
            maxsize |= 63;
            trim_problem_dirs(g_settings_dump_location, maxsize * (double)(1024*1024), path);
        }

        free(rootdir);
        return 0;
    }

    /* We didn't create abrt dump, but may need to create compat coredump */
    return create_user_core(user_core_fd, pid, ulimit_c);

error_exit:
    if (dd)
        dd_delete(dd);

    if (user_core_fd >= 0)
    {
        xchdir(user_pwd);
        unlink(core_basename);
    }

    xfunc_die();
}
