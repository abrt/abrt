/*
    RPMInfo.h - header file for rpm database
              - it implements query for local rpm database

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

#ifndef RPMINFO_H_
#define RPMINFO_H_

#include "MiddleWareTypes.h"

#include <rpm/rpmcli.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>

class CRPMInfo
{
    private:

        typedef set_strings_t set_fingerprints_t;

        poptContext m_poptContext;
        set_fingerprints_t m_setFingerprints;

    public:
        CRPMInfo();
        ~CRPMInfo();

        void LoadOpenGPGPublicKey(const std::string& pFileName);

        bool CheckFingerprint(const std::string& pPackage);
        bool CheckHash(const std::string& pPackage, const std::string&pPath);
        std::string GetPackage(const std::string& pFileName);
};

#endif /* RPMINFO_H_ */
