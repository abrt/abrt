/*
    Logger.cpp - it simply writes report to a specified file

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
#include "comm_layer_inner.h"
#include "abrt_exception.h"
#include "Logger.h"

using namespace std;

CLogger::CLogger()
{
    m_pSettings["LogPath"] = "/var/log/abrt.log";
    m_pSettings["AppendLogs"] = "yes";
}

CLogger::~CLogger()
{
}

void CLogger::SetSettings(const map_plugin_settings_t& pSettings)
{
    /* Can't simply do this:

    m_pSettings = pSettings;

     * - it will erase keys which aren't present in pSettings.
     * Example: if Bugzilla.conf doesn't have "Login = foo",
     * then there's no pSettings["Login"] and m_pSettings = pSettings
     * will nuke default m_pSettings["Login"] = "",
     * making GUI think that we have no "Login" key at all
     * and thus never overriding it - even if it *has* an override!
     */

    map_plugin_settings_t::iterator it = m_pSettings.begin();
    while (it != m_pSettings.end())
    {
        map_plugin_settings_t::const_iterator override = pSettings.find(it->first);
        if (override != pSettings.end())
        {
            VERB3 log(" logger settings[%s]='%s'", it->first.c_str(), it->second.c_str());
            it->second = override->second;
        }
        it++;
    }
}

string CLogger::Report(const map_crash_data_t& crash_data,
                const map_plugin_settings_t& pSettings,
                const char *pArgs)
{
    const char *log_path = "/var/log/abrt.log";
    bool append_logs = true;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("LogPath");
    if (it != end)
        log_path = it->second.c_str();
    it = pSettings.find("AppendLogs");
    if (it != end)
        append_logs = string_to_bool(it->second.c_str());

    /* open, not fopen - want to set mode if we create the file, not just open */
    int fd = open(log_path, append_logs ? O_WRONLY|O_CREAT|O_APPEND : O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0)
        throw CABRTException(EXCEP_PLUGIN, "Can't open '%s'", log_path);

    update_client(_("Writing report to '%s'"), log_path);

    /* abrt-action-print -d DIR NULL */
    char *argv[4];
    char **pp = argv;
    *pp++ = (char*)"abrt-action-print";
    *pp++ = (char*)"-d";
    *pp++ = (char*)get_crash_data_item_content_or_NULL(crash_data, CD_DUMPDIR);
    *pp = NULL;
    int pipefds[2];
    pid_t pid = fork_execv_on_steroids(EXECFLG_OUTPUT, // + EXECFLG_ERR2OUT,
                argv,
                pipefds,
                /* unsetenv_vec: */ NULL,
                /* dir: */ NULL,
                /* uid(unused): */ 0
    );

    /* Consume log from stdout, write it to log file */
    FILE *fp = fdopen(pipefds[0], "r");
    if (!fp)
        die_out_of_memory();
    char *buf;
    while ((buf = xmalloc_fgets(fp)) != NULL)
    {
        full_write_str(fd, buf);
    }
    fclose(fp); /* this also closes pipefds[0] */
    /* wait for child to actually exit, and prevent leaving a zombie behind */
    waitpid(pid, NULL, 0);

    /* Add separating empty lines, then close log file */
    full_write_str(fd, "\n\n\n");
    close(fd);

    const char *format = append_logs ? _("The report was appended to %s") : _("The report was stored to %s");
    return ssprintf(format, log_path);
}

PLUGIN_INFO(REPORTER,
            CLogger,
            "Logger",
            "0.0.1",
            _("Writes report to a file"),
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Logger.glade");
