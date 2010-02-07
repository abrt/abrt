/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
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

        virtual vector_map_crash_data_t GetCrashInfos(const char *pSender);
        virtual report_status_t Report(const map_crash_data_t& pReport, const char *pSender);
        virtual void DeleteDebugDump(const char *pUUID, const char *pSender);

        virtual void Crash(const char *arg1);
};
