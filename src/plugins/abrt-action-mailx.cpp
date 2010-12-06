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
#include "parse_options.h"
#include "abrt_crash_dump.h"
#include "abrt_exception.h"

#define PROGNAME "abrt-action-mailx"

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

    int status;
    waitpid(child, &status, 0); /* wait for command completion */
    if (status != 0)
        error_msg_and_die("Error running '%s'", args[0]);
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

static void create_and_send_email(
                const char *dump_dir_name,
                const map_plugin_settings_t& settings)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        exit(1); /* error msg is already logged by dd_opendir */

    crash_data_t *crash_data = load_crash_data_from_crash_dump_dir(dd);
    dd_close(dd);

    char* env;
    map_plugin_settings_t::const_iterator end = settings.end();
    map_plugin_settings_t::const_iterator it;
    env = getenv("Mailx_Subject");
    it = settings.find("Subject");
    const char *subject = xstrdup(env ? env : (it != end ? it->second.c_str() : "[abrt] full crash report"));
    env = getenv("Mailx_EmailFrom");
    it = settings.find("EmailFrom");
    const char *email_from = (env ? env : (it != end ? it->second.c_str() : "user@localhost"));
    env = getenv("Mailx_EmailTo");
    it = settings.find("EmailTo");
    const char *email_to = (env ? env : (it != end ? it->second.c_str() : "root@localhost"));
    env = getenv("Mailx_SendBinaryData");
    it = settings.find("SendBinaryData");
    bool send_binary_data = string_to_bool(env ? env : (it != end ? it->second.c_str() : "0"));

    char **args = NULL;
    unsigned arg_size = 0;
    args = append_str_to_vector(args, arg_size, "/bin/mailx");

    char *dsc = make_description_mailx(crash_data);

    if (send_binary_data)
    {
        GHashTableIter iter;
        char *name;
        struct crash_item *value;
        g_hash_table_iter_init(&iter, crash_data);
        while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
        {
            if (value->flags & CD_FLAG_BIN)
            {
                args = append_str_to_vector(args, arg_size, "-a");
                args = append_str_to_vector(args, arg_size, value->content);
            }
        }
    }

    args = append_str_to_vector(args, arg_size, "-s");
    args = append_str_to_vector(args, arg_size, subject);
    args = append_str_to_vector(args, arg_size, "-r");
    args = append_str_to_vector(args, arg_size, email_from);
    args = append_str_to_vector(args, arg_size, email_to);

    log(_("Sending an email..."));
    const char *uid_str = get_crash_item_content_or_NULL(crash_data, FILENAME_UID);
    exec_and_feed_input(xatoi_u(uid_str), dsc, args);

    free(dsc);

    while (*args)
        free(*args++);
    args -= arg_size;
    free(args);

    free_crash_data(crash_data);

    log("Email was sent to: %s", email_to);
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    const char *dump_dir_name = ".";
    const char *conf_file = NULL;

    const char *program_usage = _(
        PROGNAME" [-v] -d DIR [-c CONFFILE]\n"
        "\n"
        "Upload compressed tarball of crash dump"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_c = 1 << 2,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR"     , _("Crash dump directory")),
        OPT_STRING('c', NULL, &conf_file    , "CONFFILE", _("Config file")),
        OPT_END()
    };

    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));
    //msg_prefix = PROGNAME;
    //if (optflags & OPT_s)
    //{
    //    openlog(msg_prefix, 0, LOG_DAEMON);
    //    logmode = LOGMODE_SYSLOG;
    //}

    map_plugin_settings_t settings;
    if (conf_file)
        LoadPluginSettings(conf_file, settings);

    try
    {
        create_and_send_email(dump_dir_name, settings);
    }
    catch (CABRTException& e)
    {
        error_msg_and_die("%s", e.what());
    }

    return 0;
}
