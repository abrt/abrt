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
#ifndef _ABRT_JOURNAL_H_
#define _ABRT_JOURNAL_H_

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A systemd-journal wrapper
 * (isolates systemd API in a single compile unit)
 */
struct abrt_journal;
typedef struct abrt_journal abrt_journal_t;

/* Only journal files generated on the local machine and all journal file types
 * will be opened.
 */
int abrt_journal_new(abrt_journal_t **journal);

/* Journal files generated on ALL machines and all journal file types will be
 * opened.
 */
int abrt_journal_new_merged(abrt_journal_t **journal);

/* Open a directory and merge all journal files placed there.
 */
int abrt_journal_open_directory(abrt_journal_t **journal, const char *directory);

void abrt_journal_free(abrt_journal_t *journal);

int abrt_journal_set_journal_filter(abrt_journal_t *journal,
                                    const char *const *journal_filter_list);

int abrt_journal_get_field(abrt_journal_t *journal,
                           const char *field,
                           const void **value,
                           size_t *value_len);

int abrt_journal_get_int_field(abrt_journal_t *journal,
                               const char *field,
                               int *value);

int abrt_journal_get_unsigned_field(abrt_journal_t *journal,
                                    const char *field,
                                    unsigned *value);

/* Returns allocated memory if value is NULL; otherwise makes copy of journald
 * field to memory pointed by value arg. */
char *abrt_journal_get_string_field(abrt_journal_t *journal,
                                  const char *field,
                                  char *value);

char *abrt_journal_get_log_line(abrt_journal_t *journal);

char *abrt_journal_get_next_log_line(void *data);

int abrt_journal_get_cursor(abrt_journal_t *journal, char **cursor);

int abrt_journal_set_cursor(abrt_journal_t *journal, const char *cursor);

int abrt_journal_seek_tail(abrt_journal_t *journal);

int abrt_journal_next(abrt_journal_t *journal);

int abrt_journal_save_current_position(abrt_journal_t *journal,
                                       const char *file_name);

int abrt_journal_restore_position(abrt_journal_t *journal,
                                  const char *file_name);

/*
 * A systemd-journal listener which waits for new messages a loop and notifies
 * them via a call back
 */
struct abrt_journal_watch;
typedef struct abrt_journal_watch abrt_journal_watch_t;

typedef void (* abrt_journal_watch_callback)(struct abrt_journal_watch *watch,
                                             void *data);

int abrt_journal_watch_new(abrt_journal_watch_t **watch,
                           abrt_journal_t *journal,
                           abrt_journal_watch_callback callback,
                           void *callback_data);

void abrt_journal_watch_free(abrt_journal_watch_t *watch);

/*
 * Returns the watched journal.
 */
abrt_journal_t *abrt_journal_watch_get_journal(abrt_journal_watch_t *watch);

/*
 * Starts reading journal messages and waiting for new messages in a loop.
 *
 * SIGTERM and SIGINT terminates the loop gracefully.
 */
int abrt_journal_watch_run_sync(abrt_journal_watch_t *watch);

/*
 * Can be used to terminate the loop in abrt_journal_watch_run_sync()
 */
void abrt_journal_watch_stop(abrt_journal_watch_t *watch);


/*
 * A decorator for abrt_journal_watch call backs which calls the decorated call
 * back in case where journal message contains a string from the interested
 * list.
 */
struct abrt_journal_watch_notify_strings
{
    abrt_journal_watch_callback decorated_cb;
    void *decorated_cb_data;
    GList *strings;
};

void abrt_journal_watch_notify_strings(abrt_journal_watch_t *watch, void *data);

#ifdef __cplusplus
}
#endif

#endif /*_ABRT_JOURNAL_H_*/
