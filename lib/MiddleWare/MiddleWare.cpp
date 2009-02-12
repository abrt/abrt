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
    m_pPluginManager(NULL)
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

void CMiddleWare::DebugDump2Report(const std::string& pDebugDumpDir, CReporter::report_t& pReport)
{
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_ARCHITECTURE, pReport.m_sArchitecture);
    dd.LoadText(FILENAME_KERNEL, pReport.m_sKernel);
    dd.LoadText(FILENAME_PACKAGE, pReport.m_sPackage);
    dd.LoadText(FILENAME_EXECUTABLE, pReport.m_sExecutable);

    if (dd.Exist(FILENAME_TEXTDATA1))
    {
        dd.LoadText(FILENAME_TEXTDATA1, pReport.m_sTextData1);
    }
    if (dd.Exist(FILENAME_TEXTDATA2))
    {
        dd.LoadText(FILENAME_TEXTDATA2, pReport.m_sTextData2);
    }
    if (dd.Exist(FILENAME_BINARYDATA1))
    {
        pReport.m_bBinaryData1 = pDebugDumpDir + "/" + FILENAME_BINARYDATA1;
    }
    if (dd.Exist(FILENAME_BINARYDATA2))
    {
        pReport.m_bBinaryData2 = pDebugDumpDir + "/" + FILENAME_BINARYDATA2;
    }
}

void CMiddleWare::RegisterPlugin(const std::string& pName)
{
    m_pPluginManager->RegisterPlugin(pName);
}

void CMiddleWare::UnRegisterPlugin(const std::string& pName)
{
    m_pPluginManager->UnRegisterPlugin(pName);
}


std::string CMiddleWare::GetLocalUUIDLanguage(const std::string& pLanguage,
                                              const std::string& pDebugDumpDir)
{
    CLanguage* language = m_pPluginManager->GetLanguage(pLanguage);
    return language->GetLocalUUID(pDebugDumpDir);
}

std::string CMiddleWare::GetLocalUUIDApplication(const std::string& pApplication,
                                                 const std::string& pDebugDumpDir)
{
    CApplication* application = m_pPluginManager->GetApplication(pApplication);
    return application->GetLocalUUID(pDebugDumpDir);
}


void CMiddleWare::CreateReportLanguage(const std::string& pLanguage,
                                       const std::string& pDebugDumpDir)
{
    CLanguage* language = m_pPluginManager->GetLanguage(pLanguage);
    return language->CreateReport(pDebugDumpDir);
}

void CMiddleWare::CreateReportApplication(const std::string& pApplication,
                                          const std::string& pDebugDumpDir)
{
    CApplication* application = m_pPluginManager->GetApplication(pApplication);
    return application->CreateReport(pDebugDumpDir);
}


void CMiddleWare::CreateReport(const std::string& pDebugDumpDir,
                               crash_report_t& pCrashReport)
{
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    if (dd.Exist(FILENAME_APPLICATION))
    {
        std::string application;
        dd.LoadText(FILENAME_APPLICATION, application);
        pCrashReport.m_sPlugin2ReportersName = application;
        CreateReportApplication(application, pDebugDumpDir);
    }
    if (dd.Exist(FILENAME_LANGUAGE))
    {
        std::string language;
        dd.LoadText(FILENAME_LANGUAGE, language);
        pCrashReport.m_sPlugin2ReportersName = language;
        CreateReportLanguage(language, pDebugDumpDir);
    }
    DebugDump2Report(pDebugDumpDir, pCrashReport.m_Report);
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
    if (row.m_sUUID != pUUID)
    {
        throw std::string("CMiddleWare::GetReport(): UUID '"+pUUID+"' is not in database.");
    }

    CreateReport(row.m_sDebugDumpPath, pCrashReport);
}

void CMiddleWare::Report(const crash_report_t& pCrashReport)
{
    std::string plugin2ReportersName = pCrashReport.m_sPlugin2ReportersName;
    if (m_mapPlugin2Reporters.find(plugin2ReportersName) != m_mapPlugin2Reporters.end())
    {
        set_reporters_t::iterator it_r;
        for (it_r = m_mapPlugin2Reporters[plugin2ReportersName].begin();
             it_r != m_mapPlugin2Reporters[plugin2ReportersName].end();
             it_r++)
        {
            CReporter* reporter = m_pPluginManager->GetReporter(*it_r);
            reporter->Report(pCrashReport.m_Report);
        }
    }
}

int CMiddleWare::SaveDebugDump(const std::string& pDebugDumpPath, crash_info_t& pCrashInfo)
{
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);

    std::string UUID;
    std::string UID;
    std::string package;
    std::string executable;
    std::string time;

    CDebugDump dd;
    dd.Open(pDebugDumpPath);

    dd.LoadText(FILENAME_PACKAGE, package);
    dd.LoadText(FILENAME_TIME, time);

    if (package == "" ||
        m_setBlackList.find(package.substr(0, package.find("-"))) != m_setBlackList.end())
    {
        dd.Delete(pDebugDumpPath);
        return 0;
    }

    if (dd.Exist(FILENAME_APPLICATION))
    {
        std::string application;
        dd.LoadText(FILENAME_APPLICATION, application);
        UUID = GetLocalUUIDApplication(application, pDebugDumpPath);
    }
    if (dd.Exist(FILENAME_LANGUAGE))
    {
        std::string language;
        dd.LoadText(FILENAME_LANGUAGE, language);
        UUID = GetLocalUUIDLanguage(language, pDebugDumpPath);
    }
    if (UUID == "")
    {
        throw std::string("CMiddleWare::SaveDebugDumpToDataBase(): Wrong UUID.");
    }

    dd.LoadText(FILENAME_UID, UID);
    dd.LoadText(FILENAME_EXECUTABLE, executable);

    database_row_t row;
    database->Connect();
    database->Insert(UUID, UID, pDebugDumpPath, time);
    row = database->GetUUIDData(UUID, UID);
    database->DisConnect();

    if (row.m_sReported == "1")
    {
        dd.Delete(pDebugDumpPath);
        return 0;
    }
    if (row.m_sCount != "1")
    {
        dd.Delete(pDebugDumpPath);
    }

    pCrashInfo.m_sUUID = UUID;
    pCrashInfo.m_sUID = UID;
    pCrashInfo.m_sCount = row.m_sCount;
    pCrashInfo.m_sExecutable = executable;
    pCrashInfo.m_sPackage = package;
    pCrashInfo.m_sTime = row.m_sTime;

    return 1;
}

CMiddleWare::vector_crash_infos_t CMiddleWare::GetCrashInfos(const std::string& pUID)
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
        crash_info_t info;
        CDebugDump dd;
        info.m_sUUID = rows[ii].m_sUUID;
        info.m_sUID = rows[ii].m_sUID;
        info.m_sCount = rows[ii].m_sCount;

        dd.Open(rows[ii].m_sDebugDumpPath);
        dd.LoadText(FILENAME_EXECUTABLE, data);
        info.m_sExecutable = data;
        dd.LoadText(FILENAME_PACKAGE, data);
        info.m_sPackage = data;

        infos.push_back(info);
    }

    return infos;
}

