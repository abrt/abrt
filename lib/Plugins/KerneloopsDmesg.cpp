/*
 * Copyright 2007, Intel Corporation
 * Copyright 2009, Red Hat Inc.
 *
 * This file is part of %TBD%
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named COPYING; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * Authors:
 *      Anton Arapov <anton@redhat.com>
 *      Arjan van de Ven <arjan@linux.intel.com>
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <syslog.h>
#include <limits.h>
#include <asm/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "DebugDump.h"
#include "KerneloopsDmesg.h"


#define MAX(A,B) ((A) > (B) ? (A) : (B))

/*
 * This limits the number of oopses we'll submit per session;
 * it's important that this is bounded to avoid feedback loops
 * for the scenario where submitting an oopses causes a warning/oops
 */
#define MAX_OOPS 16


struct oops {
	struct oops *next;
	char *text;
	char *version;
};

/* we queue up oopses, and then submit in a batch.
 * This is useful to be able to cancel all submissions, in case
 * we later find our marker indicating we submitted everything so far already
 * previously.
 */
static struct oops *queued_oopses;
static int submitted;

static char **linepointer;

static char *linelevel;
static int linecount;

void queue_oops(char *oops, char *version)
{
	struct oops *newoops;

	if (submitted > MAX_OOPS)
		return;

	newoops = (struct oops*)malloc(sizeof(struct oops));
	memset(newoops, 0, sizeof(struct oops));
	newoops->next = queued_oopses;
	newoops->text = strdup(oops);
	newoops->version = strdup(version);
	queued_oopses = newoops;
	submitted++;
}

static void write_logfile(int count)
{
	openlog("abrt", 0, LOG_KERN);
	syslog(LOG_WARNING, "Kerneloops hook: Reported %i kernel oopses to Abrt", count);
	closelog();
}

/*
 * This function splits the dmesg buffer data into lines
 * (null terminated). The linepointer array is assumed to be
 * allocated already.
 */
static void fill_linepointers(char *buffer, int remove_syslog)
{
	char *c;
	linecount = 0;
	c = buffer;
	while (c) {
		int len = 0;
		char *c9;

		c9 = strchr(c, '\n');
		if (c9)
			len = c9 - c;

		/* in /var/log/messages, we need to strip the first part off, upto the 3rd ':' */
		if (remove_syslog) {
			char *c2;

			/* skip non-kernel lines */
			c2 = (char*)memmem(c, len, "kernel:", 7);
			if (!c2)
				c2 = (char*)memmem(c, len, "abrt:", 5);
			if (!c2) {
				c2 = c9;
				if (c2) {
					c = c2 + 1;
					continue;
				} else
					break;
			}
			c = strchr(c, ':');
			if (!c)
				break;
			c++;
			c = strchr(c, ':');
			if (!c)
				break;
			c++;
			c = strchr(c, ':');
			if (!c)
				break;
			c++;
			if (*c)
				c++;
		}

		linepointer[linecount] = c;
		linelevel[linecount] = 0;
		/* store and remove kernel log level */
		if (*c == '<' && *(c+2) == '>') {
			linelevel[linecount] = *(c+1);
			c = c + 3;
			linepointer[linecount] = c;
		}
		/* remove jiffies time stamp counter if present */
		if (*c == '[') {
			char *c2, *c3;
			c2 = strchr(c, '.');
			c3 = strchr(c, ']');
			if (c2 && c3 && (c2 < c3) && (c3-c) < 14 && (c2-c) < 8) {
				c = c3+1;
				if (*c == ' ')
					c++;
				linepointer[linecount] = c;
			}
		}

		c = strchr(c, '\n'); /* turn the \n into a string termination */
		if (c) {
			*c = 0;
			c = c+1;
		}

		/* if we see our own marker, we know we submitted everything upto here already */
		if (strstr(linepointer[linecount], "Abrt")) {
			linecount = 0;
			linepointer[0] = NULL;
		}
		linecount++;
	}
}

/*
 * extract_version tries to find the kernel version in given data
 */
static inline int extract_version(char *linepointer, char *version)
{
	int ret;
	
	ret = 0;
	if ((strstr(linepointer, "Pid") != NULL) ||
		(strstr(linepointer, "comm") != NULL) ||
		(strstr(linepointer, "CPU") != NULL) ||
		(strstr(linepointer, "REGS") != NULL) ||
		(strstr(linepointer, "EFLAGS") != NULL))
	{
		char* start;
		char* end;

		start = strstr(linepointer, "2.6.");
		if (start) {
			end = index(start, 0x20);
			strncpy(version, start, end-start);
			ret = 1;
		}
	}

	if (!ret)
		strncpy(version, "undefined", 9);

	return ret;
}

/*
 * extract_oops tries to find oops signatures in a log
 */
