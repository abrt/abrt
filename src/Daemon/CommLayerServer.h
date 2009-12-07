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
        virtual void Crash(const char *progname, const char *uid_str) {}
        virtual void JobDone(const char* pDest, const char* pUUID) = 0;
        virtual void QuotaExceed(const char* str) {}

        virtual void Update(const char* pMessage, const char* peer, uint64_t pJobID) {};
        virtual void Warning(const char* pMessage, const char* peer, uint64_t pJobID) {};
};

#endif
