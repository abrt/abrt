#include "CommLayerServer.h"
#include "DBusCommon.h"
#include <glib.h>

#define SOCKET_FILE VAR_RUN"/abrt.socket"
#define SOCKET_PERMISSION 0666

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

        virtual vector_crash_infos_t GetCrashInfos(const char *pSender);
        virtual report_status_t Report(const map_crash_report_t& pReport, const char *pSender);
        virtual void DeleteDebugDump(const char *pUUID, const char *pSender);

        virtual void Crash(const char *arg1);
};
