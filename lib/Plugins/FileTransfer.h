/*
    FileTransfer.h - header file for the file transfer plugin
                   - it uploads the file via ftp or sctp

    Copyright (C) 2009  Daniel Novotny (dnovotny@redhat.com)
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

#ifndef FILETRANSFER_H_
#define FILETRANSFER_H_

#include <string>
#include "Plugin.h"
#include "Action.h"

class CFileTransfer : public CAction
{
    private:
        std::string m_sURL;
        std::string m_sArchiveType;
        int m_nRetryCount;
        int m_nRetryDelay;

        void CreateArchive(const char *pArchiveName, const char *pDir);
        void SendFile(const char *pURL, const char *pFilename);

    public:
        CFileTransfer();
        virtual void SetSettings(const map_plugin_settings_t& pSettings);
//ok to delete?
//        virtual const map_plugin_settings_t& GetSettings();
        virtual void Run(const char *pActionDir, const char *pArgs);
};

#endif /* FILETRANSFER_H_ */
