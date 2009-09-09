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
#include <stdio.h>
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

#define COMMAND     0
#define FILENAME    1

void CActionRunApp::ParseArgs(const std::string& psArgs, vector_string_t& pArgs)
{
    unsigned int ii;
    bool is_quote = false;
    std::string item = "";
    for (ii = 0; ii < psArgs.length(); ii++)
    {
        if (psArgs[ii] == '\"')
        {
            is_quote = is_quote == true ? false : true;
        }
        else if (psArgs[ii] == ',' && !is_quote)
        {
            pArgs.push_back(item);
            item = "";
        }
        else
        {
            item += psArgs[ii];
        }
    }
    if (item != "")
    {
        pArgs.push_back(item);
    }
}

void CActionRunApp::Run(const std::string& pActionDir,
                        const std::string& pArgs)
{
    update_client(_("Executing RunApp plugin..."));

    char line[1024];
    std::string output = "";

    vector_string_t args;

    ParseArgs(pArgs, args);

    FILE *fp = popen(args[COMMAND].c_str(), "r");
    if (fp == NULL)
    {
        throw CABRTException(EXCEP_PLUGIN, "CActionRunApp::Run(): cannot execute " + args[COMMAND]);
    }
    while (fgets(line, 1024, fp) != NULL)
    {
        output += line;
    }
    pclose(fp);

    if (args.size() > 1)
    {
        CDebugDump dd;
        dd.Open(pActionDir);
        dd.SaveText(args[FILENAME].c_str(), output);
    }
}

PLUGIN_INFO(ACTION,
            CActionRunApp,
            "RunApp",
            "0.0.1",
            "Simple action plugin which runs a command "
            "and it can save command's output",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
