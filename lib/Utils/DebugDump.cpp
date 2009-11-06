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

#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/utsname.h>
//#include <magic.h>
#include "abrtlib.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

static bool isdigit_str(const char *str)
{
    do
    {
        if (*str < '0' || *str > '9') return false;
        str++;
    } while (*str);
    return true;
}

static std::string RemoveBackSlashes(const char *pDir)
{
    unsigned len = strlen(pDir);
    while (len != 0 && pDir[len-1] == '/')
        len--;
    return std::string(pDir, len);
}

static bool ExistFileDir(const char* pPath);
static void LoadTextFile(const char *pPath, std::string& pData);

CDebugDump::CDebugDump() :
    m_sDebugDumpDir(""),
    m_pGetNextFileDir(NULL),
    m_bOpened(false),
    m_bLocked(false)
{}

void CDebugDump::Open(const char *pDir)
{
    if (m_bOpened)
    {
        throw CABRTException(EXCEP_ERROR, "CDebugDump::CDebugDump(): DebugDump is already opened.");
    }
    m_sDebugDumpDir = RemoveBackSlashes(pDir);
    if (!ExistFileDir(m_sDebugDumpDir.c_str()))
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::CDebugDump(): " + m_sDebugDumpDir + " does not exist.");
    }
    Lock();
    m_bOpened = true;
}

bool CDebugDump::Exist(const char* pPath)
{
    std::string fullPath = m_sDebugDumpDir + "/" + pPath;
    return ExistFileDir(fullPath.c_str());
}


static bool ExistFileDir(const char* pPath)
{
    struct stat buf;
    if (stat(pPath, &buf) == 0)
    {
        if (S_ISDIR(buf.st_mode) || S_ISREG(buf.st_mode))
        {
            return true;
        }
    }
    return false;
}

static bool GetAndSetLock(const char* pLockFile, const char* pPID)
{
    while (symlink(pPID, pLockFile) != 0)
    {
        if (errno != EEXIST)
            perror_msg_and_die("Can't create lock file '%s'", pLockFile);

        char pid_buf[sizeof(pid_t)*3 + 4];
        ssize_t r = readlink(pLockFile, pid_buf, sizeof(pid_buf) - 1);
        if (r < 0)
            perror_msg_and_die("Can't read lock file '%s'", pLockFile);
        pid_buf[r] = '\0';

        if (strcmp(pid_buf, pPID) == 0)
        {
            log("Lock file '%s' is already locked by us", pLockFile);
            return false;
        }
        if (isdigit_str(pid_buf))
        {
            if (access(ssprintf("/proc/%s", pid_buf).c_str(), F_OK) == 0)
            {
                log("Lock file '%s' is locked by process %s", pLockFile, pid_buf);
                return false;
            }
            log("Lock file '%s' was locked by process %s, but it crashed?", pLockFile, pid_buf);
        }
        /* The file may be deleted by now by other process. Ignore ENOENT */
        if (unlink(pLockFile) != 0 && errno != ENOENT)
        {
            perror_msg_and_die("Can't remove stale lock file '%s'", pLockFile);
        }
    }

    VERB1 log("Locked '%s'", pLockFile);
    return true;

#if 0
/* Old code was using ordinary files instead of symlinks,
 * but it had a race window between open and write, during which file was
 * empty. It was seen to happen in practice.
 */
    int fd;
    while ((fd = open(pLockFile, O_WRONLY | O_CREAT | O_EXCL, 0640)) < 0)
    {
        if (errno != EEXIST)
            perror_msg_and_die("Can't create lock file '%s'", pLockFile);
        fd = open(pLockFile, O_RDONLY);
        if (fd < 0)
        {
            if (errno == ENOENT)
                continue; /* someone else deleted the file */
            perror_msg_and_die("Can't open lock file '%s'", pLockFile);
        }
        char pid_buf[sizeof(pid_t)*3 + 4];
        int r = read(fd, pid_buf, sizeof(pid_buf) - 1);
        if (r < 0)
            perror_msg_and_die("Can't read lock file '%s'", pLockFile);
        close(fd);
        if (r == 0)
        {
            /* Other process did not write out PID yet.
             * We HOPE it did not crash... */
            continue;
        }
        pid_buf[r] = '\0';
        if (strcmp(pid_buf, pPID) == 0)
        {
            log("Lock file '%s' is already locked by us", pLockFile);
            return -1;
        }
        if (isdigit_str(pid_buf))
        {
            if (access(ssprintf("/proc/%s", pid_buf).c_str(), F_OK) == 0)
            {
                log("Lock file '%s' is locked by process %s", pLockFile, pid_buf);
                return -1;
            }
            log("Lock file '%s' was locked by process %s, but it crashed?", pLockFile, pid_buf);
        }
        /* The file may be deleted by now by other process. Ignore errors */
        unlink(pLockFile);
    }

    int len = strlen(pPID);
    if (write(fd, pPID, len) != len)
    {
        unlink(pLockFile);
        /* close(fd); - not needed, exiting does it too */
        perror_msg_and_die("Can't write lock file '%s'", pLockFile);
    }
    close(fd);

    VERB1 log("Locked '%s'", pLockFile);
    return true;
#endif
}

