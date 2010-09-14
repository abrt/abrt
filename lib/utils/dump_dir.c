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
#include "comm_layer_inner.h"
#include "strbuf.h"

static bool isdigit_str(const char *str)
{
    do
    {
        if (*str < '0' || *str > '9') return false;
        str++;
    } while (*str);
    return true;
}

static char* rm_trailing_slashes(const char *dir)
{
    unsigned len = strlen(dir);
    while (len != 0 && dir[len-1] == '/')
        len--;
    return xstrndup(dir, len);
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

static char *load_text_file(const char *path);
static void dd_lock(dump_dir_t *dd);
static void dd_unlock(dump_dir_t *dd);

dump_dir_t* dd_init(void)
{
    return (dump_dir_t*)xzalloc(sizeof(dump_dir_t));
}

void dd_close(dump_dir_t *dd)
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

int dd_opendir(dump_dir_t *dd, const char *dir)
{
    if (dd->locked)
        error_msg_and_die("dump_dir is already opened"); /* bug */

    dd->dd_dir = rm_trailing_slashes(dir);
    if (!exist_file_dir(dd->dd_dir))
    {
        error_msg("'%s' does not exist", dd->dd_dir);
        free(dd->dd_dir);
        return 0;
    }

    dd_lock(dd);

    /* In case caller would want to create more files, he'll need uid:gid */
    struct stat stat_buf;
    if (stat(dd->dd_dir, &stat_buf) == 0)
    {
        dd->dd_uid = stat_buf.st_uid;
        dd->dd_gid = stat_buf.st_gid;
    }

    return 1;
}

int dd_exist(dump_dir_t *dd, const char *path)
{
    char *full_path = concat_path_file(dd->dd_dir, path);
    int ret = exist_file_dir(full_path);
    free(full_path);
    return ret;
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

static void dd_lock(dump_dir_t *dd)
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

static void dd_unlock(dump_dir_t *dd)
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
int dd_create(dump_dir_t *dd, const char *dir, uid_t uid)
{
    if (dd->locked)
        error_msg_and_die("dump_dir is already opened"); /* bug */

    dd->dd_dir = rm_trailing_slashes(dir);
    if (exist_file_dir(dd->dd_dir))
    {
        error_msg("'%s' already exists", dd->dd_dir);
        free(dd->dd_dir);
        return 0;
    }

    dd_lock(dd);

    /* Was creating it with mode 0700 and user as the owner, but this allows
     * the user to replace any file in the directory, changing security-sensitive data
     * (e.g. "uid", "analyzer", "executable")
     */
    if (mkdir(dd->dd_dir, 0750) == -1)
    {
        dd_unlock(dd);
        error_msg("Can't create dir '%s'", dir);
        return 0;
    }

    /* mkdir's mode (above) can be affected by umask, fix it */
    if (chmod(dd->dd_dir, 0750) == -1)
    {
        dd_unlock(dd);
        error_msg("Can't change mode of '%s'", dir);
        return false;
    }

    /* Get ABRT's user id */
    dd->dd_uid = 0;
    struct passwd *pw = getpwnam("abrt");
    if (pw)
        dd->dd_uid = pw->pw_uid;
    else
        error_msg("User 'abrt' does not exist, using uid 0");

    /* Get crashed application's group id */
    dd->dd_gid = 0;
    pw = getpwuid(uid);
    if (pw)
        dd->dd_gid = pw->pw_gid;
    else
        error_msg("User %lu does not exist, using gid 0", (long)uid);

    if (chown(dd->dd_dir, dd->dd_uid, dd->dd_gid) == -1)
    {
        perror_msg("can't change '%s' ownership to %lu:%lu", dd->dd_dir,
                   (long)dd->dd_uid, (long)dd->dd_gid);
    }

    char uid_str[sizeof(long) * 3 + 2];
    sprintf(uid_str, "%lu", (long)uid);
    dd_save_text(dd, CD_UID, uid_str);

    {
        struct utsname buf;
        if (uname(&buf) != 0)
        {
            perror_msg_and_die("uname");
        }
        dd_save_text(dd, FILENAME_KERNEL, buf.release);
        dd_save_text(dd, FILENAME_ARCHITECTURE, buf.machine);
        char *release = load_text_file("/etc/redhat-release");
        strchrnul(release, '\n')[0] = '\0';
        dd_save_text(dd, FILENAME_RELEASE, release);
        free(release);
    }

    time_t t = time(NULL);
    char t_str[sizeof(time_t) * 3 + 2];
    sprintf(t_str, "%lu", (time_t)t);
    dd_save_text(dd, FILENAME_TIME, t_str);

    return 1;
}

static bool delete_file_dir(const char *dir)
{
    if (!exist_file_dir(dir))
        return true;

    DIR *d = opendir(dir);
    if (!d)
    {
        error_msg("Can't open dir '%s'", dir);
        return false;
    }

    struct dirent *dent;
    while ((dent = readdir(d)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue;
        char *full_path = concat_path_file(dir, dent->d_name);
        if (unlink(full_path) == -1)
        {
            if (errno != EISDIR)
            {
                closedir(d);
                error_msg("Can't remove dir '%s'", full_path);
                free(full_path);
                return false;
            }
            delete_file_dir(full_path);
        }
        free(full_path);
    }
    closedir(d);
    if (rmdir(dir) == -1)
    {
        error_msg("Can't remove dir %s", dir);
        return false;
    }

    return true;
}

void dd_delete(dump_dir_t *dd)
{
    if (!exist_file_dir(dd->dd_dir))
    {
        return;
    }

    delete_file_dir(dd->dd_dir);
}

static char *load_text_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        perror_msg("Can't open file '%s'", path);
        return xstrdup("");
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

char* dd_load_text(const dump_dir_t *dd, const char *name)
{
    if (!dd->locked)
        error_msg_and_die("dump_dir is not opened"); /* bug */

    char *full_path = concat_path_file(dd->dd_dir, name);
    char *ret = load_text_file(full_path);
    free(full_path);

    return ret;
}

void dd_save_text(dump_dir_t *dd, const char *name, const char *data)
{
    if (!dd->locked)
        error_msg_and_die("dump_dir is not opened"); /* bug */

    char *full_path = concat_path_file(dd->dd_dir, name);
    save_binary_file(full_path, data, strlen(data), dd->dd_uid, dd->dd_gid);
    free(full_path);
}

void dd_save_binary(dump_dir_t* dd, const char* name, const char* data, unsigned size)
{
    if (!dd->locked)
        error_msg_and_die("dump_dir is not opened"); /* bug */

    char *full_path = concat_path_file(dd->dd_dir, name);
    save_binary_file(full_path, data, size, dd->dd_uid, dd->dd_gid);
    free(full_path);
}

DIR *dd_init_next_file(dump_dir_t *dd)
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

int dd_get_next_file(dump_dir_t *dd, char **short_name, char **full_name)
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
    dump_dir_t *dd = dd_init();
    if (dd_opendir(dd, dd_dir))
        dd_delete(dd);
    else
        VERB1 log("Unable to open debug dump '%s'", dd_dir);
    dd_close(dd);
}
