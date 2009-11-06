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
        virtual void Crash(const char *progname, const char *uid_str);
        virtual void JobDone(const char* pDest, const char* pUUID);
        virtual void QuotaExceed(const char* str);

        virtual void Update(const std::string& pMessage, const char* peer, uint64_t pJobID);
        virtual void Warning(const std::string& pMessage, const char* peer, uint64_t pJobID);
};

#endif
