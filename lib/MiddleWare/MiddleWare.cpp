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
#include "ABRTException.h"
#include "CommLayerInner.h"

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
    std::string fileName;
    std::string content;
    bool isTextFile;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);

    if (!dd.Exist(FILENAME_ARCHITECTURE) ||
        !dd.Exist(FILENAME_KERNEL) ||
        !dd.Exist(FILENAME_PACKAGE) ||
        !dd.Exist(FILENAME_RELEASE) ||
        !dd.Exist(FILENAME_EXECUTABLE))
    {
        dd.Delete();
        dd.Close();
        throw CABRTException(EXCEP_ERROR, "CMiddleWare::DebugDumpToCrashReport(): One or more of important file(s)'re missing.");
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
            if (fileName == FILENAME_ARCHITECTURE ||
                fileName == FILENAME_KERNEL ||
                fileName == FILENAME_PACKAGE ||
                fileName == FILENAME_RELEASE ||
                fileName == FILENAME_EXECUTABLE)
            {
                add_crash_data_to_crash_report(pCrashReport, fileName, CD_TXT, CD_ISNOTEDITABLE, content);
            }
            else if (fileName != FILENAME_UID &&
                     fileName != FILENAME_ANALYZER &&
                     fileName != FILENAME_TIME &&
                     fileName != FILENAME_DESCRIPTION )
            {
                if (content.length() < CD_ATT_SIZE)
                {
                    add_crash_data_to_crash_report(pCrashReport, fileName, CD_TXT, CD_ISEDITABLE, content);
                }
                else
                {
                    add_crash_data_to_crash_report(pCrashReport, fileName, CD_ATT, CD_ISEDITABLE, content);
                }
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

int CMiddleWare::CreateCrashReport(const std::string& pUUID,
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
        throw CABRTException(EXCEP_ERROR, "CMiddleWare::CreateCrashReport(): UUID '"+pUUID+"' is not in database.");
    }

    try
    {
        std::string analyzer;
        std::string gUUID;
        CDebugDump dd;
        dd.Open(row.m_sDebugDumpDir);
        dd.LoadText(FILENAME_ANALYZER, analyzer);
        dd.Close();

        CreateReport(analyzer, row.m_sDebugDumpDir);

        gUUID = GetGlobalUUID(analyzer, row.m_sDebugDumpDir);

        RunAnalyzerActions(analyzer, row.m_sDebugDumpDir);
        DebugDumpToCrashReport(row.m_sDebugDumpDir, pCrashReport);

        add_crash_data_to_crash_report(pCrashReport, CD_UUID, CD_TXT, CD_ISNOTEDITABLE, gUUID);
        add_crash_data_to_crash_report(pCrashReport, CD_MWANALYZER, CD_SYS, CD_ISNOTEDITABLE, analyzer);
        add_crash_data_to_crash_report(pCrashReport, CD_MWUID, CD_SYS, CD_ISNOTEDITABLE, pUID);
        add_crash_data_to_crash_report(pCrashReport, CD_MWUUID, CD_SYS, CD_ISNOTEDITABLE, pUUID);
        add_crash_data_to_crash_report(pCrashReport, CD_COMMENT, CD_TXT, CD_ISEDITABLE, "");
        add_crash_data_to_crash_report(pCrashReport, CD_REPRODUCE, CD_TXT, CD_ISEDITABLE, "1.\n2.\n3.\n");
    }
    catch (CABRTException& e)
    {
        if (e.type() == EXCEP_DD_LOAD)
        {
            DeleteCrashInfo(row.m_sUID, row.m_sUUID, true);
        }
        else if (e.type() == EXCEP_DD_OPEN)
        {
            DeleteCrashInfo(row.m_sUUID, row.m_sUID, false);
        }
        comm_layer_inner_warning("CMiddleWare::CreateCrashReport(): " + e.what());
        return 0;
    }

    return 1;
}

void CMiddleWare::RunAction(const std::string& pActionDir,
                            const std::string& pPluginName,
                            const std::string& pPluginArgs)
{
    try
    {
        CAction* action = m_pPluginManager->GetAction(pPluginName);
        if (action)
        {
            action->Run(pActionDir, pPluginArgs);
        }
        else
        {
            throw CABRTException(EXCEP_ERROR, "Plugin '"+pPluginName+"' is not registered.");
        }
    }
    catch (CABRTException& e)
    {
        comm_layer_inner_warning("CMiddleWare::RunAction(): " + e.what());
        comm_layer_inner_status("Execution of '"+pPluginName+"' was not successful: " + e.what());
    }

}

void CMiddleWare::RunActionsAndReporters(const std::string& pDebugDumpDir)
{
    vector_actions_and_reporters_t::iterator it_ar;
    for (it_ar = m_vectorActionsAndReporters.begin(); it_ar != m_vectorActionsAndReporters.end(); it_ar++)
    {
        try
        {
            CReporter* reporter = m_pPluginManager->GetReporter((*it_ar).first);
            CAction* action = m_pPluginManager->GetAction((*it_ar).first);
            if (reporter)
            {
                map_crash_report_t crashReport;
                DebugDumpToCrashReport(pDebugDumpDir, crashReport);
                reporter->Report(crashReport, (*it_ar).second);
            }
            else if (action)
            {
                action->Run(pDebugDumpDir, (*it_ar).second);
            }
            else
            {
                throw CABRTException(EXCEP_ERROR, "Plugin '"+(*it_ar).first+"' is not registered.");
            }
        }
        catch (CABRTException& e)
        {
            comm_layer_inner_warning("CMiddleWare::RunActionsAndReporters(): " + e.what());
            comm_layer_inner_status("Reporting via '"+(*it_ar).first+"' was not successful: " + e.what());
        }
    }
}

void CMiddleWare::Report(const map_crash_report_t& pCrashReport)
{
    if (pCrashReport.find(CD_MWANALYZER) == pCrashReport.end() ||
        pCrashReport.find(CD_MWUID) == pCrashReport.end() ||
        pCrashReport.find(CD_MWUUID) == pCrashReport.end())
    {
        throw CABRTException(EXCEP_ERROR, "CMiddleWare::Report(): System data are missing in crash report.");
    }
    std::string analyzer = pCrashReport.find(CD_MWANALYZER)->second[CD_CONTENT];
    std::string UID = pCrashReport.find(CD_MWUID)->second[CD_CONTENT];
    std::string UUID = pCrashReport.find(CD_MWUUID)->second[CD_CONTENT];

    if (m_mapAnalyzerActionsAndReporters.find(analyzer) != m_mapAnalyzerActionsAndReporters.end())
    {
        vector_actions_and_reporters_t::iterator it_r;
        for (it_r = m_mapAnalyzerActionsAndReporters[analyzer].begin();
             it_r != m_mapAnalyzerActionsAndReporters[analyzer].end();
             it_r++)
        {
            try
            {
                CReporter* reporter = m_pPluginManager->GetReporter((*it_r).first);
                if (reporter)
                {
                    reporter->Report(pCrashReport, (*it_r).second);
                }
                else
                {
                    throw CABRTException(EXCEP_ERROR, "Plugin '"+(*it_r).first+"' is not registered.");
                }
            }
            catch (CABRTException& e)
            {
                comm_layer_inner_warning("CMiddleWare::Report(): " + e.what());
                comm_layer_inner_status("Reporting via '"+(*it_r).first+"' was not successful: " + e.what());
            }
        }
    }

    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database->Connect();
    database->SetReported(UUID, UID);
    database->DisConnect();
}

void CMiddleWare::DeleteDebugDumpDir(const std::string& pDebugDumpDir)
{
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.Delete();
    dd.Close();
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
        DeleteDebugDumpDir(row.m_sDebugDumpDir);
    }
}


