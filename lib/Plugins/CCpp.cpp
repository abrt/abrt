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
#include <hash_map>

#define CORE_PATTERN_IFACE "/proc/sys/kernel/core_pattern"
#define CORE_PATTERN "|"CCPP_HOOK_PATH" %p %s"

#define DEBUGINFO_COMMAND "debuginfo-install -y "
#define GDB_COMMAND "gdb -batch -x "

CLanguageCCpp::CLanguageCCpp() :
	m_bMemoryMap(false)
{}

void CLanguageCCpp::InstallDebugInfos(const std::string& pPackage)
{
    char line[1024];
    std::string command = DEBUGINFO_COMMAND + pPackage;
    std::string packageName = pPackage.substr(0, pPackage.rfind("-", pPackage.rfind("-") - 1));
    std::string packageERV = pPackage.substr(packageName.length());
    std::string packageDebuginfo = packageName+"-debuginfo"+packageERV;
    std::string installed = "already installed and latest version";
    std::string canNotInstall = "No debuginfo packages available to install";
    FILE *fp = popen(command.c_str(), "r");
    std::cout << installed << std::endl;
    if (fp == NULL)
    {
        throw "CLanguageCCpp::InstallDebugInfos(): cannot execute " + command ;
    }
    while (fgets(line, sizeof(line), fp))
    {
        std::string text = line;
        if (text.find(packageDebuginfo) != std::string::npos &&
            text.find(installed) != std::string::npos)
        {
            pclose(fp);
            return;
        }
        if (text.find(canNotInstall) != std::string::npos)
        {
            pclose(fp);
            throw "CLanguageCCpp::InstallDebugInfos(): cannot install debuginfos for " + pPackage + " (" + canNotInstall + ")" ;
        }
    }
    if (pclose(fp) != 0)
    {
        throw "CLanguageCCpp::InstallDebugInfos(): cannot install debuginfos for " + pPackage ;
    }
}

void CLanguageCCpp::GetBacktrace(const std::string& pDebugDumpDir, std::string& pBacktrace)
{
    std::string tmpFile = "/tmp/" + pDebugDumpDir.substr(pDebugDumpDir.rfind("/"));
    std::ofstream fTmp;
    fTmp.open(tmpFile.c_str());
    if (fTmp.is_open())
    {
        std::string executable;
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.Close();
        fTmp << "file " << executable << std::endl;
        fTmp << "core " << pDebugDumpDir << "/" << FILENAME_BINARYDATA1 << std::endl;
        fTmp << "bt full" << std::endl;
        fTmp << "q" << std::endl;
        fTmp.close();
    }
    else
    {
        throw "CLanguageCCpp::GetBacktrace(): cannot create gdb script " + tmpFile ;
    }
    std::string command = GDB_COMMAND + tmpFile;
    RunCommand(command, pBacktrace);
}

void CLanguageCCpp::GetIndependentBacktrace(const std::string& pBacktrace, std::string& pIndependentBacktrace)
{
    int ii = 0;
    while (ii < pBacktrace.length())
    {
        std::string line = "";
        int jj = 0;

        while (pBacktrace[ii] != '\n' && ii < pBacktrace.length())
        {
            line += pBacktrace[ii];
            ii++;
        }
        while (isspace(line[jj]))
        {
            jj++;
        }
        if (line[jj] == '#')
        {
            while(jj < line.length())
            {
                if (isspace(line[jj]))
                {
                    jj++;
                }
                else if (line[jj] == '0' && line[jj+1] == 'x')
                {
                    while (isalnum(line[jj]))
                    {
                        jj++;
                    }
                }
                else
                {
                    pIndependentBacktrace += line[jj];
                    jj++;
                }
            }
        }
        ii++;
    }
}

void CLanguageCCpp::RunCommand(const std::string& pCommand, std::string& pOutput)
{
    char line[1024];

    FILE *fp = popen(pCommand.c_str(), "r");
    if (fp == NULL)
    {
        throw "CLanguageCCpp::GetBacktrace(): cannot execute " + pCommand ;
    }
    pOutput = "";
    while (fgets(line, sizeof(line), fp))
    {
        pOutput += line;
    }
    pclose(fp);
}

std::string CLanguageCCpp::GetLocalUUID(const std::string& pDebugDumpDir)
{
	std::stringstream ss;
	char* core;
	unsigned int size;
	std::string executable;
	CDebugDump dd;

	dd.Open(pDebugDumpDir);
	dd.LoadBinary(FILENAME_BINARYDATA1, &core, &size);
	dd.LoadText(FILENAME_EXECUTABLE, executable);
	dd.Close();
	// TODO: compute local UUID, remove this hack
	ss << executable << "_" << size;
	return ss.str();
}
std::string CLanguageCCpp::GetGlobalUUID(const std::string& pDebugDumpDir)
{
    std::stringstream ss;
    std::string backtrace;
    std::string independentBacktrace;
    __gnu_cxx::hash<const char*> hash;

    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_TEXTDATA1, backtrace);
    dd.Close();
    GetIndependentBacktrace(backtrace, independentBacktrace);
    // TODO: compute global UUID, remove this hack
    ss << hash(independentBacktrace.c_str());
    return ss.str();
}

void CLanguageCCpp::CreateReport(const std::string& pDebugDumpDir)
{
    std::string package;
    std::string backtrace;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_PACKAGE, package);

    InstallDebugInfos(package);
    GetBacktrace(pDebugDumpDir, backtrace);

    dd.SaveText(FILENAME_TEXTDATA1, backtrace);
    if (m_bMemoryMap)
    {
        dd.SaveText(FILENAME_TEXTDATA2, "memory map of the crashed C/C++ application, not implemented yet");
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
