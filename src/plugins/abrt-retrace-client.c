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
#include <spawn.h>
#include <glib-unix.h>
#include <glib/gstdio.h>

#define MAX_FORMATS 16
#define MAX_RELEASES 32
#define MAX_DOTS_PER_LINE 80
#define MIN_EXPLOITABLE_RATING 4

extern char **environ;

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
    .uri = "https://retrace.fedoraproject.org",
    .ssl_allow_insecure = false,
};

static void print_header(const char *name,
                         const char *value,
                         gpointer    user_data)
{
    fprintf(user_data, "%s: %s\n", name, value);
}

static void alert_crash_too_large()
{
    libreport_alert(_("Retrace server can not be used, because the crash "
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
    pid_t xz_child, tar_child;
    int err;
    int xz_attr_set = 0, tar_attr_set = 0;
    int xz_actions_set = 0, tar_actions_set = 0;
    posix_spawnattr_t xz_attr, tar_attr;
    posix_spawn_file_actions_t xz_actions, tar_actions;

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return -1;

    /* Open a temporary file. */
    char *filename = g_strdup(LARGE_DATA_TMP_DIR"/abrt-retrace-client-archive-XXXXXX.tar.xz");
    int tempfd = mkstemps(filename, /*suffixlen:*/7);
    if (tempfd == -1)
        perror_msg_and_die(_("Can't create temporary file in "LARGE_DATA_TMP_DIR));
    if (unlink_temp)
        g_unlink(filename);
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
    g_unix_open_pipe(tar_xz_pipe, 0, NULL);

    fflush(NULL); /* paranoia */

    if ((err = posix_spawn_file_actions_init(&xz_actions)) != 0
#ifdef POSIX_SPAWN_USEVFORK
         || (err = posix_spawnattr_init(&xz_attr)) != 0
         || (xz_attr_set = 1,
             err = posix_spawnattr_setflags(&xz_attr, POSIX_SPAWN_USEVFORK)) != 0
#endif
         || (xz_actions_set = 1,
             err = posix_spawn_file_actions_addclose(&xz_actions, tar_xz_pipe[1])) != 0
         || (err = posix_spawn_file_actions_adddup2(&xz_actions, tar_xz_pipe[0], STDIN_FILENO)) != 0
         || (err = posix_spawn_file_actions_adddup2(&xz_actions, tempfd, STDOUT_FILENO)) != 0)
    {
        if (xz_actions_set == 1)
            posix_spawn_file_actions_destroy(&xz_actions);

        if (xz_attr_set == 1)
            posix_spawnattr_destroy(&xz_attr);

        perror_msg_and_die("posix_spawn init");
    }
    if ((err = posix_spawnp(&xz_child, xz_args[0], &xz_actions, &xz_attr, (char * const*)xz_args, environ)) != 0)
        perror_msg_and_die(_("Can't execute '%s'"), xz_args[0]);

    if ((err = posix_spawn_file_actions_destroy(&xz_actions)) != 0
         || ((xz_attr_set == 1) && ((err = posix_spawnattr_destroy(&xz_attr)) != 0)))
         perror_msg_and_die("posix_spawn destroy");

    close(tar_xz_pipe[0]);

    /* Run tar, and set output to a pipe with xz waiting on the other
     * end.
     */
    const char *tar_args[10];
    tar_args[0] = "tar";
    tar_args[1] = "cO";
    tar_args[2] = g_strdup_printf("--directory=%s", dump_dir_name);

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

    const char *dev_null_path = "/dev/null";

    if ((err = posix_spawn_file_actions_init(&tar_actions)) != 0
#ifdef POSIX_SPAWN_USEVFORK
         || (err = posix_spawnattr_init(&tar_attr)) != 0
         || (tar_attr_set = 1,
             err = posix_spawnattr_setflags(&tar_attr, POSIX_SPAWN_USEVFORK)) != 0
#endif
         || (tar_actions_set = 1,
             err = posix_spawn_file_actions_addopen(&tar_actions, STDIN_FILENO, dev_null_path, O_RDWR, 0)) != 0
         || (err = posix_spawn_file_actions_adddup2(&tar_actions, tar_xz_pipe[1], STDOUT_FILENO)) != 0)
    {
         if (tar_actions_set == 1)
             posix_spawn_file_actions_destroy(&tar_actions);

         if (tar_attr_set == 1)
             posix_spawnattr_destroy(&tar_attr);

         perror_msg_and_die("posix_spawn init");
    }
    if ((err = posix_spawnp(&tar_child, tar_args[0], &tar_actions, &tar_attr, (char * const*)tar_args, environ)) != 0)
        perror_msg_and_die(_("Can't execute '%s'"), tar_args[0]);

    if ((err = posix_spawn_file_actions_destroy(&tar_actions)) != 0
         || ((tar_attr_set == 1) && ((err = posix_spawnattr_destroy(&tar_attr)) != 0)))
         perror_msg_and_die("posix_spawn destroy");

    free((void*)tar_args[2]);
    close(tar_xz_pipe[1]);

    /* Wait for tar and xz to finish successfully */
    int status;
    log_notice("Waiting for tar...");
    libreport_safe_waitpid(tar_child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        /* Hopefully, by this time child emitted more meaningful
         * error message. But just in case it didn't:
         */
        error_msg_and_die(_("Can't create temporary file in "LARGE_DATA_TMP_DIR));
    log_notice("Waiting for xz...");
    libreport_safe_waitpid(xz_child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        error_msg_and_die(_("Can't create temporary file in "LARGE_DATA_TMP_DIR));
    log_notice("Done...");

    libreport_xlseek(tempfd, 0, SEEK_SET);
    return tempfd;
}

G_GNUC_NULL_TERMINATED
static SoupURI *build_uri_from_config(struct https_cfg *config,
                                      const char       *segment,
                                      ...)
{
    SoupURI *uri;
    va_list args;
    g_autofree const char *path = NULL;

    g_return_val_if_fail(NULL != config, NULL);

    uri = soup_uri_new_with_base(NULL, config->uri);
    /* Really only for compatibility. */
    if (NULL == soup_uri_get_scheme(uri))
    {
        g_autofree char *uri_string = NULL;

        uri_string = g_strdup_printf("https://%s", config->uri);
        uri = soup_uri_new(uri_string);
    }

    if (NULL == segment)
    {
        return uri;
    }

    path = soup_uri_get_path(uri);
    path = g_build_path("/", path, segment, NULL);

    va_start(args, segment);

    for (segment = va_arg(args, const char *); NULL != segment; segment = va_arg(args, const char *))
    {
        g_autofree const char *tmp = NULL;

        tmp = path;
        path = g_build_path("/", path, segment, NULL);
    }

    va_end(args);

    soup_uri_set_path(uri, path);

    return uri;
}

struct retrace_settings *get_settings(SoupSession *session)
{
    g_autoptr(SoupURI) uri = NULL;
    g_autoptr(SoupMessage) message = NULL;
    guint response_code;
    g_autoptr(SoupBuffer) response = NULL;
    char *c;
    char *value;
    const char *row;

    struct retrace_settings *settings = g_new0(struct retrace_settings, 1);
    uri = build_uri_from_config(&cfg, "settings", NULL);
    message = soup_message_new_from_uri("GET", uri);

    soup_message_headers_append(message->request_headers, "Accept-Charset", lang.charset);

    response_code = soup_session_send_message(session, message);

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(response_code))
    {
        alert_connection_error(cfg.uri);
        error_msg_and_die("%s", message->reason_phrase);
    }

    response = soup_message_body_flatten(message->response_body);

    if (http_show_headers)
    {
        soup_message_headers_foreach(message->response_headers, print_header, stderr);
    }

    if (response_code != 200)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, response->data);
    }

    row = response->data;

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
                settings->supported_formats[i] = g_strdup(value);
                value = space + 1;
            }

            /* last element */
            settings->supported_formats[i] = g_strdup(value);
        }
        else if (0 == strcasecmp("supported_releases", row))
        {
            char *space;
            int i;
            for (i = 0; i < MAX_RELEASES - 1 && (space = strchr(value, ' ')); ++i)
            {
                *space = '\0';
                settings->supported_releases[i] = g_strdup(value);
                value = space + 1;
            }

            /* last element */
            settings->supported_releases[i] = g_strdup(value);
        }

        /* the beginning of the next row */
        row = c + 1;
    } while (c);

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
    char *arch = g_strdup(architecture);

    if (strcmp("i686", arch) == 0 || strcmp("i586", arch) == 0)
    {
        free(arch);
        arch = g_strdup("i386");
    }

    char *result = NULL;
    char *release = NULL;
    char *version = NULL;
    libreport_parse_osinfo_for_rhts(osinfo, (char **)&release, (char **)&version);

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

    result = g_strdup_printf("%s-%s-%s", release, version, arch);

    free(release);
    free(version);
    free(arch);
    return result;
}

