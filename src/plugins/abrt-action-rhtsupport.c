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

#include <libtar.h>
#include "abrtlib.h"
#include "abrt_curl.h"
#include "abrt_xmlrpc.h"
#include "abrt_rh_support.h"
#include "parse_options.h"

#define PROGNAME "abrt-action-rhtsupport"

static void report_to_rhtsupport(
                const char *dump_dir_name,
                map_string_h *settings)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        exit(1); /* error msg is already logged by dd_opendir */

    crash_data_t *crash_data = create_crash_data_from_dump_dir(dd);
    dd_close(dd);

    /* Gzipping e.g. 0.5gig coredump takes a while. Let client know what we are doing */
    log(_("Compressing data"));

    const char* errmsg = NULL;
    TAR* tar = NULL;
    pid_t child;
    char* tempfile = NULL;
    reportfile_t* file = NULL;
    char* dsc = NULL;
    char* summary = NULL;
    const char* function;
    const char* reason;
    const char* package;

    char* env;
    env = getenv("RHTSupport_URL");
    char *url = xstrdup(env ? env : (get_map_string_item_or_NULL(settings, "URL") ?  : "https://api.access.redhat.com/rs"));

    env = getenv("RHTSupport_Login");
    char *login = xstrdup(env ? env : get_map_string_item_or_empty(settings, "Login"));

    env = getenv("RHTSupport_Password");
    char *password = xstrdup(env ? env : get_map_string_item_or_empty(settings, "Password"));

    env = getenv("RHTSupport_SSLVerify");
    bool ssl_verify = string_to_bool(env ? env : get_map_string_item_or_empty(settings, "SSLVerify"));

    if (!login[0] || !password[0])
    {
        free_crash_data(crash_data);
        free(url);
        free(login);
        free(password);
        error_msg_and_die(_("Empty RHTS login or password"));
        return;
    }

    package  = get_crash_item_content_or_NULL(crash_data, FILENAME_PACKAGE);
    reason   = get_crash_item_content_or_NULL(crash_data, FILENAME_REASON);
    function = get_crash_item_content_or_NULL(crash_data, FILENAME_CRASH_FUNCTION);

    {
        struct strbuf *buf_summary = strbuf_new();
        strbuf_append_strf(buf_summary, "[abrt] %s", package);
        if (function && strlen(function) < 30)
            strbuf_append_strf(buf_summary, ": %s", function);
        if (reason)
            strbuf_append_strf(buf_summary, ": %s", reason);
        summary = strbuf_free_nobuf(buf_summary);

        char *bz_dsc = make_description_bz(crash_data);
        dsc = xasprintf("abrt version: "VERSION"\n%s", bz_dsc);
        free(bz_dsc);
    }
    file = new_reportfile();
    const char *dt_string = iso_date_string(NULL);
    char tmpdir_name[sizeof("/tmp/rhtsupport-YYYY-MM-DD-hh:mm:ss-XXXXXX")];
    sprintf(tmpdir_name, "/tmp/rhtsupport-%s-XXXXXX", dt_string);
    /* mkdtemp does mkdir(xxx, 0700), should be safe (is it?) */
    if (mkdtemp(tmpdir_name) == NULL)
    {
        error_msg_and_die(_("Can't create a temporary directory in /tmp"));
    }
    tempfile = xasprintf("%s/tmp-%s-%lu.tar.gz",tmpdir_name, iso_date_string(NULL), (long)getpid());

    int pipe_from_parent_to_child[2];
    xpipe(pipe_from_parent_to_child);
    child = fork();
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

    if (tar_fdopen(&tar, pipe_from_parent_to_child[1], tempfile,
                /*fileops:(standard)*/ NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU) != 0)
    {
        errmsg = "can't create temporary file in "LOCALSTATEDIR"/run/abrt";
        goto ret;
    }

    {
        GHashTableIter iter;
        char *name;
        struct crash_item *value;
        g_hash_table_iter_init(&iter, crash_data);
        while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
        {
            if (strcmp(name, FILENAME_COUNT) == 0) continue;
            if (strcmp(name, CD_DUMPDIR) == 0) continue;

            const char *content = value->content;
            if (value->flags & CD_FLAG_TXT)
            {
                reportfile_add_binding_from_string(file, name, content);
            }
            else if (value->flags & CD_FLAG_BIN)
            {
                const char *basename = strrchr(content, '/');
                if (basename)
                    basename++;
                else
                    basename = content;
                char *xml_name = concat_path_file("content", basename);
                reportfile_add_binding_from_namedfile(file,
                        /*on_disk_filename */ content,
                        /*binding_name     */ name,
                        /*recorded_filename*/ xml_name,
                        /*binary           */ 1);
                if (tar_append_file(tar, (char*)content, xml_name) != 0)
                {
                    errmsg = "can't create temporary file in "LOCALSTATEDIR"/run/abrt";
                    free(xml_name);
                    goto ret;
                }
                free(xml_name);
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
            free(block);
            errmsg = "can't create temporary file in "LOCALSTATEDIR"/run/abrt";
            goto ret;
        }
        tar = NULL;
        free(block);
    }

    {
        log(_("Creating a new case..."));
        char* result = send_report_to_new_case(url,
                login,
                password,
                ssl_verify,
                summary,
                dsc,
                package,
                tempfile
        );
        /* Temporary hackish detection of errors. Ideally,
         * send_report_to_new_case needs to have better error reporting.
         */
        if (strncasecmp(result, "error", 5) == 0)
        {
            /*
             * result can contain "...server says: 'multi-line <html> text'"
             * Replace all '\n' with spaces:
             * we want this message to be, logically, one log entry.
             * IOW: one line, not many lines.
             */
            char *src, *dst;
            dst = src = result;
            while (1)
            {
                unsigned char c = *src++;
                if (c == '\n')
                    c = ' ';
                *dst++ = c;
                if (c == '\0')
                    break;
            }
            /* Use sanitized string as error message */
            error_msg_and_die("%s", result);
        }
        /* No error */
        log("%s", result);
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
    rmdir(tmpdir_name);

    free(summary);
    free(dsc);

    free(url);
    free(login);
    free(password);
    free_crash_data(crash_data);

    if (errmsg)
        error_msg_and_die("%s", errmsg);
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    map_string_h *settings = new_map_string();
    const char *dump_dir_name = ".";
    GList *conf_file = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] -c CONFFILE -d DIR\n"
        "\n"
        "Reports a problem to RHTSupport"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_c = 1 << 2,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR" , _("Dump directory")),
        OPT_LIST(  'c', NULL, &conf_file    , "FILE", _("Configuration file (may be given many times)")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    char *pfx = getenv("ABRT_PROG_PREFIX");
    if (pfx && string_to_bool(pfx))
        msg_prefix = PROGNAME;

    while (conf_file)
    {
        char *fn = (char *)conf_file->data;
        VERB1 log("Loading settings from '%s'", fn);
        load_conf_file(fn, settings, /*skip key w/o values:*/ true);
        VERB3 log("Loaded '%s'", fn);
        conf_file = g_list_remove(conf_file, fn);
    }

    VERB1 log("Initializing XML-RPC library");
    xmlrpc_env env;
    xmlrpc_env_init(&env);
    xmlrpc_client_setup_global_const(&env);
    if (env.fault_occurred)
        error_msg_and_die("XML-RPC Fault: %s(%d)", env.fault_string, env.fault_code);
    xmlrpc_env_clean(&env);

    report_to_rhtsupport(dump_dir_name, settings);

    free_map_string(settings);
    return 0;
}
