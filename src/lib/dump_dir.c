/*
    DebugDump.cpp

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#include "abrtlib.h"
#include "strbuf.h"

// TODO:
//
// Perhaps dd_opendir should do some sanity checking like
// "if there is no "uid" file in the directory, it's not a crash dump",
// and fail.

static char *load_text_file(const char *path, unsigned flags);

static bool isdigit_str(const char *str)
{
    do
    {
        if (*str < '0' || *str > '9') return false;
        str++;
    } while (*str);
    return true;
}

static bool exist_file_dir(const char *path)
{
    struct stat buf;
    if (stat(path, &buf) == 0)
    {
        if (S_ISDIR(buf.st_mode) || S_ISREG(buf.st_mode))
        {
            return true;
        }
    }
    return false;
}

static bool get_and_set_lock(const char* lock_file, const char* pid)
{
    while (symlink(pid, lock_file) != 0)
    {
        if (errno != EEXIST)
            perror_msg_and_die("Can't create lock file '%s'", lock_file);

        char pid_buf[sizeof(pid_t)*3 + 4];
        ssize_t r = readlink(lock_file, pid_buf, sizeof(pid_buf) - 1);
        if (r < 0)
        {
            if (errno == ENOENT)
            {
                /* Looks like lock_file was deleted */
                usleep(10 * 1000); /* avoid CPU eating loop */
                continue;
            }
            perror_msg_and_die("Can't read lock file '%s'", lock_file);
        }
        pid_buf[r] = '\0';

        if (strcmp(pid_buf, pid) == 0)
        {
            log("Lock file '%s' is already locked by us", lock_file);
            return false;
        }
        if (isdigit_str(pid_buf))
        {
            char pid_str[sizeof("/proc/") + strlen(pid_buf)];
            sprintf(pid_str, "/proc/%s", pid_buf);
            if (access(pid_str, F_OK) == 0)
            {
                log("Lock file '%s' is locked by process %s", lock_file, pid_buf);
                return false;
            }
            log("Lock file '%s' was locked by process %s, but it crashed?", lock_file, pid_buf);
        }
        /* The file may be deleted by now by other process. Ignore ENOENT */
        if (unlink(lock_file) != 0 && errno != ENOENT)
        {
            perror_msg_and_die("Can't remove stale lock file '%s'", lock_file);
        }
    }

    VERB1 log("Locked '%s'", lock_file);
    return true;
}

static void dd_lock(struct dump_dir *dd)
{
    if (dd->locked)
        error_msg_and_die("Locking bug on '%s'", dd->dd_dir);

    char lock_buf[strlen(dd->dd_dir) + sizeof(".lock")];
    sprintf(lock_buf, "%s.lock", dd->dd_dir);

    char pid_buf[sizeof(long)*3 + 2];
    sprintf(pid_buf, "%lu", (long)getpid());
    while ((dd->locked = get_and_set_lock(lock_buf, pid_buf)) != true)
    {
        sleep(1); /* was 0.5 seconds */
    }
}

static void dd_unlock(struct dump_dir *dd)
{
    if (dd->locked)
    {
        dd->locked = 0;
        char lock_buf[strlen(dd->dd_dir) + sizeof(".lock")];
        sprintf(lock_buf, "%s.lock", dd->dd_dir);
        xunlink(lock_buf);
        VERB1 log("Unlocked '%s'", lock_buf);
    }
}

static inline struct dump_dir *dd_init(void)
{
    return (struct dump_dir*)xzalloc(sizeof(struct dump_dir));
}

int dd_exist(struct dump_dir *dd, const char *path)
{
    char *full_path = concat_path_file(dd->dd_dir, path);
    int ret = exist_file_dir(full_path);
    free(full_path);
    return ret;
}

void dd_close(struct dump_dir *dd)
{
    if (!dd)
        return;

    dd_unlock(dd);
    if (dd->next_dir)
    {
        closedir(dd->next_dir);
        /* free(dd->next_dir); - WRONG! */
    }

    free(dd->dd_dir);
    free(dd);
}

static char* rm_trailing_slashes(const char *dir)
{
    unsigned len = strlen(dir);
    while (len != 0 && dir[len-1] == '/')
        len--;
    return xstrndup(dir, len);
}

struct dump_dir *dd_opendir(const char *dir, int flags)
{
    struct dump_dir *dd = dd_init();

    /* Used to use rm_trailing_slashes(dir) here, but with dir = "."
     * or "..", or if the last component is a symlink,
     * then lock file is created in the wrong place.
     * IOW: this breaks locking.
     */
    dd->dd_dir = realpath(dir, NULL);
    if (!dd->dd_dir)
    {
        if (!(flags & DD_FAIL_QUIETLY))
            error_msg("'%s' does not exist", dir);
        dd_close(dd);
        return NULL;
    }
    dir = dd->dd_dir;

