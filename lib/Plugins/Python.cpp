/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#include "abrtlib.h"
#include "Python.h"
#include "DebugDump.h"
#include "ABRTException.h"

static std::string CreateHash(const char *pDebugDumpDir)
{
	CDebugDump dd;
	dd.Open(pDebugDumpDir);
	std::string uuid;
	dd.LoadText(FILENAME_UUID, uuid);
	return uuid;
}

std::string CAnalyzerPython::GetLocalUUID(const char *pDebugDumpDir)
{
	return CreateHash(pDebugDumpDir);
}
std::string CAnalyzerPython::GetGlobalUUID(const char *pDebugDumpDir)
{
	return GetLocalUUID(pDebugDumpDir);
}

void CAnalyzerPython::Init()
{
}

void CAnalyzerPython::DeInit()
{
}

PLUGIN_INFO(ANALYZER,
            CAnalyzerPython,
            "Python",
            "0.0.1",
            "Analyzes crashes in Python programs",
            "zprikryl@redhat.com, jmoskovc@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
