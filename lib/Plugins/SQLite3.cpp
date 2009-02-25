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


#define TABLE_NAME "crash_catcher"

CSQLite3::CSQLite3() :
    m_sDBPath("/tmp/CCDB"),
    m_pDB(NULL)
{}

bool CSQLite3::Exist(const std::string& pUUID, const std::string& pUID)
{
    vector_database_rows_t table;
    GetTable("SELECT "DATABASE_COLUMN_REPORTED" FROM "TABLE_NAME" WHERE "
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
            throw std::string("SQLite3::Exec(): Error on: " + pCommand + " " + err);
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
                throw std::string("SQLite3::GetTable(): Error on: " + pCommand + " " + err);
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
    int ret = sqlite3_open_v2(m_sDBPath.c_str(),
                              &m_pDB,
                              SQLITE_OPEN_READWRITE,
                              NULL);

    if(ret == SQLITE_CANTOPEN)
    {
        Create();
    }
    else if (ret != SQLITE_OK)
    {
        throw std::string("SQLite3::Connect(): Could not open database.") + sqlite3_errmsg(m_pDB);
    }
}

void CSQLite3::Create()
{
    int ret = sqlite3_open_v2(m_sDBPath.c_str(),
                              &m_pDB,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                              NULL);
    if(ret != SQLITE_OK)
    {
        throw std::string("SQLite3::Create(): Could not create database.") + sqlite3_errmsg(m_pDB);
    }

    Exec("CREATE TABLE "TABLE_NAME" ("
         DATABASE_COLUMN_UUID" VARCHAR NOT NULL,"
         DATABASE_COLUMN_UID" VARCHAR NOT NULL,"
         DATABASE_COLUMN_DEBUG_DUMP_PATH" VARCHAR NOT NULL,"
         DATABASE_COLUMN_COUNT" INT NOT NULL DEFAULT 1,"
         DATABASE_COLUMN_REPORTED" INT NOT NULL DEFAULT 0,"
         DATABASE_COLUMN_TIME" VARCHAR NOT NULL DEFAULT 0,"
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
            Exec("INSERT INTO "TABLE_NAME"("
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
            Exec("UPDATE "TABLE_NAME" "
                 "SET "DATABASE_COLUMN_COUNT" = "DATABASE_COLUMN_COUNT" + 1, "
                       DATABASE_COLUMN_TIME" = '"+pTime+"' "
                 "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"' "
                 "AND "DATABASE_COLUMN_UID" = '"+pUID+"';");
    }
}

void CSQLite3::Delete(const std::string& pUUID, const std::string& pUID)
{
    if (Exist(pUUID, pUID))
    {
            Exec("DELETE FROM "TABLE_NAME" "
                 "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"' "
                 "AND "DATABASE_COLUMN_UID" = '"+pUID+"';");
    }
}

void CSQLite3::SetReported(const std::string& pUUID, const std::string& pUID)
{
    if (Exist(pUUID, pUID))
    {
            Exec("UPDATE "TABLE_NAME" "
                 "SET "DATABASE_COLUMN_REPORTED" = 1 "
                 "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"' "
                 "AND "DATABASE_COLUMN_UID" = '"+pUID+"';");
    }
}

const vector_database_rows_t CSQLite3::GetUIDData(const std::string& pUID)
{
    vector_database_rows_t table;
    if (pUID == "0")
    {
        GetTable("SELECT * FROM "TABLE_NAME";", table);
    }
    else
    {
        GetTable("SELECT * FROM "TABLE_NAME
                 " WHERE "DATABASE_COLUMN_UID" = '"+pUID+"';",
                 table);
    }
    return table;
}

const database_row_t CSQLite3::GetUUIDData(const std::string& pUUID, const std::string& pUID)
{
    vector_database_rows_t table;

    GetTable("SELECT * FROM "TABLE_NAME" "
             "WHERE "DATABASE_COLUMN_UUID" = '"+pUUID+"' "
             "AND "DATABASE_COLUMN_UID" = '"+pUID+"';",
             table);

    if (table.size() == 0)
    {
        return database_row_t();
    }
    return table[0];
}

void CSQLite3::SetSettings(const map_settings_t& pSettings)
{
    if (pSettings.find("DBPath")!= pSettings.end())
    {
        m_sDBPath = pSettings.find("DBPath")->second;
    }
}
