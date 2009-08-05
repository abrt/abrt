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

#include "abrtlib.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <sys/utsname.h>
#include <magic.h>

#include "CommLayerInner.h"
// BUG? in C/C++, compiler may assume that function address is never NULL
#pragma weak comm_layer_inner_debug
#define comm_layer_inner_debug(msg) ({\
    if (comm_layer_inner_debug)\
    {\
        comm_layer_inner_debug(msg);\
    }})

#define PID_STR_MAX 16

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

static std::string RemoveBackSlashes(const std::string& pDir);
static bool ExistFileDir(const std::string& pPath);
static void LoadTextFile(const std::string& pPath, std::string& pData);

CDebugDump::CDebugDump() :
    m_sDebugDumpDir(""),
    m_bOpened(false),
    m_bUnlock(true),
    m_pGetNextFileDir(NULL),
    m_nFD(-1)
{}

void CDebugDump::Open(const std::string& pDir)
{
    if (m_bOpened)
    {
        throw CABRTException(EXCEP_ERROR, "CDebugDump::CDebugDump(): DebugDump is already opened.");
    }
    m_sDebugDumpDir = RemoveBackSlashes(pDir);
    if (!ExistFileDir(m_sDebugDumpDir))
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::CDebugDump(): "+m_sDebugDumpDir+" does not exist.");
    }
    Lock();
    m_bOpened = true;
}

bool CDebugDump::Exist(const std::string& pPath)
{
    std::string fullPath = m_sDebugDumpDir + "/" + pPath;
    return ExistFileDir(fullPath);
}


static bool ExistFileDir(const std::string& pPath)
{
    struct stat buf;
    if (stat(pPath.c_str(), &buf) == 0)
    {
        if (S_ISDIR(buf.st_mode) || S_ISREG(buf.st_mode))
        {
            return true;
        }
    }
    return false;
}

bool CDebugDump::GetAndSetLock(const std::string& pLockFile, const std::string& pPID)
{
    int fd = open(pLockFile.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0640);
    if (fd == -1)
    {
        if (errno != EEXIST)
        {
            throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::GetAndSetLock(): cannot create lock file");
        }
        fd = open(pLockFile.c_str(), O_RDONLY);
        if (fd == -1)
        {
            throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::GetAndSetLock(): cannot get lock status");
        }
        char pid[PID_STR_MAX + 1];
        int r = read(fd, pid, sizeof(pid) - 1);
        if (r == -1)
        {
            close(fd);
            throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::GetAndSetLock(): cannot get a pid");
        }
        pid[r] = '\0';
        if (pid == pPID)
        {
            close(fd);
            m_bUnlock = false;
            comm_layer_inner_debug("Lock file '"+pLockFile+"' is locked by same process");
            return true;
        }
        if (lockf(fd, F_TEST, 0) == 0)
        {
            close(fd);
            remove(pLockFile.c_str());
            Delete();
            throw CABRTException(EXCEP_ERROR, "CDebugDump::GetAndSetLock(): dead lock found");
        }
        comm_layer_inner_debug("Lock file '"+pLockFile+"' is locked by another process");
        close(fd);
        return false;
    }

    if (write(fd, pPID.c_str(), pPID.length()) != pPID.length())
    {
        close(fd);
        remove(pLockFile.c_str());
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::GetAndSetLock(): cannot write a pid");
    }
    if (lockf(fd, F_LOCK, 0) == -1)
    {
        close(fd);
        remove(pLockFile.c_str());
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::GetAndSetLock(): cannot get lock file");
    }
    m_nFD = fd;
    m_bUnlock = true;

    comm_layer_inner_debug("Locking '"+pLockFile+"'...");

    return true;
}

void CDebugDump::Lock()
{
    std::string lockFile = m_sDebugDumpDir + ".lock";
    pid_t nPID = getpid();
    std::stringstream ss;
    ss << nPID;
    while (!GetAndSetLock(lockFile, ss.str()))
    {
        usleep(500000);
    }
}

void CDebugDump::UnLock()
{
    std::string lockFile = m_sDebugDumpDir + ".lock";
    if (m_bUnlock)
    {
        comm_layer_inner_debug("UnLocking '"+lockFile+"'...");
        close(m_nFD);
        m_nFD = -1;
        remove(lockFile.c_str());
    }
}

void CDebugDump::Create(const std::string& pDir, const std::string& pUID)
{
    if (m_bOpened)
    {
        throw CABRTException(EXCEP_ERROR, "CDebugDump::CDebugDump(): DebugDump is already opened.");
    }

    m_sDebugDumpDir = RemoveBackSlashes(pDir);
    if (ExistFileDir(m_sDebugDumpDir))
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
    uid_t uid = atoi(pUID.c_str());
    struct passwd* pw = getpwuid(uid);
    gid_t gid = pw ? pw->pw_gid : uid;
    if (chown(m_sDebugDumpDir.c_str(), uid, gid) == -1)
    {
        /* if /var/cache/abrt is writable by all, _aborting_ here is not useful */
        /* let's just warn */
        perror_msg("can't change '%s' ownership to %u:%u", m_sDebugDumpDir.c_str(), (int)uid, (int)gid);
    }

    SaveText(FILENAME_UID, pUID);
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
    if (!ExistFileDir(m_sDebugDumpDir))
    {
        return;
    }
    DeleteFileDir(m_sDebugDumpDir);
}

void CDebugDump::Close()
{
    UnLock();
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
    std::stringstream ss;
    time_t t = time(NULL);
    ss << t;
    SaveText(FILENAME_TIME, ss.str());
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

void CDebugDump::LoadText(const std::string& pName, std::string& pData)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::LoadText(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    LoadTextFile(fullPath, pData);
}
void CDebugDump::LoadBinary(const std::string& pName, char** pData, unsigned int* pSize)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::LoadBinary(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    LoadBinaryFile(fullPath, pData, pSize);
}

void CDebugDump::SaveText(const std::string& pName, const std::string& pData)
{
    if (!m_bOpened)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::SaveText(): DebugDump is not opened.");
    }
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    SaveTextFile(fullPath, pData);
}
void CDebugDump::SaveBinary(const std::string& pName, const char* pData, const unsigned int pSize)
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
        m_pGetNextFileDir= NULL;
    }
    m_pGetNextFileDir = opendir(m_sDebugDumpDir.c_str());
    if (m_pGetNextFileDir == NULL)
    {
        throw CABRTException(EXCEP_DD_OPEN, "CDebugDump::InitGetNextFile(): Cannot open dir " + m_sDebugDumpDir);
    }
}

bool CDebugDump::GetNextFile(std::string& pFileName, std::string& pContent, bool& pIsTextFile)
{
    struct dirent *dent;

    if (m_pGetNextFileDir == NULL)
    {
        return false;
    }
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
                LoadText(pFileName, pContent);
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
    return false;
}