static void extract_oops(char *buffer, size_t buflen, int remove_syslog)
{
	int i;
	char prevlevel = 0;
	int oopsstart = -1;
	int oopsend;
	int inbacktrace = 0;

	linepointer = (char**)calloc(buflen+1, sizeof(char*));
	if (!linepointer)
		return;
	linelevel = (char*)calloc(buflen+1, sizeof(char));
	if (!linelevel) {
		free(linepointer);
		linepointer = NULL;
		return;
	}

	fill_linepointers(buffer, remove_syslog);

	oopsend = linecount;

	i = 0;
	while (i < linecount) {
		char *c = linepointer[i];

		if (c == NULL) {
			i++;
			continue;
		}
		if (oopsstart < 0) {
			/* find start-of-oops markers */
			if (strstr(c, "general protection fault:"))
				oopsstart = i;
			if (strstr(c, "BUG:"))
				oopsstart = i;
			if (strstr(c, "kernel BUG at"))
				oopsstart = i;
			if (strstr(c, "do_IRQ: stack overflow:"))
				oopsstart = i;
			if (strstr(c, "RTNL: assertion failed"))
				oopsstart = i;
			if (strstr(c, "Eeek! page_mapcount(page) went negative!"))
				oopsstart = i;
			if (strstr(c, "near stack overflow (cur:"))
				oopsstart = i;
			if (strstr(c, "double fault:"))
				oopsstart = i;
			if (strstr(c, "Badness at"))
				oopsstart = i;
			if (strstr(c, "NETDEV WATCHDOG"))
				oopsstart = i;
			if (strstr(c, "WARNING:") &&
			    !strstr(c, "appears to be on the same physical disk"))
				oopsstart = i;
			if (strstr(c, "Unable to handle kernel"))
				oopsstart = i;
			if (strstr(c, "sysctl table check failed"))
				oopsstart = i;
			if (strstr(c, "------------[ cut here ]------------"))
				oopsstart = i;
			if (strstr(c, "list_del corruption."))
				oopsstart = i;
			if (strstr(c, "list_add corruption."))
				oopsstart = i;
			if (strstr(c, "Oops:") && i >= 3)
				oopsstart = i-3;
#if 0
			/* debug information */
			if (oopsstart >= 0) {
				printf("Found start of oops at line %i\n", oopsstart);
				printf("    start line is -%s-\n", linepointer[oopsstart]);
				if (oopsstart != i)
					printf("    trigger line is -%s-\n", c);
			}
#endif
			/* try to find the end marker */
			if (oopsstart >= 0) {
				int i2;
				i2 = i+1;
				while (i2 < linecount && i2 < (i+50)) {
					if (strstr(linepointer[i2], "---[ end trace")) {
						inbacktrace = 1;
						i = i2;
						break;
					}
					i2++;
				}
			}
		}

		/* a calltrace starts with "Call Trace:" or with the " [<.......>] function+0xFF/0xAA" pattern */
		if (oopsstart >= 0 && strstr(linepointer[i], "Call Trace:"))
			inbacktrace = 1;

		else if (oopsstart >= 0 && inbacktrace == 0 && strlen(linepointer[i]) > 8) {
			char *c1, *c2, *c3;
			c1 = strstr(linepointer[i], ">]");
			c2 = strstr(linepointer[i], "+0x");
			c3 = strstr(linepointer[i], "/0x");
			if (linepointer[i][0] == ' ' && linepointer[i][1] == '[' && linepointer[i][2] == '<' && c1 && c2 && c3)
				inbacktrace = 1;
		} else

		/* try to see if we're at the end of an oops */
		if (oopsstart >= 0 && inbacktrace > 0) {
			char c2, c3;
			c2 = linepointer[i][0];
			c3 = linepointer[i][1];

			/* line needs to start with " [" or have "] ["*/
			if ((c2 != ' ' || c3 != '[') &&
				strstr(linepointer[i], "] [") == NULL &&
				strstr(linepointer[i], "--- Exception") == NULL &&
				strstr(linepointer[i], "    LR =") == NULL &&
				strstr(linepointer[i], "<#DF>") == NULL &&
				strstr(linepointer[i], "<IRQ>") == NULL &&
				strstr(linepointer[i], "<EOI>") == NULL &&
				strstr(linepointer[i], "<<EOE>>") == NULL)
				oopsend = i-1;

			/* oops lines are always more than 8 long */
			if (strlen(linepointer[i]) < 8)
				oopsend = i-1;
			/* single oopses are of the same loglevel */
			if (linelevel[i] != prevlevel)
				oopsend = i-1;
			/* The Code: line means we're done with the backtrace */
			if (strstr(linepointer[i], "Code:") != NULL)
				oopsend = i;
			if (strstr(linepointer[i], "Instruction dump::") != NULL)
				oopsend = i;
			/* if a new oops starts, this one has ended */
			if (strstr(linepointer[i], "WARNING:") != NULL && oopsstart != i)
				oopsend = i-1;
			if (strstr(linepointer[i], "Unable to handle") != NULL && oopsstart != i)
				oopsend = i-1;
			/* kernel end-of-oops marker */
			if (strstr(linepointer[i], "---[ end trace") != NULL)
				oopsend = i;

			if (oopsend <= i) {
				int q;
				int len;
				int is_version;
				char *oops;
				char *version;

				len = 2;
				for (q = oopsstart; q <= oopsend; q++)
					len += strlen(linepointer[q])+1;

				oops = (char*)calloc(len, 1);
				version = (char*)calloc(len, 1);

				is_version = 0;
				for (q = oopsstart; q <= oopsend; q++) {
					if (!is_version)
						is_version = extract_version(linepointer[q], version);
					strcat(oops, linepointer[q]);
					strcat(oops, "\n");
				}
				/* too short oopses are invalid */
				if (strlen(oops) > 100)
					queue_oops(oops, version);
				oopsstart = -1;
				inbacktrace = 0;
				oopsend = linecount;
				free(oops);
				free(version);
			}
		}
		prevlevel = linelevel[i];
		i++;
		if (oopsstart > 0 && i-oopsstart > 50) {
			oopsstart = -1;
			inbacktrace = 0;
			oopsend = linecount;
		}
		if (oopsstart > 0 && !inbacktrace && i-oopsstart > 30) {
			oopsstart = -1;
			inbacktrace = 0;
			oopsend = linecount;
		}
	}
	if (oopsstart >= 0)  {
		int q;
		int len;
		int is_version;
		char *oops;
		char *version;

		oopsend = i-1;

		len = 2;
		while (oopsend > 0 && linepointer[oopsend] == NULL)
			oopsend--;
		for (q = oopsstart; q <= oopsend; q++)
			len += strlen(linepointer[q])+1;

		oops = (char*)calloc(len, 1);
		version = (char*)calloc(len, 1);

		is_version = 0;
		for (q = oopsstart; q <= oopsend; q++) {
			if (!is_version)
				is_version = extract_version(linepointer[q], version);
			strcat(oops, linepointer[q]);
			strcat(oops, "\n");
		}
		/* too short oopses are invalid */
		if (strlen(oops) > 100)
			queue_oops(oops, version);
		oopsstart = -1;
		inbacktrace = 0;
		oopsend = linecount;
		free(oops);
		free(version);
	}
	free(linepointer);
	free(linelevel);
	linepointer = NULL;
	linelevel = NULL;
}

