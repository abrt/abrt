#define _GNU_SOURCE
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

static void try_to_segfault(void)
{
    fprintf(NULL, "Going to die!");
}

int main(int argc, char *argv[])
{
    int r;
    int status;
    pid_t pid;
    pid_t tid;

    if (unshare(CLONE_NEWPID) == -1)
        err(EXIT_FAILURE, "Failed to unshare PID");

    pid = fork();

    if (pid == -1)
        err(EXIT_FAILURE, "Failed to create child process");

    if (pid == 0)
    {
        tid = getpid();

        if (tid != 1)
            errx(EXIT_FAILURE, "TID is not 1 : %d", tid);

        try_to_segfault();

        errx(EXIT_FAILURE, "Failed to segfault");
    }

    r = waitpid(pid, &status, 0);

    if (r == -1)
        err(EXIT_FAILURE, "Failed to wait for child");

    if (WIFEXITED(status))
        errx(EXIT_FAILURE, "Child gracefully exited but should crash");

    if (WIFSIGNALED(status))
    {
        if (SIGSEGV != WTERMSIG(status) && SIGBUS != WTERMSIG(status))
            errx(EXIT_FAILURE, "Child did not crash : %d", WTERMSIG(status));

        return EXIT_SUCCESS;
    }

    errx(EXIT_FAILURE, "Child exit failed");
}
