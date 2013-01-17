#include <stdio.h>
#include <dump_dir.h>
#include <errno.h>


int main(int argc, char **argv)
{
    int retval = 0;
    struct dump_dir *dd = dd_opendir(argv[1], 0);
    char *content = dd_load_text_ext(dd, "notlink", DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    printf("Loaded text: '%s'\n", content);
    if (!content || strcmp(content, "Hello World"))
        retval = 1;
    dd_close(dd);

    return retval;
}
