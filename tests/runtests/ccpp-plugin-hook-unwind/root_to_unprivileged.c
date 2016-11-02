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
    if (argc != 2) {
        errx(EXIT_FAILURE, "Usage:\n  %s USERNAME", argv[0]);
    }

    if (getuid() != 0) {
        errx(EXIT_FAILURE, "No running under root user");
    }

    const char *const username = argv[1];
    struct passwd *pwd = getpwnam(username);

    if (!pwd) {
        err(EXIT_FAILURE, "Cannot find passwd entry for the %s user", username);
    }

    if (setresgid(pwd->pw_gid, pwd->pw_gid, pwd->pw_gid) == -1) {
        err(EXIT_FAILURE, "Can't change gid!\n");
    }

    if (setresuid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid) == -1) {
        err(EXIT_FAILURE, "Can't change uid!\n");
    }

    kill(getpid(), SIGSEGV);
    return 0;
}
