#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

/*
  Launches Retrace Server worker (worker.py) with root permissions.
  Binary needs to be owned by root and needs to set SUID bit.
*/

int main(int argc, char **argv)
{
  char command[256];
  FILE *pipe;
  int i;

  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s task_id\n", argv[0]);
    return 1;
  }

  if (setuid(0) != 0)
  {
    fprintf(stderr, "You must run %s with root permissions.\n", argv[0]);
    return 2;
  }

  for (i = 0; argv[1][i]; ++i)
    if (!isdigit(argv[1][i]))
    {
      fputs("Task ID may only contain digits.", stderr);
      return 3;
    }

  /* needs to be set to make mock work properly */
  setenv("SUDO_USER", "root", 1);
  setenv("SUDO_UID", "0", 1);
  setenv("SUDO_GID", "0", 1);

  /* launch worker.py */
  sprintf(command, "/usr/bin/python /usr/share/abrt-retrace/worker.py \"%s\"", argv[1]);
  pipe = popen(command, "r");
  if (pipe == NULL)
  {
    fputs("Unable to run 'worker.py'.", stderr);
    return 4;
  }

  return pclose(pipe) >> 8;
}
