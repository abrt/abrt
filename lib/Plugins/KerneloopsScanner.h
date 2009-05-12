#ifndef KERNELOOPSSCANNER_H_
#define KERNELOOPSSCANNER_H_

#include "KerneloopsSysLog.h"
#include "Plugin.h"
#include "Action.h"

class CKerneloopsScanner : public CAction
{
    private:
        std::string m_sSysLogFile;
        CSysLog m_pSysLog;
        bool m_bSysLogFileScanned;

        void SaveOopsToDebugDump();
        void ScanDmesg();
        void ScanSysLogFile(const char *filename, int issyslog);
        void WriteSysLog(int m_nCount);
    public:
        CKerneloopsScanner() :
            m_sSysLogFile("/var/log/messages"),
            m_bSysLogFileScanned(false)
        {}
        virtual ~CKerneloopsScanner() {}
        virtual void Run(const std::string& pActionDir,
                         const std::string& pArgs);
        virtual void LoadSettings(const std::string& pPath);
};

PLUGIN_INFO(ACTION,
            CKerneloopsScanner,
            "KerneloopsScanner",
            "0.0.1",
            "Save new Kerneloops crashes into debug dump dir",
            "anton@redhat.com",
            "http://people.redhat.com/aarapov");

#endif /* KERNELOOPSSCANNER_H_ */
