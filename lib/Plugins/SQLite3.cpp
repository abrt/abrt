/*
    SQLite3.cpp

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

#include <sqlite3.h>
#include <string>
#include <stdlib.h>
#include "SQLite3.h"
#include "ABRTException.h"
#include <limits.h>
#include <abrtlib.h>

using namespace std;

#define ABRT_TABLE_VERSION      2
#define ABRT_TABLE_VERSION_STR "2"
#define ABRT_TABLE             "abrt_v"ABRT_TABLE_VERSION_STR
#define SQLITE3_MASTER_TABLE   "sqlite_master"

#define COL_UUID               "UUID"
#define COL_UID                "UID"
#define COL_DEBUG_DUMP_PATH    "DebugDumpPath"
#define COL_COUNT              "Count"
#define COL_REPORTED           "Reported"
#define COL_TIME               "Time"
#define COL_MESSAGE            "Message"

// after a while, we can drop support for update, so a table can stay in
// normal limits
static const char *const upate_sql_commands[][ABRT_TABLE_VERSION + 1] = {
    // v0 -> *
    {
        // v0 -> v0
        ";",
        // v0 -> v1
        "ALTER TABLE abrt ADD "COL_MESSAGE" VARCHAR NOT NULL DEFAULT '';",
        // v0 -> v2
        "BEGIN TRANSACTION;"
        "ALTER TABLE abrt RENAME TO abrt_v2;"
        "ALTER TABLE abrt_v2 ADD "COL_MESSAGE" VARCHAR NOT NULL DEFAULT '';"
        "COMMIT;",

    },
    //v1 -> *
    {
        // v1 -> v0
        // TODO: does it make sense to support downgrade?
        ";",
        // v1 -> v1
        ";",
        // v1 -> v2
        "BEGIN TRANSACTION;"
        "CREATE TABLE abrt_v2 ("
                 COL_UUID" VARCHAR NOT NULL,"
                 COL_UID" VARCHAR NOT NULL,"
                 COL_DEBUG_DUMP_PATH" VARCHAR NOT NULL,"
                 COL_COUNT" INT NOT NULL DEFAULT 1,"
                 COL_REPORTED" INT NOT NULL DEFAULT 0,"
                 COL_TIME" VARCHAR NOT NULL DEFAULT 0,"
                 COL_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
                 "PRIMARY KEY ("COL_UUID","COL_UID"));"
        "INSERT INTO abrt_v2 "
            "SELECT "COL_UUID","
                     COL_UID","
                     COL_DEBUG_DUMP_PATH","
                     COL_COUNT","
                     COL_REPORTED","
                     COL_TIME","
                     COL_MESSAGE
            " FROM abrt;"
        "DROP TABLE abrt;"
        "COMMIT;",
    },
};


/* Is this string safe wrt SQL injection?
 * PHP's mysql_real_escape_string() treats \, ', ", \x00, \n, \r, and \x1a as special.
 * We are a bit more paranoid and disallow any control chars.
 */
static bool is_string_safe(const char *str)
{
    const char *p = str;
    while (*p)
    {
        if ((unsigned char)(*p) < ' ' || strchr("\\\"\'", *p))
        {
            error_msg("Probable SQL injection: '%s'", str);
            return false;
        }
        p++;
    }
    return true;
}

