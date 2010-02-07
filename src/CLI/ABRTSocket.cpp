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
#include "ABRTSocket.h"
#include "ABRTException.h"
#include "CrashTypesSocket.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

CABRTSocket::CABRTSocket() : m_nSocket(-1)
{}

CABRTSocket::~CABRTSocket()
{
    /* Paranoia. In C++, destructor will abort() if it was called while unwinding
     * the stack and it throws an exception.
     */
    try
    {
        Disconnect();
    }
    catch (...)
    {
        error_msg_and_die("Internal error");
    }
}

void CABRTSocket::Send(const std::string& pMessage)
{
    int ret = 0;
    int len = pMessage.length();
    int offset = 0;
    char* message = new char[len + 3];
    memcpy(message, pMessage.c_str(), len);
    message[len] = MESSAGE_END_MARKER;
    message[len + 1] = '\n';
    message[len + 2] = '\0';

    while (ret != strlen(message + offset))
    {
        offset += ret;
        ret = send(m_nSocket, message + offset, strlen(message + offset), 0);
        if (ret == -1)
        {
            throw CABRTException(EXCEP_FATAL, "CABRTSocket::Send(): Can not send data");
        }
    }
    delete[] message;
}

void CABRTSocket::Recv(std::string& pMessage)
{
    std::string message;
    bool receivingMessage = true;
    char buff[1];
    int ret;

    pMessage = "";
    while (receivingMessage)
    {
        ret = recv(m_nSocket, buff, 1, 0);
        if (ret == -1)
        {
            throw CABRTException(EXCEP_FATAL, "CABRTSocket::Recv(): Can not recv data");
        }
        else if (ret == 0)
        {
            throw CABRTException(EXCEP_FATAL, "CABRTSocket::Recv(): Connection closed by abrt server");
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
    pMessage = message;
}


void CABRTSocket::Connect(const std::string& pPath)
{
    int len;
    struct sockaddr_un remote;
    if ((m_nSocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        throw CABRTException(EXCEP_FATAL, "CABRTSocket::Connect(): Can not create socket");
    }
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, pPath.c_str());
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(m_nSocket, (struct sockaddr *)&remote, len) == -1)
    {
        throw CABRTException(EXCEP_FATAL, "CABRTSocket::Connect(): Can not connect to remote");
    }
}

void CABRTSocket::Disconnect()
{
    if (m_nSocket != -1)
        close(m_nSocket);
}

vector_map_crash_data_t CABRTSocket::GetCrashInfos()
{
    std::string message = MESSAGE_GET_CRASH_INFOS;
    Send(message);
    Recv(message);
    message.erase(0, sizeof(MESSAGE_GET_CRASH_INFOS) - 1);
    return string_to_crash_infos(message);
}

map_crash_data_t CABRTSocket::CreateReport(const std::string &pUUID)
{
    std::string message = MESSAGE_CREATE_REPORT + pUUID;
    Send(message);
    Recv(message);
    message.erase(0, sizeof(MESSAGE_CREATE_REPORT) - 1);
    return string_to_crash_report(message);
}

void CABRTSocket::Report(const map_crash_data_t& pReport)
{
    std::string message = MESSAGE_REPORT + crash_report_to_string(pReport);
    Send(message);
}

void CABRTSocket::DeleteDebugDump(const std::string& pUUID)
{
    std::string message = MESSAGE_DELETE_DEBUG_DUMP + pUUID;
    Send(message);
}
