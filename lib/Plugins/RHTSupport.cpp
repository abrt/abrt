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

#define _GNU_SOURCE 1    /* for stpcpy */
#include <libtar.h>
#include "abrtlib.h"
#include "abrt_curl.h"
#include "abrt_rh_support.h"
#include "crash_types.h"
#include "debug_dump.h"
#include "abrt_exception.h"
#include "comm_layer_inner.h"
#include "RHTSupport.h"
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

using namespace std;


#if 0 //unused
static char *xml_escape(const char *str)
{
    const char *s = str;
    unsigned count = 1; /* for NUL */
    while (*s)
    {
        if (*s == '&')
            count += sizeof("&amp;")-2;
        if (*s == '<')
            count += sizeof("&lt;")-2;
        if (*s == '>')
            count += sizeof("&gt;")-2;
        if ((unsigned char)*s > 126 || (unsigned char)*s < ' ')
            count += sizeof("\\x00")-2;
        count++;
        s++;
    }
    char *result = (char*)xmalloc(count);
    char *d = result;
    s = str;
    while (*s)
    {
        if (*s == '&')
            d = stpcpy(d, "&amp;");
        else if (*s == '<')
            d = stpcpy(d, "&lt;");
        else if (*s == '>')
            d = stpcpy(d, "&gt;");
        else
        if ((unsigned char)*s > 126
         || (  (unsigned char)*s < ' '
            && *s != '\t'
            && *s != '\n'
            && *s != '\r'
            )
        ) {
            *d++ = '\\';
            *d++ = 'x';
            *d++ = "0123456789abcdef"[(unsigned char)*s >> 4];
            *d++ = "0123456789abcdef"[(unsigned char)*s & 0xf];
        }
        else
            *d++ = *s;
        s++;
    }
    *d = '\0';
    return result;
}
#endif


/*
 * CReporterRHticket
 */

CReporterRHticket::CReporterRHticket() :
    m_bSSLVerify(true),
    m_sStrataURL("https://api.access.redhat.com/rs")
{}

CReporterRHticket::~CReporterRHticket()
{}

