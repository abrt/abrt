#ifndef COMMLAYERSERVERDBUS_H_
#define COMMLAYERSERVERDBUS_H_

#include "CommLayerServer.h"

class CCommLayerServerDBus
: public CCommLayerServer
{
    public:
        CCommLayerServerDBus();
        virtual ~CCommLayerServerDBus();

        /* DBus signal senders */
        virtual void Crash(const std::string& progname, const std::string& uid);
        virtual void AnalyzeComplete(const map_crash_report_t& arg1);
        virtual void Error(const std::string& arg1);
        virtual void Update(const std::string& pMessage, uint64_t pJobID);
        //the job id should be enough in jobdone
        virtual void JobDone(const char* pDest, uint64_t pJobID);
        virtual void JobStarted(const char* pDest, uint64_t pJobID);
        virtual void Warning(const std::string& pMessage);
        virtual void Warning(const std::string& pMessage, uint64_t pJobID);
};

#endif
