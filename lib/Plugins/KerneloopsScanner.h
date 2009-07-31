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

    public:
        /* For standalone oops processor */
        void SaveOopsToDebugDump();
        int ScanDmesg();
        int ScanSysLogFile(const char *filename);

        /* Plugin intarface */
        CKerneloopsScanner() :
            m_sSysLogFile("/var/log/messages"),
            m_bSysLogFileScanned(false)
        {}
        virtual ~CKerneloopsScanner() {}
        virtual void Run(const std::string& pActionDir,
                         const std::string& pArgs);
        virtual void LoadSettings(const std::string& pPath);
};

#endif /* KERNELOOPSSCANNER_H_ */
