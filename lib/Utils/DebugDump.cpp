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

CDebugDump::CDebugDump() :
    m_sDebugDumpDir(""),
    m_nFD(0),
    m_bUnlock(true)
{}

void CDebugDump::Open(const std::string& pDir)
{
    m_sDebugDumpDir = pDir;
    std::string lockPath = m_sDebugDumpDir + "/.lock";
    if (!ExistFileDir(pDir))
    {
        throw "CDebugDump::CDebugDump(): "+pDir+" does not exist.";
    }
    Lock();
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

void CDebugDump::Lock()
{
    std::string lockPath = m_sDebugDumpDir + ".lock";
    if (ExistFileDir(lockPath))
    {
        int res;
        if ((m_nFD = open(lockPath.c_str(), O_RDWR)) < 0)
        {
            throw std::string("CDebugDump::Lock(): can not create lock file");
        }
        res = lockf(m_nFD, F_TEST, 0);
        if (res < 0)
        {
            throw std::string("CDebugDump::Lock(): cannot lock DebugDump");
        }
        else if (res == 0)
        {
            close(m_nFD);
            m_bUnlock = false;
            return;
        }
    }

    while (ExistFileDir(lockPath))
    {
        std::cerr << "CDebugDump::Lock(): waiting..." << std::endl;
        usleep(10);
    }

    if ((m_nFD = open(lockPath.c_str(), O_RDWR | O_CREAT, 0640)) < 0)
    {
        throw std::string("CDebugDump::Lock(): can not create lock file");
    }
    if (lockf(m_nFD,F_LOCK, 0) < 0)
    {
        remove(lockPath.c_str());
        throw std::string("CDebugDump::Lock(): cannot lock DebugDump");
    }
}

void CDebugDump::UnLock()
{
    std::string lockPath = m_sDebugDumpDir + ".lock";
    if (m_bUnlock)
    {
        lockf(m_nFD,F_ULOCK, 0);
        close(m_nFD);
        remove(lockPath.c_str());
        m_bUnlock = true;
    }
}

void CDebugDump::Create(const std::string& pDir)
{
    m_sDebugDumpDir = pDir;
    std::string lockPath = pDir + ".lock";
    if (ExistFileDir(pDir))
    {
        throw "CDebugDump::CDebugDump(): "+pDir+" already exists.";
    }

    Lock();

    if (mkdir(pDir.c_str(), 0755) == -1)
    {
        throw "CDebugDump::Create(): Cannot create dir: " + pDir;
    }

    SaveEnvironment();
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

void CDebugDump::Delete()
{
    if (!ExistFileDir(m_sDebugDumpDir))
    {
        return;
    }
    Lock();
    DeleteFileDir(m_sDebugDumpDir);
    UnLock();
}

void CDebugDump::Close()
{
    UnLock();
}

void CDebugDump::SaveEnvironment()
{
    struct utsname buf;
    if (uname(&buf) == 0)
    {
        SaveText(FILENAME_KERNEL, buf.release);
        SaveText(FILENAME_ARCHITECTURE, buf.machine);
    }
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
        std::string line;
        while (!fIn.eof())
        {
             getline (fIn,line);
             pData += line;
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


void CDebugDump::SaveProc(const std::string& pPID)
{
    std::string path = "/proc/"+pPID+"/exe";
    std::string data;
    char executable[PATH_MAX];
    int len;

    if ((len = readlink(path.c_str(), executable, PATH_MAX)) != -1)
    {
        executable[len] = '\0';
        SaveText(FILENAME_EXECUTABLE, executable);
    }


    path = "/proc/"+pPID+"/status";
    std::string uid = "";
    int ii = 0;

    LoadTextFile(path, data);
    data = data.substr(data.find("Uid:")+5);

    while (!isspace(data[ii]))
    {
        uid += data[ii];
        ii++;
    }
    SaveText(FILENAME_UID, uid);

    path = "/proc/"+pPID+"/cmdline";
    LoadTextFile(path, data);
    SaveText(FILENAME_CMDLINE, data);
}
