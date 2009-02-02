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

		void Create();
		void Exec(const std::string& pCommand);
		void GetTable(const std::string& pCommand, vector_database_rows_t& pTable);
		bool IsReported(const std::string& pUUID);

	public:
		CSQLite3();
		virtual ~CSQLite3() {}

		void Connect();
		void DisConnect();

		void Insert(const std::string& pUUID,
					const std::string& pDebugDumpPath,
					const std::string& pArch,
					const std::string& pKernel,
					const std::string& pExecutable,
					const std::string& pPackage,
					const std::string& pUID,
					const std::string& pTime);

		void InsertBackTrace(const std::string& pUUID,
							 const std::string& pBackTrace);

		void InsertTextData1(const std::string& pUUID,
							 const std::string& pData);

		void Delete(const std::string& pUUID);

		const vector_database_rows_t GetUIDData(const std::string& pUID);

		void Init(const map_settings_t& pSettings);
		void DeInit() {}
};

PLUGIN_INFO(DATABASE,
		    "SQLite3",
		    "0.0.1",
		    "SQLite3 database plugin.",
		    "zprikryl@redhat.com",
		    "https://fedorahosted.org/crash-catcher/wiki");

PLUGIN_INIT(CSQLite3);

#endif /* SQLITE3_H_ */