static int check_package(SoupSession   *session,
                         const char    *nvr,
                         const char    *arch,
                         map_string_t  *osinfo,
                         char         **msg)
{
    g_autofree char *release_id = NULL;
    g_autoptr(SoupURI) uri = NULL;
    g_autoptr(SoupMessage) message = NULL;
    guint response_code;
    g_autoptr(SoupBuffer) response = NULL;

    release_id = get_release_id(osinfo, arch);
    uri = build_uri_from_config(&cfg, "checkpackage", NULL);
    message = soup_message_new_from_uri("GET", uri);

    soup_message_headers_append(message->request_headers, "X-Package-NVR", nvr);
    soup_message_headers_append(message->request_headers, "X-Package-Arch", arch);
    soup_message_headers_append(message->request_headers, "X-OS-Release", release_id);
    soup_message_headers_append(message->request_headers, "Accept-Charset", lang.charset);

    response_code = soup_session_send_message(session, message);

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(response_code))
    {
        alert_connection_error(cfg.uri);
        error_msg_and_die("%s", message->reason_phrase);
    }

    response = soup_message_body_flatten(message->response_body);

    if (http_show_headers)
    {
        soup_message_headers_foreach(message->response_headers, print_header, stderr);
    }

    if (response_code != 302 && response_code != 404)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                            response_code, response->data);
    }

    if (msg)
    {
        if (response_code == 404)
        {
            const char *os = libreport_get_map_string_item_or_empty(osinfo, OSINFO_PRETTY_NAME);
            if (!os)
                os = libreport_get_map_string_item_or_empty(osinfo, OSINFO_NAME);

            *msg = g_strdup_printf(_("Retrace server is unable to process package "
                                     "'%s.%s'.\nIs it a part of official '%s' repositories?"),
                                   nvr, arch, os);
        }
        else
            *msg = NULL;
    }

    return response_code == 302;
}

