/*
    Copyright (C) 2010  Red Hat, Inc.

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
#include "https-utils.h"

#define MAX_FORMATS 16
#define MAX_RELEASES 32
#define MAX_DOTS_PER_LINE 80
#define MIN_EXPLOITABLE_RATING 4

enum
{
    TASK_RETRACE,
    TASK_DEBUG,
    TASK_VMCORE,
};

static struct language lang;

struct retrace_settings
{
    int running_tasks;
    int max_running_tasks;
    long long max_packed_size;
    long long max_unpacked_size;
    char *supported_formats[MAX_FORMATS];
    char *supported_releases[MAX_RELEASES];
};

static const char *dump_dir_name = NULL;
static const char *coredump = NULL;
static const char *required_retrace[] = { FILENAME_COREDUMP,
                                          FILENAME_EXECUTABLE,
                                          FILENAME_PACKAGE,
                                          FILENAME_OS_RELEASE,
                                          NULL };
static const char *optional_retrace[] = { FILENAME_ROOTDIR,
                                          FILENAME_OS_RELEASE_IN_ROOTDIR,
                                          NULL };
static const char *required_vmcore[] = { FILENAME_VMCORE,
                                         NULL };
static unsigned delay = 0;
static int task_type = TASK_RETRACE;
static bool http_show_headers;
static bool no_pkgcheck;

static struct https_cfg cfg =
{
    .url = "retrace.fedoraproject.org",
    .port = 443,
    .ssl_allow_insecure = false,
};

static void alert_crash_too_large()
{
    alert(_("Retrace server can not be used, because the crash "
            "is too large. Try local retracing."));
}

/* Add an entry name to the args array if the entry name exists in a
 * problem directory. The entry is added to argindex offset to the array,
 * and the argindex is then increased.
 */
static void args_add_if_exists(const char *args[],
                               struct dump_dir *dd,
                               const char *name,
                               int *argindex)
{
    if (dd_exist(dd, name))
    {
        args[*argindex] = name;
        *argindex += 1;
    }
}

/* Create an archive with files required for retrace server and return
 * a file descriptor. Returns -1 if it fails.
 */
