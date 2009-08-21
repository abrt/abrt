#ifndef COMMLAYERSERVER_H_
#define COMMLAYERSERVER_H_

#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <iostream>
#include "Observer.h"
#include "CrashTypes.h"

/* just a helper function */
template< class T >
std::string
to_string( T x )
{
    std::ostringstream o;
    o << x;
    return o.str();
}


class CCommLayerServer {
    protected:
        CObserver *m_pObserver;
    public:
        CCommLayerServer();
        virtual ~CCommLayerServer();
        /* observer */
        void Attach(CObserver *pObs);
        void Detach(CObserver *pObs);
        void Notify(const std::string& pMessage);

        virtual vector_crash_infos_t GetCrashInfos(const std::string &pSender) = 0;
        virtual map_crash_report_t CreateReport(const std::string &pUUID,const std::string &pSender) = 0;
        virtual report_status_t Report(map_crash_report_t pReport,const std::string &pSender) = 0;
        virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pSender) = 0;

    public:
    /* just stubs to be called when not implemented in specific comm layer */
        virtual void Crash(const std::string& arg1) {}
        virtual void AnalyzeComplete(map_crash_report_t arg1) {}
        virtual void Error(const std::string& arg1) {}
        virtual void Update(const std::string& pDest, const std::string& pMessage) {};
        virtual void Warning(const std::string& pDest, const std::string& pMessage) {};
        virtual void JobDone(const std::string &pDest, uint64_t pJobID) {};
};

#endif //COMMLAYERSERVER_H_