typedef struct
{
    time_t start;
    size_t bytes_written;
} CreateProgressCallbackData;

static void on_wrote_body_data(SoupMessage *msg,
                               SoupBuffer  *chunk,
                               gpointer     user_data)
{
    CreateProgressCallbackData *callback_data;
    time_t now;

    callback_data = user_data;
    time(&now);

    callback_data->bytes_written += chunk->length;

    if (now - callback_data->start >= delay)
    {
        printf(_("Uploading %d%%\n"),
               (int)(100 * callback_data->bytes_written / msg->request_body->length));
        fflush(stdout);
    }
}

static int create(SoupSession  *session,
                  bool          delete_temp_archive,
                  char        **task_id,
                  char        **task_password)
{
    if (delay)
    {
        puts(_("Querying server settings"));
        fflush(stdout);
    }

    struct retrace_settings *settings = get_settings(session);

    g_return_val_if_fail(NULL != settings, EXIT_FAILURE);

    if (settings->running_tasks >= settings->max_running_tasks)
    {
        libreport_alert(_("The server is fully occupied. Try again later."));
        error_msg_and_die(_("The server denied your request."));
    }

    long long unpacked_size = 0;
    struct stat file_stat;

    /* get raw size */
    if (coredump)
    {
        g_stat(coredump, &file_stat);
        unpacked_size = (long long)file_stat.st_size;
    }
    else if (dump_dir_name != NULL)
    {
        struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags*/ 0);
        if (!dd)
            libreport_xfunc_die(); /* dd_opendir already emitted error message */
        if (dd_exist(dd, FILENAME_VMCORE))
            task_type = TASK_VMCORE;
        dd_close(dd);

        char *path;
        int i = 0;
        const char **required_files = task_type == TASK_VMCORE ? required_vmcore : required_retrace;
        while (required_files[i])
        {
            path = g_build_filename(dump_dir_name, required_files[i], NULL);
            g_stat(path, &file_stat);
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
                path = g_build_filename(dump_dir_name, optional_retrace[i], NULL);
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
            alert_server_error(cfg.uri);
            error_msg_and_die(_("The server does not support "
                                "xz-compressed tarballs."));
        }
    }


    if (task_type != TASK_VMCORE && dump_dir_name)
    {
        struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
        if (!dd)
            libreport_xfunc_die();
        problem_data_t *pd = create_problem_data_from_dump_dir(dd);
        dd_close(dd);

        char *package = problem_data_get_content_or_NULL(pd, FILENAME_PACKAGE);
        char *arch = problem_data_get_content_or_NULL(pd, FILENAME_ARCHITECTURE);
        map_string_t *osinfo = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
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
                char *msg = g_strdup_printf(_("The release '%s' is not supported by the"
                                              " Retrace server."), releaseid);
                libreport_alert(msg);
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
            int known = check_package(session, package, arch, osinfo, &msg);
            if (msg)
            {
                libreport_alert(msg);
                free(msg);
            }

            if (!known)
                error_msg_and_die(_("Unknown package sent to Retrace server."));
        }

        libreport_free_map_string(osinfo);
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
        char *question = g_strdup_printf(_("You are going to upload %s. "
                                           "Continue?"), human_size);

        int response = libreport_ask_yes_no(question);
        free(question);

        if (!response)
        {
            libreport_set_xfunc_error_retval(EXIT_CANCEL_BY_USER);
            error_msg_and_die(_("Cancelled by user"));
        }
    }

    g_autoptr(SoupURI) uri = NULL;
    g_autoptr(SoupMessage) message = NULL;
    g_autoptr(GMappedFile) file = NULL;
    char *contents;
    g_autofree char *task_type_string = NULL;
    g_autofree CreateProgressCallbackData *callback_data = NULL;
    guint response_code;
    g_autoptr(SoupBuffer) response = NULL;
    const char *header_value;

    uri = build_uri_from_config(&cfg, "create", NULL);
    message = soup_message_new_from_uri("POST", uri);
    file = g_mapped_file_new_from_fd(tempfd, FALSE, NULL);
    contents = g_mapped_file_get_contents(file);
    task_type_string = g_strdup_printf("%d", task_type);

    soup_message_set_request(message, "application/x-xz-compressed-tar",
                             SOUP_MEMORY_TEMPORARY, contents, file_stat.st_size);

    soup_message_headers_append(message->request_headers, "X-Task-Type", task_type_string);
    soup_message_headers_append(message->request_headers, "Accept-Charset", lang.charset);

    if (delay)
    {
        printf(_("Uploading %s\n"), human_size);
        fflush(stdout);

        callback_data = g_new0(CreateProgressCallbackData, 1);

        time(&callback_data->start);

        g_signal_connect(message, "wrote-body-data",
                         G_CALLBACK(on_wrote_body_data), &callback_data);
    }

    response_code = soup_session_send_message(session, message);

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(response_code))
    {
        alert_connection_error(cfg.uri);
        error_msg("%s", message->reason_phrase);
    }

    response = soup_message_body_flatten(message->response_body);

    close(tempfd);

    if (delay)
    {
        puts(_("Upload successful"));
        fflush(stdout);
    }

    if (http_show_headers)
    {
        soup_message_headers_foreach(message->response_headers, print_header, stderr);
    }

    if (response_code == 500 || response_code == 507)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die("%s", response->data);
    }
    else if (response_code == 403)
    {
        libreport_alert(_("Your problem directory is corrupted and can not "
                "be processed by the Retrace server."));
        error_msg_and_die(_("The archive contains malicious files (such as symlinks) "
                            "and thus can not be processed."));
    }
    else if (response_code != 201)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, response->data);
    }

    g_free(human_size);

    header_value = soup_message_headers_get_one(message->response_headers, "X-Task-Id");
    if (header_value == NULL)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Invalid response from server: missing X-Task-Id."));
    }

    *task_id = g_strdup(header_value);

    header_value = soup_message_headers_get_one(message->response_headers, "X-Task-Password");
    if (header_value == NULL)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Invalid response from server: missing X-Task-Password."));
    }

    *task_password = g_strdup(header_value);

    if (delay)
    {
        puts(_("Retrace job started"));
        fflush(stdout);
    }

    return 0;
}

