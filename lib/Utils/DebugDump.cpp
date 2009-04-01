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

#include "DebugDump.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <limits.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <magic.h>
#include <string.h>

CDebugDump::CDebugDump() :
    m_sDebugDumpDir(""),
    m_bOpened(false),
    m_bUnlock(true),
    m_pGetNextFileDir(NULL)
{}

void CDebugDump::Open(const std::string& pDir)
{
    if (m_bOpened)
    {
        throw std::string("CDebugDump::CDebugDump(): DebugDump is already opened.");
    }
    m_sDebugDumpDir = pDir;
    std::string lockPath = m_sDebugDumpDir + "/.lock";
    if (!ExistFileDir(pDir))
    {
        throw "CDebugDump::CDebugDump(): "+pDir+" does not exist.";
    }
    Lock();
    m_bOpened = true;
}

bool CDebugDump::Exist(const std::string& pPath)
{
    std::string fullPath = m_sDebugDumpDir + "/" + pPath;
    return ExistFileDir(fullPath);
}


bool CDebugDump::ExistFileDir(const std::string& pPath)
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
    std::ifstream fIn;
    std::ofstream fOut;

    fIn.open(pLockFile.c_str());
    if (!fIn.is_open())
    {
        fOut.open(pLockFile.c_str());
        fOut << pPID;
        fOut.close();
        m_bUnlock = true;
        return true;
    }
    else
    {
        std::string line;
        std::stringstream ss;
        getline(fIn, line);
        if (line == pPID)
        {
            m_bUnlock = false;
            return true;
        }
        ss << "/proc/" << line << "/";
        if (!ExistFileDir(ss.str()))
        {
            remove(pLockFile.c_str());
            Delete();
            throw std::string("CDebugDump::GetAndSetLock(): dead lock found");
        }
        fIn.close();
        return false;
    }
}

void CDebugDump::Lock()
{
    std::string lockPath = m_sDebugDumpDir + ".lock";
    pid_t nPID = getpid();
    std::stringstream ss;
    ss << nPID;
    std::cerr << "CDebugDump::Lock(): waiting...";
    while (!GetAndSetLock(lockPath, ss.str()))
    {
        std::cerr << ".";
        usleep(5000);
    }
    std::cerr << "done." << std::endl;
}

void CDebugDump::UnLock()
{
    std::string lockPath = m_sDebugDumpDir + ".lock";
    if (m_bUnlock)
    {
        remove(lockPath.c_str());
    }
}

void CDebugDump::Create(const std::string& pDir)
{
    if (m_bOpened)
    {
        throw std::string("CDebugDump::CDebugDump(): DebugDump is already opened.");
    }

    m_sDebugDumpDir = pDir;
    std::string lockPath = pDir + ".lock";
    if (ExistFileDir(pDir))
    {
        throw "CDebugDump::CDebugDump(): "+pDir+" already exists.";
    }

    Lock();
    m_bOpened = true;

    if (mkdir(pDir.c_str(), 0755) == -1)
    {
        throw "CDebugDump::Create(): Cannot create dir: " + pDir;
    }

    SaveKernelArchitectureRelease();
    SaveTime();
}

void CDebugDump::DeleteFileDir(const std::string& pDir)
{
    if (!ExistFileDir(pDir))
    {
        return;
    }
    DIR *dir = opendir(pDir.c_str());
    std::string fullPath;
    struct dirent *dent = NULL;
    if (dir != NULL)
    {
        while ((dent = readdir(dir)) != NULL)
        {
            if (std::string(dent->d_name) != "." && std::string(dent->d_name) != "..")
            {
                fullPath = pDir + "/" + dent->d_name;
                if (dent->d_type == DT_DIR)
                {
                    DeleteFileDir(fullPath);
                }
                if (remove(fullPath.c_str()) == -1)
                {
                    throw "CDebugDump::DeleteFileDir(): Cannot remove file: " + fullPath;
                }
            }
        }
        closedir(dir);
        if (remove(pDir.c_str()) == -1)
        {
            throw "CDebugDump::DeleteFileDir(): Cannot remove dir: " + fullPath;
        }
    }
}