void scan_dmesg(void __unused *unused)
{
	char *buffer;

	buffer = (char*)calloc(getpagesize()+1, 1);

	syscall(__NR_syslog, 3, buffer, getpagesize());
	extract_oops(buffer, strlen(buffer), 0);
	free(buffer);
}

void scan_filename(char *filename, int issyslog)
{
	char *buffer;
	struct stat statb;
	FILE *file;
	int ret;
	size_t buflen;

	memset(&statb, 0, sizeof(statb));

	ret = stat(filename, &statb);

	if (statb.st_size < 1 || ret != 0)
		return;

	/*
	 * in theory there's a race here, since someone could spew
	 * to /var/log/messages before we read it in... we try to
	 * deal with it by reading at most 1023 bytes extra. If there's
	 * more than that.. any oops will be in dmesg anyway.
	 * Do not try to allocate an absurt amount of memory; ignore
	 * older log messages because they are unlikely to have
	 * sufficiently recent data to be useful.  32MB is more
	 * than enough; it's not worth looping through more log
	 * if the log is larger than that.
	 */
	buflen = MAX(statb.st_size+1024, 32*1024*1024);
	buffer = (char*)calloc(buflen, 1);
	assert(buffer != NULL);

	file = fopen(filename, "rm");
	if (!file) {
		free(buffer);
		return;
	}
	fseek(file, -buflen, SEEK_END);
	ret = fread(buffer, 1, buflen-1, file);
	fclose(file);

	if (ret > 0)
		extract_oops(buffer, buflen-1, issyslog);
	free(buffer);
}

int scan_logs()
{
	int ret;
	struct oops *oops;
	struct oops *queue;
	char path[PATH_MAX];
	int count;

	ret = 0;

	time_t t = time(NULL);
	if (((time_t) -1) == t)
	{
		fprintf(stderr, "Kerneloops: cannot get local time.\n");
		perror("");
		return -4;
	}

	/* scan dmesg and messages for oopses */
	scan_dmesg(NULL);
	scan_filename("/var/log/messages", 1);

	CDebugDump dd;
	queue = queued_oopses;
	queued_oopses = NULL;
	barrier();
	oops = queue;
	count = 0;
	while (oops) {
		struct oops *next;

		snprintf(path, sizeof(path), "%s/kerneloops-%d-%d", DEBUG_DUMPS_DIR, t, count);
		try
		{
			dd.Create(path);
			dd.SaveText(FILENAME_APPLICATION, "Kerneloops");
			dd.SaveText(FILENAME_UID, "0");
			dd.SaveText(FILENAME_EXECUTABLE, "kernel");
			dd.SaveText(FILENAME_KERNEL, oops->version);
			dd.SaveText(FILENAME_PACKAGE, "not_applicable");
			dd.SaveText(FILENAME_TEXTDATA1, oops->text);
			count++;
			dd.Close();
		}
		catch (std::string sError)
		{
			fprintf(stderr, "Kerneloops: %s\n", sError.c_str());
			ret = -2;
		}
		next = oops->next;
		free(oops->text);
		free(oops->version);
		free(oops);
		oops = next;
	}

	if (ret == 0)
		write_logfile(count);

	return ret;
}
