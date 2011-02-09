#include "abrtlib.h"

GList *get_dump_list(const char *path)
{
    DIR *dp = opendir(path);
    if (!dp)
    {
        perror_msg("Can't open directory '%s'", path);
        return NULL;
    }

    GList *dumps = NULL;
    struct dirent *dent;
    while ((dent = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue; /* skip "." and ".." */

        char *current_dir_path = concat_path_file(path, dent->d_name);
        //FIXME: should be int ret = access(current_dir_path, R_OK|W_OK);
        int ret = access(current_dir_path, R_OK);
        if (ret == 0)
            dumps = g_list_prepend(dumps, current_dir_path);
        else
            free(current_dir_path);
    }
    closedir(dp);
    return dumps;
}