bool CDebugDump::IsTextFile(const std::string& pName)
{
    bool isText = false;
    magic_t m = magic_open(MAGIC_MIME);

    if (m == NULL)
    {
        throw std::string("CDebugDump::IsTextFile(): Cannot open magic cookie: ") + magic_error(m);
    }

    int r = magic_load(m,NULL);

    if (r == -1)
    {
        throw std::string("CDebugDump::IsTextFile(): Cannot load magic db: ") + magic_error(m);
    }

    char* ch = (char *) magic_file(m, pName.c_str());

    if (ch == NULL)
    {
        throw std::string("CDebugDump::IsTextFile(): Cannot determine file type: ") + magic_error(m);
    }

    if (!strncmp(ch, "text", 4))
    {
        isText = true;
    }

    magic_close(m);
    return isText;
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
    if (((time_t) -1) == t)
    {
        throw std::string("CDebugDump::SaveTime(): Cannot get local time.");
    }
    ss << t;
    SaveText(FILENAME_TIME, ss.str());
}

void CDebugDump::LoadTextFile(const std::string& pPath, std::string& pData)
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
        throw "CDebugDump: LoadTextFile(): Cannot open file " + pPath;
    }
}

void CDebugDump::LoadBinaryFile(const std::string& pPath, char** pData, unsigned int* pSize)
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
        throw "CDebugDump: LoadBinaryFile(): Cannot open file " + pPath;
    }
}


void CDebugDump::SaveTextFile(const std::string& pPath, const std::string& pData)
{
    std::ofstream fOut;
    fOut.open(pPath.c_str());
    if (fOut.is_open())
    {
        fOut << pData;
        if (!fOut.good())
        {
            throw "CDebugDump: SaveTextFile(): Cannot save file " + pPath;
        }
        fOut.close();
    }
    else
    {
        throw "CDebugDump: SaveTextFile(): Cannot open file " + pPath;
    }
}

void CDebugDump::SaveBinaryFile(const std::string& pPath, const char* pData, const unsigned pSize)
{
    std::ofstream fOut;
    fOut.open(pPath.c_str(), std::ios::binary);
    if (fOut.is_open())
    {
        fOut.write(pData, pSize);
        if (!fOut.good())
        {
            throw "CDebugDump: SaveBinaryFile(): Cannot save file " + pPath;
        }
        fOut.close();
    }
    else
    {
        throw "CDebugDump: SaveBinaryFile(): Cannot open file " + pPath;
    }
}

void CDebugDump::LoadText(const std::string& pName, std::string& pData)
{
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    LoadTextFile(fullPath, pData);
}
void CDebugDump::LoadBinary(const std::string& pName, char** pData, unsigned int* pSize)
{
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    LoadBinaryFile(fullPath, pData, pSize);
}

void CDebugDump::SaveText(const std::string& pName, const std::string& pData)
{
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    SaveTextFile(fullPath, pData);
}
void CDebugDump::SaveBinary(const std::string& pName, const char* pData, const unsigned int pSize)
{
    std::string fullPath = m_sDebugDumpDir + "/" + pName;
    SaveBinaryFile(fullPath, pData, pSize);
}

void CDebugDump::InitGetNextFile()
{
    if (m_pGetNextFileDir != NULL)
    {
        closedir(m_pGetNextFileDir);
        m_pGetNextFileDir= NULL;
    }
    m_pGetNextFileDir = opendir(m_sDebugDumpDir.c_str());
    if (m_pGetNextFileDir == NULL)
    {
        throw "CDebugDump::InitGetNextFile(): Cannot open dir " + m_sDebugDumpDir;
    }
}

bool CDebugDump::GetNextFile(std::string& pFileName, std::string& pContent, bool& pIsTextFile)
{
    static struct dirent *dent = NULL;

    if (m_pGetNextFileDir == NULL)
    {
        false;
    }
    while ((dent = readdir(m_pGetNextFileDir)) != NULL)
    {
        if (dent->d_type == DT_REG)
        {
            pFileName = dent->d_name;
            if (IsTextFile(m_sDebugDumpDir + "/" + pFileName))
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

