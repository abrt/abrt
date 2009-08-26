#ifndef COMMLAYERSERVER_H_
#define COMMLAYERSERVER_H_

#include <string>
#include "abrtlib.h"
#include "CrashTypes.h"

class CCommLayerServer {
    public:
        CCommLayerServer();
        virtual ~CCommLayerServer();

        /* just stubs to be called when not implemented in specific comm layer */
        virtual void Crash(const std::string& progname, const std::string& uid) {}
        virtual void AnalyzeComplete(const map_crash_report_t& arg1) {}
        virtual void Error(const std::string& arg1) {}
        virtual void Update(const std::string& pDest, const std::string& pMessage) {};
        virtual void Warning(const std::string& pMessage) {};
        virtual void JobDone(const std::string &pDest, uint64_t pJobID) {};
};

#endif //COMMLAYERSERVER_H_
