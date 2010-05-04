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
#include "dumpsocket.h"
#include "abrtlib.h"
#include <glib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "DebugDump.h"
#include "CrashTypes.h"
#include "ABRTException.h"
#include "hooklib.h"
#include "strbuf.h"

#define SOCKET_FILE VAR_RUN"/abrt.socket"
#define SOCKET_PERMISSION 0666

#define MAX_BACKTRACE_SIZE (1024*1024)

static int socketfd = -1;
static GIOChannel *channel = NULL;

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
};

/* Initializes a new client session data structure. */
static struct client *client_new(uid_t uid)
{
    struct client *client = (struct client*)xmalloc(sizeof(struct client));
    client->uid = uid;
    client->messagebuf = g_byte_array_new();
    client->executable = NULL;
    client->pid = 0;
    client->backtrace = NULL;
    client->analyzer = NULL;
    client->basename = NULL;
    client->reason = NULL;
    return client;
}

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
    free(client);
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
    char path[PATH_MAX];
    unsigned path_len = snprintf(path,
                                 sizeof(path),
                                 DEBUG_DUMPS_DIR"/%s-%ld-%u.new",
                                 client->basename,
                                 (long)time(NULL),
                                 client->pid);
    if (path_len >= sizeof(path))
    {
        error_msg("dumpsocket: Dump path too long -> ignoring dump");
        return;
    }

    CDebugDump dd;
    try {
        dd.Create(path, client->uid);
    } catch (CABRTException &e) {
        dd.Delete();
        dd.Close();
        error_msg_and_die("dumpsocket: Error while creating crash dump %s: %s", path, e.what());
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
    char *newpath = xstrndup(path, path_len - strlen(".new"));
    if (rename(path, newpath) != 0)
        strcpy(path, newpath);
    free(newpath);

    log("dumpsocket: Saved %s crash dump of pid %u to %s",
        client->analyzer, client->pid, path);

    /* Handle free space checking. */
    unsigned maxCrashReportsSize = 0;
    parse_conf(NULL, &maxCrashReportsSize, NULL);
    if (maxCrashReportsSize > 0)
    {
        check_free_space(maxCrashReportsSize);
        trim_debug_dumps(maxCrashReportsSize, path);
    }
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
 *  Caller is responsible to call free() to the returned
 *  pointer.
 *  If NULL is returned, string extraction failed.
 */
static char *try_to_get_string(const char *message, const char *tag,
                               size_t max_len, bool printable)
{
    if (!starts_with(message, tag))
        return NULL;

    const char *contents = message + strlen(tag);
    if (printable && !printable_str(contents))
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
    if (starts_with(message, "NEW"))
    {
        /* Clear all fields. */
        free(client->executable);
        client->executable = NULL;
        free(client->backtrace);
        client->backtrace = NULL;
        free(client->analyzer);
        client->analyzer = NULL;
        free(client->basename);
        client->basename = NULL;
        free(client->reason);
        client->reason = NULL;
        client->pid = 0;
        return;
    }

/* @param __tag
 *  The message identifier. Message starting with it
 *  is handled by this macro.
 * @param __field
 *  Member in struct client, which should be filled by
 *  the field contents.
 * @param __max_len
 *  Maximum length of the field in bytes.
 *  Exceeding bytes are trimmed.
 * @param __printable
 *  Whether to limit the field contents to ASCII only.
 */
#define HANDLE_INCOMING_STRING(__tag, __field, __max_len, __printable) \
    char *__field = try_to_get_string(message, __tag, __max_len,  __printable); \
    if (__field) \
    { \
        free(client->__field); \
        client->__field = __field; \
        return; \
    }

    HANDLE_INCOMING_STRING("EXECUTABLE=", executable, PATH_MAX, true);
    HANDLE_INCOMING_STRING("BACKTRACE=", backtrace, MAX_BACKTRACE_SIZE, false);
    HANDLE_INCOMING_STRING("ANALYZER=", analyzer, 100, true);
    HANDLE_INCOMING_STRING("BASENAME=", basename, 100, true);
    HANDLE_INCOMING_STRING("REASON=", reason, 512, false);

#undef HANDLE_INCOMING_STRING

    /* PID is not handled as a string, we convert it to pid_t. */
    if (starts_with(message, "PID="))
    {
        /* xatou() cannot be used here, because it would
         * kill whole daemon by non-numeric string.
         */
        char *endptr;
        int old_errno = errno;
        errno = 0;
        const char *nptr = message + strlen("PID=");
        unsigned long number = strtoul(nptr, &endptr, 10);
        /* pid == 0 is error, the lowest PID is 1. */
        if (errno || nptr == endptr || *endptr != '\0' || number > UINT_MAX || number == 0)
        {
            error_msg("dumpsocket: invalid PID received -> ignoring");
            return;
        }
        errno = old_errno;
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
        gsize len = 0;
        guint8 *buf = NULL;
        GError *err = NULL;
        GIOStatus result = g_io_channel_read_to_end(source, (gchar**)&buf,
                                                    &len, &err);
        if (result == G_IO_STATUS_ERROR)
        {
            g_assert(err);
            error_msg("dumpsocket: Error while reading data from client socket: %s", err->message);
            g_error_free(err);
            return FALSE;
        }

        if (!buf)
            return TRUE;

        guint loop = client->messagebuf->len;
        /* Append the incoming data to the message buffer and check, if we have
           a complete message now. */
        g_byte_array_append(client->messagebuf, buf, len);
        for (; loop < client->messagebuf->len; ++loop)
        {
            if (client->messagebuf->data[loop] != '\0')
                continue;

            VERB3 log("dumpsocket: Processing message: %s",
                      client->messagebuf->data);

            /* Process the complete message. */
            process_message(client, (char*)client->messagebuf->data);
            /* Remove the message including the ending \0 */
            g_byte_array_remove_range(client->messagebuf, 0, loop + 1);
            loop = 0;
        }
        g_free(buf);
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
        /* g_io_add_watch call incremented channel's reference count,
           so unref this one now. */
        g_io_channel_unref(source);
        return FALSE;
    }

    return TRUE;
}

/* Callback called by glib main loop when a client newly opens ABRT's socket. */
static gboolean server_socket_cb(GIOChannel *source,
                                 GIOCondition condition,
                                 gpointer data)
{
    int socket;
    struct sockaddr_un remote;
    socklen_t len = sizeof(remote);

    if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL))
    {
        error_msg("dumpsocket: Server socket error");
        return FALSE;
    }

    if ((socket = accept(socketfd, (struct sockaddr*)&remote, &len)) == -1)
    {
        error_msg("dumpsocket: Server can not accept client");
        return TRUE;
    }

    log("dumpsocket: New client connected");

    GIOChannel* gsocket = g_io_channel_unix_new(socket);

    /* Disable channel encoding to protect binary data. */
    GError *err = NULL;
    GIOStatus status = g_io_channel_set_encoding(gsocket, NULL, &err);
    if (status != G_IO_STATUS_NORMAL)
    {
        if (err)
        {
            error_msg("dumpsocket: Error while setting encoding: %s", err->message);
            g_error_free(err);
        }
        else
            error_msg("dumpsocket: Error while setting encoding");
    }

    /* Get credentials for the newly attached socket client. */
    struct ucred cr;
    socklen_t crlen = sizeof(struct ucred);
    if (0 != getsockopt(socket, SOL_SOCKET, SO_PEERCRED, &cr, &crlen))
        perror_msg_and_die("dumpsocket: Failed to get client uid");
    if (crlen != sizeof(struct ucred))
        perror_msg_and_die("dumpsocket: Failed to get client uid (crlen)");

    /* Register client callback.
     * Provide a messsage buffer to store incoming data there.
     */
    struct client *client = client_new(cr.uid);
    if (!g_io_add_watch(gsocket,
                        (GIOCondition)(G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL),
                        (GIOFunc)client_socket_cb,
                        client))
    {
        error_msg("dumpsocket: Can't add client socket watch");
        return TRUE;
    }
    return TRUE;
}

