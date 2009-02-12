/*
    Packages.cpp - PackageKit wrapper

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

#include "Packages.h"
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmcli.h>
#include <sstream>


CPackages::CPackages() :
    m_pPkClient(NULL),
    m_bBusy(false)
{
    g_type_init();
    m_pPkClient = pk_client_new();
//    pk_client_set_synchronous (m_pPkClient, TRUE, NULL);
}

CPackages::~CPackages()
{
    g_object_unref(m_pPkClient);
}

std::string CPackages::SearchFile(const std::string& pPath)
{
    std::stringstream ss;
    char *argv[] = {(char*)""};
    poptContext context = rpmcliInit(0, argv, NULL);
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, pPath.c_str(), 0);
    Header header;
    char* nerv = NULL;

    if ((header = rpmdbNextIterator(iter)) != NULL)
    {
        nerv = headerGetNEVR(header, NULL);
    }

    headerFree(header);
    rpmcliFini(context);
    rpmtsFree(ts);

    if (nerv != NULL)
    {
        std::string ret = nerv;
        free(nerv);
        return ret;
    }

    return "";
}

bool CPackages::Install(const std::string& pPackage)
{
    // TODO: write this
}


bool CPackages::GetInstallationStatus()
{
    GError *error = NULL;
    PkStatusEnum status;
    gboolean ret = pk_client_get_status(m_pPkClient, &status, &error);
    if (ret == FALSE)
    {
        std::string err = error->message;
        g_error_free(error);
        error = NULL;
        throw std::string("CPackages::SearchFile(): ") + err;
    }
    if (status != PK_STATUS_ENUM_FINISHED)
    {
        return false;
    }
    return true;
}
