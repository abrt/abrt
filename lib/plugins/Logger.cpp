/*
    Logger.cpp - it simple writes report to specific file

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
#include "abrtlib.h"
#include "Logger.h"
#include "comm_layer_inner.h"
#include "abrt_exception.h"

CLogger::CLogger()
{
    m_log_path = xstrdup("/var/log/abrt.log");
    m_append_logs = true;
}

CLogger::~CLogger()
{
    free(m_log_path);
}

void CLogger::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("LogPath");
    if (it != end)
    {
        free(m_log_path);
        m_log_path = xstrdup(it->second.c_str());
    }
    it = pSettings.find("AppendLogs");
    if (it != end)
        m_append_logs = string_to_bool(it->second.c_str());
}

std::string CLogger::Report(const map_crash_data_t& pCrashData,
                const map_plugin_settings_t& pSettings,
                const char *pArgs)
{
    char *dsc = make_description_logger(pCrashData);
    char *full_dsc = xasprintf("%s\n\n\n", dsc);
    free(dsc);

    /* open, not fopen - want to set mode if we create the file, not just open */
    const char *fname = m_log_path;
    int fd = open(fname, m_append_logs ? O_WRONLY|O_CREAT|O_APPEND : O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0)
        throw CABRTException(EXCEP_PLUGIN, "Can't open '%s'", fname);

    update_client(_("Writing report to '%s'"), fname);
    full_write_str(fd, full_dsc);
    free(full_dsc);

    close(fd);

    const char *format = m_append_logs ? _("The report was appended to %s") : _("The report was stored to %s");
    return ssprintf(format, m_log_path);
}

PLUGIN_INFO(REPORTER,
            CLogger,
            "Logger",
            "0.0.1",
            _("Writes report to a file"),
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Logger.glade");
