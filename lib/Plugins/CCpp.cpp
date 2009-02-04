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

#define CORE_PATTERN_IFACE "/proc/sys/kernel/core_pattern"
#define CORE_PATTERN CCPP_HOOK_PATH" %p %t %s"

CLanguageCCpp::CLanguageCCpp() :
	m_bMemoryMap(false)
{}

std::string CLanguageCCpp::GetLocalUUID(const std::string& pDebugDumpPath)
{
	std::stringstream ss;
	char* core;
	unsigned int size;
	CDebugDump dd;
	dd.Open(pDebugDumpPath);
	dd.LoadBinary(FILENAME_BINARYDATA1, &core, &size);

	// TODO: write proper handler
	ss << size;
	return ss.str();
}

std::string CLanguageCCpp::GetReport(const std::string& pDebugDumpPath)
{
	// TODO: install or mount debug-infos
	// TODO:
	return "report";
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
