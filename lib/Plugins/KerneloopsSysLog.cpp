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

    Authors:
       Anton Arapov <anton@redhat.com>
       Arjan van de Ven <arjan@linux.intel.com>
 */

#include "abrtlib.h"
#include "KerneloopsSysLog.h"
#include <assert.h>

static void queue_oops(vector_string_t &vec, const char *data, const char *version)
{
        vec.push_back(ssprintf("%s\n%s", version, data));
}

/*
 * extract_version tries to find the kernel version in given data
 */
static int extract_version(const char *linepointer, char *version)
{
	int ret;

	ret = 0;
	if (strstr(linepointer, "Pid")
	 || strstr(linepointer, "comm")
	 || strstr(linepointer, "CPU")
	 || strstr(linepointer, "REGS")
	 || strstr(linepointer, "EFLAGS")
	) {
		char* start;
		char* end;

		start = strstr((char*)linepointer, "2.6.");
		if (start) {
			end = strchr(start, ')');
			if (!end)
				end = strchrnul(start, ' ');
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
struct line_info {
	char *ptr;
	char level;
};
static int record_oops(vector_string_t &oopses, struct line_info* lines_info, int oopsstart, int oopsend)
{
	int q;
	int len;
	int is_version;
	char *oops;
	char *version;

	len = 2;
	for (q = oopsstart; q <= oopsend; q++)
		len += strlen(lines_info[q].ptr) + 1;

	oops = (char*)xzalloc(len);
	version = (char*)xzalloc(len);

	is_version = 0;
	for (q = oopsstart; q <= oopsend; q++) {
		if (!is_version)
			is_version = extract_version(lines_info[q].ptr, version);
		if (lines_info[q].ptr[0]) {
			strcat(oops, lines_info[q].ptr);
			strcat(oops, "\n");
		}
	}
	int rv = 1;
	/* too short oopses are invalid */
	if (strlen(oops) > 100) {
		queue_oops(oopses, oops, version);
	} else {
		VERB3 log("Dropped oops: too short");
		rv = 0;
	}
	free(oops);
	free(version);
	return rv;
}
#define REALLOC_CHUNK 1000
int extract_oopses(vector_string_t &oopses, char *buffer, size_t buflen)
{
	char *c;
	int linecount = 0;
	int lines_info_alloc = 0;
	struct line_info *lines_info = NULL;

	/* Split buffer into lines */

	if (buflen != 0)
		buffer[buflen - 1] = '\n';  /* the buffer usually ends with \n, but let's make sure */
	c = buffer;
	while (c < buffer + buflen) {
		char linelevel;
		char *c9;
		char *colon;

		c9 = (char*)memchr(c, '\n', buffer + buflen - c); /* a \n will always be found */
		assert(c9);
		*c9 = '\0'; /* turn the \n into a string termination */
		if (c9 == c)
			goto next_line;

		/* Is it a syslog file (/var/log/messages or similar)?
		 * Even though _usually_ it looks like "Nov 19 12:34:38 localhost kernel: xxx",
		 * some users run syslog in non-C locale:
		 * "2010-02-22T09:24:08.156534-08:00 gnu-4 gnome-session[2048]: blah blah"
		 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ !!!
		 * We detect it by checking for N:NN:NN pattern in first 15 chars
		 * (and this still is not good enough... false positive: "pci 0000:15:00.0: PME# disabled")
		 */
		colon = strchr(c, ':');
		if (colon && colon > c && colon < c + 15
		 && isdigit(colon[-1]) /* N:... */
		 && isdigit(colon[1]) /* ...N:NN:... */
		 && isdigit(colon[2])
		 && colon[3] == ':'
		 && isdigit(colon[4]) /* ...N:NN:NN... */
		 && isdigit(colon[5])
		) {
			/* It's syslog file, not a bare dmesg */

			/* Skip non-kernel lines */
			char *kernel_str = strstr(c, "kernel: ");
			if (kernel_str == NULL) {
				/* if we see our own marker:
				 * "hostname abrt: Kerneloops: Reported 1 kernel oopses to Abrt"
				 * we know we submitted everything upto here already */
				if (strstr(c, "abrt:") && strstr(c, "Abrt")) {
					VERB3 log("Found our marker at line %d, restarting line count from 0", linecount);
					linecount = 0;
					lines_info_alloc = 0;
					free(lines_info);
					lines_info = NULL;
				}
				goto next_line;
			}
			c = kernel_str + sizeof("kernel: ")-1;
		}

		linelevel = 0;
		/* store and remove kernel log level */
		if (*c == '<' && c[1] && c[2] == '>') {
			linelevel = c[1];
			c += 3;
		}
		/* remove jiffies time stamp counter if present */
		if (*c == '[') {
			char *c2 = strchr(c, '.');
			char *c3 = strchr(c, ']');
			if (c2 && c3 && (c2 < c3) && (c3-c) < 14 && (c2-c) < 8) {
				c = c3 + 1;
				if (*c == ' ')
					c++;
			}
		}
		if (linecount >= lines_info_alloc) {
			lines_info_alloc += REALLOC_CHUNK;
			lines_info = (line_info*)xrealloc(lines_info,
					lines_info_alloc * sizeof(struct line_info));
		}
		lines_info[linecount].ptr = c;
		lines_info[linecount].level = linelevel;
		linecount++;
next_line:
		c = c9 + 1;
	}

	/* Analyze lines */

	int i;
	char prevlevel = 0;
	int oopsstart = -1;
	int inbacktrace = 0;
	int oopsesfound = 0;

	i = 0;
	while (i < linecount) {
		char *curline = lines_info[i].ptr;

		if (curline == NULL) {
			i++;
			continue;
		}
		while (*curline == ' ')
			curline++;

		if (oopsstart < 0) {
			/* find start-of-oops markers */
			if (strstr(curline, "general protection fault:"))
				oopsstart = i;
			else if (strstr(curline, "BUG:"))
				oopsstart = i;
			else if (strstr(curline, "kernel BUG at"))
				oopsstart = i;
			else if (strstr(curline, "do_IRQ: stack overflow:"))
				oopsstart = i;
			else if (strstr(curline, "RTNL: assertion failed"))
				oopsstart = i;
			else if (strstr(curline, "Eeek! page_mapcount(page) went negative!"))
				oopsstart = i;
			else if (strstr(curline, "near stack overflow (cur:"))
				oopsstart = i;
			else if (strstr(curline, "double fault:"))
				oopsstart = i;
			else if (strstr(curline, "Badness at"))
				oopsstart = i;
			else if (strstr(curline, "NETDEV WATCHDOG"))
				oopsstart = i;
			else if (strstr(curline, "WARNING: at ") /* WARN_ON() generated message */
			 && !strstr(curline, "appears to be on the same physical disk")
			) {
				oopsstart = i;
			}
			else if (strstr(curline, "Unable to handle kernel"))
				oopsstart = i;
			else if (strstr(curline, "sysctl table check failed"))
				oopsstart = i;
			else if (strstr(curline, "------------[ cut here ]------------"))
				oopsstart = i;
			else if (strstr(curline, "list_del corruption."))
				oopsstart = i;
			else if (strstr(curline, "list_add corruption."))
				oopsstart = i;
			if (strstr(curline, "Oops:") && i >= 3)
				oopsstart = i-3;

			if (oopsstart >= 0) {
				/* debug information */
				VERB3 {
					log("Found oops at line %d: '%s'", oopsstart, lines_info[oopsstart].ptr);
					if (oopsstart != i)
						log("Trigger line is %d: '%s'", i, c);
				}
				/* try to find the end marker */
				int i2 = i + 1;
				while (i2 < linecount && i2 < (i+50)) {
					if (strstr(lines_info[i2].ptr, "---[ end trace")) {
						inbacktrace = 1;
						i = i2;
						break;
					}
					i2++;
				}
			}
		}

		/* Are we entering a call trace part? */
		/* a call trace starts with "Call Trace:" or with the " [<.......>] function+0xFF/0xAA" pattern */
		if (oopsstart >= 0 && !inbacktrace) {
			if (strstr(curline, "Call Trace:"))
				inbacktrace = 1;
			else
			if (strnlen(curline, 9) > 8
			 && curline[0] == '[' && curline[1] == '<'
			 && strstr(curline, ">]")
			 && strstr(curline, "+0x")
			 && strstr(curline, "/0x")
			) {
				inbacktrace = 1;
			}
		}

		/* Are we at the end of an oops? */
		else if (oopsstart >= 0 && inbacktrace) {
			int oopsend = INT_MAX;

			/* line needs to start with " [" or have "] [" if it is still a call trace */
			/* example: "[<ffffffffa006c156>] radeon_get_ring_head+0x16/0x41 [radeon]" */
			if (curline[0] != '['
			 && !strstr(curline, "] [")
			 && !strstr(curline, "--- Exception")
			 && !strstr(curline, "LR =")
			 && !strstr(curline, "<#DF>")
			 && !strstr(curline, "<IRQ>")
			 && !strstr(curline, "<EOI>")
			 && !strstr(curline, "<<EOE>>")
			 && strncmp(curline, "Code: ", 6) != 0
			 && strncmp(curline, "RIP ", 4) != 0
			 && strncmp(curline, "RSP ", 4) != 0
			) {
				oopsend = i-1; /* not a call trace line */
			}
			/* oops lines are always more than 8 chars long */
			else if (strnlen(curline, 8) < 8)
				oopsend = i-1;
			/* single oopses are of the same loglevel */
			else if (lines_info[i].level != prevlevel)
				oopsend = i-1;
			else if (strstr(curline, "Instruction dump:"))
				oopsend = i;
			/* if a new oops starts, this one has ended */
			else if (strstr(curline, "WARNING: at ") && oopsstart != i) /* WARN_ON() generated message */
				oopsend = i-1;
			else if (strstr(curline, "Unable to handle") && oopsstart != i)
				oopsend = i-1;
			/* kernel end-of-oops marker (not including marker itself) */
			else if (strstr(curline, "---[ end trace"))
				oopsend = i-1;

			if (oopsend <= i) {
				VERB3 log("End of oops at line %d (%d): '%s'", oopsend, i, lines_info[oopsend].ptr);
				if (record_oops(oopses, lines_info, oopsstart, oopsend))
					oopsesfound++;
				oopsstart = -1;
				inbacktrace = 0;
			}
		}

		prevlevel = lines_info[i].level;
		i++;

		if (oopsstart >= 0) {
			/* Do we have a suspiciously long oops? Cancel it */
			if (i-oopsstart > 60) {
				inbacktrace = 0;
				oopsstart = -1;
				VERB3 log("Dropped oops, too long");
				continue;
			}
			if (!inbacktrace && i-oopsstart > 40) {
				/*inbacktrace = 0; - already is */
				oopsstart = -1;
				VERB3 log("Dropped oops, too long");
				continue;
			}
		}
	} /* while (i < linecount) */

	/* process last oops if we have one */
	if (oopsstart >= 0 && inbacktrace) {
		int oopsend = i-1;
		VERB3 log("End of oops at line %d (end of file): '%s'", oopsend, lines_info[oopsend].ptr);
		if (record_oops(oopses, lines_info, oopsstart, oopsend))
			oopsesfound++;
	}

	free(lines_info);
	return oopsesfound;
}
