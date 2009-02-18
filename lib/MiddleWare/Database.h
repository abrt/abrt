/*
    Database.h - header file for database plugin

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


#ifndef DATABASE_H_
#define DATABASE_H_

#include <string>
#include <vector>
#include "Plugin.h"

/*
 * Table
 * =====
 * UUID | UID| DebugDumpPath | Count | Reported
 *
 * primary key (UUID, UID)
 */

#define DATABASE_COLUMN_UUID            "UUID"
#define DATABASE_COLUMN_UID             "UID"
#define DATABASE_COLUMN_DEBUG_DUMP_PATH "DebugDumpPath"
#define DATABASE_COLUMN_COUNT           "Count"
#define DATABASE_COLUMN_REPORTED        "Reported"
#define DATABASE_COLUMN_TIME            "Time"

typedef struct SDatabaseRow
{
    std::string m_sUUID;
    std::string m_sUID;
    std::string m_sDebugDumpDir;
    std::string m_sCount;
    std::string m_sReported;
    std::string m_sTime;
} database_row_t;

// <column_name, <array of values in all selected rows> >
typedef std::vector<database_row_t> vector_database_rows_t;

class CDatabase : public CPlugin
{
    public:
        virtual ~CDatabase() {}

        virtual void Connect() = 0;
        virtual void DisConnect() = 0;
        virtual void Insert(const std::string& pUUID,
                            const std::string& pUID,
                            const std::string& pDebugDumpPath,
                            const std::string& pTime) = 0;

        virtual void Delete(const std::string& pUUID, const std::string& pUID) = 0;

        virtual void SetReported(const std::string& pUUID, const std::string& pUID) = 0;
        virtual const vector_database_rows_t GetUIDData(const std::string& pUID) = 0;
        virtual const database_row_t GetUUIDData(const std::string& pUUID, const std::string& pUID) = 0;
};

#endif /* DATABASE_H_ */