static void get_table(vector_database_rows_t& pTable,
                sqlite3 *db, const char *fmt, ...)
{
    va_list p;
    va_start(p, fmt);
    char *sql = xvasprintf(fmt, p);
    va_end(p);

    char **table;
    int ncol, nrow;
    char *err = NULL;
    int ret = sqlite3_get_table(db, sql, &table, &nrow, &ncol, &err);
    if (ret != SQLITE_OK)
    {
        string errstr = ssprintf("Error in SQL:'%s' error: %s", sql, err);
        free(sql);
        sqlite3_free(err);
        throw CABRTException(EXCEP_PLUGIN, errstr.c_str());
    }
    VERB2 log("%d rows returned by SQL:%s", nrow, sql);
    free(sql);

    pTable.clear();
    int ii;
    for (ii = 0; ii < nrow; ii++)
    {
        int jj;
        database_row_t row;
        for (jj = 0; jj < ncol; jj++)
        {
            char *val = table[jj + (ncol*ii) + ncol];
            switch (jj)
            {
                case 0: row.m_sUUID         = val; break;
                case 1: row.m_sUID          = val; break;
                case 2: row.m_sDebugDumpDir = val; break;
                case 3: row.m_sCount        = val; break;
                case 4: row.m_sReported     = val; break;
                case 5: row.m_sTime         = val; break;
                case 6: row.m_sMessage      = val; break;
            }
        }
        pTable.push_back(row);

    }
    sqlite3_free_table(table);
}

static void execute_sql(sqlite3 *db, const char *fmt, ...)
{
    va_list p;
    va_start(p, fmt);
    char *sql = xvasprintf(fmt, p);
    va_end(p);

    char *err = NULL;
    int ret = sqlite3_exec(db, sql, /*callback:*/ NULL, /*callback param:*/ NULL, &err);
    if (ret != SQLITE_OK)
    {
        string errstr = ssprintf("Error in SQL:'%s' error: %s", sql, err);
        free(sql);
        sqlite3_free(err);
        throw CABRTException(EXCEP_PLUGIN, errstr.c_str());
    }
    VERB2 log("%d rows affected by SQL:%s", sqlite3_changes(db), sql);
    free(sql);
}

static bool exists_uuid_uid(sqlite3 *db, const char *pUUID, const char *pUID)
{
    vector_database_rows_t table;
    get_table(table, db,
                "SELECT "COL_REPORTED" FROM "ABRT_TABLE" WHERE "
                COL_UUID" = '%s' "
                "AND ("COL_UID" = '%s' OR "COL_UID" = '-1');",
                pUUID, pUID
    );
    return !table.empty();
}

static void update_from_old_ver(sqlite3 *db, int pOldVersion)
{
    execute_sql(db, upate_sql_commands[pOldVersion][ABRT_TABLE_VERSION]);
}

static bool check_table(sqlite3 *db)
{
    const char *command = "SELECT NAME, SQL FROM "SQLITE3_MASTER_TABLE" "
                          "WHERE TYPE='table';";
    char **table;
    int ncol, nrow;
    char *err;
    int ret = sqlite3_get_table(db, command, &table, &nrow, &ncol, &err);
    if (ret != SQLITE_OK)
    {
        /* Should never happen */
        error_msg_and_die("SQLite3 database is corrupted");
    }
    if (!nrow)
    {
        return false;
    }

    string tableName = table[0 + ncol];
    string::size_type pos = tableName.find("_");
    if (pos != string::npos)
    {
        string tableVersion = tableName.substr(pos + 2);
        if (xatoi_u(tableVersion.c_str()) < ABRT_TABLE_VERSION)
        {
            update_from_old_ver(db, xatoi_u(tableVersion.c_str()));
        }
        return true;
    }

    // TODO: after some time could be removed, and if the table is that old,
    // then simply drop it and create new one

    // hack for version 0 and 1
    string sql = table[1 + ncol];
    if (sql.find(COL_MESSAGE) != string::npos)
    {
        update_from_old_ver(db, 1);
        return true;
    }
    update_from_old_ver(db, 0);
    return true;
}
/*
static bool check_table()
{
    vector_database_rows_t table;
    get_table(table, m_pDB,
             "SELECT NAME FROM "SQLITE3_MASTER_TABLE" "
             "WHERE TYPE='table' AND NAME='"ABRT_TABLE"';");

    return table.size() == 1;
}
*/


CSQLite3::CSQLite3() :
    m_sDBPath(LOCALSTATEDIR "/cache/abrt/abrt-db"),
    m_pDB(NULL)
{}

