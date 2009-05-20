#include "CommLayerServer.h"
#include <glib.h>

#define SOCKET_PATH "/tmp/abrt.socket"

#define MESSAGE_DELETE_DEBUG_DUMP   "(DELETE_DEBUG_DUMP)"
#define MESSAGE_GET_CRASH_INFOS     "(GET_CRASH_INFOS)"
#define MESSAGE_REPORT              "(REPORT)"
#define MESSAGE_CREATE_REPORT       "(CREATE_REPORT)"

class CCommLayerServerSocket : public CCommLayerServer
{
    private:
        typedef std::map<int, GIOChannel*> map_clinet_channels_t;

        int m_nSocket;
        GIOChannel* m_pGSocket;
        map_clinet_channels_t m_mapClientChannels;

        void Send(const std::string& pData, GIOChannel *pDestination);

        static gboolean server_socket_cb(GIOChannel *source, GIOCondition condition, gpointer data);
        static gboolean client_socket_cb(GIOChannel *source, GIOCondition condition, gpointer data);

        std::string GetSenderUID(int pSenderSocket);
        void ProcessMessage(const std::string& pMessage, GIOChannel *pSource);

    public:
        CCommLayerServerSocket();
        virtual ~CCommLayerServerSocket();

        virtual vector_crash_infos_t GetCrashInfos(const std::string &pDBusSender);
        virtual map_crash_report_t CreateReport(const std::string &pUUID,const std::string &pDBusSender);
        virtual bool Report(map_crash_report_t pReport);
        virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pDBusSender);

        virtual void Crash(const std::string& arg1);
        virtual void AnalyzeComplete(map_crash_report_t arg1);
        virtual void Error(const std::string& arg1);
};
