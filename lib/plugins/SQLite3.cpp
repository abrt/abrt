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
#include "abrtlib.h"
#include "SQLite3.h"
#include "abrt_exception.h"

using namespace std;

#define ABRT_TABLE_VERSION      4
#define ABRT_TABLE_VERSION_STR "4"
#define ABRT_TABLE             "abrt_v"ABRT_TABLE_VERSION_STR
#define ABRT_REPRESULT_TABLE   "abrt_v"ABRT_TABLE_VERSION_STR"_reportresult"
#define SQLITE3_MASTER_TABLE   "sqlite_master"

#define COL_UID                "UID"
#define COL_UUID               "UUID"
#define COL_INFORMALL          "InformAll"
#define COL_DEBUG_DUMP_PATH    "DebugDumpPath"
#define COL_COUNT              "Count"
#define COL_REPORTED           "Reported"
#define COL_TIME               "Time"
#define COL_MESSAGE            "Message"

#define COL_REPORTER           "Reporter"

#define NUM_COL                8

/* Is this string safe wrt SQL injection?
 * PHP's mysql_real_escape_string() treats \, ', ", \x00, \n, \r, and \x1a as special.
 * We are a bit more paranoid and disallow any control chars.
 */
static bool is_string_safe(const char *str)
{
// Apparently SQLite allows unescaped newlines. More surprisingly,
// it does not unescape escaped ones - I see lines ending with \ when I do it.
// I wonder whether this is a bug in SQLite, and whether using unescaped
// newlines is a danger with other SQL servers.
// For now, I disabled newline escaping, and also allowed double quote.
    const char *p = str;
    while (*p)
    {
        unsigned char c = *p;
//        if (c == '\\' && p[1] != '\0')
//        {
//            p += 2;
//            continue;
//        }
        if ((c < ' ' && c != '\n')
         || strchr("\\\'", c) //was: "\\\"\'"
        ) {
            error_msg("Probable SQL injection: '%s'", str);
            return false;
        }
        p++;
    }
    return true;
}

#ifdef UNUSED_FOR_NOW
/* Escape \n */
static string sql_escape(const char *str)
{
    const char *s = str;
    unsigned len = 0;
    do
    {
        if (*s == '\n')
            len++;
        len++;
    } while (*s++);

    char buf[len];
    s = str;
    char *d = buf;
    do
    {
        if (*s == '\n')
            *d++ = '\\';
        *d++ = *s;
    } while (*s++);

    return buf;
}
#endif

/* Note:
 * expects "SELECT * FROM ...", not "SELECT <only some fields> FROM ..."
 */
static GList *vget_table(sqlite3 *db, const char *fmt, va_list p)
{
    char *sql = xvasprintf(fmt, p);

    char **table;
    int ncol, nrow;
    char *err = NULL;
    int ret = sqlite3_get_table(db, sql, &table, &nrow, &ncol, &err);
    if (ret != SQLITE_OK)
    {
        error_msg("Error in SQL:'%s' error: %s", sql, err);
        free(sql);
        sqlite3_free(err);
        return (GList*)ERR_PTR;
    }
    VERB2 log("%s: %d rows returned by SQL:%s", __func__, nrow, sql);
    free(sql);

    if (nrow > 0 && ncol < NUM_COL)
        error_msg_and_die("Unexpected number of columns: %d", ncol);

    GList *rows = NULL;
    int ii;
    for (ii = 0; ii < nrow; ii++)
    {
        int jj;
        struct db_row *row = (struct db_row*)xzalloc(sizeof(struct db_row));
        for (jj = 0; jj < ncol; jj++)
        {
            char *val = table[jj + (ncol*ii) + ncol];
            switch (jj)
            {
                case 0: row->db_uuid       = xstrdup(val); break;
                case 1: row->db_uid        = xstrdup(val); break;
                case 2: row->db_inform_all = xstrdup(val); break;
                case 3: row->db_dump_dir   = xstrdup(val); break;
                case 4: row->db_count      = xstrdup(val); break;
                case 5: row->db_reported   = xstrdup(val); break;
                case 6: row->db_time       = xstrdup(val); break;
                case 7: row->db_message    = xstrdup(val); break;
            }
        }

        VERB3 log("%s: row->db_uuid = '%s'", __func__, row->db_uuid);
        VERB3 log("%s: row->db_uid = '%s'", __func__, row->db_uid);
        VERB3 log("%s: row->db_inform_all = '%s'", __func__, row->db_inform_all);
        VERB3 log("%s: row->db_dump_dir = '%s'", __func__, row->db_dump_dir);
        VERB3 log("%s: row->db_count = '%s'", __func__, row->db_count);
        VERB3 log("%s: row->db_reported = '%s'", __func__, row->db_reported);
        VERB3 log("%s: row->db_time = '%s'", __func__, row->db_time);
        VERB3 log("%s: row->db_message = '%s'", __func__, row->db_message);
        rows = g_list_append(rows, row);

    }
    sqlite3_free_table(table);

    return rows;
}

