#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
  Launches Retrace Server worker (worker.py) with root permissions.
  Binary needs to be owned by root and needs to set SUID bit.
*/

int main(int argc, char **argv)
{
  char command[1024], line[256];
  FILE *pipe, *log;
  int retcode;

  if (argc != 2)
  {
    printf("Usage: %s retrace_directory\n", argv[0]);
    return 1;
  }

  if (setgid(0) != 0 || setuid(0) != 0)
  {
    printf("You must run %s with root permissions.\n", argv[0]);
    return 2;
  }

  /* do not allow double quotes in path */
  if (strchr(argv[1], '"') != NULL){
    puts("Invalid character(s) in retrace_directory.");
    return 3;
  }

  sprintf(command, "%s/log", argv[1]);

  /* create retrace log */
  log = fopen(command, "w");
  if (log == NULL)
  {
    puts("Error writing log file.");
    return 4;
  }

  fputs("Starting retrace\n", log);

  sprintf(command, "/usr/bin/python /usr/share/abrt-retrace/worker.py \"%s\"", argv[1]);

  /* needs to be set to make mock work properly */
  setenv("SUDO_USER", "root", 1);
  setenv("SUDO_UID", "0", 1);
  setenv("SUDO_GID", "0", 1);

  /* launch worker.py */
  pipe = popen(command, "r");
  if (pipe == NULL)
  {
    puts("Unable to run 'worker.py'.");
    return 5;
  }

  /* read worker's output */
  while (fgets(line, 255, pipe) != NULL)
  {
    fputs(line, log);
    fflush(log);
  }

  retcode = pclose(pipe) >> 8;

  /* make backtrace accessible for clients */
  if (retcode == 0)
  {
    fputs("Retrace successful\n", log);

    sprintf(command, "mv \"%s/backtrace\" \"%s/retrace_backtrace\"", argv[1], argv[1]);
    system(command);
  }
  else
  {
    fputs("Retrace failed\n", log);
  }

  fclose(log);

  /* make log accessible for clients */
  sprintf(command, "mv \"%s/log\" \"%s/retrace_log\"", argv[1], argv[1]);
  system(command);

  return retcode;
}
