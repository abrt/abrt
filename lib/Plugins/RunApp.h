/*
    RunApp.h - Simple action plugin which execute command

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

#ifndef RUNAPP_H_
#define RUNAPP_H_

#include "Action.h"
#include <string>
#include <vector>

class CActionRunApp : public CAction
{
    private:
        typedef std::vector<std::string> vector_args_t;
        void ParseArgs(const std::string& psArgs, vector_args_t& pArgs);

    public:
        virtual ~CActionRunApp() {}
        virtual void Run(const std::string& pDebugDumpDir,
                         const std::string& pParams);
};

PLUGIN_INFO(ACTION,
            CActionRunApp,
            "RunApp",
            "0.0.1",
            "Simple action plugin which runs a command "
            "and it can save command's output",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/crash-catcher/wiki");

#endif