string CReporterRHticket::Report(const map_crash_data_t& pCrashData,
        const map_plugin_settings_t& pSettings,
        const char *pArgs)
{
    string retval;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("URL");
    string URL = (it == end ? m_sStrataURL : it->second);
    it = pSettings.find("Login");
    string login = (it == end ? m_sLogin : it->second);
    it = pSettings.find("Password");
    string password = (it == end ? m_sPassword : it->second);
    it = pSettings.find("SSLVerify");
    bool ssl_verify = (it == end ? m_bSSLVerify : string_to_bool(it->second.c_str()));

    const string& package   = get_crash_data_item_content(pCrashData, FILENAME_PACKAGE);
//  const string& component = get_crash_data_item_content(pCrashData, FILENAME_COMPONENT);
//  const string& release   = get_crash_data_item_content(pCrashData, FILENAME_RELEASE);
//  const string& arch      = get_crash_data_item_content(pCrashData, FILENAME_ARCHITECTURE);
//  const string& duphash   = get_crash_data_item_content(pCrashData, CD_DUPHASH);
    const char *reason      = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_REASON);
    const char *function    = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_CRASH_FUNCTION);

    string summary = "[abrt] " + package;
    if (function && strlen(function) < 30)
    {
        summary += ": ";
        summary += function;
    }
    if (reason)
    {
        summary += ": ";
        summary += reason;
    }

    string description = "abrt version: "VERSION"\n";
    description += make_description_bz(pCrashData);

    reportfile_t* file = new_reportfile();

    /* SELinux guys are not happy with /tmp, using /var/run/abrt */
    char *tempfile = xasprintf(LOCALSTATEDIR"/run/abrt/tmp-%lu-%lu.tar.gz", (long)getpid(), (long)time(NULL));

    int pipe_from_parent_to_child[2];
    xpipe(pipe_from_parent_to_child);
    pid_t child = fork();
    if (child == 0)
    {
        /* child */
        close(pipe_from_parent_to_child[1]);
        xmove_fd(xopen3(tempfile, O_WRONLY | O_CREAT | O_EXCL, 0600), 1);
        xmove_fd(pipe_from_parent_to_child[0], 0);
        execlp("gzip", "gzip", NULL);
        perror_msg_and_die("can't execute '%s'", "gzip");
    }
    close(pipe_from_parent_to_child[0]);

    TAR *tar = NULL;
    if (tar_fdopen(&tar, pipe_from_parent_to_child[1], tempfile,
                /*fileops:(standard)*/ NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU) != 0)
    {
        retval = "can't create temporary file in "LOCALSTATEDIR"/run/abrt";
        goto ret;
    }

    {
        map_crash_data_t::const_iterator it = pCrashData.begin();
        for (; it != pCrashData.end(); it++)
        {
            if (it->first == CD_COUNT) continue;
            if (it->first == CD_DUMPDIR) continue;
            if (it->first == CD_INFORMALL) continue;
            if (it->first == CD_REPORTED) continue;
            if (it->first == CD_MESSAGE) continue; // plugin's status message (if we already reported it yesterday)
            if (it->first == FILENAME_DESCRIPTION) continue; // package description

            const char *content = it->second[CD_CONTENT].c_str();
            if (it->second[CD_TYPE] == CD_TXT)
            {
                reportfile_add_binding_from_string(file, it->first.c_str(), content);
            }
            else if (it->second[CD_TYPE] == CD_BIN)
            {
                const char *basename = strrchr(content, '/');
                if (basename)
                    basename++;
                else
                    basename = content;
                string xml_name = concat_path_file("content", basename);
                reportfile_add_binding_from_namedfile(file,
                        /*on_disk_filename */ content,
                        /*binding_name     */ it->first.c_str(),
                        /*recorded_filename*/ xml_name.c_str(),
                        /*binary           */ 1);
                if (tar_append_file(tar, (char*)content, (char*)(xml_name.c_str())) != 0)
                {
                    retval = "can't create temporary file in "LOCALSTATEDIR"/run/abrt";
                    goto ret;
                }
            }
        }
    }

    /* Write out content.xml in the tarball's root */
    {
        const char *signature = reportfile_as_string(file);
        unsigned len = strlen(signature);
        unsigned len512 = (len + 511) & ~511;
        char *block = (char*)memcpy(xzalloc(len512), signature, len);
        th_set_type(tar, S_IFREG | 0644);
        th_set_mode(tar, S_IFREG | 0644);
      //th_set_link(tar, char *linkname);
      //th_set_device(tar, dev_t device);
      //th_set_user(tar, uid_t uid);
      //th_set_group(tar, gid_t gid);
      //th_set_mtime(tar, time_t fmtime);
        th_set_path(tar, (char*)"content.xml");
        th_set_size(tar, len);
        th_finish(tar); /* caclulate and store th xsum etc */
        if (th_write(tar) != 0
         || full_write(tar_fd(tar), block, len512) != len512
         || tar_close(tar) != 0
        ) {
            retval = "can't create temporary file in "LOCALSTATEDIR"/run/abrt";
            goto ret;
        }
        tar = NULL;
    }

    {
        update_client(_("Creating a new case..."));
        char* result = send_report_to_new_case(URL.c_str(),
                login.c_str(),
                password.c_str(),
                ssl_verify,
                summary.c_str(),
                description.c_str(),
                package.c_str(),
                tempfile
        );
        VERB3 log("post result:'%s'", result);
        retval = result;
        free(result);
    }

 ret:
    // Damn, selinux does not allow SIGKILLing our own child! wtf??
    //kill(child, SIGKILL); /* just in case */
    waitpid(child, NULL, 0);
    if (tar)
        tar_close(tar);
    //close(pipe_from_parent_to_child[1]); - tar_close() does it itself
    unlink(tempfile);
    free(tempfile);
    reportfile_free(file);

    if (strncasecmp(retval.c_str(), "error", 5) == 0)
    {
        throw CABRTException(EXCEP_PLUGIN, "%s", retval.c_str());
    }
    return retval;
}

void CReporterRHticket::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("URL");
    if (it != end)
    {
        m_sStrataURL = it->second;
    }
    it = pSettings.find("Login");
    if (it != end)
    {
        m_sLogin = it->second;
    }
    it = pSettings.find("Password");
    if (it != end)
    {
        m_sPassword = it->second;
    }
    it = pSettings.find("SSLVerify");
    if (it != end)
    {
        m_bSSLVerify = string_to_bool(it->second.c_str());
    }
}

/* Should not be deleted (why?) */
const map_plugin_settings_t& CReporterRHticket::GetSettings()
{
    m_pSettings["URL"] = m_sStrataURL;
    m_pSettings["Login"] = m_sLogin;
    m_pSettings["Password"] = m_sPassword;
    m_pSettings["SSLVerify"] = m_bSSLVerify ? "yes" : "no";

    return m_pSettings;
}

PLUGIN_INFO(REPORTER,
    CReporterRHticket,
    "RHticket",
    "0.0.4",
    _("Reports bugs to Red Hat support"),
    "Denys Vlasenko <dvlasenk@redhat.com>",
    "https://fedorahosted.org/abrt/wiki",
    PLUGINS_LIB_DIR"/RHTSupport.glade");
