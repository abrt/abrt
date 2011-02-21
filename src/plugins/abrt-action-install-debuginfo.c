#include <unistd.h>
#include <string.h>

#define EXECUTABLE "abrt-action-install-debuginfo.py"

/* A binary wrapper is needed around python scripts if we want
 * to run them in sgid/suid mode.
 *
 * This is such a wrapper.
 */
int main(int argc, char **argv)
{
    execvp(EXECUTABLE, argv);
    write(2, "Can't execute "EXECUTABLE"\n", strlen("Can't execute "EXECUTABLE"\n"));
    return 1;
}
