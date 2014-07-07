/*
 * Copyright (C) 2014  ABRT team
 * Copyright (C) 2014  RedHat Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <stdlib.h>
#include <abrt/libabrt.h>
#include <stdio.h>

#include "abrt-journal.h"

#include <systemd/sd-journal.h>


struct abrt_journal
{
    sd_journal *j;
};

int abrt_journal_new(abrt_journal_t **journal)
{
    sd_journal *j;
    const int r = sd_journal_open(&j, SD_JOURNAL_LOCAL_ONLY);
    if (r < 0)
    {
        log_notice("Failed to open journal: %s", strerror(-r));
        return r;
    }

    *journal = xzalloc(sizeof(**journal));
    (*journal)->j = j;

    return 0;
}

void abrt_journal_free(abrt_journal_t *journal)
{
    sd_journal_close(journal->j);
    journal->j = (void *)0xDEADBEAF;

    free(journal);
}

int abrt_journal_set_journal_filter(abrt_journal_t *journal, const char *const *journal_filter_list)
{
    const char *const *cursor = journal_filter_list;

    while (*cursor)
    {
        const int r = sd_journal_add_match(journal->j, *cursor, strlen(*cursor));
        if (r < 0)
        {
            log_notice("Failed to set journal filter: %s", strerror(-r));
            return r;
        }

        ++cursor;
    }

    return 0;
}

int abrt_journal_get_field(abrt_journal_t *journal, const char *field, const void **value, size_t *value_len)
{
    const int r = sd_journal_get_data(journal->j, field, value, value_len);
    if (r < 0)
    {
        log_notice("Failed to read '%s' field: %s", field, strerror(-r));
        return r;
    }

    return 0;
}

int abrt_journal_get_string_field(abrt_journal_t *journal, const char *field, const char **value)
{
    size_t value_len;
    const int r = abrt_journal_get_field(journal, field, (const void **)value, &value_len);
    if (r < 0)
    {
        return r;
    }

    const size_t pfx_len = strlen(field) + 1;
    if (value_len < pfx_len)
    {
        error_msg("Invalid data format from journal: field data are not prefixed with field name");
        return -EBADMSG;
    }

    *value += pfx_len;
    return 0;
}

int abrt_journal_get_log_line(abrt_journal_t *journal, const char **line)
{
    const int r = abrt_journal_get_string_field(journal, "MESSAGE", line);
    if (r < 0)
        log_notice("Cannot read journal data. Exiting");

    return r;
}

int abrt_journal_get_cursor(abrt_journal_t *journal, char **cursor)
{
    const int r = sd_journal_get_cursor(journal->j, cursor);

    if (r < 0)
    {
        log_notice("Could not get journal cursor: '%s'", strerror(-r));
        return r;
    }

    return 0;
}

int abrt_journal_set_cursor(abrt_journal_t *journal, const char *cursor)
{
    const int r = sd_journal_seek_cursor(journal->j, cursor);
    if (r < 0)
    {
        log_notice("Failed to seek journal to cursor '%s': %s\n", cursor, strerror(-r));
        return r;
    }

    return 0;
}

int abrt_journal_seek_tail(abrt_journal_t *journal)
{
    const int r = sd_journal_seek_tail(journal->j);
    if (r < 0)
    {
        log_notice("Failed to seek journal to the end: %s\n", strerror(-r));
        return r;
    }

    /* BUG: https://bugzilla.redhat.com/show_bug.cgi?id=979487 */
    sd_journal_previous_skip(journal->j, 1);
    return 0;
}

int abrt_journal_next(abrt_journal_t *journal)
{
    const int r = sd_journal_next(journal->j);
    if (r < 0)
        log_notice("Failed to iterate to next entry: %s", strerror(-r));
    return r;
}

/*
 * ABRT systemd-journal wrapper end
 */

static volatile int s_loop_terminated;
void signal_loop_to_terminate(int signum)
{
    signum = signum;
    s_loop_terminated = 1;
}

enum abrt_journal_watch_state
{
    ABRT_JOURNAL_WATCH_READY,
    ABRT_JOURNAL_WATCH_STOPPED,
};

struct abrt_journal_watch
{
    abrt_journal_t *j;
    int state;

    abrt_journal_watch_callback callback;
    void *callback_data;
};

int abrt_journal_watch_new(abrt_journal_watch_t **watch, abrt_journal_t *journal, abrt_journal_watch_callback callback, void *callback_data)
{
    assert(callback != NULL || !"ABRT watch needs valid callback ptr");

    *watch = xzalloc(sizeof(**watch));
    (*watch)->j = journal;
    (*watch)->callback = callback;
    (*watch)->callback_data = callback_data;

    return 0;
}

void abrt_journal_watch_free(abrt_journal_watch_t *watch)
{
    watch->j = (void *)0xDEADBEAF;
    free(watch);
}

abrt_journal_t *abrt_journal_watch_get_journal(abrt_journal_watch_t *watch)
{
    return watch->j;
}

int abrt_journal_watch_run_sync(abrt_journal_watch_t *watch)
{
    sigset_t mask;
    sigfillset(&mask);

    /* Exit gracefully: */
    /* services usually exit on SIGTERM and SIGHUP */
    sigdelset(&mask, SIGTERM);
    signal(SIGTERM, signal_loop_to_terminate);
    sigdelset(&mask, SIGHUP);
    signal(SIGHUP, signal_loop_to_terminate);
    /* Ctrl-C for easier debugging */
    sigdelset(&mask, SIGINT);
    signal(SIGINT, signal_loop_to_terminate);

    /* Die on kill $PID */
    sigdelset(&mask, SIGKILL);

    struct pollfd pollfd;
    pollfd.fd = sd_journal_get_fd(watch->j->j);
    pollfd.events = sd_journal_get_events(watch->j->j);

    int r = 0;

    while (!s_loop_terminated && watch->state == ABRT_JOURNAL_WATCH_READY)
    {
        r = sd_journal_next(watch->j->j);
        if (r < 0)
        {
            log_warning("Failed to iterate to next entry: %s", strerror(-r));
            break;
        }
        else if (r == 0)
        {
            ppoll(&pollfd, 1, NULL, &mask);
            r = sd_journal_process(watch->j->j);
            if (r < 0)
            {
                log_warning("Failed to get journal changes: %s\n", strerror(-r));
                break;
            }
            continue;
        }

        watch->callback(watch, watch->callback_data);
    }

    return r;
}

void abrt_journal_watch_stop(abrt_journal_watch_t *watch)
{
    watch->state = ABRT_JOURNAL_WATCH_STOPPED;
}

/*
 * ABRT systemd-journal watch - end
 */

void abrt_journal_watch_notify_strings(abrt_journal_watch_t *watch, void *data)
{
    struct abrt_journal_watch_notify_strings *conf = (struct abrt_journal_watch_notify_strings *)data;

    const char *message = NULL;

    if (abrt_journal_get_string_field(abrt_journal_watch_get_journal(watch), "MESSAGE", &message) < 0)
        error_msg_and_die("Cannot read journal data.");

    GList *cur = conf->strings;
    while (cur)
    {
        if (strstr(message, cur->data) != NULL)
            break;

        cur = g_list_next(cur);
    }

    if (cur)
        conf->decorated_cb(watch, conf->decorated_cb_data);
}

/*
 * ABRT systemd-journal strings notifier - end
 */
