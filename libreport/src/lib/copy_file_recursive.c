/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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

#include "libreport.h"

int copy_file_recursive(const char *source, const char *dest)
{
	/* This is a recursive function, try to minimize stack usage */
	/* NB: each struct stat is ~100 bytes */
	struct stat source_stat;
	struct stat dest_stat;
	int retval = 0;
	int dest_exists = 0;

	if (strcmp(source, ".lock") == 0)
		goto skip;

	if (stat(source, &source_stat) < 0) {
		perror_msg("Can't stat '%s'", source);
		return -1;
	}

	if (lstat(dest, &dest_stat) < 0) {
		if (errno != ENOENT) {
			perror_msg("Can't stat '%s'", dest);
			return -1;
		}
	} else {
		if (source_stat.st_dev == dest_stat.st_dev
		 && source_stat.st_ino == dest_stat.st_ino
		) {
			error_msg("'%s' and '%s' are the same file", source, dest);
			return -1;
		}
		dest_exists = 1;
	}

	if (S_ISDIR(source_stat.st_mode)) {
		DIR *dp;
		struct dirent *d;

		if (dest_exists) {
			if (!S_ISDIR(dest_stat.st_mode)) {
				error_msg("Target '%s' is not a directory", dest);
				return -1;
			}
			/* race here: user can substitute a symlink between
			 * this check and actual creation of files inside dest */
		} else {
			/* Create DEST */
			mode_t mode = source_stat.st_mode;
			/* Allow owner to access new dir (at least for now) */
			mode |= S_IRWXU;
			if (mkdir(dest, mode) < 0) {
				perror_msg("Can't create directory '%s'", dest);
				return -1;
			}
		}
		/* Recursively copy files in SOURCE */
		dp = opendir(source);
		if (dp == NULL) {
			retval = -1;
			goto ret;
		}

		while (retval == 0 && (d = readdir(dp)) != NULL) {
			char *new_source, *new_dest;

			if (dot_or_dotdot(d->d_name))
				continue;
			new_source = concat_path_file(source, d->d_name);
			new_dest = concat_path_file(dest, d->d_name);
			if (copy_file_recursive(new_source, new_dest) < 0)
				retval = -1;
			free(new_source);
			free(new_dest);
		}
		closedir(dp);

		goto ret;
	}

	if (S_ISREG(source_stat.st_mode)) {
		int src_fd;
		int dst_fd;
		mode_t new_mode;

		src_fd = open(source, O_RDONLY);
		if (src_fd < 0) {
			perror_msg("Can't open '%s'", source);
			return -1;
		}

		/* Do not try to open with weird mode fields */
		new_mode = source_stat.st_mode;

		// security problem versus (sym)link attacks
		// dst_fd = open(dest, O_WRONLY|O_CREAT|O_TRUNC, new_mode);
		/* safe way: */
		dst_fd = open(dest, O_WRONLY|O_CREAT|O_EXCL, new_mode);
		if (dst_fd < 0) {
			close(src_fd);
			return -1;
		}

		if (copyfd_eof(src_fd, dst_fd, COPYFD_SPARSE) == -1)
			retval = -1;
		close(src_fd);
		/* Careful: do check that buffered writes succeeded... */
		if (close(dst_fd) < 0) {
			perror_msg("Error writing to '%s'", dest);
			retval = -1;
		} else {
			/* (Try to) copy atime and mtime */
			struct timeval atime_mtime[2];
			atime_mtime[0].tv_sec = source_stat.st_atime;
			// note: if "st_atim.tv_nsec" doesn't compile, try "st_atimensec":
			atime_mtime[0].tv_usec = source_stat.st_atim.tv_nsec / 1000;
			atime_mtime[1].tv_sec = source_stat.st_mtime;
			atime_mtime[1].tv_usec = source_stat.st_mtim.tv_nsec / 1000;
			// note: can use utimensat when it is more widely supported:
			utimes(dest, atime_mtime);
		}
		goto ret;
	}

	/* Neither dir not regular file: skip */

 skip:
	log("Skipping '%s'", source);
 ret:
	return retval;
}
