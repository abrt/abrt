/*
    Mailx.h - header file for Mailx reporter plugin
            - it simple sends an email to specific address via mailx command

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
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

#ifndef MAILX_H_
#define MAILX_H_

#include <string>
#include "Plugin.h"
#include "Reporter.h"

class CMailx : public CReporter
{
    private:
        std::string m_sEmailFrom;
        std::string m_sEmailTo;
        std::string m_sParameters;
        std::string m_sAttachments;
        std::string m_sSubject;
        bool m_bSendBinaryData;

        void SendEmail(const std::string& pSubject, const std::string& pText);

    public:
        CMailx();
        virtual ~CMailx() {}

        virtual void LoadSettings(const std::string& pPath);
        virtual void Report(const map_crash_report_t& pCrashReport, const std::string& pArgs);
};


PLUGIN_INFO(REPORTER,
            CMailx,
            "Mailx",
            "0.0.2",
            "Sends an email with a report via mailx command",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/crash-catcher/wiki");

#endif /* MAILX_H_ */
