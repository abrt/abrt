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
        virtual void JobDone(const char* pDest, const char* pUUID) = 0;
        virtual void QuotaExceed(const char* str) {}

        virtual void Update(const std::string& pMessage, const char* peer, uint64_t pJobID) {};
        virtual void Warning(const std::string& pMessage, const char* peer, uint64_t pJobID) {};
};

#endif
