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

struct child_process 
{
  const char **argv;
  pid_t pid;
};

static int start_command(struct child_process *cmd)
{
  cmd->pid = fork();
  if (cmd->pid == 0)
  { // new process
    execvp(cmd->argv[0], (char *const*)cmd->argv);
    exit(127);
  }
  if (cmd->pid < 0)
  {
    error_msg_and_die("Unable to fork for %s: %s", cmd->argv[0], strerror(errno));
    return -1;
  }
  return 0;
}

static int finish_command(struct child_process *cmd)
{
  pid_t waiting;
  int status, code = -1;
  while ((waiting = waitpid(cmd->pid, &status, 0)) < 0 && errno == EINTR)
    ;       /* nothing */
  
  if (waiting < 0) 
    error_msg_and_die("waitpid for %s failed: %s", cmd->argv[0], strerror(errno));
  else if (waiting != cmd->pid) 
    error_msg_and_die("waitpid is confused (%s)", cmd->argv[0]);
  else if (WIFSIGNALED(status)) 
  {
    code = WTERMSIG(status);
    error_msg("%s died of signal %d", cmd->argv[0], code);
  } 
  else if (WIFEXITED(status)) 
  {
    code = WEXITSTATUS(status);
    if (code == 127) 
    {
      code = -1;
      error_msg_and_die("cannot run %s: %s", cmd->argv[0], strerror(ENOENT));
    }
  } 
  else 
    error_msg_and_die("waitpid is confused (%s)", cmd->argv[0]);
  
  return code;
}

int run_command(const char **argv)
{
  struct child_process cmd;
  cmd.argv = argv;
  int code = start_command(&cmd);
  if (code)
    return code;
  return finish_command(&cmd);
}
