/*
  Copyright (C) 2010  ABRT team

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
#include <glib.h>
#include <sys/un.h>
#include "abrtlib.h"
#include "dumpsocket.h"
#include "debug_dump.h"
#include "crash_types.h"
#include "abrt_exception.h"
#include "hooklib.h"

#define SOCKET_FILE VAR_RUN"/abrt/abrt.socket"
#define SOCKET_PERMISSION 0666

/* Maximal length of backtrace. */
#define MAX_BACKTRACE_SIZE (1024*1024)

/* Amount of data received from one client for a message before reporting error. */
#define MAX_MESSAGE_SIZE (4*MAX_BACKTRACE_SIZE)

/* Maximum number of simultaneously opened client connections. */
#define MAX_CLIENT_COUNT 10

/* Interval between checks of client halt, in seconds. */
#define CLIENT_CHECK_INTERVAL 10

/* Interval with no data received from client, after which the client is
   considered halted, in seconds. */
#define CLIENT_HALT_INTERVAL 10

/* Maximal number of characters read from socket at once. */
#define INPUT_BUFFER_SIZE 1024

static GIOChannel *channel = NULL;
static guint channel_cb_id = 0;
static int client_count = 0;

/* Information about single socket session. */
struct client
{
    /* Client user id */
    uid_t uid;
    /* Buffer for incomplete incoming messages. */
    GByteArray *messagebuf;
    /* Executable. */
    char *executable;
    /* Process ID. */
    int pid;
    char *backtrace;
    /* Python, Ruby etc. */
    char *analyzer;
    /* Directory base name: python (or pyhook), ruby etc. */
    char *basename;
    /* Crash reason.
     * Python example: "CCMainWindow.py:1:<module>:ZeroDivisionError:
     *                 integer division or modulo by zero"
     */
    char *reason;
    /* Last time some data were received over the socket
     * from the client.
     */
    time_t lastwrite;
    /* Timer checking client halt. */
    guint timer_id;
    /* Client socket callback id. */
    guint socket_id;
    /* Client socket channel */
    GIOChannel *channel;
};

static gboolean server_socket_cb(GIOChannel *source,
                                 GIOCondition condition,
                                 gpointer data);

/* Releases all memory that belongs to a client session. */
static void client_free(struct client *client)
{
    /* Delete the uncompleted message if there is some. */
    g_byte_array_free(client->messagebuf, TRUE);
    free(client->executable);
    free(client->backtrace);
    free(client->analyzer);
    free(client->basename);
    free(client->reason);
    g_source_remove(client->timer_id);
    g_source_remove(client->socket_id);
    g_io_channel_unref(client->channel);
    free(client);
    --client_count;
    if (!channel_cb_id)
    {
        channel_cb_id = g_io_add_watch(channel,
                                       (GIOCondition)(G_IO_IN | G_IO_PRI),
                                       (GIOFunc)server_socket_cb,
                                       NULL);
        if (!channel_cb_id)
            perror_msg_and_die("dumpsocket: Can't add socket watch");
    }
}

/* Callback called by glib main loop at regular intervals when
   some client is connected. */
static gboolean client_check_cb(gpointer data)
{
    struct client *client = (struct client*)data;
    if (time(NULL) - client->lastwrite > CLIENT_HALT_INTERVAL)
    {
        log("dumpsocket: client socket timeout reached, closing connection");
        client_free(client);
        return FALSE;
    }
    return TRUE;
}

/* Caller is responsible to free() the returned value. */
static char *giocondition_to_string(GIOCondition condition)
{
    struct strbuf *strbuf = strbuf_new();
    if (condition & G_IO_HUP)
        strbuf_append_str(strbuf, "G_IO_HUP | ");
    if (condition & G_IO_ERR)
        strbuf_append_str(strbuf, "G_IO_ERR | ");
    if (condition & G_IO_NVAL)
        strbuf_append_str(strbuf, "G_IO_NVAL | ");
    if (condition & G_IO_IN)
        strbuf_append_str(strbuf, "G_IO_IN | ");
    if (condition & G_IO_OUT)
        strbuf_append_str(strbuf, "G_IO_OUT | ");
    if (condition & G_IO_PRI)
        strbuf_append_str(strbuf, "G_IO_PRI | ");
    if (strbuf->len == 0)
        strbuf_append_str(strbuf, "none");
    else
    {
        /* remove the last " | " */
        strbuf->len -= 3;
        strbuf->buf[strbuf->len] = '\0';
    }
    char *result = strbuf->buf;
    strbuf_free_nobuf(strbuf);
    return result;
}

