#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <glib.h>

GList *get_dump_list(const char *path)
{
    GList *dumps = NULL;
    DIR *dp;
    struct dirent *ep;
    //char *path = "./";
    char *current_dir_path;

    dp = opendir(path);

    if (dp != NULL)
    {
        while ((ep = readdir (dp)))
        /* d_type is not supported on all systems, but should be ok for us
         * can be implemented using stat if someone wants to do it...
         */
        if(ep->d_name && ep->d_name[0] != '.' && ep->d_type == DT_DIR)
        {
            if(asprintf(&current_dir_path, "%s/%s", path, ep->d_name) != -1)
            {
                //FIXME: should be int ret = access(current_dir_path, R_OK|W_OK);
                int ret = access(current_dir_path, R_OK);
                if(ret == 0)
                {
                    dumps = g_list_prepend(dumps, current_dir_path);
                }
            }
        }
        (void) closedir (dp);
    }
    else
        fprintf(stderr, "Couldn't open the directory %s: %s\n", path, strerror(errno));
    return dumps;
}
