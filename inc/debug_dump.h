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

class CDebugDump
{
    private:
        std::string m_sDebugDumpDir;
        DIR* m_pGetNextFileDir;
        bool m_bOpened;
        bool m_bLocked;
        uid_t m_uid;
        gid_t m_gid;

        void Lock();
        void UnLock();

    public:
        CDebugDump();
        ~CDebugDump();

        bool Open(const char *pDir);
        bool Create(const char *pDir, uid_t uid);
        void Delete();
        void Close();

        bool Exist(const char* pFileName);

        void LoadText(const char* pName, std::string& pData);

        void SaveText(const char* pName, const char *pData);
        void SaveBinary(const char* pName, const char* pData, unsigned pSize);

        bool InitGetNextFile();
        /* Pointers may be NULL */
        bool GetNextFile(std::string *short_name, std::string *full_name);
};

/**
 * Deletes particular debugdump directory.
 * @param pDebugDumpDir A debugdump directory.
 */
void delete_debug_dump_dir(const char *pDebugDumpDir);

#endif
