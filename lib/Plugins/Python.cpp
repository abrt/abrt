#include <fstream>
#include "Python.h"
#include "DebugDump.h"
#include "ABRTException.h"

#define FILENAME_BACKTRACE      "backtrace"
#define PYHOOK_CONFIG          "/etc/abrt/pyhook.conf"

static std::string CreateHash(const char *pDebugDumpDir)
{
	std::string uuid;
	CDebugDump dd;
	dd.Open(pDebugDumpDir);
	dd.LoadText("uuid", uuid);
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

PLUGIN_INFO(ANALYZER,
            CAnalyzerPython,
            "Python",
            "0.0.1",
            "Analyzes crashes in Python programs",
            "zprikryl@redhat.com, jmoskovc@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
