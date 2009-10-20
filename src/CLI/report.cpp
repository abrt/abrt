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
#include "report.h"
#include "run-command.h"
#include "dbus.h"
#include "abrtlib.h"
#if HAVE_CONFIG_H
#include <config.h>
#endif
#if ENABLE_NLS
#include <libintl.h>
#define _(S) gettext(S)
#else
#define _(S) (S)
#endif

/* Field separator for the crash report file that is edited by user. */
#define FIELD_SEP "%----"

/* 
 * Escapes the field content string to avoid confusion with file comments. 
 * Returned field must be free()d by caller. 
 */
static char *escape(const char *str)
{
  // Determine the size of resultant string.
  // Count the required number of escape characters.
  // 1. NEWLINE followed by #
  // 2. NEWLINE followed by \# (escaped version)
  const char *ptr = str;
  bool newline = true;
  int count = 0;
  while (*ptr)
  {
    if (newline)
    {
      if (*ptr == '#')
	++count;
      if (*ptr == '\\' && *(ptr + 1) == '#')
	++count;
    }

    newline = (*ptr == '\n');
    ++ptr;
  }

  // Copy the input string to the resultant string, and escape all 
  // occurences of \# and #.
  char *result = (char*)malloc(strlen(str) + 1 + count);
  if (!result)
    error_msg_and_die("Memory error while escaping a field.");
  
  const char *src = str;
  char *dest = result;
  newline = true;
  while (*src)
  {
    if (newline)
    {
      if (*src == '#')
	*dest++ = '\\';
      else if (*src == '\\' && *(src + 1) == '#')
	*dest++ = '\\';
    }

    newline = (*src == '\n');
    *dest++ = *src++;
  }
  *dest = '\0';  
  return result;
}

/*
 * Removes all comment lines, and unescapes the string previously escaped 
 * by escape(). Works in-place. 
 */
static char *remove_comments_and_unescape(char *str)
{
  char *src = str, *dest = str;
  bool newline = true;
  while (*src)
  {
    if (newline)
    {
      if (*src == '#') 
      { // Skip the comment line!
	while (*src && *src != '\n')
	  ++src;

	if (*src == '\0')
	  break;

	++src;
	continue;
      }
      else if (*src == '\\' && 
	       (*(src + 1) == '#' || 
		(*(src + 1) == '\\' && *(src + 2) == '#')))
      {
	++src; // Unescape escaped char.
      }
    }
    
    newline = (*src == '\n');
    *dest++ = *src++;
  }
  *dest = '\0';
}

/* Writes a field of crash report to a file.
 * Field must be writable.
 */
static void write_crash_report_field(FILE *fp, const map_crash_report_t &report, 
				     const char *field, const char *description)
{
  const map_crash_report_t::const_iterator it = report.find(field);
  if (it == report.end())
  {
    error_msg("Field %s not found.\n", field);
    return;
  }

  if (it->second[CD_TYPE] == CD_SYS)
  {
    error_msg("Cannot write field %s because it is a system value\n", field);
    return; 
  }
  
  fprintf(fp, "%s%s\n", FIELD_SEP, it->first.c_str());

  fprintf(fp, "%s\n", description);
  if (it->second[CD_EDITABLE] != CD_ISEDITABLE)
    fprintf(fp, _("# This field is read only.\n"));

  char *escaped_content = escape(it->second[CD_CONTENT].c_str());
  fprintf(fp, "%s\n", escaped_content);
  free(escaped_content);
}

/* 
 * Saves the crash report to a file. 
 * Parameter 'fp' must be opened before write_crash_report is called.
 * Returned value:
 *  If the report is successfully stored to the file, a zero value is returned.
 *  On failure, nonzero value is returned.
 */
static int write_crash_report(const map_crash_report_t &report, FILE *fp)
{
  fprintf(fp, "# Please check this report. Lines starting with '#' will be ignored.\n"
	  "# Lines starting with '%%----' separate fields, please do not delete them.\n\n");

  write_crash_report_field(fp, report, "Comment", 
			   _("# Describe the circumstances of this crash below."));
  write_crash_report_field(fp, report, "How to reproduce",
			   _("# How to reproduce the crash?"));
  write_crash_report_field(fp, report, "backtrace",
			   _("# Stack trace: a list of active stack frames at the time the crash occurred\n# Check that it does not contain any sensitive data such as passwords."));
  write_crash_report_field(fp, report, "UUID", _("# UUID"));
  write_crash_report_field(fp, report, "architecture", _("# Architecture"));
  write_crash_report_field(fp, report, "cmdline", _("# Command line"));
  write_crash_report_field(fp, report, "component", _("# Component"));
  write_crash_report_field(fp, report, "coredump", _("# Core dump"));
  write_crash_report_field(fp, report, "executable", _("# Executable"));
  write_crash_report_field(fp, report, "kernel", _("# Kernel version"));
  write_crash_report_field(fp, report, "package", _("# Package"));
  write_crash_report_field(fp, report, "reason", _("# Reason of crash"));
  write_crash_report_field(fp, report, "release", _("# Release string of the operating system"));
  
  return 0;
}

