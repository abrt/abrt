#ifndef PYTHON_H_
#define PYTHON_H_

#include <string>
#include "Plugin.h"
#include "Analyzer.h"

class CAnalyzerPython : public CAnalyzer
{
    public:
        virtual std::string GetLocalUUID(const std::string& pDebugDumpDir);
        virtual std::string GetGlobalUUID(const std::string& pDebugDumpDir);
        virtual void CreateReport(const std::string& pDebugDumpDir) {}
        virtual void Init();
        virtual void DeInit();
        virtual std::string CreateHash(const std::string& pInput);
};

#endif /* PYTHON_H_ */