static int run_create(SoupSession *session,
                      bool         delete_temp_archive)
{
    char *task_id, *task_password;
    int result = create(session, delete_temp_archive, &task_id, &task_password);
    if (0 != result)
        return result;
    printf(_("Task Id: %s\nTask Password: %s\n"), task_id, task_password);
    free(task_id);
    free(task_password);
    return 0;
}

/* Caller must free task_status and status_message */
static void status(SoupSession  *session,
                   const char   *task_id,
                   const char   *task_password,
                   char        **task_status,
                   char        **status_message)
{
    g_autoptr(SoupURI) uri = NULL;
    g_autoptr(SoupMessage) message = NULL;
    guint response_code;
    g_autoptr(SoupBuffer) response = NULL;
    const char *task_status_header;

    uri = build_uri_from_config(&cfg, task_id, NULL);
    message = soup_message_new_from_uri("GET", uri);

    soup_message_headers_append(message->request_headers, "X-Task-Password", task_password);
    soup_message_headers_append(message->request_headers, "Accept-Charset", lang.charset);

    response_code = soup_session_send_message(session, message);

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(response_code))
    {
        alert_connection_error(cfg.uri);
        error_msg_and_die("%s", message->reason_phrase);
    }

    response = soup_message_body_flatten(message->response_body);

    if (http_show_headers)
    {
        soup_message_headers_foreach(message->response_headers, print_header, stderr);
    }

    if (response_code != 200)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, response->data);
    }

    task_status_header = soup_message_headers_get_one(message->response_headers, "X-Task-Status");
    if (task_status_header == NULL)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Invalid response from server: missing X-Task-Status."));
    }
    *task_status = g_strdup(task_status_header);
    *status_message = g_strdup(response->data);
}