static int create_archive(bool unlink_temp)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return -1;

    /* Open a temporary file. */
    char *filename = xstrdup(LARGE_DATA_TMP_DIR"/abrt-retrace-client-archive-XXXXXX.tar.xz");
    int tempfd = mkstemps(filename, /*suffixlen:*/7);
    if (tempfd == -1)
        perror_msg_and_die(_("Can't create temporary file in "LARGE_DATA_TMP_DIR));
    if (unlink_temp)
        xunlink(filename);
    free(filename);

    /* Run xz:
     * - xz reads input from a pipe
     * - xz writes output to the temporary file.
     */
    const char *xz_args[4];
    xz_args[0] = "xz";
    xz_args[1] = "-2";
    xz_args[2] = "-";
    xz_args[3] = NULL;

    int tar_xz_pipe[2];
    xpipe(tar_xz_pipe);

    fflush(NULL); /* paranoia */
    pid_t xz_child = vfork();
    if (xz_child == -1)
        perror_msg_and_die("vfork");
    if (xz_child == 0)
    {
        close(tar_xz_pipe[1]);
        xmove_fd(tar_xz_pipe[0], STDIN_FILENO);
        xmove_fd(tempfd, STDOUT_FILENO);
        execvp(xz_args[0], (char * const*)xz_args);
        perror_msg_and_die(_("Can't execute '%s'"), xz_args[0]);
    }

    close(tar_xz_pipe[0]);

    /* Run tar, and set output to a pipe with xz waiting on the other
     * end.
     */
    const char *tar_args[10];
    tar_args[0] = "tar";
    tar_args[1] = "cO";
    tar_args[2] = xasprintf("--directory=%s", dump_dir_name);

    const char **required_files = task_type == TASK_VMCORE ? required_vmcore : required_retrace;
    int index = 3;
    while (required_files[index - 3])
        args_add_if_exists(tar_args, dd, required_files[index - 3], &index);

    if (task_type == TASK_RETRACE || task_type == TASK_DEBUG)
    {
        int i;
        for (i = 0; optional_retrace[i]; ++i)
            args_add_if_exists(tar_args, dd, optional_retrace[i], &index);
    }

    tar_args[index] = NULL;
    dd_close(dd);

    fflush(NULL); /* paranoia */
    pid_t tar_child = vfork();
    if (tar_child == -1)
        perror_msg_and_die("vfork");
    if (tar_child == 0)
    {
        xmove_fd(xopen("/dev/null", O_RDWR), STDIN_FILENO);
        xmove_fd(tar_xz_pipe[1], STDOUT_FILENO);
        execvp(tar_args[0], (char * const*)tar_args);
        perror_msg_and_die(_("Can't execute '%s'"), tar_args[0]);
    }

    free((void*)tar_args[2]);
    close(tar_xz_pipe[1]);

    /* Wait for tar and xz to finish successfully */
    int status;
    log_notice("Waiting for tar...");
    safe_waitpid(tar_child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        /* Hopefully, by this time child emitted more meaningful
         * error message. But just in case it didn't:
         */
        error_msg_and_die(_("Can't create temporary file in "LARGE_DATA_TMP_DIR));
    log_notice("Waiting for xz...");
    safe_waitpid(xz_child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        error_msg_and_die(_("Can't create temporary file in "LARGE_DATA_TMP_DIR));
    log_notice("Done...");

    xlseek(tempfd, 0, SEEK_SET);
    return tempfd;
}

struct retrace_settings *get_settings()
{
    struct retrace_settings *settings = xzalloc(sizeof(struct retrace_settings));

    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect(&cfg, &tcp_sock, &ssl_sock);
    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "GET /settings HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "\r\n", cfg.url);
    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        alert_connection_error(cfg.url);
        error_msg_and_die(_("Failed to send HTTP header of length %d: NSS error %d"),
                          http_request->len, PR_GetError());
    }
    strbuf_free(http_request);

    char *http_response = tcp_read_response(tcp_sock);
    if (http_show_headers)
        http_print_headers(stderr, http_response);
    int response_code = http_get_response_code(http_response);
    if (response_code != 200)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, http_response);
    }

    char *headers_end = strstr(http_response, "\r\n\r\n");
    char *c, *row, *value;
    if (!headers_end)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Invalid response from server: missing HTTP message body."));
    }
    row = headers_end + strlen("\r\n\r\n");

    do
    {
        /* split rows */
        c = strchr(row, '\n');
        if (c)
            *c = '\0';

        /* split key and values */
        value = strchr(row, ' ');
        if (!value)
        {
            row = c + 1;
            continue;
        }

        *value = '\0';
        ++value;

        if (0 == strcasecmp("running_tasks", row))
            settings->running_tasks = atoi(value);
        else if (0 == strcasecmp("max_running_tasks", row))
            settings->max_running_tasks = atoi(value);
        else if (0 == strcasecmp("max_packed_size", row))
            settings->max_packed_size = atoll(value) * 1024 * 1024;
        else if (0 == strcasecmp("max_unpacked_size", row))
            settings->max_unpacked_size = atoll(value) * 1024 * 1024;
        else if (0 == strcasecmp("supported_formats", row))
        {
            char *space;
            int i;
            for (i = 0; i < MAX_FORMATS - 1 && (space = strchr(value, ' ')); ++i)
            {
                *space = '\0';
                settings->supported_formats[i] = xstrdup(value);
                value = space + 1;
            }

            /* last element */
            settings->supported_formats[i] = xstrdup(value);
        }
        else if (0 == strcasecmp("supported_releases", row))
        {
            char *space;
            int i;
            for (i = 0; i < MAX_RELEASES - 1 && (space = strchr(value, ' ')); ++i)
            {
                *space = '\0';
                settings->supported_releases[i] = xstrdup(value);
                value = space + 1;
            }

            /* last element */
            settings->supported_releases[i] = xstrdup(value);
        }

        /* the beginning of the next row */
        row = c + 1;
    } while (c);

    free(http_response);
    ssl_disconnect(ssl_sock);

    return settings;
}

static void free_settings(struct retrace_settings *settings)
{
    if (!settings)
        return;

    int i;
    for (i = 0; i < MAX_FORMATS; ++i)
        free(settings->supported_formats[i]);

    for (i = 0; i < MAX_RELEASES; ++i)
        free(settings->supported_releases[i]);

    free(settings);
}

