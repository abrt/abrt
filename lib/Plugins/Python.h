#ifndef PYTHON_H_
#define PYTHON_H_

#include <string>
#include "Plugin.h"
#include "Analyzer.h"

class CAnalyzerPython : public CAnalyzer
{
    public:
        virtual std::string GetLocalUUID(const char *pDebugDumpDir);
        virtual std::string GetGlobalUUID(const char *pDebugDumpDir);
        virtual void CreateReport(const char *pDebugDumpDir, int force) {}
        virtual void Init();
        virtual void DeInit();
};

#endif /* PYTHON_H_ */
