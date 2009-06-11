#include "CommLayerServerSocket.h"
#include "CommLayerInner.h"
#include "ABRTException.h"
#include "CrashTypesSocket.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

void CCommLayerServerSocket::Send(const std::string& pData, GIOChannel *pDestination)
{
    ssize_t ret = -1;
    gsize len = pData.length();
    int offset = 0;
    GError *err = NULL;
    gchar* message = new gchar[len + 3];
    memcpy(message, pData.c_str(), len);
    message[len] = MESSAGE_END_MARKER;
    message[len + 1] = '\n';
    message[len + 2] = '\0';

    len = 0;
    while (len != strlen(message + offset))
    {
        offset += len;
        ret = g_io_channel_write_chars(pDestination, message + offset, strlen(message + offset), &len, &err);
        if (ret == G_IO_STATUS_ERROR)
        {
            comm_layer_inner_warning("Error during sending data.");
        }
    }

    g_io_channel_flush(pDestination, &err);
    delete[] message;
}

std::string CCommLayerServerSocket::GetSenderUID(int pSenderSocket)
{
    struct ucred creds;
    socklen_t len = sizeof(creds);
    if (getsockopt(pSenderSocket, SOL_SOCKET, SO_PEERCRED, &creds, &len) == -1)
    {
        throw CABRTException(EXCEP_ERROR, "CCommLayerServerSocket::GetSenderUID(): Error can get sender uid.");
    }
    std::stringstream ss;
    ss << creds.uid;
    return ss.str();
}

gboolean CCommLayerServerSocket::client_socket_cb(GIOChannel *source, GIOCondition condition, gpointer data)
{
    CCommLayerServerSocket* serverSocket = static_cast<CCommLayerServerSocket*>(data);
    std::string senderUID = serverSocket->GetSenderUID(g_io_channel_unix_get_fd(source));
    gchar buff[1];
    gsize len;
    GIOStatus ret;
    GError *err = NULL;
    bool receivingMessage = true;
    std::string message = "";

    if (condition & G_IO_HUP ||
        condition & G_IO_ERR ||
        condition & G_IO_NVAL)
    {
        comm_layer_inner_debug("Socket client disconnected.");
        g_io_channel_unref(serverSocket->m_mapClientChannels[g_io_channel_unix_get_fd(source)]);
        serverSocket->m_mapClientChannels.erase(g_io_channel_unix_get_fd(source));
        return FALSE;
    }

    // TODO: rewrite this
    while (receivingMessage)
    {
        ret = g_io_channel_read_chars(source, buff, 1, &len, &err);
        if (ret == G_IO_STATUS_ERROR)
        {
            comm_layer_inner_warning(std::string("Error while reading data from client socket: ") + err->message);
            return FALSE;
        }
        message += buff[0];

        if (message.length() > 2 &&
            message[message.length() - 2] == MESSAGE_END_MARKER &&
            message[message.length() - 1] == '\n')
        {
            receivingMessage = false;
            message = message.substr(0, message.length() - 2);
        }
    }

    serverSocket->ProcessMessage(message, source);
    return TRUE;
}

gboolean CCommLayerServerSocket::server_socket_cb(GIOChannel *source, GIOCondition condition, gpointer data)
{
    CCommLayerServerSocket* serverSocket = static_cast<CCommLayerServerSocket*>(data);
    int socket;
    struct sockaddr_un remote;
    socklen_t len = sizeof(remote);

    if (condition & G_IO_HUP ||
        condition & G_IO_ERR ||
        condition & G_IO_NVAL)
    {
        comm_layer_inner_warning("Server socket error.");
        return FALSE;
    }

    if ((socket = accept(serverSocket->m_nSocket,  (struct sockaddr *)&remote, &len)) == -1)
    {
        comm_layer_inner_warning("Server can not accept client.");
        return TRUE;
    }
    comm_layer_inner_debug("New socket client connected.");
    GIOChannel* gSocket = g_io_channel_unix_new(socket);
    if (!g_io_add_watch(gSocket,
                        static_cast<GIOCondition>(G_IO_IN |G_IO_PRI| G_IO_ERR | G_IO_HUP | G_IO_NVAL),
                        static_cast<GIOFunc>(client_socket_cb),
                        data))
    {
        comm_layer_inner_warning("Can not init g_io_channel.");
        return TRUE;
    }
    serverSocket->m_mapClientChannels[socket] = gSocket;
    return TRUE;
}