/* returns release identifier as dist-ver-arch */
/* or NULL if unknown */
static char *get_release_id(map_string_t *osinfo, const char *architecture)
{
    char *arch = xstrdup(architecture);

    if (strcmp("i686", arch) == 0 || strcmp("i586", arch) == 0)
    {
        free(arch);
        arch = xstrdup("i386");
    }

    char *result = NULL;
    char *release = NULL;
    char *version = NULL;
    parse_osinfo_for_rhts(osinfo, (char **)&release, (char **)&version);

    if (release == NULL || version == NULL)
        error_msg_and_die("Can't parse OS release name or version");

    char *space = strchr(version, ' ');
    if (space)
        *space = '\0';

    if (strcmp("Fedora", release) == 0)
    {
        /* Because of inconsistency between Fedora's os-release and retrace
         * server.
         *
         * Adding the reporting fields to Fedora's os-release was a bit
         * frustrating for all participants and fixing it on the retrace server
         * side is neither feasible nor acceptable.
         *
         * Therefore, we have decided to add the following hack.
         */
        if (strcmp("Rawhide", version) == 0)
        {
            /* Rawhide -> rawhide */
            version[0] = 'r';
        }
        /* Fedora -> fedora */
        release[0] = 'f';
    }
    else if (strcmp("Red Hat Enterprise Linux", release) == 0)
        strcpy(release, "rhel");

    result = xasprintf("%s-%s-%s", release, version, arch);

    free(release);
    free(version);
    free(arch);
    return result;
}

static int check_package(const char *nvr, const char *arch, map_string_t *osinfo, char **msg)
{
    char *releaseid = get_release_id(osinfo, arch);

    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect(&cfg, &tcp_sock, &ssl_sock);
    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "GET /checkpackage HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "X-Package-NVR: %s\r\n"
                       "X-Package-Arch: %s\r\n"
                       "X-OS-Release: %s\r\n"
                       "%s"
                       "%s"
                       "\r\n",
                       cfg.url, nvr, arch, releaseid,
                       lang.accept_charset,
                       lang.accept_language
    );

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        alert_connection_error(cfg.url);
        error_msg_and_die(_("Failed to send HTTP header of length %d: NSS error %d"),
                          http_request->len, PR_GetError());
    }
    strbuf_free(http_request);
    char *http_response = tcp_read_response(tcp_sock);
    ssl_disconnect(ssl_sock);
    if (http_show_headers)
        http_print_headers(stderr, http_response);
    int response_code = http_get_response_code(http_response);
    /* we are expecting either 302 or 404 */
    if (response_code != 302 && response_code != 404)
    {
        char *http_body = http_get_body(http_response);
        alert_server_error(cfg.url);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                            response_code, http_body);
    }

    if (msg)
    {
        if (response_code == 404)
        {
            const char *os = get_map_string_item_or_empty(osinfo, OSINFO_PRETTY_NAME);
            if (!os)
                os = get_map_string_item_or_empty(osinfo, OSINFO_NAME);

            *msg = xasprintf(_("Retrace server is unable to process package "
                               "'%s.%s'.\nIs it a part of official '%s' repositories?"),
                               nvr, arch, os);
        }
        else
            *msg = NULL;
    }

    free(http_response);
    free(releaseid);

    return response_code == 302;
}