/* Create a new debug dump from client session.
 * Caller must ensure that all fields in struct client
 * are properly filled.
 */
static void create_debug_dump(struct client *client)
{
    /* Create temp directory with the debug dump.
       This directory is renamed to final directory name after
       all files have been stored into it.
    */
    char *path = xasprintf(DEBUG_DUMPS_DIR"/%s-%ld-%u.new",
                           client->basename,
                           (long)time(NULL),
                           client->pid);
    /* No need to check the path length, as all variables used are limited, and dd.Create()
       fails if the path is too long. */

    CDebugDump dd;
    if (!dd.Create(path, client->uid))
    {
        dd.Delete();
        dd.Close();
        error_msg_and_die("dumpsocket: Error while creating crash dump %s", path);
    }

    dd.SaveText(FILENAME_ANALYZER, client->analyzer);
    dd.SaveText(FILENAME_EXECUTABLE, client->executable);
    dd.SaveText(FILENAME_BACKTRACE, client->backtrace);
    dd.SaveText(FILENAME_REASON, client->reason);

    /* Obtain and save the command line. */
    char *cmdline = get_cmdline(client->pid); // never NULL
    dd.SaveText(FILENAME_CMDLINE, cmdline);
    free(cmdline);

    /* Store id of the user whose application crashed. */
    char uid_str[sizeof(long) * 3 + 2];
    sprintf(uid_str, "%lu", (long)client->uid);
    dd.SaveText(CD_UID, uid_str);

    dd.Close();

    /* Move the completely created debug dump to
       final directory. */
    char *newpath = xstrndup(path, strlen(path) - strlen(".new"));
    if (rename(path, newpath) == 0)
        strcpy(path, newpath);
    free(newpath);

    log("dumpsocket: Saved %s crash dump of pid %u to %s",
        client->analyzer, client->pid, path);

    /* Handle free space checking. */
    unsigned maxCrashReportsSize = 0;
    parse_conf(NULL, &maxCrashReportsSize, NULL, NULL);
    if (maxCrashReportsSize > 0)
    {
        check_free_space(maxCrashReportsSize);
        trim_debug_dumps(maxCrashReportsSize, path);
    }

    free(path);
}

/* Checks if a string contains only printable characters. */
static bool printable_str(const char *str)
{
    do {
        if ((unsigned char)(*str) < ' ' || *str == 0x7f)
            return false;
        str++;
    } while (*str);
    return true;
}

/* Checks if a string has certain prefix. */
static bool starts_with(const char *str, const char *start)
{
    return strncmp(str, start, strlen(start)) == 0;
}

/* @returns
 *  Caller is responsible to call free() on the returned
 *  pointer.
 *  If NULL is returned, string extraction failed.
 */
static char *try_to_get_string(const char *message,
                               const char *tag,
                               size_t max_len,
                               bool printable,
                               bool allow_slashes)
{
    if (!starts_with(message, tag))
        return NULL;

    const char *contents = message + strlen(tag);
    if ((printable && !printable_str(contents)) ||
        (!allow_slashes && strchr(contents, '/')))
    {
        error_msg("dumpsocket: Received %s contains invalid characters -> skipping", tag);
        return NULL;
    }

    if (strlen(contents) > max_len)
    {
        char *max_len_str = g_format_size_for_display(max_len);
        error_msg("dumpsocket: Received %s too long -> trimming to %s", tag, max_len_str);
        g_free(max_len_str);
    }

    return xstrndup(contents, max_len);
}

