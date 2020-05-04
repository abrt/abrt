/*
    Copyright (C) 2013  RedHat inc.

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

#include "abrt-inotify.h"
#include "abrt_glib.h"
#include "libabrt.h"

#include <stdio.h>
#include <sys/ioctl.h> /* ioctl(FIONREAD) */

struct abrt_inotify_watch
{
    abrt_inotify_watch_handler handler;
    void *user_data;
    int inotify_fd;
    int inotify_wd;
    GIOChannel *channel_inotify;
    guint channel_inotify_source_id;
};

/* Inotify handler */

static gboolean handle_inotify_cb(GIOChannel *gio, GIOCondition condition, gpointer user_data)
{
    /* Default size: 128 simultaneous actions (about 1/2 meg) */
#define INOTIFY_BUF_SIZE ((sizeof(struct inotify_event) + FILENAME_MAX)*128)
    /* Determine how much to read (it usually is much smaller) */
    /* NB: this variable _must_ be int-sized, ioctl expects that! */
    int inotify_bytes = INOTIFY_BUF_SIZE;
    if (ioctl(g_io_channel_unix_get_fd(gio), FIONREAD, &inotify_bytes) != 0
    /*|| inotify_bytes < sizeof(struct inotify_event)
         ^^^^^^^^^^^^^^^^^^^ - WRONG: legitimate 0 was seen when flooded with inotify events
    */
     || inotify_bytes > INOTIFY_BUF_SIZE
    ) {
        inotify_bytes = INOTIFY_BUF_SIZE;
    }
    log_debug("FIONREAD:%d", inotify_bytes);

    if (inotify_bytes == 0)
        return TRUE; /* "please don't remove this event" */

    /* We may race: more inotify events may happen after ioctl(FIONREAD).
     * To be more efficient, allocate a bit more space to eat those events too.
     * This also would help against a bug we once had where
     * g_io_channel_read_chars() was buffering reads
     * and we were going out of sync wrt struct inotify_event's layout.
     */
    inotify_bytes += 2 * (sizeof(struct inotify_event) + FILENAME_MAX);
    char *buf = g_malloc(inotify_bytes);
    errno = 0;
    gsize len;
    GError *gerror = NULL;
    /* Note: we ensured elsewhere that this read is non-blocking, making it ok
     * for buffer len (inotify_bytes) to be larger than actual available byte count.
     */
    GIOStatus err = g_io_channel_read_chars(gio, buf, inotify_bytes, &len, &gerror);
    if (err != G_IO_STATUS_NORMAL)
    {
        error_msg("Error reading inotify fd: %s", gerror ? gerror->message : "unknown");
        free(buf);
        if (gerror)
            g_error_free(gerror);
        return FALSE; /* "remove this event" (huh??) */
    }

    struct abrt_inotify_watch *aic = (struct abrt_inotify_watch *)user_data;
    /* Reconstruct each event */
    gsize i = 0;
    for (;;)
    {
        if (i >= len)
        {
            /* This would catch one of our former bugs. Let's be paranoid */
            if (i > len)
                error_msg("warning: ran off struct inotify (this should never happen): %u > %u", (int)i, (int)len);
            break;
        }
        struct inotify_event *event = (struct inotify_event *) &buf[i];
        i += sizeof(*event) + event->len;

        aic->handler(aic, event, aic->user_data);
    }
    free(buf);
    return TRUE;
}

struct abrt_inotify_watch *
abrt_inotify_watch_init(const char *path, int inotify_flags, abrt_inotify_watch_handler handler, void *user_data)
{
    struct abrt_inotify_watch *aiw = g_new(struct abrt_inotify_watch, 1);
    aiw->handler = handler;
    aiw->user_data = user_data;

    log_notice("Initializing inotify");
    errno = 0;
    aiw->inotify_fd = inotify_init();
    if (aiw->inotify_fd == -1)
        perror_msg_and_die("inotify_init failed");
    libreport_close_on_exec_on(aiw->inotify_fd);

    aiw->inotify_wd = inotify_add_watch(aiw->inotify_fd, path, inotify_flags);
    if (aiw->inotify_wd < 0)
        perror_msg_and_die("inotify_add_watch failed on '%s'", path);

    log_notice("Adding inotify watch to glib main loop");
    /* Without nonblocking mode, users observed abrtd blocking
     * on inotify read forever. Must set fd to non-blocking:
     */
    libreport_ndelay_on(aiw->inotify_fd);
    aiw->channel_inotify = abrt_gio_channel_unix_new(aiw->inotify_fd);

    /*
     * glib's read buffering must be disabled, or else
     * FIONREAD-reported "available data" sizes and sizes of reads
     * can become inconsistent, and worse, buffering can split
     * struct inotify's (very bad!).
     */
    g_io_channel_set_buffered(aiw->channel_inotify, false);

    errno = 0;
    aiw->channel_inotify_source_id = g_io_add_watch(aiw->channel_inotify,
            G_IO_IN | G_IO_PRI | G_IO_HUP,
            handle_inotify_cb,
            aiw);
    if (!aiw->channel_inotify_source_id)
        error_msg_and_die("g_io_add_watch failed");

    return aiw;
}

void
abrt_inotify_watch_reset(struct abrt_inotify_watch *watch, const char *path, int inotify_flags)
{
    inotify_rm_watch(watch->inotify_fd, watch->inotify_wd);
    watch->inotify_wd = inotify_add_watch(watch->inotify_fd, path, inotify_flags);
    if (watch->inotify_wd < 0)
        error_msg_and_die("inotify_add_watch failed on '%s'", path);
}

void
abrt_inotify_watch_destroy(struct abrt_inotify_watch *watch)
{
    if (!watch)
        return;

    inotify_rm_watch(watch->inotify_fd, watch->inotify_wd);
    g_source_remove(watch->channel_inotify_source_id);

    GError *error = NULL;
    g_io_channel_shutdown(watch->channel_inotify, FALSE, &error);
    if (error)
    {
        log_notice("Can't shutdown inotify gio channel: '%s'", error->message);
        g_error_free(error);
    }

    g_io_channel_unref(watch->channel_inotify);
    free(watch);
}
