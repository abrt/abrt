#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <err.h>

int main(int argc, char *argv[])
{
    if (argc != 1) {
        errx(EXIT_FAILURE, "Usage:\n  %s", argv[0]);
    }

    if (geteuid() != 0) {
        errx(EXIT_FAILURE, "No running under root user");
    }

    kill(getpid(), SIGSEGV);
    return 0;
}