static int create(bool delete_temp_archive,
                  char **task_id,
                  char **task_password)
{
    if (delay)
    {
        puts(_("Querying server settings"));
        fflush(stdout);
    }

    struct retrace_settings *settings = get_settings();

    if (settings->running_tasks >= settings->max_running_tasks)
    {
        alert(_("The server is fully occupied. Try again later."));
        error_msg_and_die(_("The server denied your request."));
    }

    long long unpacked_size = 0;
    struct stat file_stat;

    /* get raw size */
    if (coredump)
    {
        xstat(coredump, &file_stat);
        unpacked_size = (long long)file_stat.st_size;
    }
    else if (dump_dir_name != NULL)
    {
        struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags*/ 0);
        if (!dd)
            xfunc_die(); /* dd_opendir already emitted error message */
        if (dd_exist(dd, FILENAME_VMCORE))
            task_type = TASK_VMCORE;
        dd_close(dd);

        char *path;
        int i = 0;
        const char **required_files = task_type == TASK_VMCORE ? required_vmcore : required_retrace;
        while (required_files[i])
        {
            path = concat_path_file(dump_dir_name, required_files[i]);
            xstat(path, &file_stat);
            free(path);

            if (!S_ISREG(file_stat.st_mode))
                error_msg_and_die(_("'%s' must be a regular file in "
                                    "order to use Retrace server."),
                                  required_files[i]);

            unpacked_size += (long long)file_stat.st_size;
            ++i;
        }

        if (task_type == TASK_RETRACE || task_type == TASK_DEBUG)
        {
            for (i = 0; optional_retrace[i]; ++i)
            {
                path = concat_path_file(dump_dir_name, optional_retrace[i]);
                if (stat(path, &file_stat) != -1)
                {
                    if (!S_ISREG(file_stat.st_mode))
                        error_msg_and_die(_("'%s' must be a regular file in "
                                            "order to use Retrace server."),
                                          required_files[i]);

                    unpacked_size += (long long)file_stat.st_size;
                }
                free(path);
            }
        }
    }

    if (unpacked_size > settings->max_unpacked_size)
    {
        alert_crash_too_large();

        /* Leaking size and max_size in hope the memory will be released in
         * error_msg_and_die() */
        gchar *size = g_format_size_full(unpacked_size, G_FORMAT_SIZE_IEC_UNITS);
        gchar *max_size = g_format_size_full(settings->max_unpacked_size, G_FORMAT_SIZE_IEC_UNITS);

        error_msg_and_die(_("The size of your crash is %s, "
                            "but the retrace server only accepts "
                            "crashes smaller or equal to %s."),
                            size, max_size);
    }

    if (settings->supported_formats)
    {
        int i;
        bool supported = false;
        for (i = 0; i < MAX_FORMATS && settings->supported_formats[i]; ++i)
            if (strcmp("application/x-xz-compressed-tar", settings->supported_formats[i]) == 0)
            {
                supported = true;
                break;
            }

        if (!supported)
        {
            alert_server_error(cfg.url);
            error_msg_and_die(_("The server does not support "
                                "xz-compressed tarballs."));
        }
    }


    if (task_type != TASK_VMCORE && dump_dir_name)
    {
        struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
        if (!dd)
            xfunc_die();
        problem_data_t *pd = create_problem_data_from_dump_dir(dd);
        dd_close(dd);

        char *package = problem_data_get_content_or_NULL(pd, FILENAME_PACKAGE);
        char *arch = problem_data_get_content_or_NULL(pd, FILENAME_ARCHITECTURE);
        map_string_t *osinfo = new_map_string();
        problem_data_get_osinfo(pd, osinfo);

        /* not needed for TASK_VMCORE - the information is kept in the vmcore itself */
        if (settings->supported_releases)
        {
            char *releaseid = get_release_id(osinfo, arch);
            if (!releaseid)
                error_msg_and_die("Unable to parse release.");

            int i;
            bool supported = false;
            for (i = 0; i < MAX_RELEASES && settings->supported_releases[i]; ++i)
                if (strcmp(releaseid, settings->supported_releases[i]) == 0)
                {
                    supported = true;
                    break;
                }

            if (!supported)
            {
                char *msg = xasprintf(_("The release '%s' is not supported by the"
                                        " Retrace server."), releaseid);
                alert(msg);
                free(msg);
                error_msg_and_die(_("The server is not able to"
                                    " handle your request."));
            }

            free(releaseid);
        }

        /* not relevant for vmcores - it may take a long time to get package from vmcore */
        if (!no_pkgcheck)
        {
            char *msg;
            int known = check_package(package, arch, osinfo, &msg);
            if (msg)
            {
                alert(msg);
                free(msg);
            }

            if (!known)
                error_msg_and_die(_("Unknown package sent to Retrace server."));
        }

        free_map_string(osinfo);
        problem_data_free(pd);
    }

    if (delay)
    {
        puts(_("Preparing an archive to upload"));
        fflush(stdout);
    }

    int tempfd = create_archive(delete_temp_archive);
    if (-1 == tempfd)
        return 1;

    /* Get the file size. */
    fstat(tempfd, &file_stat);
    gchar *human_size = g_format_size_full((long long)file_stat.st_size, G_FORMAT_SIZE_IEC_UNITS);
    if ((long long)file_stat.st_size > settings->max_packed_size)
    {
        alert_crash_too_large();

        /* Leaking human_size and max_size in hope the memory will be released in
         * error_msg_and_die() */
        gchar *max_size = g_format_size_full(settings->max_packed_size, G_FORMAT_SIZE_IEC_UNITS);

        error_msg_and_die(_("The size of your archive is %s, "
                            "but the retrace server only accepts "
                            "archives smaller or equal to %s."),
                          human_size, max_size);
    }

    free_settings(settings);

    int size_mb = file_stat.st_size / (1024 * 1024);

    if (size_mb > 8) /* 8 MB - should be configurable */
    {
        char *question = xasprintf(_("You are going to upload %s. "
                                     "Continue?"), human_size);

        int response = ask_yes_no(question);
        free(question);

        if (!response)
        {
            set_xfunc_error_retval(EXIT_CANCEL_BY_USER);
            error_msg_and_die(_("Cancelled by user"));
        }
    }

    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect(&cfg, &tcp_sock, &ssl_sock);
    /* Upload the archive. */
    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "POST /create HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "Content-Type: application/x-xz-compressed-tar\r\n"
                       "Content-Length: %lld\r\n"
                       "Connection: close\r\n"
                       "X-Task-Type: %d\r\n"
                       "%s"
                       "%s"
                       "\r\n",
                       cfg.url, (long long)file_stat.st_size, task_type,
                       lang.accept_charset,
                       lang.accept_language
    );

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        alert_connection_error(cfg.url);
        error_msg_and_die(_("Failed to send HTTP header of length %d: NSS error %d"),
                          http_request->len, PR_GetError());
    }

    if (delay)
    {
        printf(_("Uploading %s\n"), human_size);
        fflush(stdout);
    }

    g_free(human_size);

    strbuf_free(http_request);
    int result = 0;
    int i;
    char buf[32768];

    time_t start, now;
    time(&start);

    for (i = 0;; ++i)
    {
        if (delay)
        {
            time(&now);
            if (now - start >= delay)
            {
                time(&start);
                int progress = 100 * i * sizeof(buf) / file_stat.st_size;
                if (progress > 100)
                    continue;

                printf(_("Uploading %d%%\n"), progress);
                fflush(stdout);
            }
        }

        int r = read(tempfd, buf, sizeof(buf));
        if (r <= 0)
        {
            if (r == -1)
            {
                if (EINTR == errno || EAGAIN == errno || EWOULDBLOCK == errno)
                    continue;
                perror_msg_and_die(_("Failed to read from a pipe"));
            }
            break;
        }
        written = PR_Send(tcp_sock, buf, r,
                          /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
        if (written == -1)
        {
            /* Print error message, but do not exit.  We need to check
               if the server send some explanation regarding the
               error. */
            result = 1;
            alert_connection_error(cfg.url);
            error_msg(_("Failed to send data: NSS error %d (%s): %s"),
                      PR_GetError(),
                      PR_ErrorToName(PR_GetError()),
                      PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
            break;
        }
    }
    close(tempfd);

    if (delay)
    {
        puts(_("Upload successful"));
        fflush(stdout);
    }

    /* Read the HTTP header of the response from server. */
    char *http_response = tcp_read_response(tcp_sock);
    char *http_body = http_get_body(http_response);
    if (!http_body)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Invalid response from server: missing HTTP message body."));
    }
    if (http_show_headers)
        http_print_headers(stderr, http_response);
    int response_code = http_get_response_code(http_response);
    if (response_code == 500 || response_code == 507)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(http_body);
    }
    else if (response_code == 403)
    {
        alert(_("Your problem directory is corrupted and can not "
                "be processed by the Retrace server."));
        error_msg_and_die(_("The archive contains malicious files (such as symlinks) "
                            "and thus can not be processed."));
    }
    else if (response_code != 201)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"), response_code, http_body);
    }
    free(http_body);
    *task_id = http_get_header_value(http_response, "X-Task-Id");
    if (!*task_id)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Invalid response from server: missing X-Task-Id."));
    }
    *task_password = http_get_header_value(http_response, "X-Task-Password");
    if (!*task_password)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Invalid response from server: missing X-Task-Password."));
    }
    free(http_response);
    ssl_disconnect(ssl_sock);

    if (delay)
    {
        puts(_("Retrace job started"));
        fflush(stdout);
    }

    return result;
}

