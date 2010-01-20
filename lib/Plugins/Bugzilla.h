#ifndef BUGZILLA_H_
#define BUGZILLA_H_

#include "Plugin.h"
#include "Reporter.h"

class CReporterBugzilla : public CReporter
{
    private:
        bool m_bNoSSLVerify;
        std::string m_sBugzillaURL;
        std::string m_sBugzillaXMLRPC;
        std::string m_sLogin;
        std::string m_sPassword;
        std::string m_sAttchmentInBase64;

        map_plugin_settings_t parse_settings(const map_plugin_settings_t& pSettings);

    public:
        CReporterBugzilla();
        virtual ~CReporterBugzilla();

        virtual std::string Report(const map_crash_data_t& pCrashData,
                                   const map_plugin_settings_t& pSettings,
                                   const char *pArgs);

        virtual void SetSettings(const map_plugin_settings_t& pSettings);
        virtual const map_plugin_settings_t& GetSettings();
};

#endif /* BUGZILLA_H_ */