CSQLite3::~CSQLite3()
{
    /* Paranoia. In C++, destructor will abort() if it was called while unwinding
     * the stack and it throws an exception.
     */
    try
    {
        DisConnect();
        m_sDBPath.clear();
    }
    catch (...)
    {
        error_msg_and_die("Internal error");
    }
}

void CSQLite3::DisConnect()
{
    if (m_pDB)
    {
        sqlite3_close(m_pDB);
        m_pDB = NULL;
    }
}

void CSQLite3::Connect()
{
    int ret = sqlite3_open_v2(m_sDBPath.c_str(),
                &m_pDB,
                SQLITE_OPEN_READWRITE,
                NULL
    );

    if (ret != SQLITE_OK)
    {
        if (ret != SQLITE_CANTOPEN)
        {
            throw CABRTException(EXCEP_PLUGIN, "Can't open database '%s': %s", m_sDBPath.c_str(), sqlite3_errmsg(m_pDB));
        }

        ret = sqlite3_open_v2(m_sDBPath.c_str(),
                &m_pDB,
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                NULL
        );
        if (ret != SQLITE_OK)
        {
            throw CABRTException(EXCEP_PLUGIN, "Can't create database '%s': %s", m_sDBPath.c_str(), sqlite3_errmsg(m_pDB));
        }
    }

    if (!check_table(m_pDB))
    {
        execute_sql(m_pDB,
                "CREATE TABLE "ABRT_TABLE" ("
                COL_UUID" VARCHAR NOT NULL,"
                COL_UID" VARCHAR NOT NULL,"
                COL_DEBUG_DUMP_PATH" VARCHAR NOT NULL,"
                COL_COUNT" INT NOT NULL DEFAULT 1,"
                COL_REPORTED" INT NOT NULL DEFAULT 0,"
                COL_TIME" VARCHAR NOT NULL DEFAULT 0,"
                COL_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
                "PRIMARY KEY ("COL_UUID","COL_UID"));"
        );
    }
}

void CSQLite3::Insert_or_Update(const char *pUUID,
                const char *pUID,
                const char *pDebugDumpPath,
                const char *pTime)
{
    if (!is_string_safe(pUUID)
     || !is_string_safe(pUID)
     || !is_string_safe(pDebugDumpPath)
     || !is_string_safe(pTime)
    ) {
        return;
    }

    if (!exists_uuid_uid(m_pDB, pUUID, pUID))
    {
        execute_sql(m_pDB,
                "INSERT INTO "ABRT_TABLE" ("
                COL_UUID","
                COL_UID","
                COL_DEBUG_DUMP_PATH","
                COL_TIME
                ")"
                " VALUES ('%s','%s','%s','%s');",
                pUUID, pUID, pDebugDumpPath, pTime
        );
    }
    else
    {
        execute_sql(m_pDB,
                "UPDATE "ABRT_TABLE" SET "
                COL_COUNT" = "COL_COUNT" + 1, "
                COL_TIME" = '%s'"
                " WHERE "COL_UUID" = '%s'"
                    " AND "COL_UID" = '%s';",
                pTime, pUUID, pUID
        );
    }
}

void CSQLite3::DeleteRow(const char *pUUID, const char *pUID)
{
    if (!is_string_safe(pUUID)
     || !is_string_safe(pUID)
    ) {
        return;
    }

    if (pUID[0] == '0' && !pUID[1])
    {
        execute_sql(m_pDB,
                "DELETE FROM "ABRT_TABLE" "
                "WHERE "COL_UUID" = '%s';",
                pUUID
        );
    }
    else if (exists_uuid_uid(m_pDB, pUUID, pUID))
    {
        execute_sql(m_pDB, "DELETE FROM "ABRT_TABLE" "
                "WHERE "COL_UUID" = '%s' "
                "AND ("COL_UID" = '%s' OR "COL_UID" = '-1');",
                pUUID, pUID
        );
    }
    else
    {
        error_msg("UUID,UID %s,%s is not found in DB", pUUID, pUID);
    }
}

