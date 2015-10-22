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
#include <stdio.h>

#include "abrt-journal.h"
#include "libabrt.h"

#include <systemd/sd-journal.h>

/*
 * http://www.freedesktop.org/software/systemd/man/sd_journal_get_data.html
 * sd_journal_set_data_threshold() : This threshold defaults to 64K by default.
 */
#define JOURNALD_MAX_FIELD_SIZE (64*1024)

#define ABRT_JOURNAL_WATCH_STATE_FILE_MODE 0600
#define ABRT_JOURNAL_WATCH_STATE_FILE_MAX_SZ (4 * 1024)

struct abrt_journal
{
    sd_journal *j;
};

static int abrt_journal_new_flags(abrt_journal_t **journal, int flags)
{
    sd_journal *j;
    const int r = sd_journal_open(&j, flags);
    if (r < 0)
    {
        log_notice("Failed to open journal: %s", strerror(-r));
        return r;
    }

    *journal = xzalloc(sizeof(**journal));
    (*journal)->j = j;

    return 0;
}

int abrt_journal_new(abrt_journal_t **journal)
{
    return abrt_journal_new_flags(journal, SD_JOURNAL_LOCAL_ONLY);
}

int abrt_journal_new_merged(abrt_journal_t **journal)
{
    return abrt_journal_new_flags(journal, 0);
}

static int abrt_journal_open_directory_flags(abrt_journal_t **journal, const char *directory, int flags)
{
    sd_journal *j;
    const int r = sd_journal_open_directory(&j, directory, flags);
    if (r < 0)
    {
        log_notice("Failed to open journal directory ('%s'): %s", directory, strerror(-r));
        return r;
    }

    *journal = xzalloc(sizeof(**journal));
    (*journal)->j = j;

    return 0;
}

int abrt_journal_open_directory(abrt_journal_t **journal, const char *directory)
{
    return abrt_journal_open_directory_flags(journal, directory, 0);
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

    const size_t pfx_len = strlen(field) + 1;
    if (*value_len < pfx_len)
    {
        error_msg("Invalid data format from journal: field data are not prefixed with field name");
        return -EINVAL;
    }

    *value = *value + pfx_len;
    *value_len -= pfx_len;

    return 0;
}

static int abrt_journal_get_integer(abrt_journal_t *journal, const char *field, long min, long max, long *value)
{
    char buffer[sizeof(int)*3 + 2];
    const char *data;
    size_t data_len;

    const int r = abrt_journal_get_field(journal, field, (const void **)&data, &data_len);
    if (r < 0)
        return r;

    if (data_len >= sizeof(buffer))
    {
        log_notice("Journald field '%s' is not a number: too long", field);
        return -EINVAL;
    }

    strncpy(buffer, data, data_len);
    buffer[data_len] = '\0';

    errno = 0;
    char *e = NULL;
    *value = strtol(buffer, &e, 10);
    if (errno || buffer == e || *e != '\0' || *value < min || *value > max)
    {
        log_notice("Journald field '%s' is not a number: '%s'", field, buffer);
        return -EINVAL;
    }

    return 0;
}

int abrt_journal_get_int_field(abrt_journal_t *journal, const char *field, int *value)
{
    long v;
    int r = abrt_journal_get_integer(journal, field, INT_MIN, INT_MAX, &v);
    if (r != 0)
        return r;

    *value = (int)v;
    return 0;
}

int abrt_journal_get_unsigned_field(abrt_journal_t *journal, const char *field, unsigned *value)
{
    long v;
    int r = abrt_journal_get_integer(journal, field, 0, UINT_MAX, &v);
    if (r != 0)
        return r;

    *value = (unsigned)v;
    return 0;
}

char *abrt_journal_get_string_field(abrt_journal_t *journal, const char *field, char *value)
{
    size_t data_len;
    const char *data;
    const int r = abrt_journal_get_field(journal, field, (const void **)&data, &data_len);
    if (r < 0)
        return NULL;

    if (value == NULL)
        return xstrndup(data, data_len);
    /*else*/

    strncpy(value, data, data_len);
    /* journal data are not NULL terminated strings, so terminate the string */
    value[data_len] = '\0';
    return value;
}

char *abrt_journal_get_log_line(abrt_journal_t *journal)
{
    return abrt_journal_get_string_field(journal, "MESSAGE", NULL);
}

char *abrt_journal_get_next_log_line(void *data)
{
    abrt_journal_t *journal = (abrt_journal_t *)data;
    if (abrt_journal_next(journal) <= 0)
        return NULL;

    return abrt_journal_get_log_line(journal);
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

int abrt_journal_save_current_position(abrt_journal_t *journal, const char *file_name)
{
    char *crsr = NULL;
    const int r = abrt_journal_get_cursor(journal, &crsr);

    if (r < 0)
    {
        /* abrt_journal_set_cursor() prints error message in verbose mode */
        error_msg(_("Cannot save journal watch's position"));
        return r;
    }

    int state_fd = open(file_name,
            O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW,
            ABRT_JOURNAL_WATCH_STATE_FILE_MODE);

    if (state_fd < 0)
    {
        perror_msg(_("Cannot save journal watch's position: open('%s')"), file_name);
        return -1;
    }

    full_write_str(state_fd, crsr);
    close(state_fd);

    free(crsr);
    return 0;
}

int abrt_journal_restore_position(abrt_journal_t *journal, const char *file_name)
{
    struct stat buf;
    if (lstat(file_name, &buf) < 0)
    {
        if (errno == ENOENT)
            /* Only notice because this is expected */
            log_notice(_("Not restoring journal watch's position: file '%s' does not exist"), file_name);
        else
            perror_msg(_("Cannot restore journal watch's position form file '%s'"), file_name);

        return -errno;
    }

    if (!(buf.st_mode & S_IFREG))
    {
        error_msg(_("Cannot restore journal watch's position: path '%s' is not regular file"), file_name);
        return -EMEDIUMTYPE;
    }

    if (buf.st_size > ABRT_JOURNAL_WATCH_STATE_FILE_MAX_SZ)
    {
        error_msg(_("Cannot restore journal watch's position: file '%s' exceeds %dB size limit"),
                file_name, ABRT_JOURNAL_WATCH_STATE_FILE_MAX_SZ);
        return -EFBIG;
    }

    int state_fd = open(file_name, O_RDONLY | O_NOFOLLOW);
    if (state_fd < 0)
    {
        perror_msg(_("Cannot restore journal watch's position: open('%s')"), file_name);
        return -errno;
    }

    char *crsr = xmalloc(buf.st_size + 1);

    const int sz = full_read(state_fd, crsr, buf.st_size);
    if (sz != buf.st_size)
    {
        error_msg(_("Cannot restore journal watch's position: cannot read entire file '%s'"), file_name);
        close(state_fd);
        return -errno;
    }

    crsr[sz] = '\0';
    close(state_fd);

    const int r = abrt_journal_set_cursor(journal, crsr);
    if (r < 0)
    {
        /* abrt_journal_set_cursor() prints error message in verbose mode */
        error_msg(_("Failed to move the journal to a cursor from file '%s'"), file_name);
        return r;
    }

    free(crsr);
    return 0;
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

    char message[JOURNALD_MAX_FIELD_SIZE + 1];

    if (abrt_journal_get_string_field(abrt_journal_watch_get_journal(watch), "MESSAGE", (char *)message) == NULL)
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