static void run_status(SoupSession *session,
                       const char  *task_id,
                       const char  *task_password)
{
    char *task_status;
    char *status_message;
    status(session, task_id, task_password, &task_status, &status_message);
    printf(_("Task Status: %s\n%s\n"), task_status, status_message);
    free(task_status);
    free(status_message);
}

/* Caller must free backtrace */
static void backtrace(SoupSession  *session,
                      const char   *task_id,
                      const char   *task_password,
                      char        **backtrace)
{
    g_autoptr(SoupURI) uri = NULL;
    g_autoptr(SoupMessage) message = NULL;
    guint response_code;
    g_autoptr(SoupBuffer) response = NULL;

    uri = build_uri_from_config(&cfg, task_id, "backtrace", NULL);
    message = soup_message_new_from_uri("GET", uri);

    soup_message_headers_append(message->request_headers, "X-Task-Password", task_password);
    soup_message_headers_append(message->request_headers, "Accept-Charset", lang.charset);

    response_code = soup_session_send_message(session, message);

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(response_code))
    {
        alert_connection_error(cfg.uri);
        error_msg_and_die("%s", message->reason_phrase);
    }

    response = soup_message_body_flatten(message->response_body);

    if (http_show_headers)
    {
        soup_message_headers_foreach(message->response_headers, print_header, stderr);
    }

    if (response_code != 200)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, response->data);
    }

    *backtrace = g_strdup(response->data);
}
static void run_backtrace(SoupSession *session,
                          const char  *task_id,
                          const char  *task_password)
{
    char *backtrace_text;
    backtrace(session, task_id, task_password, &backtrace_text);
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
static void exploitable(SoupSession  *session,
                        const char   *task_id,
                        const char   *task_password,
                        char        **exploitable_text)
{
    g_autoptr(SoupURI) uri = NULL;
    g_autoptr(SoupMessage) message = NULL;
    guint response_code;
    g_autoptr(SoupBuffer) response = NULL;

    uri = build_uri_from_config(&cfg, task_id, "exploitable", NULL);
    message = soup_message_new_from_uri("GET", uri);

    soup_message_headers_append(message->request_headers, "X-Task-Password", task_password);
    soup_message_headers_append(message->request_headers, "Accept-Charset", lang.charset);

    response_code = soup_session_send_message(session, message);

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(response_code))
    {
        alert_connection_error(cfg.uri);
        error_msg_and_die("%s", message->reason_phrase);
    }

    response = soup_message_body_flatten(message->response_body);

    if (http_show_headers)
    {
        soup_message_headers_foreach(message->response_headers, print_header, stderr);
    }

    if (response_code == 404)
    {
        *exploitable_text = NULL;
    }
    else if (response_code == 200)
    {
        *exploitable_text = g_strdup(response->data);
    }
    else
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, response->data);
    }
}

