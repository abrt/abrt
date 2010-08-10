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

#include "read_write.h"
#include "xfuncs.h"

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

extern void (*g_custom_logger)(const char*);
extern const char *msg_prefix;
extern const char *msg_eol;
extern int logmode;
extern int xfunc_error_retval;
void xfunc_die(void) NORETURN;
void log_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
/* It's a macro, not function, since it collides with log() from math.h */
#undef log
#define log(...) log_msg(__VA_ARGS__)
/* error_msg family will use g_custom_logger. log_msg does not. */
void error_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
void error_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
/* Reports error message with libc's errno error description attached. */
void perror_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
void perror_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
void perror_nomsg_and_die(void) NORETURN;
void perror_nomsg(void);
void verror_msg(const char *s, va_list p, const char *strerr);
void die_out_of_memory(void) NORETURN;

/* Verbosity level */
extern int g_verbose;
/* VERB1 log("what you sometimes want to see, even on a production box") */
#define VERB1 if (g_verbose >= 1)
/* VERB2 log("debug message, not going into insanely small details") */
#define VERB2 if (g_verbose >= 2)
/* VERB3 log("lots and lots of details") */
#define VERB3 if (g_verbose >= 3)
/* there is no level > 3 */

#ifdef __cplusplus
}
#endif

#endif
