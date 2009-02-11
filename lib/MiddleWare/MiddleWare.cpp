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
        std::string blackList = settings["BlackList"];
        std::string::size_type ii_old = 0, ii_new = 0;
        ii_new = blackList.find(",");
        while (ii_new != std::string::npos)
        {
            m_setBlackList.insert(blackList.substr(ii_old, ii_new - ii_old));
            ii_old = ii_new + 1;
            ii_new = blackList.find(",",ii_old);
        }
        m_setBlackList.insert(blackList.substr(ii_old));
    }
    if (settings.find("EnabledPlugins") != settings.end())
    {
         std::string enabledPlugins = settings["EnabledPlugins"];
         std::string::size_type ii_old = 0, ii_new = 0;
         ii_new = enabledPlugins.find(",");
         while (ii_new != std::string::npos)
         {
             m_setEnabledPlugins.insert(enabledPlugins.substr(ii_old, ii_new - ii_old));
             ii_old = ii_new + 1;
             ii_new = enabledPlugins.find(",",ii_old);
         }
         m_setEnabledPlugins.insert(enabledPlugins.substr(ii_old));
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
}

void CMiddleWare::RegisterPlugin(const std::string& pName)
{
    m_pPluginManager->RegisterPlugin(pName);
}

void CMiddleWare::UnRegisterPlugin(const std::string& pName)
{
    m_pPluginManager->UnRegisterPlugin(pName);
}


std::string CMiddleWare::GetLocalUUIDLanguage(const std::string& pLanguage, const std::string& pDebugDumpPath)
{
    CLanguage* language = m_pPluginManager->GetLanguage(pLanguage);
    return language->GetLocalUUID(pDebugDumpPath);
}

std::string CMiddleWare::GetLocalUUIDApplication(const std::string& pApplication, const std::string& pDebugDumpPath)
{
    CApplication* application = m_pPluginManager->GetApplication(pApplication);
    return application->GetLocalUUID(pDebugDumpPath);
}


void CMiddleWare::GetReport(const std::string& pUUID, const std::string& pUID)
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
    // TODO: finish this
}

int CMiddleWare::Report(const std::string& pReport)
{
    // TODO: write this
}

int CMiddleWare::SaveDebugDump(const std::string& pDebugDumpPath, crash_info_t& pCrashInfo)
{
    CDatabase* database = m_pPluginManager->GetDatabase(m_sDatabase);

    std::string UUID;
    std::string UID;
    std::string package;
    std::string executable;

    CDebugDump dd;
    dd.Open(pDebugDumpPath);

    dd.LoadText(FILENAME_PACKAGE, package);

    if (package == "" ||
        m_setBlackList.find(package.substr(0, package.find("-"))) != m_setBlackList.end())
    {
        dd.Delete(pDebugDumpPath);
        return 0;
    }

    if (dd.Exist(FILENAME_LANGUAGE))
    {
        std::string language;
        dd.LoadText(FILENAME_LANGUAGE, language);
        UUID = GetLocalUUIDLanguage(language, pDebugDumpPath);
    }
    else if (0)
    {
        // TODO: how to get UUID from app?
    }
    else
    {
        throw std::string("CMiddleWare::SaveDebugDumpToDataBase(): Can not get UUID.");
    }
    if (UUID == "")
    {
        throw std::string("CMiddleWare::SaveDebugDumpToDataBase(): Wrong UUID.");
    }

    dd.LoadText(FILENAME_UID, UID);
    dd.LoadText(FILENAME_EXECUTABLE, executable);

    database_row_t row;
    database->Connect();
    database->Insert(UUID, UID, pDebugDumpPath);
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

