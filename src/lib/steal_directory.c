#include "abrtlib.h"

struct dump_dir *steal_directory(const char *base_dir, const char *dump_dir_name)
{
    const char *base_name = strrchr(dump_dir_name, '/');
    if (base_name)
        base_name++;
    else
        base_name = dump_dir_name;

    struct dump_dir *dd_dst;
    unsigned count = 100;
    char *dst_dir_name = concat_path_file(base_dir, base_name);
    while (1)
    {
        dd_dst = dd_create(dst_dir_name, (uid_t)-1);
        free(dst_dir_name);
        if (dd_dst)
            break;
        if (--count == 0)
        {
            error_msg("Can't create new dump dir in '%s'", base_dir);
            return NULL;
        }
        struct timeval tv;
        gettimeofday(&tv, NULL);
        dst_dir_name = xasprintf("%s/%s.%u", base_dir, base_name, (int)tv.tv_usec);
    }

    VERB1 log("Creating copy in '%s'", dd_dst->dd_dir);
    if (copy_file_recursive(dump_dir_name, dd_dst->dd_dir) < 0)
    {
        /* error. copy_file_recursive already emitted error message */
        /* Don't leave half-copied dir lying around */
        dd_delete(dd_dst);
        return NULL;
    }

    return dd_dst;
}
