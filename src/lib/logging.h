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

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef LOGGING_H
#define LOGGING_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NORETURN __attribute__ ((noreturn))

enum {
    LOGMODE_NONE = 0,
    LOGMODE_STDIO = (1 << 0),
    LOGMODE_SYSLOG = (1 << 1),
    LOGMODE_BOTH = LOGMODE_SYSLOG + LOGMODE_STDIO,
    LOGMODE_CUSTOM = (1 << 2),
};

#define g_custom_logger abrt_g_custom_logger
extern void (*g_custom_logger)(const char*);
#define msg_prefix abrt_msg_prefix
extern const char *msg_prefix;
#define msg_eol abrt_msg_eol
extern const char *msg_eol;
#define logmode abrt_logmode
extern int logmode;
#define xfunc_error_retval abrt_xfunc_error_retval
extern int xfunc_error_retval;

/* Verbosity level */
#define g_verbose abrt_g_verbose
extern int g_verbose;
/* VERB1 log("what you sometimes want to see, even on a production box") */
#define VERB1 if (g_verbose >= 1)
/* VERB2 log("debug message, not going into insanely small details") */
#define VERB2 if (g_verbose >= 2)
/* VERB3 log("lots and lots of details") */
#define VERB3 if (g_verbose >= 3)
/* there is no level > 3 */

#define  abrt_
#define xfunc_die abrt_xfunc_die
void xfunc_die(void) NORETURN;
#define log_msg abrt_log_msg
void log_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
/* It's a macro, not function, since it collides with log() from math.h */
#undef log
#define log(...) log_msg(__VA_ARGS__)
/* error_msg family will use g_custom_logger. log_msg does not. */
#define error_msg abrt_error_msg
void error_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
#define error_msg_and_die abrt_error_msg_and_die
void error_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
/* Reports error message with libc's errno error description attached. */
#define perror_msg abrt_perror_msg
void perror_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
#define perror_msg_and_die abrt_perror_msg_and_die
void perror_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
#define die_out_of_memory abrt_die_out_of_memory
void die_out_of_memory(void) NORETURN;

#ifdef __cplusplus
}
#endif

#endif
