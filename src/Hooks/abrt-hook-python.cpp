/*
    abrt-hook-python.cpp - writes data to the /var/cache/abrt directory
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

#include <getopt.h>
#include <unistd.h>
/* We can easily get rid of abrtlib (libABRTUtils.so) usage in this file,
 * but DebugDump will pull it in anyway */
#include "abrtlib.h"
#include "DebugDump.h"
#if HAVE_CONFIG_H
# include <config.h>
#endif

#define MAX_BT_SIZE (1024*1024)

static char *pid;
static char *executable;
static char *uuid;
static char *cmdline;

int main(int argc, char** argv)
{
  // Parse options
  static const struct option longopts[] = {
    // name       , has_arg          , flag, val
    { "pid"       , required_argument, NULL, 'p' },
    { "executable", required_argument, NULL, 'e' },
    { "uuid"      , required_argument, NULL, 'u' },
    { "cmdline"   , required_argument, NULL, 'c' },
    { 0 },
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "p:e:u:c:l:", longopts, NULL)) != -1)
  {
    switch (opt)
    {
    case 'p':
      pid = optarg;
      break;
    case 'e':
      executable = optarg;
      break;
    case 'u':
      uuid = optarg;
      break;
    case 'c':
      cmdline = optarg;
      break;
    default:
 usage:
      error_msg_and_die(
                "Usage: abrt-hook-python [OPTIONS] <BACKTRACE\n"
                "\nOptions:\n"
                "	-p,--pid PID		PID of process that caused the crash\n"
                "	-p,--executable	PATH	absolute path to the program that crashed\n"
                "	-u,--uuid UUID		hash generated from the backtrace\n"
                "	-c,--cmdline TEXT	command line of the crashed program\n"
      );
    }
  }
  if (!pid)
    goto usage;
// is it really ok if other params aren't specified? abrtd might get confused...

  // Read the backtrace from stdin
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

  // Create directory with the debug dump
  char path[PATH_MAX];
  snprintf(path, sizeof(path), DEBUG_DUMPS_DIR"/pyhook-%ld-%s",
	   (long)time(NULL), pid);

  CDebugDump dd;
  dd.Create(path, geteuid());
  dd.SaveText(FILENAME_ANALYZER, "Python");
  if (executable)
    dd.SaveText(FILENAME_EXECUTABLE, executable);
  if (cmdline)
    dd.SaveText("cmdline", cmdline);
  if (uuid)
    dd.SaveText("uuid", uuid);

  char uid[sizeof(int) * 3 + 2];
  sprintf(uid, "%d", (int)getuid());
  dd.SaveText("uid", uid);

  dd.SaveText("backtrace", bt);
  free(bt);
  dd.Close();

  return 0;
}
