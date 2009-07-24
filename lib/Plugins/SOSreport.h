/*
    SOSreport.h - Attach an sosreport to a crash dump

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

#ifndef SOSREPORT_H_
#define SOSREPORT_H_

#include "Action.h"

class CActionSOSreport : public CAction
{
    private:
        typedef std::string::size_type index_type;

        void CopyFile(const std::string& pSourceName, const std::string& pDestName);
        void ErrorCheck(const index_type pI);
        std::string ParseFilename(const std::string& pOutput);

    public:
        virtual ~CActionSOSreport() {}
        virtual void Run(const std::string& pActionDir,
                         const std::string& pArgs);
};

PLUGIN_INFO(ACTION,
            CActionSOSreport ,
            "SOSreport",
            "0.0.2",
            "Run sosreport, save the output in the crash dump",
            "gavin@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");

#endif
