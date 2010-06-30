/*
    Copyright (C) 2009  RedHat inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "run-command.h"
#include "abrtlib.h"

/*
  Inspired by git code.
  http://git.kernel.org/?p=git/git.git;a=blob;f=run-command.c;hb=HEAD
*/

static pid_t start_command(char **argv)
{
  pid_t pid = vfork();
  if (pid < 0)
  {
    perror_msg_and_die("vfork");
  }
  if (pid == 0)
  { // new process
    execvp(argv[0], argv);
    exit(127);
  }
  return pid;
}

static int finish_command(pid_t pid, char **argv)
{
  pid_t waiting;
  int status;
  while ((waiting = waitpid(pid, &status, 0)) < 0 && errno == EINTR)
    continue;
  if (waiting < 0)
    perror_msg_and_die("waitpid");

  int code = -1;
  if (WIFSIGNALED(status))
  {
    code = WTERMSIG(status);
    error_msg("'%s' killed by signal %d", argv[0], code);
    code += 128; /* shells use this convention for deaths by signal */
  }
  else /* if (WIFEXITED(status)) */
  {
    code = WEXITSTATUS(status);
    if (code == 127)
    {
      error_msg_and_die("Can't run '%s'", argv[0]);
    }
  }

  return code;
}

int run_command(char **argv)
{
  pid_t pid = start_command(argv);
  return finish_command(pid, argv);
}
