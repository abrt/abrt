/*
 * Utility routines.
 *
 * Copyright (C) 2001 Erik Andersen
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
