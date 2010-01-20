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
