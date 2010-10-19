/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#include "abrtlib.h"
#include "Python.h"
#include "abrt_exception.h"

using namespace std;

static string load(const char *dirname, const char *filename)
{
    string ret;

    struct dump_dir *dd = dd_opendir(dirname, /*flags:*/ 0);
    if (!dd)
        return ret; /* "" */
    char *s = dd_load_text(dd, filename);
    dd_close(dd);

    if (!s[0])
    {
        free(s);

        pid_t pid = fork();
        if (pid < 0)
        {
            perror_msg("fork");
            return ret; /* "" */
        }
        if (pid == 0) /* child */
        {
            char *argv[4];  /* abrt-action-analyze-python -d DIR <NULL> */
            char **pp = argv;
            *pp++ = (char*)"abrt-action-analyze-python";
            *pp++ = (char*)"-d";
            *pp++ = (char*)dirname;
            *pp = NULL;

            execvp(argv[0], argv);
            perror_msg_and_die("Can't execute '%s'", argv[0]);
        }
        /* parent */
        waitpid(pid, NULL, 0);

        dd = dd_opendir(dirname, /*flags:*/ 0);
        if (!dd)
            return ret; /* "" */
        s = dd_load_text(dd, filename);
        dd_close(dd);
    }

    ret = s;
    free(s);
    return ret;
}

string CAnalyzerPython::GetLocalUUID(const char *pDebugDumpDir)
{
    return load(pDebugDumpDir, CD_UUID);
}

string CAnalyzerPython::GetGlobalUUID(const char *pDebugDumpDir)
{
    return load(pDebugDumpDir, FILENAME_DUPHASH);
}

void CAnalyzerPython::Init()
{
}

void CAnalyzerPython::DeInit()
{
}

PLUGIN_INFO(ANALYZER,
            CAnalyzerPython,
            "Python",
            "0.0.1",
            _("Analyzes crashes in Python programs"),
            "zprikryl@redhat.com, jmoskovc@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
