/*
    Packages.h - PackageKit wrapper

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

#ifndef PACKAGES_H_
#define PACKAGES_H_

#include <glib.h>
#include <packagekit-glib/packagekit.h>
#include <string>
#include <set>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>

class CPackages
{
    private:
        typedef std::set<std::string> set_fingerprints_t;

        PkClient *m_pPkClient;
        bool m_bBusy;

        set_fingerprints_t m_setFingerprints;


        bool CheckFingerprint(const Header& pHeader);
        bool CheckHash(const Header& pHeader, const rpmts& pTs, const std::string&pPath);

    public:
        CPackages();
        ~CPackages();
        std::string SearchFile(const std::string& pPath);

        bool Install(const std::string& pPackage);
        bool GetInstallationStatus();
};

#endif /* PACKAGES_H_ */
