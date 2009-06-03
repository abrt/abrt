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

CMiddleWare::mw_result_t CMiddleWare::CreateCrashReport(const std::string& pUUID,
                                                        const std::string& pUID,
                                                        map_crash_report_t& pCrashReport)
{
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database_row_t row;
    database->Connect();
    row = database->GetUUIDData(pUUID, pUID);
    database->DisConnect();
    CDebugDump dd;

    if (pUUID == "" || row.m_sUUID != pUUID)
    {
        comm_layer_inner_warning("CMiddleWare::CreateCrashReport(): UUID '"+pUUID+"' is not in database.");
        return MW_IN_DB_ERROR;
    }

    try
    {
        std::string analyzer;
        std::string gUUID;

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
        comm_layer_inner_warning("CMiddleWare::CreateCrashReport(): " + e.what());
        if (e.type() == EXCEP_DD_OPEN)
        {
            return MW_ERROR;
        }
        else if (e.type() == EXCEP_DD_LOAD)
        {
            return MW_FILE_ERROR;
        }
        return MW_CORRUPTED;
    }

    return MW_OK;
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

std::string CMiddleWare::DeleteCrashInfo(const std::string& pUUID,
                                         const std::string& pUID)
{
    database_row_t row;
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database->Connect();
    row = database->GetUUIDData(pUUID, pUID);
    database->Delete(pUUID, pUID);
    database->DisConnect();

    return row.m_sDebugDumpDir;
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

CMiddleWare::mw_result_t CMiddleWare::SavePackageDescriptionToDebugDump(const std::string& pExecutable,
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
                comm_layer_inner_debug("Executable doesn't belong to any package");
                return MW_PACKAGE_ERROR;
            }
            comm_layer_inner_debug("Blacklisted package");
            return MW_BLACKLISTED;
        }
        if (m_bOpenGPGCheck)
        {
            if (!m_RPM.CheckFingerprint(packageName) ||
                !m_RPM.CheckHash(packageName, pExecutable))
            {
                comm_layer_inner_debug("Can not find package");
                return MW_GPG_ERROR;
            }
        }
    }

    std::string description = m_RPM.GetDescription(packageName);

    CDebugDump dd;
    try
    {
        dd.Open(pDebugDumpDir);
        dd.SaveText(FILENAME_PACKAGE, package);
        dd.SaveText(FILENAME_DESCRIPTION, description);
        dd.Close();
    }
    catch (CABRTException& e)
    {
        comm_layer_inner_warning("CMiddleWare::SavePackageDescriptionToDebugDump(): " + e.what());
        if (e.type() == EXCEP_DD_SAVE)
        {
            dd.Close();
            return MW_FILE_ERROR;
        }
        return MW_ERROR;
    }

    return MW_OK;
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

CMiddleWare::mw_result_t CMiddleWare::SaveDebugDumpToDatabase(const std::string& pUUID,
                                                              const std::string& pUID,
                                                              const std::string& pTime,
                                                              const std::string& pDebugDumpDir,
                                                              map_crash_info_t& pCrashInfo)
{
    mw_result_t res;
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database_row_t row;
    database->Connect();
    database->Insert(pUUID, pUID, pDebugDumpDir, pTime);
    row = database->GetUUIDData(pUUID, pUID);
    database->DisConnect();
    res = GetCrashInfo(pUUID, pUID, pCrashInfo);
    if (row.m_sReported == "1")
    {
        comm_layer_inner_debug("Crash is already reported");
        return MW_REPORTED;
    }
    if (row.m_sCount != "1")
    {
        comm_layer_inner_debug("Crash is in database already");
        return MW_OCCURED;
    }
    return res;
}

CMiddleWare::mw_result_t CMiddleWare::SaveDebugDump(const std::string& pDebugDumpDir)
{
    map_crash_info_t info;
    return SaveDebugDump(pDebugDumpDir, info);
}

CMiddleWare::mw_result_t CMiddleWare::SaveDebugDump(const std::string& pDebugDumpDir,
                                                    map_crash_info_t& pCrashInfo)
{
    std::string lUUID;
    std::string UID;
    std::string time;
    std::string analyzer;
    std::string executable;
    CDebugDump dd;
    mw_result_t res;

    try
    {
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_TIME, time);
        dd.LoadText(FILENAME_UID, UID);
        dd.LoadText(FILENAME_ANALYZER, analyzer);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.Close();
    }
    catch (CABRTException& e)
    {
        comm_layer_inner_warning("CMiddleWare::SaveDebugDump(): " + e.what());
        if (e.type() == EXCEP_DD_SAVE)
        {
            dd.Close();
            return MW_FILE_ERROR;
        }
        return MW_ERROR;
    }

    if (IsDebugDumpSaved(UID, pDebugDumpDir))
    {
        return MW_IN_DB;
    }
    if ((res = SavePackageDescriptionToDebugDump(executable, pDebugDumpDir)) != MW_OK)
    {
        return res;
    }

    lUUID = GetLocalUUID(analyzer, pDebugDumpDir);

    return SaveDebugDumpToDatabase(lUUID, UID, time, pDebugDumpDir, pCrashInfo);
}

CMiddleWare::mw_result_t CMiddleWare::GetCrashInfo(const std::string& pUUID,
                                                   const std::string& pUID,
                                                   map_crash_info_t& pCrashInfo)
{
    pCrashInfo.clear();
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database_row_t row;
    database->Connect();
    row = database->GetUUIDData(pUUID, pUID);
    database->DisConnect();

    CDebugDump dd;
    std::string package;
    std::string executable;
    std::string description;

    try
    {
        dd.Open(row.m_sDebugDumpDir);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_PACKAGE, package);
        dd.LoadText(FILENAME_DESCRIPTION, description);
        dd.Close();
    }
    catch (CABRTException& e)
    {
        comm_layer_inner_warning("CMiddleWare::GetCrashInfo(): " + e.what());
        if (e.type() == EXCEP_DD_LOAD)
        {
            dd.Close();
            return MW_FILE_ERROR;
        }
        return MW_ERROR;
    }
    add_crash_data_to_crash_info(pCrashInfo, CD_EXECUTABLE, executable);
    add_crash_data_to_crash_info(pCrashInfo, CD_PACKAGE, package);
    add_crash_data_to_crash_info(pCrashInfo, CD_DESCRIPTION, description);
    add_crash_data_to_crash_info(pCrashInfo, CD_UUID, row.m_sUUID);
    add_crash_data_to_crash_info(pCrashInfo, CD_UID, row.m_sUID);
    add_crash_data_to_crash_info(pCrashInfo, CD_COUNT, row.m_sCount);
    add_crash_data_to_crash_info(pCrashInfo, CD_TIME, row.m_sTime);
    add_crash_data_to_crash_info(pCrashInfo, CD_REPORTED, row.m_sReported);
    add_crash_data_to_crash_info(pCrashInfo, CD_MWDDD, row.m_sDebugDumpDir);

    return MW_OK;
}

vector_strings_t CMiddleWare::GetUUIDsOfCrash(const std::string& pUID)
{
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    vector_database_rows_t rows;
    database->Connect();
    rows = database->GetUIDData(pUID);
    database->DisConnect();

    vector_strings_t UUIDs;
    int ii;
    for (ii = 0; ii < rows.size(); ii++)
    {
        UUIDs.push_back(rows[ii].m_sUUID);
    }

    return UUIDs;
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
