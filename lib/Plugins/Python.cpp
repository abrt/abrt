#include "Python.h"
#include "DebugDump.h"

#include <fstream>
#include "ABRTException.h"

#define FILENAME_BACKTRACE      "backtrace"
#define PYHOOK_CONFIG          "/etc/abrt/pyhook.conf"

std::string CAnalyzerPython::CreateHash(const std::string& pDebugDumpDir)
{
    std::string uuid;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText("uuid", uuid);
    dd.Close();
    return uuid;
}

std::string CAnalyzerPython::GetLocalUUID(const std::string& pDebugDumpDir)
{
    return CreateHash(pDebugDumpDir);
}
std::string CAnalyzerPython::GetGlobalUUID(const std::string& pDebugDumpDir)
{
    return GetLocalUUID(pDebugDumpDir);
}

void CAnalyzerPython::Init()
{
	std::ofstream fOutPySiteCustomize;
	fOutPySiteCustomize.open(PYHOOK_CONFIG);
	if (fOutPySiteCustomize.is_open())
	{
		fOutPySiteCustomize << "enabled = yes" << std::endl;
		fOutPySiteCustomize.close();
	}
}

void CAnalyzerPython::DeInit()
{
    // TODO: remove copied abrt exception handler
    std::ofstream fOutPySiteCustomize;
	fOutPySiteCustomize.open(PYHOOK_CONFIG);
	if (fOutPySiteCustomize.is_open())
	{
		fOutPySiteCustomize << "enabled = no" << std::endl;
		fOutPySiteCustomize.close();
	}
}