static void run_exploitable(SoupSession *session,
                            const char  *task_id,
                            const char  *task_password)
{
    char *exploitable_text;
    exploitable(session, task_id, task_password, &exploitable_text);
    if (exploitable_text)
    {
        printf("%s\n", exploitable_text);
        free(exploitable_text);
    }
    else
        puts("No exploitability information available.");
}

static void run_log(SoupSession *session,
                    const char  *task_id,
                    const char  *task_password)
{
    g_autoptr(SoupURI) uri = NULL;
    g_autoptr(SoupMessage) message = NULL;
    guint response_code;
    g_autoptr(SoupBuffer) response = NULL;

    uri = build_uri_from_config(&cfg, task_id, "log", NULL);
    message = soup_message_new_from_uri("GET", uri);

    soup_message_headers_append(message->request_headers, "X-Task-Password", task_password);
    soup_message_headers_append(message->request_headers, "Accept-Charset", lang.charset);

    response_code = soup_session_send_message(session, message);

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(response_code))
    {
        alert_connection_error(cfg.uri);
        error_msg_and_die("%s", message->reason_phrase);
    }

    response = soup_message_body_flatten(message->response_body);

    if (http_show_headers)
    {
        soup_message_headers_foreach(message->response_headers, print_header, stderr);
    }

    if (response_code != 200)
    {
        alert_server_error(cfg.uri);
        error_msg_and_die(_("Unexpected HTTP response from server: %d\n%s"),
                          response_code, response->data);
    }

    puts(response->data);
}