    dd_lock(dd);

    struct stat stat_buf;
    if (stat(dir, &stat_buf) != 0 || !S_ISDIR(stat_buf.st_mode))
    {
        if (!(flags & DD_FAIL_QUIETLY))
            error_msg("'%s' does not exist", dir);
        dd_close(dd);
        return NULL;
    }

    /* In case caller would want to create more files, he'll need uid:gid */
    dd->dd_uid = stat_buf.st_uid;
    dd->dd_gid = stat_buf.st_gid;

    /* Without this check, e.g. abrt-action-print happily prints any current
     * directory when run without arguments, because its option -d DIR
     * defaults to "."! Let's require that at least some crash dump dir
     * specific files exist before we declare open successful:
     */
    char *name = concat_path_file(dir, FILENAME_ANALYZER);
    int bad = (lstat(name, &stat_buf) != 0 || !S_ISREG(stat_buf.st_mode));
    free(name);
    if (bad)
    {
        /*if (!(flags & DD_FAIL_QUIETLY))... - no, DD_FAIL_QUIETLY only means
         * "it's ok if it doesn exist", not "ok if contents is bogus"!
         */
        error_msg("'%s' is not a crash dump directory", dir);
        dd_close(dd);
        return NULL;
    }

    return dd;
}

/* Create a fresh empty debug dump dir.
 *
 * Security: we should not allow users to write new files or write
 * into existing ones, but they should be able to read them.
 *
 * @param uid
 *   Crashed application's User Id
 *
 * We currently have only three callers:
 * kernel oops hook: uid=0
 *  this hook runs under 0:0
 * ccpp hook: uid=uid of crashed user's binary
 *  this hook runs under 0:0
 * python hook: uid=uid of crashed user's script
 *  this hook runs under abrt:gid
 *
 * Currently, we set dir's gid to passwd(uid)->pw_gid parameter, and we set uid to
 * abrt's user id. We do not allow write access to group.
 */
struct dump_dir *dd_create(const char *dir, uid_t uid)
{
    struct dump_dir *dd = dd_init();

    /* Unlike dd_opendir, can't use realpath: the directory doesn't exist yet,
     * realpath will always return NULL. We don't really have to:
     * dd_opendir(".") makes sense, dd_create(".") does not.
     */
    dir = dd->dd_dir = rm_trailing_slashes(dir);

    const char *last_component = strrchr(dir, '/');
    if (last_component)
        last_component++;
    else
        last_component = dir;
    if (dot_or_dotdot(last_component))
    {
        /* dd_create("."), dd_create(".."), dd_create("dir/."),
         * dd_create("dir/..") and similar are madness, refuse them.
         */
        error_msg("Bad dir name '%s'", dir);
        dd_close(dd);
        return NULL;
    }

    dd_lock(dd);

    /* Was creating it with mode 0700 and user as the owner, but this allows
     * the user to replace any file in the directory, changing security-sensitive data
     * (e.g. "uid", "analyzer", "executable")
     */
    if (mkdir(dir, 0750) == -1)
    {
        perror_msg("Can't create dir '%s'", dir);
        dd_close(dd);
        return NULL;
    }

    /* mkdir's mode (above) can be affected by umask, fix it */
    if (chmod(dir, 0750) == -1)
    {
        perror_msg("Can't change mode of '%s'", dir);
        dd_close(dd);
        return NULL;
    }

    /* Get ABRT's user id */
    /*dd->dd_uid = 0; - dd_init did this already */
    struct passwd *pw = getpwnam("abrt");
    if (pw)
        dd->dd_uid = pw->pw_uid;
    else
        error_msg("User 'abrt' does not exist, using uid 0");

    /* Get crashed application's group id */
    /*dd->dd_gid = 0; - dd_init did this already */
    pw = getpwuid(uid);
    if (pw)
        dd->dd_gid = pw->pw_gid;
    else
        error_msg("User %lu does not exist, using gid 0", (long)uid);

    if (chown(dir, dd->dd_uid, dd->dd_gid) == -1)
    {
        perror_msg("Can't change '%s' ownership to %lu:%lu", dir,
                   (long)dd->dd_uid, (long)dd->dd_gid);
    }

    char long_str[sizeof(long) * 3 + 2];

    sprintf(long_str, "%lu", (long)uid);
    dd_save_text(dd, CD_UID, long_str);

    struct utsname buf;
    uname(&buf); /* never fails */
    dd_save_text(dd, FILENAME_KERNEL, buf.release);
    dd_save_text(dd, FILENAME_ARCHITECTURE, buf.machine);
    char *release = load_text_file("/etc/redhat-release", /*flags:*/ 0);
    strchrnul(release, '\n')[0] = '\0';
    dd_save_text(dd, FILENAME_RELEASE, release);
    free(release);

