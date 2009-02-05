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

#define FILENAME_ARCHITECTURE       "architecture"
#define FILENAME_KERNEL             "kernel"
#define FILENAME_EXECUTABLE         "executable"
#define FILENAME_TIME               "time"
#define FILENAME_UID                "uid"
#define FILENAME_PACKAGE            "package"
#define FILENAME_HASH               "hash"
#define FILENAME_LANGUAGE           "language"
#define FILENAME_APPLICATION        "application"
#define FILENAME_TEXTDATA1          "text_data1"
#define FILENAME_BINARYDATA1        "binary_data1"

class CDebugDump
{
    private:
        std::string m_sDebugDumpDir;

        void SaveEnvironment();


        void LoadTextFile(const std::string& pName, std::string& pData);
        void LoadBinaryFile(const std::string& pName, char** pData, unsigned int* pSize);

        void SaveTextFile(const std::string& pName, const std::string& pData);
        void SaveBinaryFile(const std::string& pName, const char* pData, const unsigned int pSize);
        bool ExistFileDir(const std::string& pPath);

    public:

        CDebugDump();
        void Open(const std::string& pDir);
        void Create(const std::string& pDir);
        void Delete(const std::string& pDir);
        bool Exist(const std::string& pPath);

        void LoadText(const std::string& pName, std::string& pData);
        void LoadBinary(const std::string& pName, char** pData, unsigned int* pSize);

        void SaveText(const std::string& pName, const std::string& pData);
        void SaveBinary(const std::string& pName, const char* pData, const unsigned int pSize);

        void SaveProc(const std::string& pPID);
};

#endif /*DEBUGDUMP_H_*/
