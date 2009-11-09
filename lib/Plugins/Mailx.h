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
        std::string m_sSubject;
        bool m_bSendBinaryData;

        int m_nArgs;
        char** m_pArgs;

        void FreeMailxArgs();
        void AddMailxArg(const std::string& pArg);
        void ExecMailx(uid_t uid, const std::string& pText);
        void SendEmail(const std::string& pSubject, const std::string& pText, const std::string& pUID);

    public:
        CMailx();

        virtual void SetSettings(const map_plugin_settings_t& pSettings);
        virtual const map_plugin_settings_t& GetSettings();
        virtual std::string Report(const map_crash_report_t& pCrashReport,
                                   const map_plugin_settings_t& pSettings,
                                   const std::string& pArgs);
};

#endif /* MAILX_H_ */
