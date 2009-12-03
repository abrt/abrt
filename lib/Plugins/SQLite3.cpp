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


CSQLite3::CSQLite3() :
    m_sDBPath(LOCALSTATEDIR "/cache/abrt/abrt-db"),
    m_pDB(NULL)
{}


void CSQLite3::UpdateABRTTable(const int pOldVersion)
{
    Exec(upate_sql_commands[pOldVersion][ABRT_TABLE_VERSION]);
}


bool CSQLite3::Exist(const string& pUUID, const string& pUID)
{
    vector_database_rows_t table;
    GetTable("SELECT "COL_REPORTED" FROM "ABRT_TABLE" WHERE "
             COL_UUID" = '"+pUUID+"' "
             "AND ("COL_UID" = '"+pUID+"' "
             "OR "COL_UID" = '-1');"
             , table);
    if (table.empty())
    {
        return false;
    }
    return true;
}

void CSQLite3::Exec(const string& pCommand)
{
    char *err;
    int ret = sqlite3_exec(m_pDB, pCommand.c_str(), 0, 0, &err);
    if (ret != SQLITE_OK)
    {
        throw CABRTException(EXCEP_PLUGIN, "SQLite3::Exec(): Error on: " + pCommand + " " + err);
    }
}

void CSQLite3::GetTable(const string& pCommand, vector_database_rows_t& pTable)
{
    char **table;
    int ncol, nrow;
    char *err;
    int ret = sqlite3_get_table(m_pDB, pCommand.c_str(), &table, &nrow, &ncol, &err);
    if (ret != SQLITE_OK)
    {
            throw CABRTException(EXCEP_PLUGIN, "SQLite3::GetTable(): Error on: " + pCommand + " " + err);
    }
    pTable.clear();
    int ii;
    for (ii = 0; ii < nrow; ii++)
    {
        int jj;
        database_row_t row;
        for (jj = 0; jj < ncol; jj++)
        {
            switch (jj)
            {
                case 0: row.m_sUUID = table[jj + (ncol*ii) + ncol];
                    break;
                case 1: row.m_sUID = table[jj + (ncol*ii) + ncol];
                    break;
                case 2: row.m_sDebugDumpDir = table[jj + (ncol*ii) + ncol];
                    break;
                case 3: row.m_sCount = table[jj + (ncol*ii) + ncol];
                    break;
                case 4: row.m_sReported = table[jj + (ncol*ii) + ncol];
                    break;
                case 5: row.m_sTime = table[jj + (ncol*ii) + ncol];
                    break;
                case 6: row.m_sMessage = table[jj + (ncol*ii) + ncol];
                    break;
                default:
                    break;
            }
        }
        pTable.push_back(row);

    }
    sqlite3_free_table(table);
}


void CSQLite3::Connect()
{
    if (!OpenDB())
    {
        CreateDB();
    }
    if (!CheckTable())
    {
        CreateTable();
    }
}

bool CSQLite3::OpenDB()
{
    int ret = sqlite3_open_v2(m_sDBPath.c_str(),
                              &m_pDB,
                              SQLITE_OPEN_READWRITE,
                              NULL);

    if (ret != SQLITE_OK && ret != SQLITE_CANTOPEN)
    {
        throw CABRTException(EXCEP_PLUGIN, string("SQLite3::CheckDB(): Can't open database. ") + sqlite3_errmsg(m_pDB));
    }
    return ret == SQLITE_OK;
}

void CSQLite3::CreateDB()
{
    int ret = sqlite3_open_v2(m_sDBPath.c_str(),
                              &m_pDB,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                              NULL);
    if (ret != SQLITE_OK)
    {
        throw CABRTException(EXCEP_PLUGIN, ssprintf("Can't create database. SQLite3 error: %s", sqlite3_errmsg(m_pDB)));
    }
}


bool CSQLite3::CheckTable()
{
    const char *command = "SELECT NAME, SQL FROM "SQLITE3_MASTER_TABLE" "
                          "WHERE TYPE='table';";
    char **table;
    int ncol, nrow;
    char *err;
    int ret = sqlite3_get_table(m_pDB, command, &table, &nrow, &ncol, &err);
    if (ret != SQLITE_OK)
    {
        /* Should never happen */
        error_msg_and_die("SQLite3 database is corrupted");
    }
    if (!nrow || !nrow)
    {
        return false;
    }

    string tableName = table[0 + ncol];
    string::size_type pos = tableName.find("_");
    if (pos != string::npos)
    {
        string tableVersion = tableName.substr(pos + 2);
        if (atoi(tableVersion.c_str()) < ABRT_TABLE_VERSION)
        {
            UpdateABRTTable(atoi(tableVersion.c_str()));
        }
        return true;
    }

    // TODO: after some time could be removed, and if a table is that old,
    // then simply drop it and create new one

    // hack for version 0 and 1
    string sql = table[1 + ncol];
    if (sql.find(COL_MESSAGE) != string::npos)
    {
        UpdateABRTTable(1);
        return true;
    }
    UpdateABRTTable(0);
    return true;
}
/*
bool CSQLite3::CheckTable()
{
    vector_database_rows_t table;
    GetTable("SELECT NAME FROM "SQLITE3_MASTER_TABLE" "
             "WHERE TYPE='table' AND NAME='"ABRT_TABLE"';", table);

    return table.size() == 1;
}
*/
void CSQLite3::CreateTable()
{
    Exec("CREATE TABLE "ABRT_TABLE" ("
         COL_UUID" VARCHAR NOT NULL,"
         COL_UID" VARCHAR NOT NULL,"
         COL_DEBUG_DUMP_PATH" VARCHAR NOT NULL,"
         COL_COUNT" INT NOT NULL DEFAULT 1,"
         COL_REPORTED" INT NOT NULL DEFAULT 0,"
         COL_TIME" VARCHAR NOT NULL DEFAULT 0,"
         COL_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
         "PRIMARY KEY ("COL_UUID","COL_UID"));");
}

