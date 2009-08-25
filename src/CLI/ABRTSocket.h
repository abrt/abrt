#ifndef ABRTSOCKET_H_
#define ABRTSOCKET_H_

#include <string>

#include "CrashTypes.h"

class CABRTSocket
{
    private:
        int m_nSocket;

        void Send(const std::string& pMessage);
        void Recv(std::string& pMessage);

    public:
        CABRTSocket();
        ~CABRTSocket();

        void Connect(const std::string& pPath);
        void DisConnect();

        vector_crash_infos_t GetCrashInfos();
        map_crash_report_t CreateReport(const std::string& pUUID);
        void Report(const map_crash_report_t& pReport);
        void DeleteDebugDump(const std::string& pUUID);
};

#endif /* ABRTSOCKET_H_ */
