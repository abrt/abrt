#ifndef BUGZILLA_H_
#define BUGZILLA_H_

#include "Plugin.h"
#include "Reporter.h"
#include <xmlrpc-c/client.hpp>

class CReporterBugzilla : public CReporter
{
    private:
        typedef std::map<std::string, xmlrpc_c::value> map_xmlrpc_params_t;

        void Login();
        void Logout();
        bool CheckUUIDInBugzilla(const std::string& pComponent, const std::string& pUUID);
        void NewBug(const map_crash_report_t& pCrashReport);
        void CreateNewBugDescription(const map_crash_report_t& pCrashReport,
                                     std::string& pDescription);
        void GetProductAndVersion(const std::string& pRelease,
                                  std::string& pProduct,
                                  std::string& pVersion);

        xmlrpc_c::clientXmlTransport_curl* m_pXmlrpcTransport;
        xmlrpc_c::client_xml* m_pXmlrpcClient;
        xmlrpc_c::carriageParm_curl0 *m_pCarriageParm;
        std::string m_sBugzillaURL;
        std::string m_sLogin;
        std::string m_sPassword;

    public:
        CReporterBugzilla();
        virtual ~CReporterBugzilla();
        virtual void LoadSettings(const std::string& pPath);
        virtual void Report(const map_crash_report_t& pCrashReport,
                            const std::string& pArgs);
};

PLUGIN_INFO(REPORTER,
            CReporterBugzilla,
            "Bugzilla",
            "0.0.1",
            "Check if a bug isn't already reported in a bugzilla "
            "and if not, report it.",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/crash-catcher/wiki");


#endif /* BUGZILLA_H_ */
