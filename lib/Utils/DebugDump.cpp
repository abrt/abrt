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
#include <magic.h>
#include "abrtlib.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

/* Is it "." or ".."? */
/* abrtlib candidate */
static bool dot_or_dotdot(const char *filename)
{
    if (filename[0] != '.') return false;
    if (filename[1] == '\0') return true;
    if (filename[1] != '.') return false;
    if (filename[2] == '\0') return true;
    return false;
}

static bool isdigit_str(const char *str)
{
    while (*str)
    {
        if (*str < '0' || *str > '9') return false;
        str++;
    }
    return true;
}

static std::string RemoveBackSlashes(const std::string& pDir);
static bool ExistFileDir(const char* pPath);
static void LoadTextFile(const std::string& pPath, std::string& pData);

CDebugDump::CDebugDump() :
    m_sDebugDumpDir(""),
    m_bOpened(false),
    m_pGetNextFileDir(NULL),
    m_nLockfileFD(-1)
{}

void CDebugDump::Open(const std::string& pDir)
{
    if (m_bOpened)
    {
        throw CABRTException(EXCEP_ERROR, "CDebugDump::CDebugDump(): DebugDump is already opened.");
    }
    m_sDebugDumpDir = RemoveBackSlashes(pDir);
    if (!ExistFileDir(m_sDebugDumpDir.c_str()))
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::CDebugDump(): "+m_sDebugDumpDir+" does not exist.");
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

static int GetAndSetLock(const char* pLockFile, const char* pPID)
{
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

    log("Locked '%s'", pLockFile);
    return fd;
}

void CDebugDump::Lock()
{
    if (m_nLockfileFD >= 0)
        error_msg_and_die("Locking bug on '%s'", m_sDebugDumpDir.c_str());

    std::string lockFile = m_sDebugDumpDir + ".lock";
    char pid_buf[sizeof(int)*3 + 2];
    sprintf(pid_buf, "%u", (unsigned)getpid());
    while ((m_nLockfileFD = GetAndSetLock(lockFile.c_str(), pid_buf)) < 0)
    {
        usleep(500000);
    }
}

void CDebugDump::UnLock()
{
    if (m_nLockfileFD >= 0)
    {
        std::string lockFile = m_sDebugDumpDir + ".lock";
        log("UnLocking '%s'", lockFile.c_str());
        close(m_nLockfileFD);
        m_nLockfileFD = -1;
        xunlink(lockFile.c_str());
    }
}

void CDebugDump::Create(const std::string& pDir, uid_t uid)
{
    if (m_bOpened)
    {
        throw CABRTException(EXCEP_ERROR, "CDebugDump::CDebugDump(): DebugDump is already opened.");
    }

    m_sDebugDumpDir = RemoveBackSlashes(pDir);
    if (ExistFileDir(m_sDebugDumpDir.c_str()))
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::CDebugDump(): "+m_sDebugDumpDir+" already exists.");
    }

    Lock();
    m_bOpened = true;

    if (mkdir(m_sDebugDumpDir.c_str(), 0700) == -1)
    {
        UnLock();
        m_bOpened = false;
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::Create(): Cannot create dir: " + pDir);
    }
    if (chmod(m_sDebugDumpDir.c_str(), 0700) == -1)
    {
        UnLock();
        m_bOpened = false;
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::Create(): Cannot change permissions, dir: " + pDir);
    }
    struct passwd* pw = getpwuid(uid);
    gid_t gid = pw ? pw->pw_gid : uid;
    if (chown(m_sDebugDumpDir.c_str(), uid, gid) == -1)
    {
        /* if /var/cache/abrt is writable by all, _aborting_ here is not useful */
        /* let's just warn */
        perror_msg("can't change '%s' ownership to %u:%u", m_sDebugDumpDir.c_str(), (int)uid, (int)gid);
    }

    SaveText(FILENAME_UID, ssprintf("%u", (int)uid));
    SaveKernelArchitectureRelease();
    SaveTime();
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
                throw CABRTException(EXCEP_DD_DELETE, std::string(__func__) + ": Cannot remove file: " + fullPath);
            }
            DeleteFileDir(fullPath);
        }
    }
    closedir(dir);
    if (remove(pDir.c_str()) == -1)
    {
        throw CABRTException(EXCEP_DD_DELETE, std::string(__func__) + ": Cannot remove dir: " + pDir);
    }
}

