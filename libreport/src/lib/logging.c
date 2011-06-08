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

/*
 * Utility routines.
 *
 */
#include "libreport.h"

void (*g_custom_logger)(const char*);
const char *msg_prefix = "";
const char *msg_eol = "\n";
int logmode = LOGMODE_STDIO;
int xfunc_error_retval = EXIT_FAILURE;
int g_verbose;

void xfunc_die(void)
{
    exit(xfunc_error_retval);
}

static void verror_msg_helper(const char *s,
                              va_list p,
                              const char* strerr,
                              int flags)
{
    char *msg;
    int prefix_len, strerr_len, msgeol_len, used;

    if (!logmode)
        return;

    used = vasprintf(&msg, s, p);
    if (used < 0)
        return;

    /* This is ugly and costs +60 bytes compared to multiple
     * fprintf's, but is guaranteed to do a single write.
     * This is needed for e.g. when multiple children
     * can produce log messages simultaneously. */

    prefix_len = msg_prefix[0] ? strlen(msg_prefix) + 2 : 0;
    strerr_len = strerr ? strlen(strerr) : 0;
    msgeol_len = strlen(msg_eol);
    /* +3 is for ": " before strerr and for terminating NUL */
    msg = (char*) xrealloc(msg, prefix_len + used + strerr_len + msgeol_len + 3);
    /* TODO: maybe use writev instead of memmoving? Need full_writev? */
    if (prefix_len) {
        char *p;
        memmove(msg + prefix_len, msg, used);
        used += prefix_len;
        p = stpcpy(msg, msg_prefix);
        p[0] = ':';
        p[1] = ' ';
    }
    if (strerr) {
        if (s[0]) {
            msg[used++] = ':';
            msg[used++] = ' ';
        }
        strcpy(&msg[used], strerr);
        used += strerr_len;
    }
    strcpy(&msg[used], msg_eol);

    if (flags & LOGMODE_STDIO) {
        fflush(stdout);
        full_write(STDERR_FILENO, msg, used + msgeol_len);
    }
    msg[used] = '\0'; /* remove msg_eol (usually "\n") */
    if (flags & LOGMODE_SYSLOG) {
        syslog(LOG_ERR, "%s", msg + prefix_len);
    }
    if ((flags & LOGMODE_CUSTOM) && g_custom_logger) {
        g_custom_logger(msg + prefix_len);
    }
    free(msg);
}

void log_msg(const char *s, ...)
{
    va_list p;

    va_start(p, s);
    verror_msg_helper(s, p, NULL, logmode);
    va_end(p);
}

void error_msg(const char *s, ...)
{
    va_list p;

    va_start(p, s);
    verror_msg_helper(s, p, NULL, (logmode | LOGMODE_CUSTOM));
    va_end(p);
}

void error_msg_and_die(const char *s, ...)
{
    va_list p;

    va_start(p, s);
    verror_msg_helper(s, p, NULL, (logmode | LOGMODE_CUSTOM));
    va_end(p);
    xfunc_die();
}

void perror_msg(const char *s, ...)
{
    va_list p;

    va_start(p, s);
    /* Guard against "<error message>: Success" */
    verror_msg_helper(s, p, errno ? strerror(errno) : NULL, (logmode | LOGMODE_CUSTOM));
    va_end(p);
}

void perror_msg_and_die(const char *s, ...)
{
    va_list p;

    va_start(p, s);
    /* Guard against "<error message>: Success" */
    verror_msg_helper(s, p, errno ? strerror(errno) : NULL, (logmode | LOGMODE_CUSTOM));
    va_end(p);
    xfunc_die();
}

void die_out_of_memory(void)
{
    error_msg_and_die("Out of memory, exiting");
}
