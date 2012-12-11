/*
    Copyright (C) 2012  ABRT Team
    Copyright (C) 2012  Red Hat, Inc.

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
#include <syslog.h>
#include "https-utils.h"

#define SHOW_PREFIX "SHOW "
#define SAVE_PREFIX "SAVE "

static const char *dump_dir_name = ".";
static char *component = NULL;
static char *backtrace = NULL;
static bool http_show_headers = false;
static struct https_cfg cfg =
{
    .url = "retrace.fedoraproject.org",
    .port = 443,
    .ssl_allow_insecure = false,
};

/* allowed characters are [a-zA-Z0-9_\.\-\+] */
/* http://fedoraproject.org/wiki/Packaging:NamingGuidelines */
static inline int is_valid_component_name(const char *name)
{
    int i;
    unsigned char c;
    for (i = 0; name[i]; ++i)
    {
        c = (unsigned char)name[i];
        if (!isalnum(c) &&
            c != '-' && c != '.' &&
            c != '+' && c != '_')
            return 0;
    }

    return 1;
}

/* idea from curl */
/* escape everything except [a-zA-Z0-9\.\-_~] */
static void escape(const char *src, struct strbuf *dest)
{
    int i;
    unsigned char c;
    for (i = 0; src[i]; ++i)
    {
        c = (unsigned char)src[i];
        if (isalnum(c) ||
            c == '-' || c == '.' ||
            c == '~' || c == '_')
            strbuf_append_char(dest, c);
        else
            strbuf_append_strf(dest, "%%%02X", c);
    }
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    struct language lang;
    get_language(&lang);

    enum {
        OPT_verbose   = 1 << 0,
        OPT_syslog    = 1 << 1,
        OPT_insecure  = 1 << 2,
        OPT_url       = 1 << 3,
        OPT_port      = 1 << 4,
        OPT_directory = 1 << 5,
        OPT_headers   = 1 << 6,
    };

    /* Keep enum above and order of options below in sync! */
    struct options options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL('s', "syslog", NULL, _("log to syslog")),
        OPT_BOOL('k', "insecure", NULL,
                 _("allow insecure connection to dedup server")),
        OPT_STRING(0, "url", &(cfg.url), "URL",
                   _("dedup server URL")),
        OPT_INTEGER(0, "port", &(cfg.port),
                    _("dedup server port")),
        OPT_STRING('d', "dump-dir", &dump_dir_name, "DUMP_DIR",
                   _("Problem directory")),
        OPT_BOOL(0, "headers", NULL,
                 _("(debug) show received HTTP headers")),
        OPT_END()
    };

    const char *usage = _("abrt-dedup-client component backtrace_file [options]");

    char *env_url = getenv("DEDUP_SERVER_URL");
    if (env_url)
        cfg.url = env_url;

    char *env_port = getenv("DEDUP_SERVER_PORT");
    if (env_port)
        cfg.port = xatou(env_port);

    char *env_insecure = getenv("DEDUP_SERVER_INSECURE");
    if (env_insecure)
        cfg.ssl_allow_insecure = strncmp(env_insecure, "insecure", strlen("insecure")) == 0;

    unsigned opts = parse_opts(argc, argv, options, usage);
    if (opts & OPT_syslog)
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    /* expecting no positional arguments */
    argv += optind;
    if (argv[0])
        show_usage_and_die(usage, options);

    if (!cfg.ssl_allow_insecure)
        cfg.ssl_allow_insecure = opts & OPT_insecure;

    http_show_headers = opts & OPT_headers;

    /* load component and backtrace from dump_dir */
    struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
    /* error message was emitted by dd_opendir */
    if (!dd)
        xfunc_die();
    component = dd_load_text_ext(dd, FILENAME_COMPONENT,
                                 DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    backtrace = dd_load_text_ext(dd, FILENAME_BACKTRACE,
                                 DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    dd_close(dd);

    if (!component || !backtrace)
        error_msg_and_die(_("'" FILENAME_COMPONENT "' and '" FILENAME_BACKTRACE
                            "' files are required to check for duplicates."));

    if (!is_valid_component_name(component))
        error_msg_and_die(_("Forbidden component name: '%s'."), component);

    struct strbuf *request_body = strbuf_new();
    strbuf_append_str(request_body, "component=");
    /* escape component - may contain '+' */
    escape(component, request_body);
    free(component);
    strbuf_append_str(request_body, "&backtrace=");
    escape(backtrace, request_body);
    free(backtrace);

    struct strbuf *request = strbuf_new();
    strbuf_append_strf(request,
                       "POST /btserver.cgi HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "Content-Type: application/x-www-form-urlencoded\r\n"
                       "Content-Length: %d\r\n"
                       "Connection: close\r\n",
                       cfg.url, request_body->len);

    if (lang.encoding)
        strbuf_append_strf(request, "Accept-Charset: %s\r\n", lang.encoding);

    if (lang.locale)
    {
        strbuf_append_strf(request, "Accept-Language: %s\r\n", lang.locale);
        free(lang.locale);
    }

    strbuf_append_strf(request, "\r\n%s", request_body->buf);
    strbuf_free(request_body);

    /* Initialize NSS */
    SECMODModule *mod;
    PK11GenericObject *cert;
    nss_init(&mod, &cert);

    /* start SSL communication */
    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect(&cfg, &tcp_sock, &ssl_sock);
    PRInt32 written = PR_Send(tcp_sock, request->buf, request->len,
                              0/*flags*/, PR_INTERVAL_NO_TIMEOUT);
    if (written < 0)
    {
        PR_Close(ssl_sock);
        alert_connection_error();
        error_msg_and_die(_("Failed to send HTTP request of length %d: NSS error %d."),
                            request->len, PR_GetError());
    }

    char *response = tcp_read_response(tcp_sock);
    strbuf_free(request);
    ssl_disconnect(ssl_sock);
    if (http_show_headers)
        http_print_headers(stdout, response);

    char *response_body = http_get_body(response);
    char *encoding = http_get_header_value(response, "Transfer-Encoding");
    if (encoding && strcasecmp("chunked", encoding) == 0)
    {
        char *newbody = http_join_chunked(response_body, strlen(response_body));
        free(response_body);
        response_body = newbody;
    }
    free(encoding);

    /* iterate line by line */
    struct strbuf *result = strbuf_new();
    char *line, *newline;
    for (line = response_body; line; line = newline)
    {
        newline = strchr(line, '\n');
        if (newline)
        {
            *newline = '\0';
            /* we are on '\n', the line could be terminated by "\r\n" */
            if (*(newline - 1) == '\r')
                *(newline - 1) = '\0';
            ++newline;
        }

        /* forward to client */
        if (strncmp(SHOW_PREFIX, line, strlen(SHOW_PREFIX)) == 0)
            puts(line + strlen(SHOW_PREFIX));
        /* save to FILENAME_REMOTE_RESULT */
        else if (strncmp(SAVE_PREFIX, line, strlen(SAVE_PREFIX)) == 0)
            strbuf_append_strf(result, "%s\n", line + strlen(SAVE_PREFIX));
        else
            /* ignore anything else, just print on -v */
            VERB1 printf("Ignoring: '%s'\n", line);
    }

    /* save result */
    if (result->len > 0)
    {
        dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (!dd)
            xfunc_die();
        dd_save_text(dd, FILENAME_REMOTE_RESULT, result->buf);
        dd_close(dd);
    }
    strbuf_free(result);

    free(response);
    free(response_body);

    /* Shutdown NSS. */
    nss_close(mod, cert);

    return 0;
}
