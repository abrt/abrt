/* -*- tab-width: 8 -*- */

/*
    Copyright (C) 2011 RedHat inc.

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

#define EXAMPLE_PFX "../../examples"

struct test_struct {
        const char *filename;
        const char *expected_results;
};

static inline FILE *xfopen_ro(const char *filename)
{
        FILE *fp = fopen(filename, "r");
        if (!fp)
                perror_msg_and_die("Can't open '%s'", filename);

        return fp;
}

static inline char *fread_full(const char *filenamep)
{
        FILE *fp = xfopen_ro(filenamep);

        fseek(fp, 0, SEEK_END);
        off_t size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *koops_bt = g_malloc0(size + 1);
        int r = fread(koops_bt, sizeof(char), size, fp);
        fclose(fp);
        if (r < 0)
		perror_msg_and_die("Can't read '%s'", filenamep);

        return koops_bt;
}
