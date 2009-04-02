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

void CMiddleWare::DebugDumpToCrashReport(const std::string& pDebugDumpDir, crash_report_t& pCrashReport)
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
        crash_file_t crashFile;
        crashFile.m_sType = TYPE_TXT;
        if (!isTextFile)
        {
            crashFile.m_sType = TYPE_BIN;
            content = pDebugDumpDir + "/" + fileName;
        }
        crashFile.m_sContent = content;
        pCrashReport[fileName] = crashFile;
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
                                    crash_report_t& pCrashReport)
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

    //RunAnalyzerActions(analyzer, row.m_sDebugDumpDir);
    DebugDumpToCrashReport(row.m_sDebugDumpDir, pCrashReport);

    crash_file_t file;
    file.m_sType = TYPE_SYS;
    file.m_sContent = analyzer;
    pCrashReport["_MWAnalyzer"] = file;
    file.m_sContent = pUID;
    pCrashReport["_MWUID"] = file;
    file.m_sContent = pUUID;
    pCrashReport["_MWUUID"] = file;
}

void CMiddleWare::Report(const crash_report_t& pCrashReport)
{
    if (pCrashReport.find("_MWAnalyzer") == pCrashReport.end() ||
        pCrashReport.find("_MWUID") == pCrashReport.end() ||
        pCrashReport.find("_MWUUID") == pCrashReport.end())
    {
        throw std::string("CMiddleWare::Report(): Important data are missing.");
    }
    std::string analyzer = pCrashReport.find("_MWAnalyzer")->second.m_sContent;
    std::string UID = pCrashReport.find("_MWUID")->second.m_sContent;
    std::string UUID = pCrashReport.find("_MWUUID")->second.m_sContent;;

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
}

int CMiddleWare::SaveDebugDumpToDatabase(const std::string& pDebugDumpDir, crash_info_t& pCrashInfo)
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
    crash_info_t info;
    return SaveDebugDump(pDebugDumpDir, info);
}

int CMiddleWare::SaveDebugDump(const std::string& pDebugDumpDir, crash_info_t& pCrashInfo)
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

crash_info_t CMiddleWare::GetCrashInfo(const std::string& pUUID,
                                       const std::string& pUID)
{
    crash_info_t info;
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
        return info;
    }

    std::string data;
    dd.LoadText(FILENAME_EXECUTABLE, data);
    info.m_sExecutable = data;
    dd.LoadText(FILENAME_PACKAGE, data);
    info.m_sPackage = data;
    dd.LoadText(FILENAME_DESCRIPTION, data);
    info.m_sDescription = data;
    dd.Close();

    info.m_sUUID = row.m_sUUID;
    info.m_sUID = row.m_sUID;
    info.m_sCount = row.m_sCount;
    info.m_sTime = row.m_sTime;
    info.m_sReported = row.m_sReported;

    return info;
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
        crash_info_t info = GetCrashInfo(rows[ii].m_sUUID, rows[ii].m_sUID);
        if (info.m_sUUID == rows[ii].m_sUUID)
        {
            infos.push_back(info);
        }
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
