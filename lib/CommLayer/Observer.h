#ifndef OBSERVER_H_
#define OBSERVER_H_

#include "CrashTypes.h"

class CObserver {
    public:
        //CObserver();
        virtual ~CObserver() {}
        virtual void Status(const std::string& pMessage) = 0;
        virtual void Debug(const std::string& pMessage) = 0;
        virtual void Warning(const std::string& pMessage) = 0;
/* this should be implemented in daemon */
        virtual vector_crash_infos_t GetCrashInfos(const std::string &pDBusSender) = 0;
        virtual map_crash_report_t CreateReport(const std::string &pUUID,const std::string &pDBusSender) = 0;
        virtual bool Report(map_crash_report_t pReport) = 0;
        virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pDBusSender) = 0;
};

#endif /* OBSERVER_H_ */
