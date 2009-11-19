/*
    RunApp.cpp

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
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


#include "RunApp.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#include "abrtlib.h"

#define COMMAND     0
#define FILENAME    1

using namespace std;

void CActionRunApp::Run(const char *pActionDir, const char *pArgs)
{
    /* Don't update_client() - actions run at crash time */
    log("RunApp('%s','%s')", pActionDir, pArgs);

    vector_string_t args;
    parse_args(pArgs, args, '"');

    const char *cmd = args[COMMAND].c_str();
    if (!cmd[0])
    {
        return;
    }

//FIXME: need to be able to escape " in .conf
    /* Chdir to the dump dir. Command can analyze component and such.
     * Example:
     * test x"`cat component`" = x"xorg-x11-apps" && cp /var/log/Xorg.0.log .
     */
//Can do it using chdir() in child if we'd open-code popen
    string cd_and_cmd = ssprintf("cd '%s'; %s", pActionDir, cmd);
    VERB1 log("RunApp: executing '%s'", cd_and_cmd.c_str());
    FILE *fp = popen(cd_and_cmd.c_str(), "r");
    if (fp == NULL)
    {
        /* Happens only on resource starvation (fork fails or out-of-mem) */
        return;
    }

//FIXME: RunApp("gzip -9 </var/log/Xorg.0.log", "Xorg.0.log.gz") fails
//since we mangle NULs.
    string output;
    char line[1024];
    while (fgets(line, 1024, fp) != NULL)
    {
        if (args.size() > FILENAME)
            output += line;
    }
    pclose(fp);

    if (args.size() > FILENAME)
    {
        CDebugDump dd;
        dd.Open(pActionDir);
        dd.SaveText(args[FILENAME].c_str(), output.c_str());
    }
}

PLUGIN_INFO(ACTION,
            CActionRunApp,
            "RunApp",
            "0.0.1",
            "Runs a command, saves its output",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
