/*
    SQLite3.h

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

    public:
        CSQLite3();
        ~CSQLite3();

        virtual void Connect();
        virtual void DisConnect();

        virtual void Insert_or_Update(const char *pUUID,
                        const char *pUID,
                        const char *pDebugDumpPath,
                        const char *pTime);

        virtual void DeleteRow(const char *pUUID, const char *pUID);
        virtual void DeleteRows_by_dir(const char *dump_dir);
        virtual void SetReported(const char *pUUID, const char *pUID, const char *pMessage);
        virtual vector_database_rows_t GetUIDData(const char *pUID);
        virtual database_row_t GetRow(const char *pUUID, const char *pUID);

        virtual void SetSettings(const map_plugin_settings_t& pSettings);
};

#endif