static GList *get_table_or_die(sqlite3 *db, const char *fmt, ...)
{
    va_list p;
    va_start(p, fmt);
    GList *table = vget_table(db, fmt, p);
    va_end(p);

    if (table == (GList*)ERR_PTR)
        xfunc_die();

    return table;
}

static int execute_sql(sqlite3 *db, const char *fmt, ...)
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
    int affected = sqlite3_changes(db);
    VERB2 log("%d rows affected by SQL:%s", affected, sql);
    free(sql);

    return affected;
}

static bool exists_uuid_uid(sqlite3 *db, const char *UUID, const char *UID)
{
    GList *table =  get_table_or_die(db, "SELECT * FROM "ABRT_TABLE
                                         " WHERE "COL_UUID"='%s' AND "COL_UID"='%s';",
                                         UUID, UID
    );

    if (!table)
        return false;

    db_list_free(table);

    return true;
}

static void update_from_old_ver(sqlite3 *db, int old_version)
{
    static const char *const update_sql_commands[] = {
        // v0 -> v1
        NULL,
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
        // v2 -> v3
        "BEGIN TRANSACTION;"
        "CREATE TABLE abrt_v3 ("
                COL_UUID" VARCHAR NOT NULL,"
                COL_UID" VARCHAR NOT NULL,"
                COL_DEBUG_DUMP_PATH" VARCHAR NOT NULL,"
                COL_COUNT" INT NOT NULL DEFAULT 1,"
                COL_REPORTED" INT NOT NULL DEFAULT 0,"
                COL_TIME" VARCHAR NOT NULL DEFAULT 0,"
                COL_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
                "PRIMARY KEY ("COL_UUID","COL_UID"));"
        "INSERT INTO abrt_v3 "
            "SELECT "COL_UUID","
                    COL_UID","
                    COL_DEBUG_DUMP_PATH","
                    COL_COUNT","
                    COL_REPORTED","
                    COL_TIME","
                    COL_MESSAGE
            " FROM abrt_v2;"
        "DROP TABLE abrt_v2;"
        "CREATE TABLE abrt_v3_reportresult ("
                COL_UUID" VARCHAR NOT NULL,"
                COL_UID" VARCHAR NOT NULL,"
                COL_REPORTER" VARCHAR NOT NULL,"
                COL_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
                "PRIMARY KEY ("COL_UUID","COL_UID","COL_REPORTER"));"
        "COMMIT;",
        // v3-> v4
        "BEGIN TRANSACTION;"
        "CREATE TABLE abrt_v4("
                COL_UUID" VARCHAR NOT NULL,"
                COL_UID" VARCHAR NOT NULL,"
                COL_INFORMALL" INT NOT NULL DEFAULT 0,"
                COL_DEBUG_DUMP_PATH" VARCHAR NOT NULL,"
                COL_COUNT" INT NOT NULL DEFAULT 1,"
                COL_REPORTED" INT NOT NULL DEFAULT 0,"
                COL_TIME" VARCHAR NOT NULL DEFAULT 0,"
                COL_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
                "PRIMARY KEY ("COL_UUID","COL_UID"));"
        "INSERT INTO abrt_v4 "
            "SELECT "COL_UUID","
                    COL_UID","
                    "0," /* COL_INFORMALL */
                    COL_DEBUG_DUMP_PATH","
                    COL_COUNT","
                    COL_REPORTED","
                    COL_TIME","
                    COL_MESSAGE
            " FROM abrt_v3;"
        "DROP TABLE abrt_v3;"
        "UPDATE abrt_v4"
        " SET "COL_UID"='0', "COL_INFORMALL"=1"
        " WHERE "COL_UID"='-1';"
        "CREATE TABLE abrt_v4_reportresult ("
                COL_UUID" VARCHAR NOT NULL,"
                COL_UID" VARCHAR NOT NULL,"
                COL_REPORTER" VARCHAR NOT NULL,"
                COL_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
                "PRIMARY KEY ("COL_UUID","COL_UID","COL_REPORTER"));"
        "INSERT INTO abrt_v4_reportresult "
            "SELECT * FROM abrt_v3_reportresult;"
        "DROP TABLE abrt_v3_reportresult;"
        "COMMIT;",
    };

    while (old_version < ABRT_TABLE_VERSION)
    {
        execute_sql(db, update_sql_commands[old_version]);
        old_version++;
    }
}