/* Handles a message received from client over socket. */
static void process_message(struct client *client, const char *message)
{
/* @param tag
 *  The message identifier. Message starting with it
 *  is handled by this macro.
 * @param field
 *  Member in struct client, which should be filled by
 *  the field contents.
 * @param max_len
 *  Maximum length of the field in bytes.
 *  Exceeding bytes are trimmed.
 * @param printable
 *  Whether to limit the field contents to ASCII only.
 * @param allow_slashes
 *  Whether to allow slashes to be a part of input.
 */
#define HANDLE_INCOMING_STRING(tag, field, max_len, printable, allow_slashes) \
    char *field = try_to_get_string(message, tag, max_len, printable, allow_slashes); \
    if (field) \
    { \
        free(client->field); \
        client->field = field; \
        return; \
    }

    HANDLE_INCOMING_STRING("EXECUTABLE=", executable, PATH_MAX, true, true);
    HANDLE_INCOMING_STRING("BACKTRACE=", backtrace, MAX_BACKTRACE_SIZE, false, true);
    HANDLE_INCOMING_STRING("BASENAME=", basename, 100, true, false);
    HANDLE_INCOMING_STRING("ANALYZER=", analyzer, 100, true, true);
    HANDLE_INCOMING_STRING("REASON=", reason, 512, false, true);

#undef HANDLE_INCOMING_STRING

    /* PID is not handled as a string, we convert it to pid_t. */
    if (starts_with(message, "PID="))
    {
        /* xatou() cannot be used here, because it would
         * kill whole daemon by non-numeric string.
         */
        char *endptr;
        errno = 0;
        const char *nptr = message + strlen("PID=");
        unsigned long number = strtoul(nptr, &endptr, 10);
        /* pid == 0 is error, the lowest PID is 1. */
        if (errno || nptr == endptr || *endptr != '\0' || number > UINT_MAX || number == 0)
        {
            error_msg("dumpsocket: invalid PID received -> ignoring");
            return;
        }
        client->pid = number;
        return;
    }

    /* Creates debug dump if all fields were already provided. */
    if (starts_with(message, "DONE"))
    {
        if (client->pid == 0 ||
            client->backtrace == NULL ||
            client->executable == NULL ||
            client->analyzer == NULL ||
            client->basename == NULL ||
            client->reason == NULL)
        {
            error_msg("dumpsocket: DONE received, but some data are missing -> ignoring");
            return;
        }

        create_debug_dump(client);
        return;
    }
}

/* Callback called by glib main loop when ABRT receives data that have
 * been written to the socket by some client.
 */
static gboolean client_socket_cb(GIOChannel *source,
                                 GIOCondition condition,
                                 gpointer data)
{
    struct client *client = (struct client*)data;

    /* Detailed logging, useful for debugging. */
    if (g_verbose >= 3)
    {
        char *cond = giocondition_to_string(condition);
        log("dumpsocket: client condition %s", cond);
        free(cond);
    }

    /* Handle incoming data. */
    if (condition & (G_IO_IN | G_IO_PRI))
    {
        guint loop = client->messagebuf->len;
        gsize len;
        gchar buf[INPUT_BUFFER_SIZE];
        GError *err = NULL;
        /* Read data in chunks of size INPUT_BUFFER_SIZE. This allows to limit the number of
           bytes received (to prevent memory exhaustion). */
        do {
            GIOStatus result = g_io_channel_read_chars(source, buf, INPUT_BUFFER_SIZE, &len, &err);
            if (result == G_IO_STATUS_ERROR)
            {
                g_assert(err);
                error_msg("dumpsocket: Error while reading data from client socket: %s", err->message);
                g_error_free(err);
                client_free(client);
                return FALSE;
            }

            if (g_verbose >= 3)
                log("dumpsocket: received %zu bytes of data", len);

            /* Append the incoming data to the message buffer. */
            g_byte_array_append(client->messagebuf, (guint8*)buf, len);

            if (client->messagebuf->len > MAX_MESSAGE_SIZE)
            {
                error_msg("dumpsocket: Message too long.");
                client_free(client);
                return FALSE;
            }
        } while (len > 0);

        /* Check, if we received a complete message now. */
        for (; loop < client->messagebuf->len; ++loop)
        {
            if (client->messagebuf->data[loop] != '\0')
                continue;

            VERB3 log("dumpsocket: Processing message: %s",
                      client->messagebuf->data);

            /* Process the message. */
            process_message(client, (char*)client->messagebuf->data);
            /* Remove the message including the ending \0 */
            g_byte_array_remove_range(client->messagebuf, 0, loop + 1);
            loop = 0;
        }

        /* Update the last write access time */
        client->lastwrite = time(NULL);
    }

    /* Handle socket disconnection.
       It is important to do it after handling G_IO_IN, because sometimes
       G_IO_HUP comes together with G_IO_IN. It means that some data arrived
       and then the socket has been closed.
     */
    if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL))
    {
        log("dumpsocket: Socket client disconnected");
        client_free(client);
        return FALSE;
    }

    return TRUE;
}

/* If the status indicates failure, report it. */
static void check_status(GIOStatus status, GError *err, const char *operation)
{
    if (status == G_IO_STATUS_NORMAL)
        return;

    if (err)
    {
        error_msg("dumpsocket: Error while %s: %s", operation, err->message);
        g_error_free(err);
    }
    else
        error_msg("dumpsocket: Error while %s", operation);
}