static int run_batch(SoupSession *session,
                     bool         delete_temp_archive)
{
    char *task_id, *task_password;
    int retcode = create(session, delete_temp_archive, &task_id, &task_password);
    if (0 != retcode)
        return retcode;
    char *task_status = g_strdup("");
    char *status_message = g_strdup("");
    int status_delay = delay ? delay : 10;
    int dots = 0;
    while (0 != strncmp(task_status, "FINISHED", strlen("finished")))
    {
        char *previous_status_message = status_message;
        free(task_status);
        sleep(status_delay);
        status(session, task_id, task_password, &task_status, &status_message);
        if (libreport_g_verbose > 0 || 0 != strcmp(previous_status_message, status_message))
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
            libreport_client_log(".");
            fflush(stdout);
        }
        free(previous_status_message);
        previous_status_message = status_message;
    }
    if (0 == strcmp(task_status, "FINISHED_SUCCESS"))
    {
        char *backtrace_text;
        backtrace(session, task_id, task_password, &backtrace_text);
        char *exploitable_text = NULL;
        if (task_type == TASK_RETRACE)
        {
            exploitable(session, task_id, task_password, &exploitable_text);
            if (!exploitable_text)
                log_notice("No exploitable data available");
        }

        if (dump_dir_name)
        {
            struct dump_dir *dd = dd_opendir(dump_dir_name, 0/* flags */);
            if (!dd)
            {
                free(backtrace_text);
                libreport_xfunc_die();
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
        libreport_alert(_("Retrace failed. Try again later and if the problem persists "
                "report this issue please."));
        run_log(session, task_id, task_password);
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
        OPT_uri       = 1 << 4,
        OPT_headers   = 1 << 5,
        OPT_group_1   = 1 << 6,
        OPT_dir       = 1 << 7,
        OPT_core      = 1 << 8,
        OPT_delay     = 1 << 9,
        OPT_no_unlink = 1 << 10,
        OPT_group_2   = 1 << 11,
        OPT_task      = 1 << 12,
        OPT_password  = 1 << 13
    };

    /* Keep enum above and order of options below in sync! */
    struct options options[] = {
        OPT__VERBOSE(&libreport_g_verbose),
        OPT_BOOL('s', "syslog", NULL, _("log to syslog")),
        OPT_BOOL('k', "insecure", NULL,
                 _("allow insecure connection to retrace server")),
        OPT_BOOL(0, "no-pkgcheck", NULL,
                 _("do not check whether retrace server is able to "
                   "process given package before uploading the archive")),
        OPT_STRING(0, "uri", &(cfg.uri), "URI",
                   _("retrace server URI")),
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

    char *env_uri = getenv("RETRACE_SERVER_URI");
    if (env_uri)
        cfg.uri = env_uri;

    char *env_delay = getenv("ABRT_STATUS_DELAY");
    if (env_delay)
    {
        char *endptr;
        long dly = g_ascii_strtoull(env_delay, &endptr, 10);
        if (dly >= 0 && dly <= UINT_MAX && env_delay != endptr)
            delay = (unsigned)dly;
        else
            error_msg_and_die("expected number in range <%d, %d>: '%s'", 0, UINT_MAX, env_delay);
    }

    char *env_insecure = getenv("RETRACE_SERVER_INSECURE");
    if (env_insecure)
        cfg.ssl_allow_insecure = strncmp(env_insecure, "insecure", strlen("insecure")) == 0;

    unsigned opts = libreport_parse_opts(argc, argv, options, usage);
    if (opts & OPT_syslog)
    {
        libreport_logmode = LOGMODE_JOURNAL;
    }
    const char *operation = NULL;
    if (optind < argc)
        operation = argv[optind];
    else
        libreport_show_usage_and_die(usage, options);

    if (!cfg.ssl_allow_insecure)
        cfg.ssl_allow_insecure = opts & OPT_insecure;
    http_show_headers = opts & OPT_headers;
    no_pkgcheck = opts & OPT_no_pkgchk;

    g_autoptr(SoupSession) session = NULL;

    session = soup_session_new_with_options(SOUP_SESSION_SSL_STRICT, !cfg.ssl_allow_insecure,
                                            SOUP_SESSION_ACCEPT_LANGUAGE_AUTO, TRUE,
                                            NULL);

    /* Run the desired operation. */
    int result = 0;
    if (0 == strcasecmp(operation, "create"))
    {
        if (!dump_dir_name && !coredump)
            error_msg_and_die(_("Either problem directory or coredump is needed."));
        result = run_create(session, 0 == (opts & OPT_no_unlink));
    }
    else if (0 == strcasecmp(operation, "batch"))
    {
        if (!dump_dir_name && !coredump)
            error_msg_and_die(_("Either problem directory or coredump is needed."));
        result = run_batch(session, 0 == (opts & OPT_no_unlink));
    }
    else if (0 == strcasecmp(operation, "status"))
    {
        if (!task_id)
            error_msg_and_die(_("Task id is needed."));
        if (!task_password)
            error_msg_and_die(_("Task password is needed."));
        run_status(session, task_id, task_password);
    }
    else if (0 == strcasecmp(operation, "backtrace"))
    {
        if (!task_id)
            error_msg_and_die(_("Task id is needed."));
        if (!task_password)
            error_msg_and_die(_("Task password is needed."));
        run_backtrace(session, task_id, task_password);
    }
    else if (0 == strcasecmp(operation, "log"))
    {
        if (!task_id)
            error_msg_and_die(_("Task id is needed."));
        if (!task_password)
            error_msg_and_die(_("Task password is needed."));
        run_log(session, task_id, task_password);
    }
    else if (0 == strcasecmp(operation, "exploitable"))
    {
        if (!task_id)
            error_msg_and_die(_("Task id is needed."));
        if (!task_password)
            error_msg_and_die(_("Task password is needed."));
        run_exploitable(session, task_id, task_password);
    }
    else
        error_msg_and_die(_("Unknown operation: %s."), operation);

    return result;
}
