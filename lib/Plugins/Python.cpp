#include <fstream>
#include "Python.h"
#include "DebugDump.h"
#include "ABRTException.h"

#define FILENAME_BACKTRACE      "backtrace"

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