void CCommLayerServerSocket::ProcessMessage(const std::string& pMessage, GIOChannel *pSource)
{
    std::string UID = GetSenderUID(g_io_channel_unix_get_fd(pSource));

    if (!strncmp(pMessage.c_str(), MESSAGE_GET_CRASH_INFOS, sizeof(MESSAGE_GET_CRASH_INFOS) - 1))
    {
        vector_crash_infos_t crashInfos = GetCrashInfos(UID);
        std::string message = MESSAGE_GET_CRASH_INFOS + crash_infos_to_string(crashInfos);
        Send(message, pSource);
    }
    else if (!strncmp(pMessage.c_str(), MESSAGE_REPORT, sizeof(MESSAGE_REPORT) - 1))
    {
        std::string message = pMessage.substr(sizeof(MESSAGE_REPORT) - 1);
        map_crash_report_t report = string_to_crash_report(message);
        Report(report, UID);
    }
    else if (!strncmp(pMessage.c_str(), MESSAGE_CREATE_REPORT, sizeof(MESSAGE_CREATE_REPORT) - 1))
    {
        std::string UUID = pMessage.substr(sizeof(MESSAGE_CREATE_REPORT) - 1);
        map_crash_report_t crashReport = CreateReport(UUID, UID);
        std::string message = MESSAGE_CREATE_REPORT + crash_report_to_string(crashReport);
        Send(message, pSource);
    }
    else if (!strncmp(pMessage.c_str(), MESSAGE_DELETE_DEBUG_DUMP, sizeof(MESSAGE_DELETE_DEBUG_DUMP) - 1))
    {
        std::string UUID = pMessage.substr(sizeof(MESSAGE_DELETE_DEBUG_DUMP) - 1);
        DeleteDebugDump(UUID, UID);
    }
    else
    {
        comm_layer_inner_warning("Received unknown message type.");
    }
}

CCommLayerServerSocket::CCommLayerServerSocket()
: CCommLayerServer()
{
    int len;
    struct sockaddr_un local;

    unlink(SOCKET_PATH);
    if ((m_nSocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        throw CABRTException(EXCEP_FATAL, "CCommLayerServerSocket::CCommLayerServerSocket(): Can not create socket.");
    }
    fcntl(m_nSocket, F_SETFD, FD_CLOEXEC);
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCKET_PATH);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(m_nSocket, (struct sockaddr *)&local, len) == -1)
    {
        throw CABRTException(EXCEP_FATAL, "CCommLayerServerSocket::CCommLayerServerSocket(): Can not bind to the socket.");
    }
    if (listen(m_nSocket, 5) == -1)
    {
        throw CABRTException(EXCEP_FATAL, "CCommLayerServerSocket::CCommLayerServerSocket(): Can not listen on the socket.");
    }
    chmod(SOCKET_PATH, SOCKET_PERMISSION);

    m_pGSocket = g_io_channel_unix_new(m_nSocket);
    if (!g_io_add_watch(m_pGSocket,
                        static_cast<GIOCondition>(G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL),
                        static_cast<GIOFunc>(server_socket_cb),
                        this))
    {
        throw CABRTException(EXCEP_FATAL, "CCommLayerServerSocket::CCommLayerServerSocket(): Can not init g_io_channel.");
    }
}

CCommLayerServerSocket::~CCommLayerServerSocket()
{
    g_io_channel_unref(m_pGSocket);
    close(m_nSocket);
}

vector_crash_infos_t CCommLayerServerSocket::GetCrashInfos(const std::string &pSender)
{
    vector_crash_infos_t crashInfos;
    crashInfos = m_pObserver->GetCrashInfos(pSender);
    return crashInfos;
}

map_crash_report_t CCommLayerServerSocket::CreateReport(const std::string &pUUID,const std::string &pSender)
{
    map_crash_report_t crashReport;
    crashReport = m_pObserver->CreateReport(pUUID, pSender);
    return crashReport;
}

bool CCommLayerServerSocket::Report(map_crash_report_t pReport, const std::string& pSender)
{
    m_pObserver->Report(pReport, pSender);
    return true;
}

bool CCommLayerServerSocket::DeleteDebugDump(const std::string& pUUID, const std::string& pSender)
{
    m_pObserver->DeleteDebugDump(pUUID, pSender);
    return true;
}

void CCommLayerServerSocket::Crash(const std::string& arg1)
{
    //Send("(CRASH)New Crash Detected: " + arg1);
}

void CCommLayerServerSocket::AnalyzeComplete(map_crash_report_t arg1)
{
    //Send("(ANALYZE_COMPLETE)Analyze Complete.");
}

void CCommLayerServerSocket::Error(const std::string& arg1)
{
    //Send("(ERROR)Error: " + arg1);
}
