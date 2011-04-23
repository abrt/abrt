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

    Authors:
       Anton Arapov <anton@redhat.com>
       Arjan van de Ven <arjan@linux.intel.com>
 */

#include <curl/curl.h>
#include "abrtlib.h"
#include "parse_options.h"

#define PROGNAME "abrt-action-kerneloops"

/* helpers */
static size_t writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size *= nmemb;
/*
    char *c, *c1, *c2;

    log("received: '%*.*s'", (int)size, (int)size, (char*)ptr);
    c = (char*)xzalloc(size + 1);
    memcpy(c, ptr, size);
    c1 = strstr(c, "201 ");
    if (c1)
    {
        c1 += 4;
        c2 = strchr(c1, '\n');
        if (c2)
            *c2 = 0;
    }
    free(c);
*/

    return size;
}

/* Send oops data to kerneloops.org-style site, using HTTP POST */
/* Returns 0 on success */
static CURLcode http_post_to_kerneloops_site(const char *url, const char *oopsdata)
{
    CURLcode ret;
    CURL *handle;
    struct curl_httppost *post = NULL;
    struct curl_httppost *last = NULL;

    handle = curl_easy_init();
    if (!handle)
        error_msg_and_die("Can't create curl handle");

    curl_easy_setopt(handle, CURLOPT_URL, url);

    curl_formadd(&post, &last,
            CURLFORM_COPYNAME, "oopsdata",
            CURLFORM_COPYCONTENTS, oopsdata,
            CURLFORM_END);
    curl_formadd(&post, &last,
            CURLFORM_COPYNAME, "pass_on_allowed",
            CURLFORM_COPYCONTENTS, "yes",
            CURLFORM_END);

    curl_easy_setopt(handle, CURLOPT_HTTPPOST, post);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writefunction);

    ret = curl_easy_perform(handle);

    curl_formfree(post);
    curl_easy_cleanup(handle);

    return ret;
}

static void report_to_kerneloops(
                const char *dump_dir_name,
                map_string_h *settings)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        exit(1); /* error msg is already logged */

    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
    dd_close(dd);

    const char *backtrace = get_problem_item_content_or_NULL(problem_data, FILENAME_BACKTRACE);
    if (!backtrace)
        error_msg_and_die("Error sending kernel oops due to missing backtrace");

    const char *env = getenv("KerneloopsReporter_SubmitURL");
    const char *submitURL = (env ? env : get_map_string_item_or_empty(settings, "SubmitURL"));
    if (!submitURL[0])
        submitURL = "http://submit.kerneloops.org/submitoops.php";

    log(_("Submitting oops report to %s"), submitURL);

    CURLcode ret = http_post_to_kerneloops_site(submitURL, backtrace);
    if (ret != CURLE_OK)
        error_msg_and_die("Kernel oops has not been sent due to %s", curl_easy_strerror(ret));

    free_problem_data(problem_data);

    /* Server replies with:
     * 200 thank you for submitting the kernel oops information
     * RemoteIP: 34192fd15e34bf60fac6a5f01bba04ddbd3f0558
     * - no URL or bug ID apparently...
     */
    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (dd)
    {
        char *msg = xasprintf("kerneloops: URL=%s", submitURL);
        add_reported_to(dd, msg);
        free(msg);
        dd_close(dd);
    }

    log("Kernel oops report was uploaded");
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
        PROGNAME" [-v] [-c CONFFILE]... -d DIR\n"
        "\n"
        "Reports kernel oops to kerneloops.org (or similar) site"
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
        OPT_LIST(  'c', NULL, &conf_file    , "FILE", _("Configuration file")),
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

    report_to_kerneloops(dump_dir_name, settings);

    free_map_string(settings);
    return 0;
}
