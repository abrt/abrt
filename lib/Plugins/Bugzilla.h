#ifndef BUGZILLA_H_
#define BUGZILLA_H_

#include "Plugin.h"
#include "Reporter.h"
#include <xmlrpc-c/client.hpp>

#include <nssb64.h>

class CReporterBugzilla : public CReporter
{
    private:
        typedef std::map<std::string, xmlrpc_c::value> map_xmlrpc_params_t;

        void NewXMLRPCClient();
        void DeleteXMLRPCClient();
        static PRInt32 Base64Encode_cb(void *arg, const char *obuf, PRInt32 size);
        void Login();
        void Logout();
        bool CheckUUIDInBugzilla(const std::string& pComponent, const std::string& pUUID);
        std::string NewBug(const map_crash_report_t& pCrashReport);
        void AddAttachments(const std::string& pBugId, const map_crash_report_t& pCrashReport);
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
        std::string m_sAttchmentInBase64;
        bool m_bNoSSLVerify;

    public:
        CReporterBugzilla();
        virtual ~CReporterBugzilla();
        virtual void LoadSettings(const std::string& pPath);
        virtual void SetSettings(const map_plugin_settings_t& pSettings);
        virtual map_plugin_settings_t GetSettings();
        virtual void Report(const map_crash_report_t& pCrashReport,
                            const std::string& pArgs);
};

#endif /* BUGZILLA_H_ */
