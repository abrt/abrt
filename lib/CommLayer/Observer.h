#ifndef OBSERVER_H_
#define OBSERVER_H_

#include "CrashTypes.h"
#include <string>
#include <stdint.h>

class CObserver {
    public:
        //CObserver();
        virtual ~CObserver() {}
        virtual void Status(const std::string& pMessage, const std::string& pDest="0") = 0;
        virtual void Debug(const std::string& pMessage, const std::string& pDest="0") = 0;
        virtual void Warning(const std::string& pMessage, const std::string& pDest="0") = 0;
/* this should be implemented in daemon */
        virtual vector_crash_infos_t GetCrashInfos(const std::string &pSender) = 0;
        virtual map_crash_report_t CreateReport(const std::string &pUUID,const std::string &pSender) = 0;
        virtual uint64_t CreateReport_t(const std::string &pUUID,const std::string &pUID, const std::string &pSender){std::cout << "DEFAULT OBSERVER";return 0;};
        virtual bool Report(map_crash_report_t pReport, const std::string &pSender) = 0;
        virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pSender) = 0;
        virtual map_crash_report_t GetJobResult(uint64_t pJobID, const std::string &pSender) = 0;
};

#endif /* OBSERVER_H_ */
