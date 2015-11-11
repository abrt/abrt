/* mlock() an allocated memory and kill self with SIGABRT
 *
 * Exits with 0 on failure (i.e. if it failed to lock a memory and die).
 *
 * Author: Jakub Filak <jfilak@redhat.com>
 */

#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#define SECRET_DATA "password"

int main (int argc, char **argv)
{
    char *password = (char *)malloc(sizeof(SECRET_DATA));
    if (!password) {
        err(EXIT_SUCCESS, "malloc");
    }

    memcpy(password, SECRET_DATA, sizeof(SECRET_DATA));

    if (mlock(password, sizeof(SECRET_DATA))) {
        err(EXIT_SUCCESS, "mlock");
    }

    abort();
}