static bool check_table(sqlite3 *db)
{
    const char *command = "SELECT NAME FROM "SQLITE3_MASTER_TABLE" "
                          "WHERE TYPE='table' AND NAME like 'abrt_v%';";
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
        sqlite3_free_table(table);
        return false;
    }

    // table format:
    // table[0]:"NAME"     // table[1]:"SQL"  <== field names from SELECT
    // table[2]:"abrt_vNN" // table[3]:"sql"
    char *tableName = table[0 + ncol];
    char *underscore = strchr(tableName, '_');
    if (underscore)
    {
        // It can be "abrt_vNN_something", thus using atoi(), not xatoi()
        int tableVersion = atoi(underscore + 2);
        sqlite3_free_table(table);
        if (tableVersion < ABRT_TABLE_VERSION)
        {
            update_from_old_ver(db, tableVersion);
        }
        return true;
    }
    sqlite3_free_table(table);
    update_from_old_ver(db, 1);
    return true;
}


CSQLite3::CSQLite3() :
    m_sDBPath(LOCALSTATEDIR "/spool/abrt/abrt-db"),
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
                COL_INFORMALL" INT NOT NULL DEFAULT 0,"
                COL_DEBUG_DUMP_PATH" VARCHAR NOT NULL,"
                COL_COUNT" INT NOT NULL DEFAULT 1,"
                COL_REPORTED" INT NOT NULL DEFAULT 0,"
                COL_TIME" VARCHAR NOT NULL DEFAULT 0,"
                COL_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
                "PRIMARY KEY ("COL_UUID","COL_UID"));"
        );
        execute_sql(m_pDB,
                "CREATE TABLE "ABRT_REPRESULT_TABLE" ("
                COL_UUID" VARCHAR NOT NULL,"
                COL_UID" VARCHAR NOT NULL,"
                COL_REPORTER" VARCHAR NOT NULL,"
                COL_MESSAGE" VARCHAR NOT NULL DEFAULT '',"
                "PRIMARY KEY ("COL_UUID","COL_UID","COL_REPORTER"));"
        );
    }
}

void CSQLite3::Insert_or_Update(const char *crash_id,
                bool inform_all_users,
                const char *pDebugDumpPath,
                const char *pTime)
{
    const char *UUID = strchr(crash_id, ':');
    if (!UUID
     || !is_string_safe(crash_id)
     || !is_string_safe(pDebugDumpPath)
     || !is_string_safe(pTime)
    ) {
        return;
    }

    /* Split crash_id into UID:UUID */
    unsigned uid_len = UUID - crash_id;
    UUID++;
    char UID[uid_len + 1];
    strncpy(UID, crash_id, uid_len);
    UID[uid_len] = '\0';

    if (!exists_uuid_uid(m_pDB, UUID, UID))
    {
        execute_sql(m_pDB,
                "INSERT INTO "ABRT_TABLE" ("
                COL_UUID","
                COL_UID","
                COL_INFORMALL","
                COL_DEBUG_DUMP_PATH","
                COL_TIME
                ")"
                " VALUES ('%s','%s',%u,'%s','%s');",
                UUID, UID, (unsigned)inform_all_users, pDebugDumpPath, pTime
        );
    }
    else
    {
        execute_sql(m_pDB,
                "UPDATE "ABRT_TABLE
                " SET "COL_COUNT"="COL_COUNT"+1,"COL_TIME"='%s'"
                " WHERE "COL_UUID"='%s' AND "COL_UID"='%s';",
                pTime,
                UUID, UID
        );
    }
}

void CSQLite3::DeleteRow(const char *crash_id)
{
    const char *UUID = strchr(crash_id, ':');
    if (!UUID
     || !is_string_safe(crash_id)
    ) {
        return;
    }

    /* Split crash_id into UID:UUID */
    unsigned uid_len = UUID - crash_id;
    UUID++;
    char UID[uid_len + 1];
    strncpy(UID, crash_id, uid_len);
    UID[uid_len] = '\0';

    if (exists_uuid_uid(m_pDB, UUID, UID))
    {
        execute_sql(m_pDB, "DELETE FROM "ABRT_TABLE
                " WHERE "COL_UUID"='%s' AND "COL_UID"='%s';",
                UUID, UID
        );
        execute_sql(m_pDB, "DELETE FROM "ABRT_REPRESULT_TABLE
                " WHERE "COL_UUID"='%s' AND "COL_UID"='%s';",
                UUID, UID
        );
    }
    else
    {
        error_msg("crash_id %s is not found in DB", crash_id);
    }
}

