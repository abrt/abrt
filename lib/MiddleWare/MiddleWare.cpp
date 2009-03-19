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
#include "Settings.h"

CMiddleWare::CMiddleWare(const std::string& pPlugisConfDir,
                         const std::string& pPlugisLibDir,
                         const std::string& pMiddleWareConfFile) :
    m_pPluginManager(NULL),
    m_bOpenGPGCheck(true)
{
    m_pPluginManager = new CPluginManager(pPlugisConfDir, pPlugisLibDir);
    if (m_pPluginManager == NULL)
    {
        throw std::string("Not enought memory.");
    }
    m_pPluginManager->LoadPlugins();
    LoadSettings(pMiddleWareConfFile);

    set_enabled_plugins_t::iterator it_p;
    for (it_p = m_setEnabledPlugins.begin(); it_p != m_setEnabledPlugins.end(); it_p++)
    {
        m_pPluginManager->RegisterPlugin(*it_p);
    }
}

CMiddleWare::~CMiddleWare()
{
    m_pPluginManager->UnLoadPlugins();
    delete m_pPluginManager;
}

void CMiddleWare::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    load_settings(pPath, settings);
    if (settings.find("BlackList") != settings.end())
    {
        parse_settings(settings["BlackList"], m_setBlackList);
    }
    if (settings.find("EnabledPlugins") != settings.end())
    {
        parse_settings(settings["EnabledPlugins"], m_setEnabledPlugins);
    }
    if (settings.find("OpenGPGPublicKeys") != settings.end())
    {
        parse_settings(settings["OpenGPGPublicKeys"], m_setOpenGPGKeys);
        set_opengpg_keys_t::iterator it_k;
        for (it_k = m_setOpenGPGKeys.begin(); it_k != m_setOpenGPGKeys.end(); it_k++)
        {
            m_RPM.LoadOpenGPGPublicKey(*it_k);
        }
    }
    if (settings.find("EnableOpenGPG") != settings.end())
    {
        m_bOpenGPGCheck = settings["EnableOpenGPG"] == "yes";
    }
    if (settings.find("Database") != settings.end())
    {
        m_sDatabase = settings["Database"];
        if (m_setEnabledPlugins.find(m_sDatabase) == m_setEnabledPlugins.end())
        {
            throw std::string("Database plugin '"+m_sDatabase+"' isn't enabled.");
        }
    }
    else
    {
        throw std::string("No database plugin is selected.");
    }
    set_enabled_plugins_t::iterator it_p;
    for (it_p = m_setEnabledPlugins.begin(); it_p != m_setEnabledPlugins.end(); it_p++)
    {
        if (settings.find(*it_p) != settings.end())
        {
            set_reporters_t reporters;
            parse_settings(settings[*it_p], reporters);
            m_mapPlugin2Reporters[*it_p] = reporters;
        }
    }
}

void CMiddleWare::DebugDump2Report(const std::string& pDebugDumpDir, crash_report_t& pCrashReport)
{
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_UUID, pCrashReport.m_sUUID);
    dd.LoadText(FILENAME_ARCHITECTURE, pCrashReport.m_sArchitecture);
    dd.LoadText(FILENAME_KERNEL, pCrashReport.m_sKernel);
    dd.LoadText(FILENAME_PACKAGE, pCrashReport.m_sPackage);
    dd.LoadText(FILENAME_EXECUTABLE, pCrashReport.m_sExecutable);

    if (dd.Exist(FILENAME_CMDLINE))
    {
        dd.LoadText(FILENAME_CMDLINE, pCrashReport.m_sCmdLine);
    }
    if (dd.Exist(FILENAME_RELEASE))
    {
        dd.LoadText(FILENAME_RELEASE, pCrashReport.m_sRelease);
    }
    if (dd.Exist(FILENAME_TEXTDATA1))
    {
        dd.LoadText(FILENAME_TEXTDATA1, pCrashReport.m_sTextData1);
    }
    if (dd.Exist(FILENAME_TEXTDATA2))
    {
        dd.LoadText(FILENAME_TEXTDATA2, pCrashReport.m_sTextData2);
    }
    if (dd.Exist(FILENAME_BINARYDATA1))
    {
        pCrashReport.m_sBinaryData1 = pDebugDumpDir + "/" + FILENAME_BINARYDATA1;
    }
    if (dd.Exist(FILENAME_BINARYDATA2))
    {
        pCrashReport.m_sBinaryData2 = pDebugDumpDir + "/" + FILENAME_BINARYDATA2;
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

void CMiddleWare::CreateReport(const std::string& pUUID,
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
        throw std::string("CMiddleWare::CreateReport(): UUID '"+pUUID+"' is not in database.");
    }

    std::string analyzer;
    std::string UUID;
    CDebugDump dd;
    dd.Open(row.m_sDebugDumpDir);

    dd.LoadText(FILENAME_ANALYZER, analyzer);
    CreateReport(analyzer, row.m_sDebugDumpDir);
    UUID = GetGlobalUUID(analyzer, row.m_sDebugDumpDir);

    dd.SaveText(FILENAME_UUID, UUID);
    dd.Close();

    DebugDump2Report(row.m_sDebugDumpDir, pCrashReport);

    pCrashReport.m_sMWID =  analyzer + ";" + pUID + ";" + pUUID  ;
}

void CMiddleWare::Report(const crash_report_t& pCrashReport)
{
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = pCrashReport.m_sMWID.find(";", pos1);
    std::string lanAppPlugin = pCrashReport.m_sMWID.substr(pos1, pos2);
    pos1 = pos2 + 1;
    pos2 = pCrashReport.m_sMWID.find(";", pos1);
    std::string UID = pCrashReport.m_sMWID.substr(pos1, pos2 - pos1);
    pos1 = pos2 + 1;
    std::string UUID = pCrashReport.m_sMWID.substr(pos1);;

    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);
    database->Connect();
    database->SetReported(UUID, UID);
    database->DisConnect();

    if (m_mapPlugin2Reporters.find(lanAppPlugin) != m_mapPlugin2Reporters.end())
    {
        set_reporters_t::iterator it_r;
        for (it_r = m_mapPlugin2Reporters[lanAppPlugin].begin();
             it_r != m_mapPlugin2Reporters[lanAppPlugin].end();
             it_r++)
        {
            CReporter* reporter = m_pPluginManager->GetReporter(*it_r);
            reporter->Report(pCrashReport);
        }
    }
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

