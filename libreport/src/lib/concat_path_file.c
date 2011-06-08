/*
 * Utility routines.
 *
 * Copyright (C) 2001 Erik Andersen
 *
 * Licensed under GPLv2 or later.
 */

/* Concatenate path and filename to new allocated buffer.
 * Add '/' only as needed (no duplicate // are produced).
 * If path is NULL, it is assumed to be "/".
 * filename should not be NULL.
 */

#include "libreport.h"

char *concat_path_file(const char *path, const char *filename)
{
	if (!path)
		path = "";
	const char *end = path + strlen(path);
	while (*filename == '/')
		filename++;
	return xasprintf("%s%s%s", path, (end != path && end[-1] != '/' ? "/" : ""), filename);
}