/* Initializes a new client session data structure. */
static struct client *client_new(int socket)
{
    struct client *client = (struct client*)xzalloc(sizeof(struct client));

    /* Get credentials for the socket client. */
    struct ucred cr;
    socklen_t crlen = sizeof(struct ucred);
    if (0 != getsockopt(socket, SOL_SOCKET, SO_PEERCRED, &cr, &crlen))
        perror_msg_and_die("dumpsocket: Failed to get client uid");
    if (crlen != sizeof(struct ucred))
        perror_msg_and_die("dumpsocket: Failed to get client uid (crlen)");
    client->uid = cr.uid;

    client->messagebuf = g_byte_array_new();
    client->lastwrite = time(NULL);

    close_on_exec_on(socket);

    /* Create client IO channel. */
    client->channel = g_io_channel_unix_new(socket);
    g_io_channel_set_close_on_unref(client->channel, TRUE);

    /* Set nonblocking access. */
    GError *err = NULL;
    GIOStatus status = g_io_channel_set_flags(client->channel, G_IO_FLAG_NONBLOCK, &err);
    check_status(status, err, "setting NONBLOCK flag");

    /* Disable channel encoding to protect binary data. */
    err = NULL;
    status = g_io_channel_set_encoding(client->channel, NULL, &err);
    check_status(status, err, "setting encoding");

    /* Start timer to check the client problems. */
    client->timer_id = g_timeout_add_seconds(CLIENT_CHECK_INTERVAL, client_check_cb, client);
    if (!client->timer_id)
        error_msg_and_die("dumpsocket: Can't add client timer");

    /* Register client callback. */
    client->socket_id = g_io_add_watch(client->channel,
                                       (GIOCondition)(G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL),
                                       (GIOFunc)client_socket_cb,
                                       client);
    if (!client->socket_id)
        error_msg_and_die("dumpsocket: Can't add client socket watch");

    ++client_count;
    return client;
}

/* Callback called by glib main loop when a client newly opens ABRT's socket. */
static gboolean server_socket_cb(GIOChannel *source,
                                 GIOCondition condition,
                                 gpointer data)
{
    /* Check the limit for number of simultaneously attached clients. */
    if (client_count >= MAX_CLIENT_COUNT)
    {
        error_msg("dumpsocket: Too many clients, refusing connection.");
        /* To avoid infinite loop caused by the descriptor in "ready" state,
           the callback must be disabled.
           It is added back in client_free(). */
        g_source_remove(channel_cb_id);
        channel_cb_id = 0;
        return TRUE;
    }

    struct sockaddr_un remote;
    socklen_t len = sizeof(remote);
    int socket = accept(g_io_channel_unix_get_fd(source),
                        (struct sockaddr*)&remote, &len);
    if (socket == -1)
    {
        perror_msg("dumpsocket: Server can not accept client");
        return TRUE;
    }

    log("dumpsocket: New client connected");
    client_new(socket);
    return TRUE;
}

/* Initializes the dump socket, usually in /var/run directory
 * (the path depends on compile-time configuration).
 */
void dumpsocket_init()
{
    struct sockaddr_un local;
    unlink(SOCKET_FILE); /* not caring about the result */
    int socketfd = xsocket(AF_UNIX, SOCK_STREAM, 0);
    close_on_exec_on(socketfd);
    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCKET_FILE);
    xbind(socketfd, (struct sockaddr*)&local, sizeof(local));
    xlisten(socketfd, MAX_CLIENT_COUNT);

    if (chmod(SOCKET_FILE, SOCKET_PERMISSION) != 0)
        perror_msg_and_die("dumpsocket: failed to chmod socket file");

    channel = g_io_channel_unix_new(socketfd);
    g_io_channel_set_close_on_unref(channel, TRUE);
    channel_cb_id = g_io_add_watch(channel,
                                   (GIOCondition)(G_IO_IN | G_IO_PRI),
                                   (GIOFunc)server_socket_cb,
                                   NULL);
    if (!channel_cb_id)
        perror_msg_and_die("dumpsocket: Can't add socket watch");
}

/* Releases all resources used by dumpsocket. */
void dumpsocket_shutdown()
{
    /* Set everything to pre-initialization state. */
    if (channel)
    {
        /* This one is for g_io_add_watch. */
        if (channel_cb_id)
            g_source_remove(channel_cb_id);
        /* This one is for g_io_channel_unix_new. */
        g_io_channel_unref(channel);
        channel = NULL;
    }
}
