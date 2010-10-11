/*
    Mailx.cpp

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
#include "Mailx.h"
#include "abrt_exception.h"
#include "comm_layer_inner.h"

#define MAILX_COMMAND "/bin/mailx"

CMailx::CMailx()
{
    m_email_from = xstrdup("user@localhost");
    m_email_to = xstrdup("root@localhost");
    m_subject = xstrdup("[abrt] full crash report");
    m_send_binary_data = false;
}

CMailx::~CMailx()
{
    free(m_email_from);
    free(m_email_to);
    free(m_subject);
}

static void exec_and_feed_input(uid_t uid, const char* text, char **args)
{
    int pipein[2];

    pid_t child = fork_execv_on_steroids(
                EXECFLG_INPUT | EXECFLG_QUIET | EXECFLG_SETGUID,
                args,
                pipein,
                /*unsetenv_vec:*/ NULL,
                /*dir:*/ NULL,
                uid);

    full_write_str(pipein[1], text);
    close(pipein[1]);

    waitpid(child, NULL, 0); /* wait for command completion */
}

static char** append_str_to_vector(char **vec, unsigned &size, const char *str)
{
    //log("old vec: %p", vec);
    vec = (char**) xrealloc(vec, (size+2) * sizeof(vec[0]));
    vec[size] = xstrdup(str);
    //log("new vec: %p, added [%d] %p", vec, size, vec[size]);
    size++;
    vec[size] = NULL;
    return vec;
}

std::string CMailx::Report(const map_crash_data_t& pCrashData,
                const map_plugin_settings_t& pSettings,
                const char *pArgs)
{
    SetSettings(pSettings);
    char **args = NULL;
    unsigned arg_size = 0;
    args = append_str_to_vector(args, arg_size, MAILX_COMMAND);

    char *dsc = make_dsc_mailx(pCrashData);

    map_crash_data_t::const_iterator it;
    for (it = pCrashData.begin(); it != pCrashData.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_BIN && m_send_binary_data)
        {
            args = append_str_to_vector(args, arg_size, "-a");
            args = append_str_to_vector(args, arg_size, it->second[CD_CONTENT].c_str());
        }
    }

    args = append_str_to_vector(args, arg_size, "-s");
    args = append_str_to_vector(args, arg_size, (pArgs[0] != '\0' ? pArgs : m_subject));
    args = append_str_to_vector(args, arg_size, "-r");
    args = append_str_to_vector(args, arg_size, m_email_from);
    args = append_str_to_vector(args, arg_size, m_email_to);

    update_client(_("Sending an email..."));
    const char *uid_str = get_crash_data_item_content_or_NULL(pCrashData, CD_UID);
    exec_and_feed_input(xatoi_u(uid_str), dsc, args);

    free(dsc);

    while (*args)
    {
        free(*args++);
    }
    args -= arg_size;
    free(args);

    return ssprintf("Email was sent to: %s", m_email_to);
}

void CMailx::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("Subject");
    if (it != end)
    {
        free(m_subject);
        m_subject = xstrdup(it->second.c_str());
    }
    it = pSettings.find("EmailFrom");
    if (it != end)
    {
        free(m_email_from);
        m_email_from = xstrdup(it->second.c_str());
    }
    it = pSettings.find("EmailTo");
    if (it != end)
    {
        free(m_email_to);
        m_email_to = xstrdup(it->second.c_str());
    }
    it = pSettings.find("SendBinaryData");
    if (it != end)
    {
        m_send_binary_data = string_to_bool(it->second.c_str());
    }
}

PLUGIN_INFO(REPORTER,
            CMailx,
            "Mailx",
            "0.0.2",
            _("Sends an email with a report (via mailx command)"),
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Mailx.glade");
