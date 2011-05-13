#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
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
  struct passwd *apache_user;
  const char *apache_username = "apache";
  pid_t pid;

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

  apache_user = getpwnam(apache_username);
  if (!apache_user)
  {
    fprintf(stderr, "User \"%s\" not found.\n", apache_username);
    return 4;
  }

  sprintf(command, "%d", apache_user->pw_uid);

  setenv("SUDO_USER", apache_username, 1);
  setenv("SUDO_UID", command, 1);
  /* required by mock to be able to write into result directory */
  setenv("SUDO_GID", "0", 1);

  /* fork and launch worker.py */
  pid = fork();

  if (pid < 0)
  {
    fputs("Unable to fork.", stderr);
    return 6;
  }

  /* parent - exit */
  if (pid > 0)
      return 0;

  /* child */
  sprintf(command, "/usr/bin/python /usr/share/abrt-retrace/worker.py \"%s\"", argv[1]);
  pipe = popen(command, "r");
  if (pipe == NULL)
  {
    fputs("Unable to run 'worker.py'.", stderr);
    return 5;
  }

  return pclose(pipe) >> 8;
}
