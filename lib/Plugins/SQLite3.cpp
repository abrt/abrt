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

#include <sqlite3.h>
#include "SQLite3.h"
#include <string>
#include <iostream>
#include "ABRTException.h"
#include <stdlib.h>


#define ABRT_TABLE_VERSION      2
#define ABRT_TABLE_VERSION_STR "2"
#define ABRT_TABLE "abrt_v"ABRT_TABLE_VERSION_STR
#define SQLITE3_MASTER_TABLE "sqlite_master"


static const char* upate_sql_commands[][ABRT_TABLE_VERSION + 1] = {
    // v0 -> *
    {
        // v0 -> v0
        ";",
        // v0 -> v1
        "ALTER TABLE abrt ADD "DATABASE_COLUMN_MESSAGE" VARCHAR NOT NULL DEFAULT '';",
        // v0 -> v2
        "ALTER TABLE abrt RENAME TO abrt_v2;"
        "ALTER TABLE abrt_v2 ADD "DATABASE_COLUMN_MESSAGE" VARCHAR NOT NULL DEFAULT '';",

    },
    //v1 -> *
    {
        // v1 -> v0
        // TODO: does it make sense to support downgrade?
        ";",
        // v1 -> v1
        ";",
        // v1 -> v2
        "ALTER TABLE abrt RENAME TO abrt_v2;",
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


bool CSQLite3::Exist(const std::string& pUUID, const std::string& pUID)
{
    vector_database_rows_t table;
    GetTable("SELECT "DATABASE_COLUMN_REPORTED" FROM "ABRT_TABLE" WHERE "
             DATABASE_COLUMN_UUID" = '"+pUUID+"' AND "
             DATABASE_COLUMN_UID" = '"+pUID+"';", table);
    if(table.empty())
    {
            return false;
    }
    return true;
}

void CSQLite3::Exec(const std::string& pCommand)
{
    char *err;
    int ret = sqlite3_exec(m_pDB, pCommand.c_str(), 0, 0, &err);
    if (ret != SQLITE_OK)
    {
            throw CABRTException(EXCEP_PLUGIN, "SQLite3::Exec(): Error on: " + pCommand + " " + err);
    }
}

void CSQLite3::GetTable(const std::string& pCommand, vector_database_rows_t& pTable)
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
                switch(jj)
                {
                    case 0: row.m_sUUID = table[jj +(ncol*ii) + ncol];
                        break;
                    case 1: row.m_sUID = table[jj +(ncol*ii) + ncol];
                        break;
                    case 2: row.m_sDebugDumpDir = table[jj +(ncol*ii) + ncol];
                        break;
                    case 3: row.m_sCount = table[jj +(ncol*ii) + ncol];
                        break;
                    case 4: row.m_sReported = table[jj +(ncol*ii) + ncol];
                        break;
                    case 5: row.m_sTime = table[jj +(ncol*ii) + ncol];
                        break;
                    case 6: row.m_sMessage = table[jj +(ncol*ii) + ncol];
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
        throw CABRTException(EXCEP_PLUGIN, std::string("SQLite3::CheckDB(): Could not open database. ") + sqlite3_errmsg(m_pDB));
    }
    return ret == SQLITE_OK;
}

void CSQLite3::CreateDB()
{
    int ret = sqlite3_open_v2(m_sDBPath.c_str(),
                              &m_pDB,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                              NULL);
    if(ret != SQLITE_OK)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string("SQLite3::Create(): Could not create database. ") + sqlite3_errmsg(m_pDB));
    }
}


bool CSQLite3::CheckTable()
{
    std::string command = "SELECT NAME, SQL FROM "SQLITE3_MASTER_TABLE" "
                          "WHERE TYPE='table';";
    char **table;
    int ncol, nrow;
    char *err;
    int ret = sqlite3_get_table(m_pDB, command.c_str(), &table, &nrow, &ncol, &err);
    if (ret != SQLITE_OK)
    {
            throw CABRTException(EXCEP_PLUGIN, "SQLite3::GetTable(): Error on: " + command + " " + err);
    }
    if (!nrow || !nrow)
    {
        return false;
    }

    std::string tableName = table[0 + ncol];
    std::string::size_type pos = tableName.find("_");
    if (pos != std::string::npos)
    {
        std::string tableVersion = tableName.substr(pos + 2);
        if (atoi(tableVersion.c_str()) < ABRT_TABLE_VERSION)
        {
            UpdateABRTTable(atoi(tableVersion.c_str()));
        }
        return true;
    }
    // TODO: after some time could be removed
    else
    {
        // hack for version 0 and 1
        std::string sql = table[1 + ncol];
        if (sql.find(DATABASE_COLUMN_MESSAGE) != std::string::npos)
        {
            UpdateABRTTable(1);
            return true;
        }
        UpdateABRTTable(0);
        return true;
    }

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
         DATABASE_COLUMN_UUID" VARCHAR NOT NULL,"
         DATABASE_COLUMN_UID" VARCHAR NOT NULL,"
         DATABASE_COLUMN_DEBUG_DUMP_PATH" VARCHAR NOT NULL,"
         DATABASE_COLUMN_COUNT" INT NOT NULL DEFAULT 1,"
         DATABASE_COLUMN_REPORTED" INT NOT NULL DEFAULT 0,"
         DATABASE_COLUMN_TIME" VARCHAR NOT NULL DEFAULT 0,"
         DATABASE_COLUMN_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
         "PRIMARY KEY ("DATABASE_COLUMN_UUID","DATABASE_COLUMN_UID"));");
}

