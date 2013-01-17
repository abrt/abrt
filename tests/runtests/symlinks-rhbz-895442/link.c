#include <stdio.h>
#include <dump_dir.h>
#include <errno.h>


int main(int argc, char **argv)
{
    int retval = 1;
    struct dump_dir *dd = dd_opendir(argv[1], 0);
    char *content = dd_load_text_ext(dd, "link", DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);

    /* ELOOP means "Too many levels of symbolic links" which happens when
     * trying to open a link with O_NOFOLLOW
     */
    if (errno == ELOOP && content == NULL)
        retval = 0;

    dd_close(dd);

    return retval;
}
