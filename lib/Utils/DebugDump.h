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
#include <stdint.h>

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
#define FILENAME_COMMENT            "comment"
#define FILENAME_REPRODUCE          "reproduce"
#define FILENAME_RATING             "rating"

class CDebugDump
{
    private:
        std::string m_sDebugDumpDir;
        DIR* m_pGetNextFileDir;
        bool m_bOpened;
        bool m_bLocked;

        void SaveKernelArchitectureRelease();

        void Lock();
        void UnLock();

    public:
        CDebugDump();
        ~CDebugDump() { Close(); }

        void Open(const char *pDir);
        void Create(const char *pDir, int64_t uid);
        void Delete();
        void Close();

        bool Exist(const char* pFileName);

        void LoadText(const char* pName, std::string& pData);

        void SaveText(const char* pName, const char *pData);
        void SaveBinary(const char* pName, const char* pData, unsigned pSize);

        void InitGetNextFile();
        bool GetNextFile(std::string& pFileName, std::string& pContent, bool& pIsTextFile);
};

#endif /*DEBUGDUMP_H_*/
