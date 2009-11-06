/*
    TicketUploader.h

    Attach a configureable Ticket Number and Customer name to a report.
    Create a compressed, optionally encrypted, tarball.
    Upload tarball to configureable URL.

    Copyright (C) 2009  RedHat inc.

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

#ifndef TICKETUPLOADER_H_
#define TICKETUPLOADER_H_

#include "Plugin.h"
#include "Reporter.h"
#include "CrashTypes.h"

class CTicketUploader : public CReporter
{
    private:
        std::string m_sCustomer;
        std::string m_sTicket;
        std::string m_sURL;
        bool m_bEncrypt;
        bool m_bUpload;
        int m_nRetryCount;
        int m_nRetryDelay;

        void SendFile(const char *pURL, const char *pFilename);

    public:
        CTicketUploader();
        virtual ~CTicketUploader();
        virtual map_plugin_settings_t GetSettings();
        virtual void SetSettings(const map_plugin_settings_t& pSettings);

        virtual std::string Report(const map_crash_report_t& pCrashReport,
                              const map_plugin_settings_t& pSettings,
                              const std::string& pArgs);
};

#endif
