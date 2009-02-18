#ifndef LOGGER_H_
#define LOGGER_H_

#include "Plugin.h"
#include "Reporter.h"

class CLogger : public CReporter
{
    private:
        std::string m_sLogPath;
        bool m_bAppendLogs;
    public:
        CLogger();
        virtual ~CLogger() {}

        void Init() {}
        void DeInit() {}
        void SetSettings(const map_settings_t& pSettings);

        void Report(const crash_report_t& pReport);
};


PLUGIN_INFO(REPORTER,
            "Logger",
            "0.0.1",
            "Write a report to a specific file",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/crash-catcher/wiki");

PLUGIN_INIT(CLogger);

#endif /* LOGGER_H_ */
