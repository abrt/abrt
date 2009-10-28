#ifndef CATCUT_H_
#define CATCUT_H_

#include "Plugin.h"
#include "Reporter.h"

class CReporterCatcut : public CReporter
{
    private:
	std::string m_sCatcutURL;
	std::string m_sLogin;
	std::string m_sPassword;
	bool m_bNoSSLVerify;

    public:
        CReporterCatcut();
        virtual ~CReporterCatcut();

        virtual void SetSettings(const map_plugin_settings_t& pSettings);
        virtual map_plugin_settings_t GetSettings();

        virtual std::string Report(const map_crash_report_t& pCrashReport,
                                   const std::string& pArgs);
};

#endif /* CATCUT_H_ */
