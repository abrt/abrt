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
#include "abrt_crash_data.h"

#define PROGNAME "abrt-action-kerneloops"

/* helpers */
static size_t writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size *= nmemb;
/*
    char *c, *c1, *c2;

    log("received: '%*.*s'\n", (int)size, (int)size, (char*)ptr);
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

    crash_data_t *crash_data = create_crash_data_from_dump_dir(dd);
    dd_close(dd);

    const char *backtrace = get_crash_item_content_or_NULL(crash_data, FILENAME_BACKTRACE);
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

    free_crash_data(crash_data);

    /* Server replies with:
     * 200 thank you for submitting the kernel oops information
     * RemoteIP: 34192fd15e34bf60fac6a5f01bba04ddbd3f0558
     * - no URL or bug ID apparently...
     */
    log("Kernel oops report was uploaded");
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    map_string_h *settings = new_map_string();
    const char *dump_dir_name = ".";
    enum {
        OPT_s = (1 << 0),
    };
    int optflags = 0;
    int opt;
    while ((opt = getopt(argc, argv, "c:d:vs")) != -1)
    {
        switch (opt)
        {
        case 'c':
            VERB1 log("Loading settings from '%s'", optarg);
            load_conf_file(optarg, settings, /*skip key w/o values:*/ true);
            VERB3 log("Loaded '%s'", optarg);
            break;
        case 'd':
            dump_dir_name = optarg;
            break;
        case 'v':
            g_verbose++;
            break;
        case 's':
            optflags |= OPT_s;
            break;
        default:
            /* Careful: the string below contains tabs, dont replace with spaces */
            error_msg_and_die(
                "Usage: "PROGNAME" -c CONFFILE -d DIR [-vs]"
                "\n"
                "\nReport a kernel oops to kerneloops.org (or similar) site"
                "\n"
                "\nOptions:"
                "\n	-c FILE	Configuration file (may be given many times)"
                "\n	-d DIR	Crash dump directory"
                "\n	-v	Be verbose"
                "\n	-s	Log to syslog"
            );
        }
    }

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

//DONT! our stdout/stderr goes directly to daemon, don't want to have prefix there.
//    msg_prefix = xasprintf(PROGNAME"[%u]", getpid());

    if (optflags & OPT_s)
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    report_to_kerneloops(dump_dir_name, settings);

    free_map_string(settings);
    return 0;
}