void CSQLite3::DeleteRows_by_dir(const char *dump_dir)
{
    if (!is_string_safe(dump_dir))
    {
        return;
    }

    execute_sql(m_pDB,
                "DELETE FROM "ABRT_TABLE" "
                "WHERE "COL_DEBUG_DUMP_PATH" = '%s'",
                dump_dir
    );
}

void CSQLite3::SetReported(const char *pUUID, const char *pUID, const char *pMessage)
{
    if (!is_string_safe(pUUID)
     || !is_string_safe(pUID)
     || !is_string_safe(pMessage)
    ) {
        return;
    }

    if (pUID[0] == '0' && !pUID[1])
    {
        execute_sql(m_pDB,
                "UPDATE "ABRT_TABLE" "
                "SET "COL_REPORTED" = 1 "
                "WHERE "COL_UUID" = '%s';",
                pUUID
        );
        execute_sql(m_pDB, "UPDATE "ABRT_TABLE" "
                "SET "COL_MESSAGE" = '%s' "
                "WHERE "COL_UUID" = '%s';",
                pMessage, pUUID
        );
    }
    else if (exists_uuid_uid(m_pDB, pUUID, pUID))
    {
        execute_sql(m_pDB,
                "UPDATE "ABRT_TABLE" "
                "SET "COL_REPORTED" = 1 "
                "WHERE "COL_UUID" = '%s' "
                "AND ("COL_UID" = '%s' OR "COL_UID" = '-1');",
                pUUID, pUID
        );
        execute_sql(m_pDB,
                "UPDATE "ABRT_TABLE" "
                "SET "COL_MESSAGE" = '%s' "
                "WHERE "COL_UUID" = '%s' "
                "AND ("COL_UID" = '%s' OR "COL_UID" = '-1');",
                pMessage, pUUID, pUID
        );
    }
    else
    {
        error_msg("UUID,UID %s,%s is not found in DB", pUUID, pUID);
    }
}

vector_database_rows_t CSQLite3::GetUIDData(const char *pUID)
{
    vector_database_rows_t table;

    if (!is_string_safe(pUID))
    {
        return table;
    }

    if (pUID[0] == '0' && !pUID[1])
    {
        get_table(table, m_pDB, "SELECT * FROM "ABRT_TABLE";");
    }
    else
    {
        get_table(table, m_pDB,
                "SELECT * FROM "ABRT_TABLE
                " WHERE "COL_UID" = '%s' OR "COL_UID" = '-1';",
                pUID
        );
    }
    return table;
}

database_row_t CSQLite3::GetRow(const char *pUUID, const char *pUID)
{
    if (!is_string_safe(pUUID)
     || !is_string_safe(pUID)
    ) {
        return database_row_t();
    }

    vector_database_rows_t table;

    if (pUID[0] == '0' && !pUID[1])
    {
        get_table(table, m_pDB,
                "SELECT * FROM "ABRT_TABLE" "
                "WHERE "COL_UUID" = '%s';",
                pUUID
        );
    }
    else
    {
        get_table(table, m_pDB,
                "SELECT * FROM "ABRT_TABLE" "
                "WHERE "COL_UUID" = '%s' "
                "AND ("COL_UID" = '%s' OR "COL_UID" = '-1');",
                pUUID, pUID
        );
    }

    if (table.size() == 0)
    {
        return database_row_t();
    }
    return table[0];
}

void CSQLite3::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("DBPath");
    if (it != end)
    {
        m_sDBPath = it->second;
    }
}

//ok to delete?
//const map_plugin_settings_t& CSQLite3::GetSettings()
//{
//    m_pSettings["DBPath"] = m_sDBPath;
//
//    return m_pSettings;
//}

PLUGIN_INFO(DATABASE,
            CSQLite3,
            "SQLite3",
            "0.0.2",
            "Keeps SQLite3 database about all crashes",
            "zprikryl@redhat.com,jmoskovc@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
