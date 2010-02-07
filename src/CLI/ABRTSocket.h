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
