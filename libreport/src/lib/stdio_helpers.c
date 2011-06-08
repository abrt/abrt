/*
 * Utility routines.
 *
 * Copyright (C) 2001 Matt Krai
 * Copyright (C) 2004 Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005, 2006 Rob Landley <rob@landley.net>
 * Copyright (C) 2010 ABRT Team
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libreport.h"

//TODO: add sanitizing upper limit (e.g 64K, 1M, or configurable).
//This is why we don't use GNU's getline: it doesn't have
//any upper sanity bound on line size.

static char *xmalloc_fgets_internal(FILE *file, int *sizep)
{
	unsigned idx = 0;
	char *linebuf = NULL;

	while (1) {
		char *r;

		linebuf = xrealloc(linebuf, idx + 0x100);
		r = fgets(&linebuf[idx], 0x100, file);
		if (!r) {
			/* need to terminate the line */
			linebuf[idx] = '\0';
			break;
		}

		/* stupid. fgets knows the len, it should report it somehow */
		unsigned len = strlen(&linebuf[idx]);

		idx += len;
		if (len < 0xff || linebuf[idx - 1] == '\n')
			break; /* we found \n or EOF */
	}

	*sizep = idx;

	if (!idx) {
		/* The very first fgets returned NULL. It's EOF (or error) */
		free(linebuf);
		linebuf = NULL;
	}
	return linebuf;
}

char *xmalloc_fgets(FILE *file)
{
	int sz;
	char *r = xmalloc_fgets_internal(file, &sz);
	if (!r)
		return r;
	return xrealloc(r, sz + 1);
}

char *xmalloc_fgetline(FILE *file)
{
	int sz;
	char *r = xmalloc_fgets_internal(file, &sz);
	if (!r)
		return r;
	if (r[sz - 1] == '\n')
		r[--sz] = '\0';
	return xrealloc(r, sz + 1);
}
