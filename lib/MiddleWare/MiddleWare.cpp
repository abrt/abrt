/*
    MiddleWare.cpp

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

#include "MiddleWare.h"
#include "DebugDump.h"

CMiddleWare::CMiddleWare(const std::string& pPlugisConfDir,
                         const std::string& pPlugisLibDir) :
    m_pPluginManager(NULL),
    m_bOpenGPGCheck(true)
{
    m_pPluginManager = new CPluginManager(pPlugisConfDir, pPlugisLibDir);
    m_pPluginManager->LoadPlugins();
}

CMiddleWare::~CMiddleWare()
{
    m_pPluginManager->UnLoadPlugins();
    delete m_pPluginManager;
}

void CMiddleWare::DebugDumpToCrashReport(const std::string& pDebugDumpDir, map_crash_report_t& pCrashReport)
{
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    std::string fileName, content;
    bool isTextFile;

    if (!dd.Exist(FILENAME_UUID) ||
        !dd.Exist(FILENAME_ARCHITECTURE) ||
        !dd.Exist(FILENAME_KERNEL) ||
        !dd.Exist(FILENAME_PACKAGE) ||
        !dd.Exist(FILENAME_EXECUTABLE))
    {
        dd.Close();
        throw std::string("CMiddleWare::DebugDumpToCrashReport(): One or more of important file(s)'re missing.");
    }
    pCrashReport.clear();
    dd.InitGetNextFile();
    while (dd.GetNextFile(fileName, content, isTextFile))
    {
        if (!isTextFile)
        {
            add_crash_data_to_crash_report(pCrashReport,
                                           fileName,
                                           CD_BIN,
                                           CD_ISNOTEDITABLE,
                                           pDebugDumpDir + "/" + fileName);
        }
        else
        {
            if (fileName == FILENAME_UUID ||
                fileName == FILENAME_ARCHITECTURE ||
                fileName == FILENAME_KERNEL ||
                fileName == FILENAME_PACKAGE ||
                fileName == FILENAME_EXECUTABLE)
            {
                add_crash_data_to_crash_report(pCrashReport, fileName, CD_TXT, CD_ISNOTEDITABLE, content);
            }
            else
            {
                add_crash_data_to_crash_report(pCrashReport, fileName, CD_TXT, CD_ISEDITABLE, content);
            }
        }
    }
    dd.Close();
}

void CMiddleWare::RegisterPlugin(const std::string& pName)
{
    m_pPluginManager->RegisterPlugin(pName);
}

void CMiddleWare::UnRegisterPlugin(const std::string& pName)
{
    m_pPluginManager->UnRegisterPlugin(pName);
}


std::string CMiddleWare::GetLocalUUID(const std::string& pAnalyzer,
                                      const std::string& pDebugDumpDir)
{
    CAnalyzer* analyzer = m_pPluginManager->GetAnalyzer(pAnalyzer);
    return analyzer->GetLocalUUID(pDebugDumpDir);
}

std::string CMiddleWare::GetGlobalUUID(const std::string& pAnalyzer,
                                       const std::string& pDebugDumpDir)
{
    CAnalyzer* analyzer = m_pPluginManager->GetAnalyzer(pAnalyzer);
    return analyzer->GetGlobalUUID(pDebugDumpDir);
}

void CMiddleWare::CreateReport(const std::string& pAnalyzer,
                               const std::string& pDebugDumpDir)
{
    CAnalyzer* analyzer = m_pPluginManager->GetAnalyzer(pAnalyzer);
    return analyzer->CreateReport(pDebugDumpDir);
}

void CMiddleWare::CreateCrashReport(const std::string& pUUID,
                                    const std::string& pUID,
                                    map_crash_report_t& pCrashReport)
{
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database_row_t row;
    database->Connect();
    row = database->GetUUIDData(pUUID, pUID);
    database->DisConnect();

    if (pUUID == "" || row.m_sUUID != pUUID)
    {
        throw std::string("CMiddleWare::CreateCrashReport(): UUID '"+pUUID+"' is not in database.");
    }

    std::string analyzer;
    std::string UUID;
    CDebugDump dd;
    dd.Open(row.m_sDebugDumpDir);

    dd.LoadText(FILENAME_ANALYZER, analyzer);
    try
    {
        CreateReport(analyzer, row.m_sDebugDumpDir);
    }
    catch (...)
    {
        dd.Close();
        throw;
    }
    UUID = GetGlobalUUID(analyzer, row.m_sDebugDumpDir);

    dd.SaveText(FILENAME_UUID, UUID);
    dd.Close();

    RunAnalyzerActions(analyzer, row.m_sDebugDumpDir);
    DebugDumpToCrashReport(row.m_sDebugDumpDir, pCrashReport);

    add_crash_data_to_crash_report(pCrashReport, CI_MWANALYZER, CD_SYS, CD_ISNOTEDITABLE, analyzer);
    add_crash_data_to_crash_report(pCrashReport, CI_MWUID, CD_SYS, CD_ISNOTEDITABLE, pUID);
    add_crash_data_to_crash_report(pCrashReport, CI_MWUUID, CD_SYS, CD_ISNOTEDITABLE, pUUID);
    add_crash_data_to_crash_report(pCrashReport, CI_COMMENT, CD_TXT, CD_ISEDITABLE, "");
}

void CMiddleWare::Report(const map_crash_report_t& pCrashReport)
{
    if (pCrashReport.find(CI_MWANALYZER) == pCrashReport.end() ||
        pCrashReport.find(CI_MWUID) == pCrashReport.end() ||
        pCrashReport.find(CI_MWUUID) == pCrashReport.end())
    {
        throw std::string("CMiddleWare::Report(): Important data are missing.");
    }
    std::string analyzer = pCrashReport.find(CI_MWANALYZER)->second[CD_CONTENT];
    std::string UID = pCrashReport.find(CI_MWUID)->second[CD_CONTENT];
    std::string UUID = pCrashReport.find(CI_MWUUID)->second[CD_CONTENT];

    if (m_mapAnalyzerReporters.find(analyzer) != m_mapAnalyzerReporters.end())
    {
        set_reporters_t::iterator it_r;
        for (it_r = m_mapAnalyzerReporters[analyzer].begin();
             it_r != m_mapAnalyzerReporters[analyzer].end();
             it_r++)
        {
            CReporter* reporter = m_pPluginManager->GetReporter(*it_r);
            reporter->Report(pCrashReport);
        }
    }

    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database->Connect();
    database->SetReported(UUID, UID);
    database->DisConnect();
}

void CMiddleWare::DeleteCrashInfo(const std::string& pUUID,
                                  const std::string& pUID,
                                  const bool bWithDebugDump)
{
    database_row_t row;
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database->Connect();
    row = database->GetUUIDData(pUUID, pUID);
    database->Delete(pUUID, pUID);
    database->DisConnect();

    if (bWithDebugDump)
    {
        CDebugDump dd;
        dd.Open(row.m_sDebugDumpDir);
        dd.Delete();
        dd.Close();
    }
}


bool CMiddleWare::IsDebugDumpSaved(const std::string& pDebugDumpDir)
{
    std::string UID;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_UID, UID);
    dd.Close();

    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    vector_database_rows_t rows;
    database->Connect();
    rows = database->GetUIDData(UID);
    database->DisConnect();

    int ii;
    bool found = false;
    for (ii = 0; ii < rows.size(); ii++)
    {
        if (rows[ii].m_sDebugDumpDir == pDebugDumpDir)
        {
            found = true;
            break;
        }
    }

    return found;
}

int CMiddleWare::SavePackageDescriptionToDebugDump(const std::string& pDebugDumpDir)
{
    std::string package;
    std::string packageName;
    std::string executable;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);

    dd.LoadText(FILENAME_EXECUTABLE, executable);
    if (executable == "kernel")
    {
        packageName = package = "kernel";
    }
    else
    {
        package = m_RPM.GetPackage(executable);
        packageName = package.substr(0, package.rfind("-", package.rfind("-") - 1));
        if (packageName == "" ||
            (m_setBlackList.find(packageName) != m_setBlackList.end()))
        {
            dd.Delete();
            dd.Close();
            return 0;
        }
        if (m_bOpenGPGCheck)
        {
            if (!m_RPM.CheckFingerprint(packageName) ||
                !m_RPM.CheckHash(packageName, executable))
            {
                dd.Delete();
                dd.Close();
                return 0;
            }
        }
    }

    std::string description = m_RPM.GetDescription(packageName);

    dd.SaveText(FILENAME_PACKAGE, package);
    dd.SaveText(FILENAME_DESCRIPTION, description);
    dd.Close();

    return 1;
}

int CMiddleWare::SaveUUIDToDebugDump(const std::string& pDebugDumpDir)
{
    std::string analyzer;
    std::string UUID;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);


    dd.LoadText(FILENAME_ANALYZER, analyzer);
    UUID = GetLocalUUID(analyzer, pDebugDumpDir);

    dd.SaveText(FILENAME_UUID, UUID);
    dd.Close();

    return 1;
}

void CMiddleWare::RunAnalyzerActions(const std::string& pAnalyzer, const std::string& pDebugDumpDir)
{
    if (m_mapAnalyzerActions.find(pAnalyzer) != m_mapAnalyzerActions.end())
    {
        set_actions_t::iterator it_a;
        for (it_a = m_mapAnalyzerActions[pAnalyzer].begin();
             it_a != m_mapAnalyzerActions[pAnalyzer].end();
             it_a++)
        {
            CAction* action = m_pPluginManager->GetAction((*it_a).first);
            action->Run(pDebugDumpDir, (*it_a).second);
        }
    }
}

int CMiddleWare::SaveDebugDumpToDatabase(const std::string& pDebugDumpDir, map_crash_info_t& pCrashInfo)
{
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);

    std::string UUID;
    std::string UID;
    std::string time;

    CDebugDump dd;
    dd.Open(pDebugDumpDir);

    dd.LoadText(FILENAME_TIME, time);
    dd.LoadText(FILENAME_UID, UID);
    dd.LoadText(FILENAME_UUID, UUID);

    database_row_t row;
    database->Connect();
    database->Insert(UUID, UID, pDebugDumpDir, time);
    row = database->GetUUIDData(UUID, UID);
    database->DisConnect();

    if (row.m_sReported == "1")
    {
        dd.Delete();
        dd.Close();
        return 0;
    }

    pCrashInfo = GetCrashInfo(UUID, UID);

    if (row.m_sCount != "1")
    {
        dd.Delete();
        dd.Close();
        return 2;
    }
    dd.Close();

    return 1;
}

int CMiddleWare::SaveDebugDump(const std::string& pDebugDumpDir)
{
    map_crash_info_t info;
    return SaveDebugDump(pDebugDumpDir, info);
}

int CMiddleWare::SaveDebugDump(const std::string& pDebugDumpDir, map_crash_info_t& pCrashInfo)
{
    if (IsDebugDumpSaved(pDebugDumpDir))
    {
        return 0;
    }
    if (!SavePackageDescriptionToDebugDump(pDebugDumpDir))
    {
        return 0;
    }
    if (!SaveUUIDToDebugDump(pDebugDumpDir))
    {
        return 0;
    }
    return SaveDebugDumpToDatabase(pDebugDumpDir, pCrashInfo);
}

map_crash_info_t CMiddleWare::GetCrashInfo(const std::string& pUUID,
                                       const std::string& pUID)
{
    map_crash_info_t crashInfo;
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database_row_t row;
    database->Connect();
    row = database->GetUUIDData(pUUID, pUID);
    database->DisConnect();

    CDebugDump dd;
    try
    {
        dd.Open(row.m_sDebugDumpDir);
    }
    catch (std::string sErr)
    {
        DeleteCrashInfo(row.m_sUUID, row.m_sUID, false);
        return crashInfo;
    }

    std::string data;
    dd.LoadText(FILENAME_EXECUTABLE, data);
    add_crash_data_to_crash_info(crashInfo, CI_EXECUTABLE, CD_TXT, data);
    dd.LoadText(FILENAME_PACKAGE, data);
    add_crash_data_to_crash_info(crashInfo, CI_PACKAGE, CD_TXT, data);
    dd.LoadText(FILENAME_DESCRIPTION, data);
    add_crash_data_to_crash_info(crashInfo, CI_DESCRIPTION, CD_TXT, data);
    dd.Close();
    add_crash_data_to_crash_info(crashInfo, CI_UUID, CD_TXT, row.m_sUUID);
    add_crash_data_to_crash_info(crashInfo, CI_UID, CD_TXT, row.m_sUID);
    add_crash_data_to_crash_info(crashInfo, CI_COUNT, CD_TXT, row.m_sCount);
    add_crash_data_to_crash_info(crashInfo, CI_TIME, CD_TXT, row.m_sTime);
    add_crash_data_to_crash_info(crashInfo, CI_REPORTED, CD_TXT, row.m_sReported);

    return crashInfo;
}

vector_crash_infos_t CMiddleWare::GetCrashInfos(const std::string& pUID)
{
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    vector_database_rows_t rows;
    database->Connect();
    rows = database->GetUIDData(pUID);
    database->DisConnect();

    vector_crash_infos_t infos;
    std::string data;
    int ii;
    for (ii = 0; ii < rows.size(); ii++)
    {
        map_crash_info_t info = GetCrashInfo(rows[ii].m_sUUID, rows[ii].m_sUID);
        infos.push_back(info);
    }

    return infos;
}

void CMiddleWare::SetOpenGPGCheck(const bool& pCheck)
{
    m_bOpenGPGCheck = pCheck;
}

void CMiddleWare::SetDatabase(const std::string& pDatabase)
{
    m_sDatabase = pDatabase;
}

void CMiddleWare::AddOpenGPGPublicKey(const std::string& pKey)
{
    m_RPM.LoadOpenGPGPublicKey(pKey);
}

void CMiddleWare::AddBlackListedPackage(const std::string& pPackage)
{
    m_setBlackList.insert(pPackage);
}

void CMiddleWare::AddAnalyzerReporter(const std::string& pAnalyzer,
                                      const std::string& pReporter)
{
    m_mapAnalyzerReporters[pAnalyzer].insert(pReporter);
}

void CMiddleWare::AddAnalyzerAction(const std::string& pAnalyzer,
                                    const std::string& pAction,
                                    const std::string& pArgs)
{
    m_mapAnalyzerActions[pAnalyzer].insert(make_pair(pAction, pArgs));
}