void CDebugDump::Lock()
{
    if (m_bLocked)
        error_msg_and_die("Locking bug on '%s'", m_sDebugDumpDir.c_str());

    std::string lockFile = m_sDebugDumpDir + ".lock";
    char pid_buf[sizeof(int)*3 + 2];
    sprintf(pid_buf, "%u", (unsigned)getpid());
    while ((m_bLocked = GetAndSetLock(lockFile.c_str(), pid_buf)) != true)
    {
        usleep(500000);
    }
}

void CDebugDump::UnLock()
{
    if (m_bLocked)
    {
        m_bLocked = false;
        std::string lockFile = m_sDebugDumpDir + ".lock";
        xunlink(lockFile.c_str());
        VERB1 log("UnLocked '%s'", lockFile.c_str());
    }
}

void CDebugDump::Create(const char *pDir, int64_t uid)
{
    if (m_bOpened)
    {
        throw CABRTException(EXCEP_ERROR, "DebugDump is already opened");
    }

    m_sDebugDumpDir = RemoveBackSlashes(pDir);
    if (ExistFileDir(m_sDebugDumpDir.c_str()))
    {
        throw CABRTException(EXCEP_DD_OPEN, ssprintf("'%s' already exists", m_sDebugDumpDir.c_str()));
    }

    Lock();
    m_bOpened = true;

    if (mkdir(m_sDebugDumpDir.c_str(), 0700) == -1)
    {
        UnLock();
        m_bOpened = false;
        throw CABRTException(EXCEP_DD_OPEN, ssprintf("Can't create dir '%s'", pDir));
    }
    if (chmod(m_sDebugDumpDir.c_str(), 0700) == -1)
    {
        UnLock();
        m_bOpened = false;
        throw CABRTException(EXCEP_DD_OPEN, ssprintf("Can't change mode of '%s'", pDir));
    }
    struct passwd* pw = getpwuid(uid);
    gid_t gid = pw ? pw->pw_gid : uid;
    if (chown(m_sDebugDumpDir.c_str(), uid, gid) == -1)
    {
        /* if /var/cache/abrt is writable by all, _aborting_ here is not useful */
        /* let's just warn */
        perror_msg("can't change '%s' ownership to %u:%u", m_sDebugDumpDir.c_str(), (int)uid, (int)gid);
    }

    SaveText(FILENAME_UID, to_string(uid).c_str());
    SaveKernelArchitectureRelease();
    time_t t = time(NULL);
    SaveText(FILENAME_TIME, to_string(t).c_str());
}

