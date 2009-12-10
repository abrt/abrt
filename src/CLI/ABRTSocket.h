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

        vector_crash_infos_t GetCrashInfos();
        map_crash_report_t CreateReport(const char *pUUID);
        void Report(const map_crash_report_t& pReport);
        int32_t DeleteDebugDump(const char *pUUID);
};

#endif /* ABRTSOCKET_H_ */
