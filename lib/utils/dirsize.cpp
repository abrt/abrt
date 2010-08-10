/*
    Copyright (C) 2009  Jiri Moskovcak (jmoskovc@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#include "abrtlib.h"

using namespace std;

double get_dirsize(const char *pPath)
{
    DIR *dp = opendir(pPath);
    if (dp == NULL)
        return 0;

    struct dirent *ep;
    struct stat statbuf;
    double size = 0;
    while ((ep = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(ep->d_name))
            continue;
        string dname = concat_path_file(pPath, ep->d_name);
        if (lstat(dname.c_str(), &statbuf) != 0)
            continue;
        if (S_ISDIR(statbuf.st_mode))
        {
            size += get_dirsize(dname.c_str());
        }
        else if (S_ISREG(statbuf.st_mode))
        {
            size += statbuf.st_size;
        }
    }
    closedir(dp);
    return size;
}

double get_dirsize_find_largest_dir(
		const char *pPath,
		string *worst_dir,
		const char *excluded)
{
    DIR *dp = opendir(pPath);
    if (dp == NULL)
        return 0;

    struct dirent *ep;
    struct stat statbuf;
    double size = 0;
    double maxsz = 0;
    while ((ep = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(ep->d_name))
            continue;
        string dname = concat_path_file(pPath, ep->d_name);
        if (lstat(dname.c_str(), &statbuf) != 0)
            continue;
        if (S_ISDIR(statbuf.st_mode))
        {
            double sz = get_dirsize(dname.c_str());
            size += sz;

            if (worst_dir && (!excluded || strcmp(excluded, ep->d_name) != 0))
            {
                /* Calculate "weighted" size and age
                 * w = sz_kbytes * age_mins */
                sz /= 1024;
                long age = (time(NULL) - statbuf.st_mtime) / 60;
                if (age > 0)
                    sz *= age;

                if (sz > maxsz)
                {
                    maxsz = sz;
                    *worst_dir = ep->d_name;
                }
            }
        }
        else if (S_ISREG(statbuf.st_mode))
        {
            size += statbuf.st_size;
        }
    }
    closedir(dp);
    return size;
}
