/*
 * Utility routines.
 *
 * Licensed under GPLv2, see file COPYING in this tarball for details.
 */
#include "abrtlib.h"
#include <syslog.h>

int xfunc_error_retval = EXIT_FAILURE;

int g_verbose;

void xfunc_die(void)
{
	exit(xfunc_error_retval);
}

const char *msg_prefix = "";
const char *msg_eol = "\n";
int logmode = LOGMODE_STDIO;

void verror_msg(const char *s, va_list p, const char* strerr)
{
	char *msg;
	int prefix_len, strerr_len, msgeol_len, used;

	if (!logmode)
		return;

	if (!s) /* nomsg[_and_die] uses NULL fmt */
		s = ""; /* some libc don't like printf(NULL) */

	used = vasprintf(&msg, s, p);
	if (used < 0)
		return;

	/* This is ugly and costs +60 bytes compared to multiple
	 * fprintf's, but is guaranteed to do a single write.
	 * This is needed for e.g. when multiple children
	 * can produce log messages simultaneously. */

	prefix_len = strlen(msg_prefix);
	strerr_len = strerr ? strlen(strerr) : 0;
	msgeol_len = strlen(msg_eol);
	/* +3 is for ": " before strerr and for terminating NUL */
	msg = (char*) xrealloc(msg, prefix_len + used + strerr_len + msgeol_len + 3);
	/* TODO: maybe use writev instead of memmoving? Need full_writev? */
	if (prefix_len) {
		memmove(msg + prefix_len, msg, used);
		used += prefix_len;
		memcpy(msg, msg_prefix, prefix_len);
	}
	if (strerr) {
		if (s[0]) { /* not perror_nomsg? */
			msg[used++] = ':';
			msg[used++] = ' ';
		}
		strcpy(&msg[used], strerr);
		used += strerr_len;
	}
	strcpy(&msg[used], msg_eol);

	if (logmode & LOGMODE_STDIO) {
		fflush(stdout);
		full_write(STDERR_FILENO, msg, used + msgeol_len);
	}
	if (logmode & LOGMODE_SYSLOG) {
		syslog(LOG_ERR, "%s", msg + prefix_len);
	}
	free(msg);
}

void error_msg_and_die(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	verror_msg(s, p, NULL);
	va_end(p);
	xfunc_die();
}

void error_msg(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	verror_msg(s, p, NULL);
	va_end(p);
}

void perror_msg_and_die(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	/* Guard against "<error message>: Success" */
	verror_msg(s, p, errno ? strerror(errno) : NULL);
	va_end(p);
	xfunc_die();
}

void perror_msg(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	/* Guard against "<error message>: Success" */
	verror_msg(s, p, errno ? strerror(errno) : NULL);
	va_end(p);
}

void simple_perror_msg_and_die(const char *s)
{
	perror_msg_and_die("%s", s);
}

void simple_perror_msg(const char *s)
{
	perror_msg("%s", s);
}

void die_out_of_memory(void)
{
    error_msg_and_die("Out of memory, exiting");
}
