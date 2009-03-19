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

#define COMMAND 0
#define FILENAME 1

void CActionRunApp::ParseArgs(const std::string& psArgs, vector_args_t& pArgs)
{
    std::string::size_type ii_old = 0, ii_new = 0;
    ii_new = psArgs.find(",");
    while (ii_new != std::string::npos)
    {
        pArgs.push_back(psArgs.substr(ii_old, ii_new - ii_old));
        ii_old = ii_new + 1;
        ii_new = psArgs.find(",",ii_old);
    }
    pArgs.push_back(psArgs.substr(ii_old));
}

void CActionRunApp::Run(const std::string& pDebugDumpDir,
                        const std::string& pArgs)
{
    char line[1024];
    std::string output = "";

    vector_args_t args;

    FILE *fp = popen(args[COMMAND].c_str(), "r");
    if (fp == NULL)
    {
        throw "CActionRunApp::Run(): cannot execute " + args[COMMAND];
    }
    while (fgets(line, 1024, fp) != NULL)
    {
        output += line;
    }
    pclose(fp);

    if (args.size() > 1)
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.SaveText(args[FILENAME], output);
        dd.Close();
    }

}
