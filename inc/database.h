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

#include <glib.h>

/**
 * Table
 * =====
 * UUID | UID| DebugDumpPath | Count | Reported | Time | Message
 *
 * primary key (UUID, UID)
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A struct contains one database row.
 */
struct db_row
{
    char *db_uuid; /**< A local UUID.*/
    char *db_uid; /**< An UID of an user.*/
    char *db_inform_all;
    char *db_dump_dir; /**< A debugdump directory of a crash.*/
    char *db_count; /**< Crash rate.*/
    char *db_reported; /**< Is a row reported?*/
    char *db_message; /**< if a row is reported, then there can be store message abotu that*/
    char *db_time; /**< Time of last occurred crash with same local UUID*/
};

void db_row_free(struct db_row *row);

void db_list_free(GList *list);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <string>
#include <vector>

#include "plugin.h"

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
        virtual GList *GetUIDData(long caller_uid) = 0;
        /**
         * A method, which returns one row accordind to UUID of a crash and
         * UID of an user. If there are no row, empty row is returned.
         * @param pUUID A UUID of a crash.
         * @param pUID An UID of an user.
         * @return A matched row.
         */
        virtual struct db_row *GetRow(const char *crash_id) = 0;
};
#endif

#endif
