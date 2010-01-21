#ifndef ABRTSOCKET_H_
#define ABRTSOCKET_H_

#include <string>

#include "CrashTypes.h"

class CABRTSocket
{
    private:
        int m_nSocket;

        void Send(const char *pMessage);
        void Recv(std::string& pMessage);

    public:
        CABRTSocket();
        ~CABRTSocket();

        void Connect(const char *pPath);
        void Disconnect();

        vector_map_crash_data_t GetCrashInfos();
        map_crash_data_t CreateReport(const char *pUUID);
        void Report(const map_crash_data_t& pReport);
        int32_t DeleteDebugDump(const char *pUUID);
};

#endif /* ABRTSOCKET_H_ */