bool CMiddleWare::IsDebugDumpSaved(const std::string& pUID,
                                   const std::string& pDebugDumpDir)
{
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    vector_database_rows_t rows;
    database->Connect();
    rows = database->GetUIDData(pUID);
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

int CMiddleWare::SavePackageDescriptionToDebugDump(const std::string& pExecutable,
                                                   const std::string& pDebugDumpDir)
{
    std::string package;
    std::string packageName;

    if (pExecutable == "kernel")
    {
        packageName = package = "kernel";
    }
    else
    {
        package = m_RPM.GetPackage(pExecutable);
        packageName = package.substr(0, package.rfind("-", package.rfind("-") - 1));
        if (packageName == "" ||
            (m_setBlackList.find(packageName) != m_setBlackList.end()))
        {
            if (packageName == "")
            {
                comm_layer_inner_debug("Executable doesn't belong to any package - deleting debug dump...");
            }
            else
            {
                comm_layer_inner_debug("Blacklisted package - deleting debug dump...");
            }
            DeleteDebugDumpDir(pDebugDumpDir);
            return 0;
        }
        if (m_bOpenGPGCheck)
        {
            if (!m_RPM.CheckFingerprint(packageName) ||
                !m_RPM.CheckHash(packageName, pExecutable))
            {
                comm_layer_inner_debug("Can not find package - deleting debug dump...");
                DeleteDebugDumpDir(pDebugDumpDir);
                return 0;
            }
        }
    }

    std::string description = m_RPM.GetDescription(packageName);

    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.SaveText(FILENAME_PACKAGE, package);
    dd.SaveText(FILENAME_DESCRIPTION, description);
    dd.Close();

    return 1;
}

void CMiddleWare::RunAnalyzerActions(const std::string& pAnalyzer, const std::string& pDebugDumpDir)
{
    if (m_mapAnalyzerActionsAndReporters.find(pAnalyzer) != m_mapAnalyzerActionsAndReporters.end())
    {
        vector_actions_and_reporters_t::iterator it_a;
        for (it_a = m_mapAnalyzerActionsAndReporters[pAnalyzer].begin();
             it_a != m_mapAnalyzerActionsAndReporters[pAnalyzer].end();
             it_a++)
        {
            try
            {
                CAction* action = m_pPluginManager->GetAction((*it_a).first);
                if (action)
                {
                    action->Run(pDebugDumpDir, (*it_a).second);
                }
                else if (m_pPluginManager->GetReporter((*it_a).first) == NULL)
                {
                    throw CABRTException(EXCEP_ERROR, "Plugin '"+(*it_a).first+"' is not registered.");
                }
            }
            catch (CABRTException& e)
            {
                comm_layer_inner_warning("CMiddleWare::RunAnalyzerActions(): " + e.what());
                comm_layer_inner_status("Action performed by '"+(*it_a).first+"' was not successful: " + e.what());
            }
        }
    }
}

int CMiddleWare::SaveDebugDumpToDatabase(const std::string& pUUID,
                                         const std::string& pUID,
                                         const std::string& pTime,
                                         const std::string& pDebugDumpDir,
                                         map_crash_info_t& pCrashInfo)
{
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);

    database_row_t row;
    database->Connect();
    database->Insert(pUUID, pUID, pDebugDumpDir, pTime);
    row = database->GetUUIDData(pUUID, pUID);
    database->DisConnect();
    if (row.m_sReported == "1")
    {
        comm_layer_inner_debug("Crash is already reported - deleting debug dump...");
        DeleteDebugDumpDir(pDebugDumpDir);
        return 0;
    }

    pCrashInfo = GetCrashInfo(pUUID, pUID);
    if (row.m_sCount != "1")
    {
        comm_layer_inner_debug("Crash is in database already - deleting debug dump...");
        DeleteDebugDumpDir(pDebugDumpDir);
        return 2;
    }
    return 1;
}

