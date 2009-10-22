#ifndef BUGZILLA_H_
#define BUGZILLA_H_

#include "Plugin.h"
#include "Reporter.h"
//#include <xmlrpc-c/client.hpp>

#include <nssb64.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
class CReporterBugzilla : public CReporter
{
    private:
    // C implementation

        bool m_bNoSSLVerify;
        bool m_bLoggedIn;
        static PRInt32 Base64Encode_cb(void *arg, const char *obuf, PRInt32 size);
        void Logout();
        std::string NewBug(const map_crash_report_t& pCrashReport);
        void AddAttachments(const std::string& pBugId, const map_crash_report_t& pCrashReport);
        void CreateNewBugDescription(const map_crash_report_t& pCrashReport,
                                     std::string& pDescription);
        void GetProductAndVersion(const std::string& pRelease,
                                  std::string& pProduct,
                                  std::string& pVersion);

        std::string m_sBugzillaURL;
        std::string m_sBugzillaXMLRPC;
        std::string m_sLogin;
        std::string m_sPassword;
        std::string m_sAttchmentInBase64;


    public:
        CReporterBugzilla();
        virtual ~CReporterBugzilla();
        virtual void SetSettings(const map_plugin_settings_t& pSettings);
        virtual map_plugin_settings_t GetSettings();
        virtual std::string Report(const map_crash_report_t& pCrashReport,
                                   const std::string& pArgs);
};

#endif /* BUGZILLA_H_ */
