/*
    DebugDump.cpp

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

#include "CCpp.h"
#include <fstream>
#include <ctype.h>
#include "DebugDump.h"
#include <sstream>
#include <iostream>

#define CORE_PATTERN_IFACE "/proc/sys/kernel/core_pattern"
#define CORE_PATTERN "|"CCPP_HOOK_PATH" %p %s"

#define DEBUGINFO_INSTALL "debuginfo-install -y "

CLanguageCCpp::CLanguageCCpp() :
	m_bMemoryMap(false)
{}

void CLanguageCCpp::InstallDebugInfos(const std::string& pPackage)
{
    char line[1024];
    std::string command = DEBUGINFO_INSTALL + pPackage;
    std::string packageName = pPackage.substr(0, pPackage.rfind("-", pPackage.rfind("-") - 1));
    std::string packageERV = pPackage.substr(packageName.length());
    std::string installed = "Package "+packageName+"-debuginfo"+packageERV+" already installed and latest version";
    std::string canNotInstall = "No debuginfo packages available to install";
    FILE *fp = popen(command.c_str(), "r");
    if (fp == NULL)
    {
        throw "CLanguageCCpp::InstallDebugInfos(): cannot install debuginfos for " + pPackage ;
    }
    while (fgets(line, sizeof(line), fp))
    {
        std::string text = line;
        std::cerr << text;
        if (text.find(installed) != std::string::npos)
        {
            pclose(fp);
            return;
        }
        if (text.find(canNotInstall) != std::string::npos)
        {
            pclose(fp);
            throw "CLanguageCCpp::InstallDebugInfos(): cannot install debuginfos for " + pPackage ;
        }
    }
    if (pclose(fp) != 0)
    {
        throw "CLanguageCCpp::InstallDebugInfos(): cannot install debuginfos for " + pPackage ;
    }
}

std::string CLanguageCCpp::GetLocalUUID(const std::string& pDebugDumpDir)
{
	std::stringstream ss;
	char* core;
	unsigned int size;
	CDebugDump dd;
	dd.Open(pDebugDumpDir);
	dd.LoadBinary(FILENAME_BINARYDATA1, &core, &size);
	dd.Close();
	// TODO: compute local UUID
	ss << size;
	return ss.str();
}
std::string CLanguageCCpp::GetGlobalUUID(const std::string& pDebugDumpDir)
{
    std::stringstream ss;
    CDebugDump dd;
    std::string backtrace;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_TEXTDATA1, backtrace);
    dd.Close();
    // TODO: compute global UUID
    ss << backtrace.length();
    return ss.str();
}

void CLanguageCCpp::CreateReport(const std::string& pDebugDumpDir)
{
    std::string package;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_PACKAGE, package);

    InstallDebugInfos(package);

    dd.SaveText(FILENAME_TEXTDATA1, "backtrace of the crashed C/C++ application");
    if (m_bMemoryMap)
    {
        dd.SaveText(FILENAME_TEXTDATA2, "memory map of the crashed C/C++ application");
    }
    dd.Close();
}

void CLanguageCCpp::Init()
{
	std::ifstream fInCorePattern;
	fInCorePattern.open(CORE_PATTERN_IFACE);
	if (fInCorePattern.is_open())
	{
		getline(fInCorePattern, m_sOldCorePattern);
		fInCorePattern.close();
	}
	std::ofstream fOutCorePattern;
	fOutCorePattern.open(CORE_PATTERN_IFACE);
	if (fOutCorePattern.is_open())
	{
		fOutCorePattern << CORE_PATTERN << std::endl;
		fOutCorePattern.close();
	}
}


void CLanguageCCpp::DeInit()
{
	std::ofstream fOutCorePattern;
	fOutCorePattern.open(CORE_PATTERN_IFACE);
	if (fOutCorePattern.is_open())
	{
		fOutCorePattern << m_sOldCorePattern << std::endl;
		fOutCorePattern.close();
	}
}

void CLanguageCCpp::SetSettings(const map_settings_t& pSettings)
{
    if (pSettings.find("MemoryMap")!= pSettings.end())
      {
          m_bMemoryMap = pSettings.find("MemoryMap")->second == "yes";
      }
}