static int run_create(bool delete_temp_archive)
{
    char *task_id, *task_password;
    int result = create(delete_temp_archive, &task_id, &task_password);
    if (0 != result)
        return result;
    printf(_("Task Id: %s\nTask Password: %s\n"), task_id, task_password);
    free(task_id);
    free(task_password);
    return 0;
}

/* Caller must free task_status and status_message */
static void status(const char *task_id,
                   const char *task_password,
                   char **task_status,
                   char **status_message)
{
    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect(&cfg, &tcp_sock, &ssl_sock);
    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "GET /%s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "X-Task-Password: %s\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "%s"
                       "%s"
                       "\r\n",
                       task_id, cfg.url, task_password,
                       lang.accept_charset,
                       lang.accept_language
    );

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        alert_connection_error(cfg.url);
        error_msg_and_die(_("Failed to send HTTP header of length %d: NSS error %d"),
                          http_request->len, PR_GetError());
    }
    strbuf_free(http_request);
    char *http_response = tcp_read_response(tcp_sock);
    char *http_body = http_get_body(http_response);
    if (!*http_body)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Invalid response from server: missing HTTP message body."));
    }
    if (http_show_headers)
        http_print_headers(stderr, http_response);
    int response_code = http_get_response_code(http_response);
    if (response_code != 200)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, http_body);
    }
    *task_status = http_get_header_value(http_response, "X-Task-Status");
    if (!*task_status)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Invalid response from server: missing X-Task-Status."));
    }
    *status_message = http_body;
    free(http_response);
    ssl_disconnect(ssl_sock);
}

