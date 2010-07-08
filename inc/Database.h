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

/**
 * Table
 * =====
 * UUID | UID| DebugDumpPath | Count | Reported | Time | Message
 *
 * primary key (UUID, UID)
 */

/**
 * A struct contains one database row.
 */
typedef struct database_row_t
{
    std::string m_sUUID; /**< A local UUID.*/
    std::string m_sUID; /**< An UID of an user.*/
    std::string m_sInformAll;
    std::string m_sDebugDumpDir; /**< A debugdump directory of a crash.*/
    std::string m_sCount; /**< Crash rate.*/
    std::string m_sReported; /**< Is a row reported?*/
    std::string m_sMessage; /**< if a row is reported, then there can be store message abotu that*/
    std::string m_sTime; /**< Time of last occurred crash with same local UUID*/
} database_row_t;

/**
 * A vector contains one or more database rows.
 */
typedef std::vector<database_row_t> vector_database_rows_t;

/**
 * An abstract class. The class defines a database plugin interface.
 */
class CDatabase : public CPlugin
{
    public:
        /**
         * A method, which connects to a database.
         */
        virtual void Connect() = 0;
        /**
         * A method, which disconnects from a database.
         */
        virtual void DisConnect() = 0;
        /**
         * A method, which inserts one row to a database.
         * @param pUUID A local UUID of a crash.
         * @param pUID An UID of an user.
         * @param pDebugDumpPath A debugdump path.
         * @param pTime Time when a crash occurs.
         */
        virtual void Insert_or_Update(const char *crash_id,
                        bool inform_all_users,
                        const char *pDebugDumpPath,
                        const char *pTime) = 0;
        /**
         * A method, which deletes one row in a database.
         * @param pUUID A lodal UUID of a crash.
         * @param pUID An UID of an user.
         */
        virtual void DeleteRow(const char *crash_id) = 0;
        virtual void DeleteRows_by_dir(const char *dump_dir) = 0;
        /**
         * A method, which sets that particular row was reported.
         * @param pUUID A local UUID of a crash.
         * @param pUID An UID of an user.
         * @param pMessage A text explanation of reported problem
         * (where it is stored etc)...
         */
        virtual void SetReported(const char *crash_id,
                                 const char *pMessage) = 0;
        virtual void SetReportedPerReporter(const char *crash_id,
                                 const char *reporter,
                                 const char *pMessage) = 0;
        /**
         * A method, which gets all rows which belongs to particular user.
         * If the user is root, then all rows are returned. If there are no
         * rows, empty vector is returned.
         * @param pUID An UID of an user.
         * @return A vector of matched rows.
         */
        virtual vector_database_rows_t GetUIDData(long caller_uid) = 0;
        /**
         * A method, which returns one row accordind to UUID of a crash and
         * UID of an user. If there are no row, empty row is returned.
         * @param pUUID A UUID of a crash.
         * @param pUID An UID of an user.
         * @return A matched row.
         */
        virtual database_row_t GetRow(const char *crash_id) = 0;
};

#endif
