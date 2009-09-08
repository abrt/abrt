#ifndef COMMLAYERSERVER_H_
#define COMMLAYERSERVER_H_

#include <string>
#include "abrtlib.h"
#include "CrashTypes.h"

class CCommLayerServer {
    public:
        int m_init_error;

        CCommLayerServer();
        virtual ~CCommLayerServer();

        /* just stubs to be called when not implemented in specific comm layer */
        virtual void Crash(const std::string& progname, const std::string& uid) {}
        virtual void Error(const std::string& arg1) {}
        virtual void Update(const std::string& pMessage, uint64_t pJobID) {};
        virtual void Warning(const std::string& pMessage) {};
        virtual void JobDone(const char* pDest, uint64_t pJobID) = 0;
        virtual void JobStarted(const char* pDest, uint64_t pJobID) {};
        virtual void Warning(const std::string& pMessage, uint64_t pJobID) {};
};

#endif
