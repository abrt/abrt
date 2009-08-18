/*
    SOSreport.cpp

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

#include "SOSreport.h"

#include <stdio.h>
#include <string.h>
#include <ext/stdio_filebuf.h>

#include <fstream>
#include <sstream>

#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"


void CActionSOSreport::CopyFile(const std::string& pSourceName, const std::string& pDestName)
{
    std::ifstream source(pSourceName.c_str(), std::fstream::binary);

    if (!source)
    {
        throw CABRTException(EXCEP_PLUGIN, "CActionSOSreport::CopyFile(): could not open input sosreport filename:" + pSourceName);
    }
    std::ofstream dest(pDestName.c_str(),std::fstream::trunc|std::fstream::binary);
    if (!dest)
    {
        throw CABRTException(EXCEP_PLUGIN, "CActionSOSreport::CopyFile(): could not open output sosreport filename:" + pDestName);
    }
    dest << source.rdbuf();
}

void CActionSOSreport::ErrorCheck(const index_type pI)
{
    if (pI == std::string::npos)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string("CActionSOSreport::ErrorCheck(): could not find filename in sosreport output"));
    }
}

std::string CActionSOSreport::ParseFilename(const std::string& pOutput)
{
    /*
    the sosreport's filename is embedded in sosreport's output.
    It appears on the line after the string in 'sosreport_filename_marker',
    it has leading spaces, and a trailing newline.  This function trims
    any leading and trailing whitespace from the filename.
    */
    static const char sosreport_filename_marker[] =
          "Your sosreport has been generated and saved in:";

    index_type p = pOutput.find(sosreport_filename_marker);
    ErrorCheck(p);

    p += strlen(sosreport_filename_marker);

    index_type filename_start = pOutput.find_first_not_of(" \n\t", p);
    ErrorCheck(p);

    index_type line_end = pOutput.find_first_of('\n',filename_start);
    ErrorCheck(p);

    index_type filename_end = pOutput.find_last_not_of(" \n\t",line_end);
    ErrorCheck(p);

    return pOutput.substr(filename_start,(filename_end-filename_start)+1);
}

void CActionSOSreport::Run(const std::string& pActionDir,
                           const std::string& pArgs)
{
    comm_layer_inner_status("Executing SOSreportAction plugin...");

    const char command[] = "sosreport --batch --no-progressbar -k rpm.rpmva=off 2>&1";

    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string("CActionSOSreport::Run(): cannot execute ") + command);
    }

    std::ostringstream output_stream;
    __gnu_cxx::stdio_filebuf<char> command_output_buffer(fp, std::ios_base::in);

    output_stream << std::string(command) << std::endl;
    output_stream << &command_output_buffer;

    pclose(fp);

    std::string output = output_stream.str();
    std::string sosreport_filename = ParseFilename(output);
    std::string sosreport_dd_filename = pActionDir + "/sosreport.tar.bz2";

    CDebugDump dd;
    dd.Open(pActionDir);
    CopyFile(sosreport_filename,sosreport_dd_filename);
    dd.Close();
}

PLUGIN_INFO(ACTION,
            CActionSOSreport,
            "SOSreport",
            "0.0.2",
            "Run sosreport, save the output in the crash dump",
            "gavin@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
