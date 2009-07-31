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

#ifndef SQLITE3_H_
#define SQLITE3_H_

#include "Plugin.h"
#include "Database.h"

class CSQLite3 : public CDatabase
{
    private:

        std::string m_sDBPath;
        sqlite3* m_pDB;

        bool OpenDB();
        bool CheckTable();
        void CreateDB();
        void CreateTable();
        void Exec(const std::string& pCommand);
        void GetTable(const std::string& pCommand, vector_database_rows_t& pTable);
        bool Exist(const std::string& pUUID, const std::string& pUID);

    public:
        CSQLite3();
        virtual ~CSQLite3() {}

        virtual void Connect();
        virtual void DisConnect();

        virtual void Insert(const std::string& pUUID,
                            const std::string& pUID,
                            const std::string& pDebugDumpPath,
                            const std::string& pTime);

        virtual void Delete(const std::string& pUUID, const std::string& pUID);
        virtual void SetReported(const std::string& pUUID, const std::string& pUID);
        virtual const vector_database_rows_t GetUIDData(const std::string& pUID);
        virtual const database_row_t GetUUIDData(const std::string& pUUID, const std::string& pUID);

        virtual void LoadSettings(const std::string& pPath);
};

#endif /* SQLITE3_H_ */