static void run_status(const char *task_id, const char *task_password)
{
    char *task_status;
    char *status_message;
    status(task_id, task_password, &task_status, &status_message);
    printf(_("Task Status: %s\n%s\n"), task_status, status_message);
    free(task_status);
    free(status_message);
}

/* Caller must free backtrace */
static void backtrace(const char *task_id, const char *task_password,
                      char **backtrace)
{
    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect(&cfg, &tcp_sock, &ssl_sock);
    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "GET /%s/backtrace HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "X-Task-Password: %s\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "%s"
                       "%s"
                       "\r\n",
                       task_id, cfg.url, task_password,
                       lang.accept_charset,
                       lang.accept_language
    );

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        alert_connection_error(cfg.url);
        error_msg_and_die(_("Failed to send HTTP header of length %d: NSS error %d."),
                          http_request->len, PR_GetError());
    }
    strbuf_free(http_request);
    char *http_response = tcp_read_response(tcp_sock);
    char *http_body = http_get_body(http_response);
    if (!http_body)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Invalid response from server: missing HTTP message body."));
    }
    if (http_show_headers)
        http_print_headers(stderr, http_response);
    int response_code = http_get_response_code(http_response);
    if (response_code != 200)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, http_body);
    }
    *backtrace = http_body;
    free(http_response);
    ssl_disconnect(ssl_sock);
}

static void run_backtrace(const char *task_id, const char *task_password)
{
    char *backtrace_text;
    backtrace(task_id, task_password, &backtrace_text);
    printf("%s", backtrace_text);
    free(backtrace_text);
}

/* This is not robust at all but will work for now */
static int get_exploitable_rating(const char *exploitable_text)
{
    const char *colon = strrchr(exploitable_text, ':');
    int result;
    if (!colon || sscanf(colon, ": %d", &result) != 1)
    {
        log_notice("Unable to determine exploitable rating");
        return -1;
    }

    log_notice("Exploitable rating: %d", result);
    return result;
}

/* Caller must free exploitable_text */
static void exploitable(const char *task_id, const char *task_password,
                        char **exploitable_text)
{
    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect(&cfg, &tcp_sock, &ssl_sock);
    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "GET /%s/exploitable HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "X-Task-Password: %s\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "%s"
                       "%s"
                       "\r\n",
                       task_id, cfg.url, task_password,
                       lang.accept_charset,
                       lang.accept_language
    );

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        alert_connection_error(cfg.url);
        error_msg_and_die(_("Failed to send HTTP header of length %d: NSS error %d."),
                          http_request->len, PR_GetError());
    }
    strbuf_free(http_request);
    char *http_response = tcp_read_response(tcp_sock);
    char *http_body = http_get_body(http_response);
    if (!http_body)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Invalid response from server: missing HTTP message body."));
    }
    if (http_show_headers)
        http_print_headers(stderr, http_response);
    int response_code = http_get_response_code(http_response);

    free(http_response);
    ssl_disconnect(ssl_sock);

    /* 404 = exploitability results not available
       200 = OK
       anything else = error */
    if (response_code == 404)
        *exploitable_text = NULL;
    else if (response_code == 200)
        *exploitable_text = http_body;
    else
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, http_body);
    }
}

static void run_exploitable(const char *task_id, const char *task_password)
{
    char *exploitable_text;
    exploitable(task_id, task_password, &exploitable_text);
    if (exploitable_text)
    {
        printf("%s\n", exploitable_text);
        free(exploitable_text);
    }
    else
        puts("No exploitability information available.");
}

static void run_log(const char *task_id, const char *task_password)
{
    PRFileDesc *tcp_sock, *ssl_sock;
    ssl_connect(&cfg, &tcp_sock, &ssl_sock);
    struct strbuf *http_request = strbuf_new();
    strbuf_append_strf(http_request,
                       "GET /%s/log HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "X-Task-Password: %s\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "%s"
                       "%s"
                       "\r\n",
                       task_id, cfg.url, task_password,
                       lang.accept_charset,
                       lang.accept_language
    );

    PRInt32 written = PR_Send(tcp_sock, http_request->buf, http_request->len,
                              /*flags:*/0, PR_INTERVAL_NO_TIMEOUT);
    if (written == -1)
    {
        alert_connection_error(cfg.url);
        error_msg_and_die(_("Failed to send HTTP header of length %d: NSS error %d."),
                          http_request->len, PR_GetError());
    }
    strbuf_free(http_request);
    char *http_response = tcp_read_response(tcp_sock);
    char *http_body = http_get_body(http_response);
    if (!http_body)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Invalid response from server: missing HTTP message body."));
    }
    if (http_show_headers)
        http_print_headers(stderr, http_response);
    int response_code = http_get_response_code(http_response);
    if (response_code != 200)
    {
        alert_server_error(cfg.url);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, http_body);
    }
    puts(http_body);
    free(http_body);
    free(http_response);
    ssl_disconnect(ssl_sock);
}

