/*
    DebugDump.h - header file for the library caring of writing new reports
                  to the specific directory

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

#ifndef DEBUGDUMP_H_
#define DEBUGDUMP_H_

#include <string>
#include <dirent.h>

#define FILENAME_ARCHITECTURE       "architecture"
#define FILENAME_KERNEL             "kernel"
#define FILENAME_TIME               "time"
#define FILENAME_UID                "uid"
#define FILENAME_PACKAGE            "package"
#define FILENAME_COMPONENT          "component"
#define FILENAME_DESCRIPTION        "description"
#define FILENAME_ANALYZER           "analyzer"
#define FILENAME_RELEASE            "release"
#define FILENAME_EXECUTABLE         "executable"
#define FILENAME_REASON             "reason"


class CDebugDump
{
    private:
        std::string m_sDebugDumpDir;
        bool m_bOpened;
        bool m_bUnlock;
        DIR* m_pGetNextFileDir;
        int m_nFD;

        void SaveKernelArchitectureRelease();
        void SaveTime();

        void Lock();
        bool GetAndSetLock(const char* pLockFile, const std::string& pPID);
        void UnLock();

    public:
        CDebugDump();
        void Open(const std::string& pDir);
        void Create(const std::string& pDir, uid_t nUID);
        void Delete();
        void Close();

        bool Exist(const char* pFileName);

        void LoadText(const char* pName, std::string& pData);
        void LoadBinary(const char* pName, char** pData, unsigned int* pSize);

        void SaveText(const char* pName, const std::string& pData);
        void SaveBinary(const char* pName, const char* pData, const unsigned int pSize);

        void InitGetNextFile();
        bool GetNextFile(std::string& pFileName, std::string& pContent, bool& pIsTextFile);
};

#endif /*DEBUGDUMP_H_*/
