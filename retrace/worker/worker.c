#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  char command[1024], line[256];
  FILE *pipe, *log;
  int retcode;
  DIR *dir;
  struct dirent *entry;

  if (argc != 2)
  {
    printf("Usage: %s retrace_directory\n", argv[0]);
    return 1;
  }

  if (setuid(0) != 0)
  {
    printf("You must run %s with root permissions.\n", argv[0]);
    return 2;
  }

  sprintf(command, "%s/log", argv[1]);

  log = fopen(command, "w");
  fputs("Starting retrace\n", log);

  sprintf(command, "/usr/bin/python /usr/share/abrt-retrace/worker.py \"%s\"", argv[1]);

  pipe = popen(command, "r");
  while (fgets(line, 255, pipe) != NULL)
  {
    fputs(line, log);
  }

  retcode = pclose(pipe) >> 8;

  if (retcode == 0)
  {
    fputs("Retrace successful\n", log);

    dir = opendir(argv[1]);
    if (dir == NULL)
    {
      fputs("Cleanup error: opendir()", stderr);
    }
    else
    {
      while ((entry = readdir(dir)) != NULL)
      {
        if (entry->d_name[0] != '.' && strcmp("log", entry->d_name) != 0 && strcmp("backtrace", entry->d_name) != 0 && strcmp("password", entry->d_name) != 0)
        {
          sprintf(command, "rm -rf \"%s/%s\"", argv[1], entry->d_name);
          if (system(command) != 0)
          {
            fprintf(stderr, "Cleanup error: %s", command);
          }
        }
      }

      if (closedir(dir) < 0)
      {
        fputs("Cleanup error: closedir()", stderr);
      }
    }

    sprintf(command, "mv \"%s/backtrace\" \"%s/retrace_backtrace\"", argv[1], argv[1]);
    system(command);
  }
  else
  {
    fputs("Retrace failed\n", log);
  }

  fclose(log);

  sprintf(command, "mv \"%s/log\" \"%s/retrace_log\"", argv[1], argv[1]);
  system(command);

  return retcode;
}