/* 
 * Updates appropriate field in the report from the text. The text can
 * contain multiple fields. 
 */
static void read_crash_report_field(const char *text, map_crash_report_t &report, 
				    const char *field)
{
  char separator[strlen("\n" FIELD_SEP) + strlen(field) + 2]; // 2 = '\n\0'
  sprintf(separator, "\n%s%s\n", FIELD_SEP, field);
  const char *textfield = strstr(text, separator);
  if (!textfield)
    return;

  textfield += strlen(separator);
  int length = 0;
  const char *end = strstr(textfield, "\n" FIELD_SEP);
  if (!end)
    length = strlen(textfield);
  else 
    length = end - textfield;
  
  const map_crash_report_t::iterator it = report.find(field);
  if (it == report.end())
  {
    error_msg("Field %s not found.\n", field);
    return;
  }

  if (it->second[CD_TYPE] == CD_SYS)
  {
    error_msg("Cannot update field %s because it is a system value.\n", field);
    return; 
  }

  if (it->second[CD_EDITABLE] == CD_ISEDITABLE)
    it->second[CD_CONTENT].assign(textfield, length);
}

/* 
 * Updates the crash report 'report' from the text. The text must not contain 
 * any comments. 
 */
static int read_crash_report(map_crash_report_t &report, const char *text)
{
  read_crash_report_field(text, report, "Comment");
  read_crash_report_field(text, report, "How to reproduce");
  read_crash_report_field(text, report, "backtrace");
  read_crash_report_field(text, report, "UUID");
  read_crash_report_field(text, report, "architecture");
  read_crash_report_field(text, report, "cmdline");
  read_crash_report_field(text, report, "component");
  read_crash_report_field(text, report, "coredump");
  read_crash_report_field(text, report, "executable");
  read_crash_report_field(text, report, "kernel");
  read_crash_report_field(text, report, "package");
  read_crash_report_field(text, report, "reason");
  read_crash_report_field(text, report, "release");

  return 0;
}

/* Runs external editor. */
int launch_editor(const char *path)
{
  const char *editor, *terminal;
  
  editor = getenv("ABRT_EDITOR");
  if (!editor)
    editor = getenv("VISUAL");
  if (!editor)
    editor = getenv("EDITOR");
  
  terminal = getenv("TERM");
  if (!editor && (!terminal || !strcmp(terminal, "dumb")))
  {
    error_msg(_("Terminal is dumb but no VISUAL nor EDITOR defined."));
    return 1;
  }
  
  if (!editor)
    editor = "vi";

  const char *args[6];
  args[0] = editor;
  args[1] = path;
  run_command(args);

  return 0;
}

/* Reports the crash with corresponding uuid over DBus. */
int report(const char *uuid, bool always)
{
  map_crash_report_t cr = call_CreateReport(uuid);

  if (always)
  {
    call_Report(cr);
    return 0;
  }

  /* Open a temporary file and write the crash report to it. */
  char filename[] = "/tmp/abrt-report.XXXXXX";
  int fd = mkstemp(filename);
  if (fd == -1)
  {
    error_msg("could not generate temporary file name");
    return 1;
  }

  FILE *fp = fdopen(fd, "w");
  if (!fp)
  {
    error_msg("could not open '%s' to save the crash report", filename);
    return 1;
  }

  write_crash_report(cr, fp);

  if (fclose(fp))
  {
    error_msg("could not close '%s'", filename);
    return 2;
  }

  launch_editor(filename);

  fp = fopen(filename, "r");
  if (!fp)
  {
    error_msg("could not open '%s' to read the crash report", filename);
    return 1;
  }

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *text = (char*)malloc(size + 1);
  if (fread(text, 1, size, fp) != size)
  {
    error_msg("could not read '%s'", filename);
    return 1;
  }
  text[size] = '\0';
  fclose(fp);

  remove_comments_and_unescape(text);
  read_crash_report(cr, text);
  free(text);

  /*int result = */unlink(filename);

  printf(_("\nThe report has been updated.\nDo you want to send the report? [y/N]: "));
  fflush(NULL);
  char answer[16] = "n";
  fgets(answer, sizeof(answer), stdin);
  if (answer[0] == 'Y' || answer[0] == 'y')
    call_Report(cr);

  return 0;
}