void CSQLite3::DisConnect()
{
    sqlite3_close(m_pDB);
}

void CSQLite3::Insert(const std::string& pUUID,
                      const std::string& pUID,
                      const std::string& pDebugDumpPath,
                      const std::string& pTime)
{
    if (!Exist(pUUID, pUID))
    {
            Exec("INSERT INTO "ABRT_TABLE"("
                 DATABASE_COLUMN_UUID","
                 DATABASE_COLUMN_UID","
                 DATABASE_COLUMN_DEBUG_DUMP_PATH","
                 DATABASE_COLUMN_TIME")"
                   " VALUES ('"+pUUID+"',"
                             "'"+pUID+"',"
                             "'"+pDebugDumpPath+"',"
                             "'"+pTime+"'"
                           ");");
    }
    else
    {
            Exec("UPDATE "ABRT_TABLE" "
                 "SET "DATABASE_COLUMN_COUNT" = "DATABASE_COLUMN_COUNT" + 1, "
                       DATABASE_COLUMN_TIME" = '"+pTime+"' "
                 "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"' "
                 "AND "DATABASE_COLUMN_UID" = '"+pUID+"';");
    }
}

void CSQLite3::Delete(const std::string& pUUID, const std::string& pUID)
{
    if (pUID == "0")
    {
           Exec("DELETE FROM "ABRT_TABLE" "
                "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"';");
    }
    else if (Exist(pUUID, pUID))
    {
            Exec("DELETE FROM "ABRT_TABLE" "
                 "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"' "
                 "AND "DATABASE_COLUMN_UID" = '"+pUID+"';");
    }
    else
    {
        throw CABRTException(EXCEP_PLUGIN, "CSQLite3::Delete(): UUID is not found in DB.");
    }
}

void CSQLite3::SetReported(const std::string& pUUID, const std::string& pUID, const std::string& pMessage)
{
    if (Exist(pUUID, pUID))
    {
            Exec("UPDATE "ABRT_TABLE" "
                 "SET "DATABASE_COLUMN_REPORTED" = 1 "
                 "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"' "
                 "AND "DATABASE_COLUMN_UID" = '"+pUID+"';");
            Exec("UPDATE "ABRT_TABLE" "
                 "SET "DATABASE_COLUMN_MESSAGE" = '" + pMessage + "' "
                 "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"' "
                 "AND "DATABASE_COLUMN_UID" = '"+pUID+"';");

    }
    else
    {
        throw CABRTException(EXCEP_PLUGIN, "CSQLite3::SetReported(): UUID is not found in DB.");
    }
}

const vector_database_rows_t CSQLite3::GetUIDData(const std::string& pUID)
{
    vector_database_rows_t table;
    if (pUID == "0")
    {
        GetTable("SELECT * FROM "ABRT_TABLE";", table);
    }
    else
    {
        GetTable("SELECT * FROM "ABRT_TABLE
                 " WHERE "DATABASE_COLUMN_UID" = '"+pUID+"';",
                 table);
    }
    return table;
}

const database_row_t CSQLite3::GetUUIDData(const std::string& pUUID, const std::string& pUID)
{
    vector_database_rows_t table;

    if (pUID == "0")
    {
        GetTable("SELECT * FROM "ABRT_TABLE" "
                 "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"';",
                 table);
    }
    else
    {
        GetTable("SELECT * FROM "ABRT_TABLE" "
                "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"' "
                "AND "DATABASE_COLUMN_UID" = '"+pUID+"';",
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
    if (pSettings.find("DBPath") != pSettings.end())
    {
        m_sDBPath = pSettings.find("DBPath")->second;
    }
}

map_plugin_settings_t CSQLite3::GetSettings()
{
    map_plugin_settings_t ret;

    ret["DBPath"] = m_sDBPath;

    return ret;
}

PLUGIN_INFO(DATABASE,
            CSQLite3,
            "SQLite3",
            "0.0.2",
            "SQLite3 database plugin.",
            "zprikryl@redhat.com,jmoskovc@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
