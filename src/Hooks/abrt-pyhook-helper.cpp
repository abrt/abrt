/*
    python-hook-writer.cpp - writes data to the /var/cache/abrt directory
    with SUID bit

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
#include <argp.h>
/* We can easily get rid of abrtlib (libABRTUtils.so) usage in this file,
 * but DebugDump will pull it in anyway */
#include "abrtlib.h"
#include "DebugDump.h"
#if HAVE_CONFIG_H
# include <config.h>
#endif

const char *argp_program_version = "abrt-pyhook-helper " VERSION;
const char *argp_program_bug_address = "<crash-catcher@lists.fedorahosted.org>";

static char doc[] = "abrt-pyhook-helper -- stores crash data to abrt shared directory";

static struct argp_option options[] = {
  {"pid"       , 'p', "PID"     , 0, "PID of process that caused the crash" },
  {"executable", 'e', "PATH"    , 0, "absolute path to the program that crashed" },
  {"uuid"      , 'u', "UUID"    , 0, "hash generated from the backtrace"},
  {"cmdline"   , 'c', "TEXT"    , 0, "command line of the crashed program"},
  {"loginuid"  , 'l', "UID"     , 0, "login UID"},
  { 0 }
};

struct arguments
{
  char *pid;
  char *executable;
  char *uuid;
  char *cmdline;
  char *loginuid;
};

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key)
  {
  case 'p': arguments->pid = arg; break;
  case 'e': arguments->executable = arg; break;
  case 'u': arguments->uuid = arg; break;
  case 'c': arguments->cmdline = arg; break;
  case 'l': arguments->loginuid = arg; break;

  case ARGP_KEY_ARG:
    argp_usage(state);
    exit(1);
    break;

  case ARGP_KEY_END:
    if (!arguments->pid)
    {
      argp_usage(state);
      exit(1);
    }
    break;

  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, 0, doc };

#define MAX_BT_SIZE (1024*1024)

int main(int argc, char** argv)
{
  struct arguments arguments;
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  // Read the backtrace from stdin.
  char *bt = (char*)xmalloc(MAX_BT_SIZE);
  ssize_t len = full_read(STDIN_FILENO, bt, MAX_BT_SIZE-1);
  if (len < 0)
  {
    perror_msg_and_die("Read error");
  }
  bt[len] = '\0';
  if (len == MAX_BT_SIZE-1)
  {
    error_msg("Backtrace size limit exceeded, trimming to 1 MB");
  }

  // Create directory with the debug dump.
  char path[PATH_MAX];
  snprintf(path, sizeof(path), DEBUG_DUMPS_DIR"/pyhook-%ld-%s",
	   (long)time(NULL), arguments.pid);

  CDebugDump dd;
  dd.Create(path, geteuid());
  dd.SaveText(FILENAME_ANALYZER, "Python");
  if (arguments.executable)
    dd.SaveText(FILENAME_EXECUTABLE, arguments.executable);
  if (arguments.cmdline)
    dd.SaveText("cmdline", arguments.cmdline);
  if (arguments.uuid)
    dd.SaveText("uuid", arguments.uuid);
  if (arguments.loginuid)
    dd.SaveText("uid", arguments.loginuid);
  dd.SaveText("backtrace", bt);
  free(bt);
  dd.Close();

  return 0;
}