void CSQLite3::DeleteRows_by_dir(const char *dump_dir)
{
    if (!is_string_safe(dump_dir))
    {
        return;
    }

    /* Get UID:UUID pair(s) to delete */
    GList *table = get_table_or_die(m_pDB, "SELECT * FROM "ABRT_TABLE
                                           " WHERE "COL_DEBUG_DUMP_PATH"='%s';",
                                           dump_dir
    );

    if (!table)
    {
        return;
    }

    struct db_row *row = NULL;
    /* Delete from both tables */
    for (GList *li = table; li != NULL; li = g_list_next(li))
    {
        row = (struct db_row*)li->data;
        execute_sql(m_pDB,
                "DELETE FROM "ABRT_REPRESULT_TABLE
                " WHERE "COL_UUID"='%s' AND "COL_UID"='%s';",
                row->db_uuid, row->db_uid
        );
    }
    execute_sql(m_pDB,
                "DELETE FROM "ABRT_TABLE
                " WHERE "COL_DEBUG_DUMP_PATH"='%s'",
                dump_dir
    );
}

void CSQLite3::SetReported(const char *crash_id, const char *pMessage)
{
    const char *UUID = strchr(crash_id, ':');
    if (!UUID
     || !is_string_safe(crash_id)
     || !is_string_safe(pMessage)
    ) {
        return;
    }

    /* Split crash_id into UID:UUID */
    unsigned uid_len = UUID - crash_id;
    UUID++;
    char UID[uid_len + 1];
    strncpy(UID, crash_id, uid_len);
    UID[uid_len] = '\0';

    if (exists_uuid_uid(m_pDB, UUID, UID))
    {
        execute_sql(m_pDB,
                "UPDATE "ABRT_TABLE
                " SET "COL_REPORTED"=1"
                " WHERE "COL_UUID"='%s' AND "COL_UID"='%s';",
                UUID, UID
        );
        execute_sql(m_pDB,
                "UPDATE "ABRT_TABLE
                " SET "COL_MESSAGE"='%s'"
                " WHERE "COL_UUID"='%s' AND "COL_UID"='%s';",
                pMessage, UUID, UID
        );
    }
    else
    {
        error_msg("crash_id %s is not found in DB", crash_id);
    }
}

void CSQLite3::SetReportedPerReporter(const char *crash_id,
                                 const char *reporter,
                                 const char *pMessage)
{
    const char *UUID = strchr(crash_id, ':');
    if (!UUID
     || !is_string_safe(crash_id)
     || !is_string_safe(reporter)
     || !is_string_safe(pMessage)
    ) {
        return;
    }

    /* Split crash_id into UID:UUID */
    unsigned uid_len = UUID - crash_id;
    UUID++;
    char UID[uid_len + 1];
    strncpy(UID, crash_id, uid_len);
    UID[uid_len] = '\0';

    int affected_rows = execute_sql(m_pDB,
                "UPDATE "ABRT_REPRESULT_TABLE
                " SET "COL_MESSAGE"='%s'"
                " WHERE "COL_UUID"='%s' AND "COL_UID"='%s' AND "COL_REPORTER"='%s'",
                pMessage,
                UUID, UID, reporter
    );
    if (!affected_rows)
    {
        execute_sql(m_pDB,
                "INSERT INTO "ABRT_REPRESULT_TABLE
                " ("COL_UUID","COL_UID","COL_REPORTER","COL_MESSAGE")"
                " VALUES ('%s','%s','%s','%s');",
                UUID, UID, reporter, pMessage
	);
    }
}

GList *CSQLite3::GetUIDData(long caller_uid)
{
    GList *table = NULL;

    if (caller_uid == 0)
    {
        table = get_table_or_die(m_pDB, "SELECT * FROM "ABRT_TABLE";");
    }
    else
    {
        table = get_table_or_die(m_pDB, "SELECT * FROM "ABRT_TABLE
                                        " WHERE "COL_UID"='%ld' OR "COL_INFORMALL"=1;",
                                         caller_uid
        );
    }
    return table;
}

struct db_row *CSQLite3::GetRow(const char *crash_id)
{
    const char *UUID = strchr(crash_id, ':');
    if (!UUID
     || !is_string_safe(crash_id)
    ) {
        return NULL;
    }

    /* Split crash_id into UID:UUID */
    unsigned uid_len = UUID - crash_id;
    UUID++;
    char UID[uid_len + 1];
    strncpy(UID, crash_id, uid_len);
    UID[uid_len] = '\0';

    GList *table = get_table_or_die(m_pDB, "SELECT * FROM "ABRT_TABLE
                                    " WHERE "COL_UUID"='%s' AND "COL_UID"='%s';",
                                    UUID, UID
    );

    if (!table)
    {
        return NULL;
    }

    GList *first = g_list_first(table);
    struct db_row *row = db_rowcpy_from_list(first);

    db_list_free(table);

    return row;
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
            _("Keeps SQLite3 database about all crashes"),
            "zprikryl@redhat.com,jmoskovc@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
