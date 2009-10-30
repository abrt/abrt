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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "DebugDump.h"
#if HAVE_CONFIG_H
#include <config.h>
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
parse_opt (int key, char *arg, struct argp_state *state)
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

int main(int argc, char** argv)
{
  struct arguments arguments;
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  // Read the backtrace from stdin.
  int c;
  int capacity = 1024;
  char *bt = (char*)malloc(capacity);
  if (!bt)
  {
    fprintf(stderr, "Error while allocating memory for backtrace.\n");
    return 1;
  }
  char *btptr = bt;
  while ((c = getchar()) != EOF)
  {
    *btptr++ = (char)c;
    if (btptr - bt >= capacity - 1)
    {
      capacity *= 2;
      if (capacity > 1048576) // > 1 MB
      {
	fprintf(stderr, "Backtrace size limit exceeded. Trimming to 1 MB.\n");
	break;
      }
	
      bt = (char*)realloc(bt, capacity);
      if (!bt)
      {
	fprintf(stderr, "Error while allocating memory for backtrace.\n");
	return 1;
      }
    }
  }
  *btptr = '\0';

  // Create directory with the debug dump.
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/pyhook-%ld-%s", DEBUG_DUMPS_DIR, 
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
