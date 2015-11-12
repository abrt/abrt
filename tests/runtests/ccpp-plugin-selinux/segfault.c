/* Author: Jakub Filak <jfilak@redhat.com>
 *
 * The program kills self with SIGSEGV.
 */
#define _POSIX_SOURCE

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

void die()
{
    kill(getpid(), SIGSEGV);
}

int main()
{
    printf("UID=%d EUID=%d GID=%d EGID=%d\n",
          getuid(), geteuid(), getgid(), getegid());
    die();
    return 0;
}