void CSQLite3::DisConnect()
{
    sqlite3_close(m_pDB);
}

void CSQLite3::Insert_or_Update(const char *pUUID,
                const char *pUID,
                const char *pDebugDumpPath,
                const char *pTime)
{
    if (!Exist(pUUID, pUID))
    {
        string sql = ssprintf(
            "INSERT INTO "ABRT_TABLE
                " ("
                COL_UUID","
                COL_UID","
                COL_DEBUG_DUMP_PATH","
                COL_TIME
                ")"
                " VALUES ('%s','%s','%s','%s');",
            pUUID, pUID, pDebugDumpPath, pTime
        );
        Exec(sql);
    }
    else
    {
        string sql = ssprintf(
            "UPDATE "ABRT_TABLE" SET "
                COL_COUNT" = "COL_COUNT" + 1, "
                COL_TIME" = '%s'"
                " WHERE "COL_UUID" = '%s'"
                    " AND "COL_UID" = '%s';",
            pTime, pUUID, pUID
        );
        Exec(sql);
    }
}

void CSQLite3::DeleteRow(const string& pUUID, const string& pUID)
{
    if (pUID == "0")
    {
        Exec("DELETE FROM "ABRT_TABLE" "
             "WHERE "COL_UUID" = '"+pUUID+"';");
    }
    else if (Exist(pUUID, pUID))
    {
        Exec("DELETE FROM "ABRT_TABLE" "
             "WHERE "COL_UUID" = '"+pUUID+"' "
             "AND ("COL_UID" = '"+pUID+"' "
             "OR "COL_UID" = '-1');");
    }
    else
    {
        throw CABRTException(EXCEP_PLUGIN, "UUID is not found in DB");
    }
}

void CSQLite3::DeleteRows_by_dir(const char *dump_dir)
{
    string sql = ssprintf(
                "DELETE FROM "ABRT_TABLE" "
                "WHERE "COL_DEBUG_DUMP_PATH" = '%s'",
                dump_dir
    );
    Exec(sql);
}

void CSQLite3::SetReported(const string& pUUID, const string& pUID, const string& pMessage)
{
    if (pUID == "0")
    {
        Exec("UPDATE "ABRT_TABLE" "
             "SET "COL_REPORTED" = 1 "
             "WHERE "COL_UUID" = '"+pUUID+"';");
        Exec("UPDATE "ABRT_TABLE" "
             "SET "COL_MESSAGE" = '" + pMessage + "' "
             "WHERE "COL_UUID" = '"+pUUID+"';");
    }
    else if (Exist(pUUID, pUID))
    {
        Exec("UPDATE "ABRT_TABLE" "
             "SET "COL_REPORTED" = 1 "
             "WHERE "COL_UUID" = '"+pUUID+"' "
             "AND ("COL_UID" = '"+pUID+"' "
             "OR "COL_UID" = '-1');");
        Exec("UPDATE "ABRT_TABLE" "
             "SET "COL_MESSAGE" = '" + pMessage + "' "
             "WHERE "COL_UUID" = '"+pUUID+"' "
             "AND ("COL_UID" = '"+pUID+"' "
             "OR "COL_UID" = '-1');");
    }
    else
    {
        throw CABRTException(EXCEP_PLUGIN, "CSQLite3::SetReported(): UUID"+pUID+" is not found in DB.");
    }
}

vector_database_rows_t CSQLite3::GetUIDData(const string& pUID)
{
    vector_database_rows_t table;
    if (pUID == "0")
    {
        GetTable("SELECT * FROM "ABRT_TABLE";", table);
    }
    else
    {
        GetTable("SELECT * FROM "ABRT_TABLE
                 " WHERE "COL_UID" = '"+pUID+"' "
                 "OR "COL_UID" = '-1';",
                 table);
    }
    return table;
}

database_row_t CSQLite3::GetRow(const string& pUUID, const string& pUID)
{
    vector_database_rows_t table;

    if (pUID == "0")
    {
        GetTable("SELECT * FROM "ABRT_TABLE" "
                 "WHERE "COL_UUID" = '"+pUUID+"';",
                 table);
    }
    else
    {
        GetTable("SELECT * FROM "ABRT_TABLE" "
                "WHERE "COL_UUID" = '"+pUUID+"' "
                "AND ("COL_UID" = '"+pUID+"' "
                "OR "COL_UID" = '-1');",
                table);
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
