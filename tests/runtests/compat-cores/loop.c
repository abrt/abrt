#include <stdio.h>

int main()
{
  printf("UID=%d EUID=%d GID=%d EGID=%d\n",
          getuid(), geteuid(), getgid(), getegid());

  while(1){ };
  return 0;
}