static bool IsTextFile(const std::string& pName)
{
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
}

static std::string RemoveBackSlashes(const std::string& pDir)
{
    std::string ret = pDir;
    while (ret[ret.length() - 1] == '/')
    {
        ret = ret.substr(0, ret.length() - 2);
    }
    return ret;
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
    SaveText(FILENAME_RELEASE, release);
}

void CDebugDump::SaveTime()
{
    time_t t = time(NULL);
    SaveText(FILENAME_TIME, to_string(t));
}

static void LoadTextFile(const std::string& pPath, std::string& pData)
{
    std::ifstream fIn;
    pData = "";
    fIn.open(pPath.c_str());
    if (fIn.is_open())
    {
        // TODO: rewrite this
        int ch;
        while ((ch = fIn.get())!= EOF)
        {
            if (ch == 0)
            {
                pData += " ";
            }
            else if (isspace(ch) || (isascii(ch) && !iscntrl(ch)))
            {
                pData += ch;
            }
        }
        fIn.close();
    }
    else
    {
        throw CABRTException(EXCEP_DD_LOAD, std::string(__func__) + ": Cannot open file " + pPath);
    }
}

static void LoadBinaryFile(const std::string& pPath, char** pData, unsigned int* pSize)
{
    std::ifstream fIn;
    fIn.open(pPath.c_str(), std::ios::binary | std::ios::ate);
    unsigned int size;
    if (fIn.is_open())
    {
        size = fIn.tellg();
        char *data = new char [size];
        fIn.read(data, size);

        *pData = data;
        *pSize = size;

        fIn.close();
    }
    else
    {
        throw CABRTException(EXCEP_DD_LOAD, std::string(__func__) + ": Cannot open file " + pPath);
    }
}

static void SaveTextFile(const std::string& pPath, const std::string& pData)
{
    std::ofstream fOut;
    fOut.open(pPath.c_str());
    if (fOut.is_open())
    {
        fOut << pData;
        if (!fOut.good())
        {
            throw CABRTException(EXCEP_DD_SAVE, std::string(__func__) + ": Cannot save file " + pPath);
        }
        fOut.close();
    }
    else
    {
        throw CABRTException(EXCEP_DD_SAVE, std::string(__func__) + ": Cannot open file " + pPath);
    }
}

static void SaveBinaryFile(const std::string& pPath, const char* pData, const unsigned pSize)
{
    std::ofstream fOut;
    fOut.open(pPath.c_str(), std::ios::binary);
    if (fOut.is_open())
    {
        fOut.write(pData, pSize);
        if (!fOut.good())
        {
            throw CABRTException(EXCEP_DD_SAVE, std::string(__func__) + ": Cannot save file " + pPath);
        }
        fOut.close();
    }
    else
    {
        throw CABRTException(EXCEP_DD_SAVE, std::string(__func__) + ": Cannot open file " + pPath);
    }
}

void CDebugDump::LoadText(const char* pName, std::string& pData)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::LoadText(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    LoadTextFile(fullPath, pData);
}
void CDebugDump::LoadBinary(const char* pName, char** pData, unsigned int* pSize)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::LoadBinary(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    LoadBinaryFile(fullPath, pData, pSize);
}

void CDebugDump::SaveText(const char* pName, const std::string& pData)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::SaveText(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    SaveTextFile(fullPath, pData);
}
void CDebugDump::SaveBinary(const char* pName, const char* pData, const unsigned int pSize)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::SaveBinary(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    SaveBinaryFile(fullPath, pData, pSize);
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
        struct stat statbuf;
        std::string fullname = m_sDebugDumpDir + "/" + dent->d_name;

        /* some filesystems do not report the type! they report DT_UNKNOWN */
        if (dent->d_type == DT_REG
         || (dent->d_type == DT_UNKNOWN
            && lstat(fullname.c_str(), &statbuf) == 0
            && S_ISREG(statbuf.st_mode)
            )
        ) {
            pFileName = dent->d_name;
            if (IsTextFile(fullname))
            {
                LoadText(dent->d_name, pContent);
                pIsTextFile = true;
            }
            else
            {
                pContent = "";
                pIsTextFile = false;
            }
            return true;
        }
    }
    closedir(m_pGetNextFileDir);
    m_pGetNextFileDir = NULL;
    return false;
}

