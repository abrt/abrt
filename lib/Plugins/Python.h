#ifndef PYTHON_H_
#define PYTHON_H_

#include <string>
#include "Plugin.h"
#include "Analyzer.h"

class CAnalyzerPython : public CAnalyzer
{
    public:
        virtual ~CAnalyzerPython() {}
        virtual std::string GetLocalUUID(const std::string& pDebugDumpDir);
        virtual std::string GetGlobalUUID(const std::string& pDebugDumpDir);
        virtual void CreateReport(const std::string& pDebugDumpDir) {}
        virtual void Init();
        virtual void DeInit();
        virtual std::string CreateHash(const std::string& pInput);
};


PLUGIN_INFO(ANALYZER,
            CAnalyzerPython,
            "Python",
            "0.0.1",
            "Simple Python analyzer plugin.",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki");


#endif /* PYTHON_H_ */