static int run_batch(bool delete_temp_archive)
{
    char *task_id, *task_password;
    int retcode = create(delete_temp_archive, &task_id, &task_password);
    if (0 != retcode)
        return retcode;
    char *task_status = xstrdup("");
    char *status_message = xstrdup("");
    int status_delay = delay ? delay : 10;
    int dots = 0;
    while (0 != strncmp(task_status, "FINISHED", strlen("finished")))
    {
        char *previous_status_message = status_message;
        free(task_status);
        sleep(status_delay);
        status(task_id, task_password, &task_status, &status_message);
        if (g_verbose > 0 || 0 != strcmp(previous_status_message, status_message))
        {
            if (dots)
            {   /* A same message was received and a period was printed instead
                 * but the period wasn't followed by new line and now we are 
                 * goning to print a new message thus we want to start at next line
                 */
                dots = 0;
                putchar('\n');
            }
            puts(status_message);
            fflush(stdout);
        }
        else
        {
            if (dots >= MAX_DOTS_PER_LINE)
            {
                dots = 0;
                putchar('\n');
            }
            ++dots;
            client_log(".");
            fflush(stdout);
        }
        free(previous_status_message);
        previous_status_message = status_message;
    }
    if (0 == strcmp(task_status, "FINISHED_SUCCESS"))
    {
        char *backtrace_text;
        backtrace(task_id, task_password, &backtrace_text);
        char *exploitable_text = NULL;
        if (task_type == TASK_RETRACE)
        {
            exploitable(task_id, task_password, &exploitable_text);
            if (!exploitable_text)
                log_notice("No exploitable data available");
        }

        if (dump_dir_name)
        {
            struct dump_dir *dd = dd_opendir(dump_dir_name, 0/* flags */);
            if (!dd)
            {
                free(backtrace_text);
                xfunc_die();
            }

            /* the result of TASK_VMCORE is not backtrace, but kernel log */
            const char *target = task_type == TASK_VMCORE ? FILENAME_KERNEL_LOG : FILENAME_BACKTRACE;
            dd_save_text(dd, target, backtrace_text);

            if (exploitable_text)
            {
                int exploitable_rating = get_exploitable_rating(exploitable_text);
                if (exploitable_rating >= MIN_EXPLOITABLE_RATING)
                    dd_save_text(dd, FILENAME_EXPLOITABLE, exploitable_text);
                else
                    log_notice("Not saving exploitable data, rating < %d",
                                  MIN_EXPLOITABLE_RATING);
            }

            dd_close(dd);
        }
        else
        {
            printf("%s\n", backtrace_text);
            if (exploitable_text)
                printf("%s\n", exploitable_text);
        }
        free(backtrace_text);
        free(exploitable_text);
    }
    else
    {
        alert(_("Retrace failed. Try again later and if the problem persists "
                "report this issue please."));
        run_log(task_id, task_password);
        retcode = 1;
    }
    free(task_status);
    free(status_message);
    free(task_id);
    free(task_password);
    return retcode;
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);
    get_language(&lang);

    const char *task_id = NULL;
    const char *task_password = NULL;

    enum {
        OPT_verbose   = 1 << 0,
        OPT_syslog    = 1 << 1,
        OPT_insecure  = 1 << 2,
        OPT_no_pkgchk = 1 << 3,
        OPT_url       = 1 << 4,
        OPT_port      = 1 << 5,
        OPT_headers   = 1 << 6,
        OPT_group_1   = 1 << 7,
        OPT_dir       = 1 << 8,
        OPT_core      = 1 << 9,
        OPT_delay     = 1 << 10,
        OPT_no_unlink = 1 << 11,
        OPT_group_2   = 1 << 12,
        OPT_task      = 1 << 13,
        OPT_password  = 1 << 14
    };

    /* Keep enum above and order of options below in sync! */
    struct options options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL('s', "syslog", NULL, _("log to syslog")),
        OPT_BOOL('k', "insecure", NULL,
                 _("allow insecure connection to retrace server")),
        OPT_BOOL(0, "no-pkgcheck", NULL,
                 _("do not check whether retrace server is able to "
                   "process given package before uploading the archive")),
        OPT_STRING(0, "url", &(cfg.url), "URL",
                   _("retrace server URL")),
        OPT_INTEGER(0, "port", &(cfg.port),
                    _("retrace server port")),
        OPT_BOOL(0, "headers", NULL,
                 _("(debug) show received HTTP headers")),
        OPT_GROUP(_("For create and batch operations")),
        OPT_STRING('d', "dir", &dump_dir_name, "DIR",
                   _("read data from ABRT problem directory")),
        OPT_STRING('c', "core", &coredump, "COREDUMP",
                   _("read data from coredump")),
        OPT_INTEGER('l', "status-delay", &delay,
                    _("Delay for polling operations")),
        OPT_BOOL(0, "no-unlink", NULL,
                 _("(debug) do not delete temporary archive created"
                   " from dump dir in "LARGE_DATA_TMP_DIR)),
        OPT_GROUP(_("For status, backtrace, and log operations")),
        OPT_STRING('t', "task", &task_id, "ID",
                   _("id of your task on server")),
        OPT_STRING('p', "password", &task_password, "PWD",
                   _("password of your task on server")),
        OPT_END()
    };

    const char *usage = _("abrt-retrace-client <operation> [options]\n"
        "Operations: create/status/backtrace/log/batch/exploitable");

    char *env_url = getenv("RETRACE_SERVER_URL");
    if (env_url)
        cfg.url = env_url;

    char *env_port = getenv("RETRACE_SERVER_PORT");
    if (env_port)
        cfg.port = xatou(env_port);

    char *env_delay = getenv("ABRT_STATUS_DELAY");
    if (env_delay)
        delay = xatou(env_delay);

    char *env_insecure = getenv("RETRACE_SERVER_INSECURE");
    if (env_insecure)
        cfg.ssl_allow_insecure = strncmp(env_insecure, "insecure", strlen("insecure")) == 0;

    unsigned opts = parse_opts(argc, argv, options, usage);
    if (opts & OPT_syslog)
    {
        logmode = LOGMODE_JOURNAL;
    }
    const char *operation = NULL;
    if (optind < argc)
        operation = argv[optind];
    else
        show_usage_and_die(usage, options);

    if (!cfg.ssl_allow_insecure)
        cfg.ssl_allow_insecure = opts & OPT_insecure;
    http_show_headers = opts & OPT_headers;
    no_pkgcheck = opts & OPT_no_pkgchk;

    /* Initialize NSS */
    SECMODModule *mod;
    PK11GenericObject *cert;
    nss_init(&mod, &cert);

    /* Run the desired operation. */
    int result = 0;
    if (0 == strcasecmp(operation, "create"))
    {
        if (!dump_dir_name && !coredump)
            error_msg_and_die(_("Either problem directory or coredump is needed."));
        result = run_create(0 == (opts & OPT_no_unlink));
    }
    else if (0 == strcasecmp(operation, "batch"))
    {
        if (!dump_dir_name && !coredump)
            error_msg_and_die(_("Either problem directory or coredump is needed."));
        result = run_batch(0 == (opts & OPT_no_unlink));
    }
    else if (0 == strcasecmp(operation, "status"))
    {
        if (!task_id)
            error_msg_and_die(_("Task id is needed."));
        if (!task_password)
            error_msg_and_die(_("Task password is needed."));
        run_status(task_id, task_password);
    }
    else if (0 == strcasecmp(operation, "backtrace"))
    {
        if (!task_id)
            error_msg_and_die(_("Task id is needed."));
        if (!task_password)
            error_msg_and_die(_("Task password is needed."));
        run_backtrace(task_id, task_password);
    }
    else if (0 == strcasecmp(operation, "log"))
    {
        if (!task_id)
            error_msg_and_die(_("Task id is needed."));
        if (!task_password)
            error_msg_and_die(_("Task password is needed."));
        run_log(task_id, task_password);
    }
    else if (0 == strcasecmp(operation, "exploitable"))
    {
        if (!task_id)
            error_msg_and_die(_("Task id is needed."));
        if (!task_password)
            error_msg_and_die(_("Task password is needed."));
        run_exploitable(task_id, task_password);
    }
    else
        error_msg_and_die(_("Unknown operation: %s."), operation);

    /* Shutdown NSS. */
    nss_close(mod, cert);

    return result;
}