/* Set the FD_CLOEXEC flag of desc if value is nonzero,
   or clear the flag if value is 0.
   Return 0 on success, or -1 on error with errno set.
   Copied from GNU C Library Manual.
*/
static int set_cloexec_flag(int desc, int value)
{
    int oldflags = fcntl(desc, F_GETFD, 0);
    /* If reading the flags failed, return error indication now. */
    if (oldflags < 0)
        return oldflags;
    /* Set just the flag we want to set. */
    if (value != 0)
        oldflags |= FD_CLOEXEC;
    else
        oldflags &= ~FD_CLOEXEC;
    /* Store modified flag word in the descriptor. */
    return fcntl(desc, F_SETFD, oldflags);
}

/* Initializes the dump socket, usually in /var/run directory
 * (the path depends on compile-time configuration).
 */
void dumpsocket_init()
{
    struct sockaddr_un local;
    unlink(SOCKET_FILE);
    socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketfd == -1)
        perror_msg_and_die("dumpsocket: Can't create AF_UNIX socket");

    if (set_cloexec_flag(socketfd, 1) != 0)
        perror_msg_and_die("dumpsocket: Failed to set CLOEXEC flag.");

    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCKET_FILE);
    if (bind(socketfd, (struct sockaddr*)&local, sizeof(local)) == -1)
        perror_msg_and_die("dumpsocket: Can't bind AF_UNIX socket to '%s'", SOCKET_FILE);

    if (listen(socketfd, 10) != 0)
        perror_msg_and_die("dumpsocket: Can't listen on AF_UNIX socket");

    if (chmod(SOCKET_FILE, SOCKET_PERMISSION) != 0)
        perror_msg_and_die("dumpsocket: failed to chmod socket file");

    channel = g_io_channel_unix_new(socketfd);
    if (!g_io_add_watch(channel,
                        (GIOCondition)(G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL),
                        (GIOFunc)server_socket_cb,
                        NULL))
    {
        perror_msg_and_die("dumpsocket: Can't add socket watch");
    }
}

/* Releases all resources used by dumpsocket. */
void dumpsocket_shutdown()
{
    /* Set everything to pre-initialization state. */
    if (channel)
    {
        g_io_channel_unref(channel);
        channel = NULL;
    }
    if (socketfd != -1)
    {
        close(socketfd);
        socketfd = -1;
    }
}