static void DeleteFileDir(const std::string& pDir)
{
    DIR *dir = opendir(pDir.c_str());
    if (!dir)
        return;

    struct dirent *dent;
    while ((dent = readdir(dir)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue;
        std::string fullPath = pDir + "/" + dent->d_name;
        if (unlink(fullPath.c_str()) == -1)
        {
            if (errno != EISDIR)
            {
                closedir(dir);
                throw CABRTException(EXCEP_DD_DELETE, "Can't remove file: " + fullPath);
            }
            DeleteFileDir(fullPath);
        }
    }
    closedir(dir);
    if (remove(pDir.c_str()) == -1)
    {
        throw CABRTException(EXCEP_DD_DELETE, "Can't remove dir: " + pDir);
    }
}

static bool IsTextFile(const char *name)
{
/* This idiotic library thinks that file containing just "0" is not text (!!)

    magic_t m = magic_open(MAGIC_MIME_TYPE);

    if (m == NULL)
    {
        throw CABRTException(EXCEP_ERROR, std::string(__func__) + "Cannot open magic cookie: " + magic_error(m));
    }

    int r = magic_load(m, NULL);

    if (r == -1)
    {
        magic_close(m);
        throw CABRTException(EXCEP_ERROR, std::string(__func__) + "Cannot load magic db: " + magic_error(m));
    }

    char* ch = (char *) magic_file(m, pName.c_str());

    if (ch == NULL)
    {
        magic_close(m);
        throw CABRTException(EXCEP_ERROR, std::string(__func__) + "Cannot determine file type: " + magic_error(m));
    }

    bool isText = (strncmp(ch, "text", 4) == 0);

    magic_close(m);

    return isText;
 */
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        return false;

    unsigned char buf[4*1024];
    int r = full_read(fd, buf, sizeof(buf));
    close(fd);

    while (--r >= 0)
    {
        if (buf[r] >= 0x7f)
            return false;
        /* Among control chars, only '\t','\n' etc are allowed */
        if (buf[r] < ' ' && !isspace(buf[r]))
            return false;
    }
    return true;
}

void CDebugDump::Delete()
{
    if (!ExistFileDir(m_sDebugDumpDir.c_str()))
    {
        return;
    }
    DeleteFileDir(m_sDebugDumpDir);
}

void CDebugDump::Close()
{
    UnLock();
    if (m_pGetNextFileDir != NULL)
    {
        closedir(m_pGetNextFileDir);
        m_pGetNextFileDir = NULL;
    }
    m_bOpened = false;
}

void CDebugDump::SaveKernelArchitectureRelease()
{
    struct utsname buf;
    if (uname(&buf) == 0)
    {
        SaveText(FILENAME_KERNEL, buf.release);
        SaveText(FILENAME_ARCHITECTURE, buf.machine);
    }
    std::string release;
    LoadTextFile("/etc/redhat-release", release);
    const char *release_ptr = release.c_str();
    unsigned len_1st_str = strchrnul(release_ptr, '\n') - release_ptr;
    release.erase(len_1st_str); /* usually simply removes trailing '\n' */
    SaveText(FILENAME_RELEASE, release.c_str());
}

static void LoadTextFile(const char *pPath, std::string& pData)
{
    FILE *fp = fopen(pPath, "r");
    if (!fp)
    {
        throw CABRTException(EXCEP_DD_LOAD, ssprintf("Can't open file '%s'", pPath));
    }
    pData = "";
    int ch;
    while ((ch = fgetc(fp)) != EOF)
    {
        if (ch == '\0')
        {
            pData += ' ';
        }
        else if (isspace(ch) || (isascii(ch) && !iscntrl(ch)))
        {
            pData += ch;
        }
    }
    fclose(fp);
}

static void SaveBinaryFile(const char *pPath, const char* pData, unsigned pSize)
{
    int fd = open(pPath, O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if (fd < 0)
    {
        throw CABRTException(EXCEP_DD_SAVE, ssprintf("Can't open file '%s'", pPath));
    }
    unsigned r = full_write(fd, pData, pSize);
    close(fd);
    if (r != pSize)
    {
        throw CABRTException(EXCEP_DD_SAVE, ssprintf("Can't save file '%s'", pPath));
    }
}

void CDebugDump::LoadText(const char* pName, std::string& pData)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::LoadText(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + '/' + pName;
    LoadTextFile(fullPath.c_str(), pData);
}

void CDebugDump::SaveText(const char* pName, const char* pData)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::SaveText(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    SaveBinaryFile(fullPath.c_str(), pData, strlen(pData));
}
void CDebugDump::SaveBinary(const char* pName, const char* pData, unsigned pSize)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::SaveBinary(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    SaveBinaryFile(fullPath.c_str(), pData, pSize);
}

void CDebugDump::InitGetNextFile()
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::InitGetNextFile(): DebugDump is not opened.");
    }
    if (m_pGetNextFileDir != NULL)
    {
        closedir(m_pGetNextFileDir);
    }
    m_pGetNextFileDir = opendir(m_sDebugDumpDir.c_str());
    if (m_pGetNextFileDir == NULL)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::InitGetNextFile(): Cannot open dir " + m_sDebugDumpDir);
    }
}

bool CDebugDump::GetNextFile(std::string& pFileName, std::string& pContent, bool& pIsTextFile)
{
    if (m_pGetNextFileDir == NULL)
    {
        return false;
    }

    struct dirent *dent;
    while ((dent = readdir(m_pGetNextFileDir)) != NULL)
    {
        if (is_regular_file(dent, m_sDebugDumpDir.c_str()))
        {
            std::string fullname = m_sDebugDumpDir + '/' + dent->d_name;

            pFileName = dent->d_name;
            if (IsTextFile(fullname.c_str()))
            {
                LoadText(dent->d_name, pContent);
                pIsTextFile = true;
            }
            else
            {
                pContent.clear();
                pIsTextFile = false;
            }
            return true;
        }
    }
    closedir(m_pGetNextFileDir);
    m_pGetNextFileDir = NULL;
    return false;
}
