/*
    abrt-hook-python.cpp - writes data to the /var/spool/abrt directory
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
#include <syslog.h>
// We can easily get rid of abrtlib (libABRTUtils.so) usage in this file,
// but DebugDump will pull it in anyway
#include "abrtlib.h"
#include "hooklib.h"
#include "DebugDump.h"
#include "CrashTypes.h"
#include "ABRTException.h"
#if HAVE_CONFIG_H
# include <config.h>
#endif

#define MAX_BT_SIZE (1024*1024)
#define MAX_BT_SIZE_STR "1 MB"

static char *pid;
static char *executable;

static bool printable_str(const char *str)
{
  do {
    if ((unsigned char)(*str) < ' ' || *str == 0x7f)
      return false;
    str++;
  } while (*str);
  return true;
}

int main(int argc, char** argv)
{
  // Parse options
  static const struct option longopts[] = {
    // name       , has_arg          , flag, val
    { "pid"       , required_argument, NULL, 'p' },
    { "executable", required_argument, NULL, 'e' },
    { 0 },
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "p:e:u:l:", longopts, NULL)) != -1)
  {
    switch (opt)
    {
    case 'p':
      pid = optarg;
      break;
    case 'e':
      executable = optarg;
      break;
    default:
 usage:
      error_msg_and_die(
                "Usage: abrt-hook-python [OPTIONS] <BACKTRACE\n"
                "\nOptions:\n"
                "	-p,--pid PID		PID of process that caused the crash\n"
                "	-p,--executable	PATH	absolute path to the program that crashed\n"
      );
    }
  }
  if (!pid || !executable)
    goto usage;
  if (strlen(executable) > PATH_MAX || !printable_str(executable))
    goto usage;
  // pid string is sanitized later by xatou()

  openlog("abrt", LOG_PID, LOG_DAEMON);
  logmode = LOGMODE_SYSLOG;

  // Error if daemon is not running
  if (!daemon_is_ok())
    error_msg_and_die("daemon is not running, python crash dump aborted");

  unsigned setting_MaxCrashReportsSize = 0;
  parse_conf(NULL, &setting_MaxCrashReportsSize, NULL, NULL);
  if (setting_MaxCrashReportsSize > 0)
  {
    check_free_space(setting_MaxCrashReportsSize);
  }

  // Read the backtrace from stdin
  char *bt = (char*)xmalloc(MAX_BT_SIZE);
  ssize_t len = full_read(STDIN_FILENO, bt, MAX_BT_SIZE-1);
  if (len < 0)
  {
    perror_msg_and_die("read error");
  }
  bt[len] = '\0';
  if (len == MAX_BT_SIZE-1)
  {
    error_msg("backtrace size limit exceeded, trimming to " MAX_BT_SIZE_STR);
  }

  // This also checks that pid is a valid numeric string
  char *cmdline = get_cmdline(xatou(pid)); // never NULL

  // Create directory with the debug dump
  char path[PATH_MAX];
  unsigned path_len = snprintf(path, sizeof(path), DEBUG_DUMPS_DIR"/pyhook-%ld-%s.new",
	   (long)time(NULL), pid);
  if (path_len >= sizeof(path))
    exit(1);
  CDebugDump dd;
  try {
    dd.Create(path, getuid());
  } catch (CABRTException &e) {
    dd.Delete();
    dd.Close();
    error_msg_and_die("error while creating crash dump %s: %s", path, e.what());
  }

  dd.SaveText(FILENAME_ANALYZER, "Python");
  dd.SaveText(FILENAME_EXECUTABLE, executable);
  dd.SaveText(FILENAME_BACKTRACE, bt);
  // python handler puts a short(er) crash descr in 1st line. Example:
  // "CCMainWindow.py:1:<module>:ZeroDivisionError: integer division or modulo by zero"
  strchrnul(bt, '\n')[0] = '\0';
  dd.SaveText(FILENAME_REASON, bt);
  free(bt);
  dd.SaveText(FILENAME_CMDLINE, cmdline);
  free(cmdline);
  char uid[sizeof(long) * 3 + 2];
  sprintf(uid, "%lu", (long)getuid());
  dd.SaveText(CD_UID, uid);

  dd.Close();

  char *newpath = xstrndup(path, path_len - (sizeof(".new")-1));
  if (rename(path, newpath) == 0)
    strcpy(path, newpath);
  free(newpath);

  log("saved python crash dump of pid %s to %s", pid, path);

  if (setting_MaxCrashReportsSize > 0)
  {
    trim_debug_dumps(setting_MaxCrashReportsSize, path);
  }

  return 0;
}