int CMiddleWare::SaveDebugDump(const std::string& pDebugDumpDir)
{
    map_crash_info_t info;
    return SaveDebugDump(pDebugDumpDir, info);
}

int CMiddleWare::SaveDebugDump(const std::string& pDebugDumpDir, map_crash_info_t& pCrashInfo)
{
    std::string lUUID;
    std::string UID;
    std::string time;
    std::string analyzer;
    std::string executable;
    CDebugDump dd;

    try
    {
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_TIME, time);
        dd.LoadText(FILENAME_UID, UID);
        dd.LoadText(FILENAME_ANALYZER, analyzer);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.Close();

        if (IsDebugDumpSaved(UID, pDebugDumpDir))
        {
            return 0;
        }
        if (!SavePackageDescriptionToDebugDump(executable, pDebugDumpDir))
        {
            return 0;
        }

        lUUID = GetLocalUUID(analyzer, pDebugDumpDir);

        return SaveDebugDumpToDatabase(lUUID, UID, time, pDebugDumpDir, pCrashInfo);
    }
    catch (CABRTException& e)
    {
        if (e.type() == EXCEP_DD_LOAD ||
            e.type() == EXCEP_DD_SAVE)
        {
            DeleteDebugDumpDir(pDebugDumpDir);
        }
        comm_layer_inner_warning("CMiddleWare::SaveDebugDump(): " + e.what());
        return 0;
    }
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
    catch (CABRTException& e)
    {
        if (e.type() == EXCEP_DD_OPEN)
        {
            DeleteCrashInfo(row.m_sUUID, row.m_sUID, false);
        }
        comm_layer_inner_warning("CMiddleWare::GetCrashInfo(): " + e.what());
        return crashInfo;
    }

    std::string data;
    dd.LoadText(FILENAME_EXECUTABLE, data);
    add_crash_data_to_crash_info(crashInfo, CD_EXECUTABLE, data);
    dd.LoadText(FILENAME_PACKAGE, data);
    add_crash_data_to_crash_info(crashInfo, CD_PACKAGE, data);
    dd.LoadText(FILENAME_DESCRIPTION, data);
    add_crash_data_to_crash_info(crashInfo, CD_DESCRIPTION, data);
    dd.Close();
    add_crash_data_to_crash_info(crashInfo, CD_UUID, row.m_sUUID);
    add_crash_data_to_crash_info(crashInfo, CD_UID, row.m_sUID);
    add_crash_data_to_crash_info(crashInfo, CD_COUNT, row.m_sCount);
    add_crash_data_to_crash_info(crashInfo, CD_TIME, row.m_sTime);
    add_crash_data_to_crash_info(crashInfo, CD_REPORTED, row.m_sReported);
    add_crash_data_to_crash_info(crashInfo, CD_MWDDD, row.m_sDebugDumpDir);

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
        if (info[CD_UUID][CD_CONTENT] != "")
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

void CMiddleWare::AddAnalyzerActionOrReporter(const std::string& pAnalyzer,
                                              const std::string& pAnalyzerOrReporter,
                                              const std::string& pArgs)
{
    m_mapAnalyzerActionsAndReporters[pAnalyzer].push_back(make_pair(pAnalyzerOrReporter, pArgs));
}

void CMiddleWare::AddActionOrReporter(const std::string& pActionOrReporter,
                                      const std::string& pArgs)
{
    m_vectorActionsAndReporters.push_back(make_pair(pActionOrReporter, pArgs));
}
