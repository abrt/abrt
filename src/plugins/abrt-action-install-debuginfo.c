#include <unistd.h>
#include <string.h>

#define EXECUTABLE "abrt-action-install-debuginfo.py"

static void error_msg_and_die(const char *msg, const char *arg)
{
    write(2, msg, strlen(msg));
    if (arg)
    {
        write(2, " '", 2);
        write(2, msg, strlen(msg));
        write(2, "'", 1);
    }
    write(2, "\n", 1);
    exit(1);
}


/* A binary wrapper is needed around python scripts if we want
 * to run them in sgid/suid mode.
 *
 * This is such a wrapper.
 */
int main(int argc, char **argv)
{
    /*
     * We disallow passing of arguments which point to writable dirs.
     * This way, the script will always use default arguments.
     */
    char **pp = argv;
    char *arg;
    while ((arg = *++pp) != NULL)
    {
        if (strncmp(arg, "--cache", 7) == 0)
            error_msg_and_die("bad option", arg);
        if (strncmp(arg, "--tmpdir", 8) == 0)
            error_msg_and_die("bad option", arg);
    }

    execvp(EXECUTABLE, argv);
    error_msg_and_die("Can't execute", EXECUTABLE);
}