    time_t t = time(NULL);
    sprintf(long_str, "%lu", (long)t);
    dd_save_text(dd, FILENAME_TIME, long_str);

    return dd;
}

static void delete_file_dir(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d)
        return;

    struct dirent *dent;
    while ((dent = readdir(d)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue;
        char *full_path = concat_path_file(dir, dent->d_name);
        if (unlink(full_path) == -1 && errno != ENOENT)
        {
            if (errno != EISDIR)
            {
                error_msg("Can't remove '%s'", full_path);
                free(full_path);
                closedir(d);
                return;
            }
            delete_file_dir(full_path);
        }
        free(full_path);
    }
    closedir(d);
    if (rmdir(dir) == -1)
    {
        error_msg("Can't remove dir '%s'", dir);
    }
}

void dd_delete(struct dump_dir *dd)
{
    delete_file_dir(dd->dd_dir);
    dd_close(dd);
}

static char *load_text_file(const char *path, unsigned flags)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        if (!(flags & DD_FAIL_QUIETLY))
            perror_msg("Can't open file '%s'", path);
        return (flags & DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE ? NULL : xstrdup(""));
    }

    struct strbuf *buf_content = strbuf_new();
    int ch;
    while ((ch = fgetc(fp)) != EOF)
    {
        if (ch == '\0')
            strbuf_append_char(buf_content, ' ');
        else if (isspace(ch) || (isascii(ch) && !iscntrl(ch)))
            strbuf_append_char(buf_content, ch);
    }
    fclose(fp);

    return strbuf_free_nobuf(buf_content);
}

static bool save_binary_file(const char *path, const char* data, unsigned size, uid_t uid, gid_t gid)
{
    /* "Why 0640?!" See ::Create() for security analysis */
    unlink(path);
    int fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0640);
    if (fd < 0)
    {
        perror_msg("Can't open file '%s'", path);
        return false;
    }
    if (fchown(fd, uid, gid) == -1)
    {
        perror_msg("can't change '%s' ownership to %lu:%lu", path, (long)uid, (long)gid);
    }
    unsigned r = full_write(fd, data, size);
    close(fd);
    if (r != size)
    {
        error_msg("Can't save file '%s'", path);
        return false;
    }

    return true;
}

char* dd_load_text_ext(const struct dump_dir *dd, const char *name, unsigned flags)
{
    if (!dd->locked)
        error_msg_and_die("dump_dir is not opened"); /* bug */

    char *full_path = concat_path_file(dd->dd_dir, name);
    char *ret = load_text_file(full_path, flags);
    free(full_path);

    return ret;
}

char* dd_load_text(const struct dump_dir *dd, const char *name)
{
    return dd_load_text_ext(dd, name, /*flags:*/ 0);
}

void dd_save_text(struct dump_dir *dd, const char *name, const char *data)
{
    if (!dd->locked)
        error_msg_and_die("dump_dir is not opened"); /* bug */

    char *full_path = concat_path_file(dd->dd_dir, name);
    save_binary_file(full_path, data, strlen(data), dd->dd_uid, dd->dd_gid);
    free(full_path);
}

void dd_save_binary(struct dump_dir* dd, const char* name, const char* data, unsigned size)
{
    if (!dd->locked)
        error_msg_and_die("dump_dir is not opened"); /* bug */

    char *full_path = concat_path_file(dd->dd_dir, name);
    save_binary_file(full_path, data, size, dd->dd_uid, dd->dd_gid);
    free(full_path);
}

DIR *dd_init_next_file(struct dump_dir *dd)
{
    if (!dd->locked)
        error_msg_and_die("dump_dir is not opened"); /* bug */

    if (dd->next_dir)
        closedir(dd->next_dir);

    dd->next_dir = opendir(dd->dd_dir);
    if (!dd->next_dir)
    {
        error_msg("Can't open dir '%s'", dd->dd_dir);
    }

    return dd->next_dir;
}

int dd_get_next_file(struct dump_dir *dd, char **short_name, char **full_name)
{
    if (dd->next_dir == NULL)
        return 0;

    struct dirent *dent;
    while ((dent = readdir(dd->next_dir)) != NULL)
    {
        if (is_regular_file(dent, dd->dd_dir))
        {
            if (short_name)
                *short_name = xstrdup(dent->d_name);
            if (full_name)
                *full_name = concat_path_file(dd->dd_dir, dent->d_name);
            return 1;
        }
    }

    closedir(dd->next_dir);
    dd->next_dir = NULL;
    return 0;
}

/* Utility function */
void delete_debug_dump_dir(const char *dd_dir)
{
    struct dump_dir *dd = dd_opendir(dd_dir, /*flags:*/ 0);
    if (dd)
    {
        dd_delete(dd);
    }
}
