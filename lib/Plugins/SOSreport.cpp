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

#include <ext/stdio_filebuf.h>
#include <fstream>
#include <sstream>
#include "abrtlib.h"
#include "abrt_types.h"
#include "ABRTException.h"
#include "SOSreport.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

static void ErrorCheck(int pos)
{
    if (pos < 0)
    {
        throw CABRTException(EXCEP_PLUGIN, "Can't find filename in sosreport output");
    }
}

static std::string ParseFilename(const std::string& pOutput)
{
    /*
    the sosreport's filename is embedded in sosreport's output.
    It appears on the line after the string in 'sosreport_filename_marker',
    it has leading spaces, and a trailing newline.  This function trims
    any leading and trailing whitespace from the filename.
    */
    static const char sosreport_filename_marker[] =
          "Your sosreport has been generated and saved in:";

    int p = pOutput.find(sosreport_filename_marker);
    ErrorCheck(p);

    p += sizeof(sosreport_filename_marker)-1;

    int filename_start = pOutput.find_first_not_of(" \n\t", p);
    ErrorCheck(p);

    int line_end = pOutput.find_first_of('\n',filename_start);
    ErrorCheck(p);

    int filename_end = pOutput.find_last_not_of(" \n\t", line_end);
    ErrorCheck(p);

    return pOutput.substr(filename_start, filename_end - filename_start + 1);
}

/* TODO: do not duplicate: RunApp.cpp has same function too */
static void ParseArgs(const char *psArgs, vector_string_t& pArgs)
{
    unsigned ii;
    bool is_quote = false;
    std::string item;

    for (ii = 0; psArgs[ii]; ii++)
    {
        if (psArgs[ii] == '"')
        {
            is_quote = !is_quote;
        }
        else if (psArgs[ii] == ',' && !is_quote)
        {
            pArgs.push_back(item);
            item.clear();
        }
        else
        {
            item += psArgs[ii];
        }
    }

    if (item.size() != 0)
    {
        pArgs.push_back(item);
    }
}

void CActionSOSreport::Run(const char *pActionDir, const char *pArgs)
{
    update_client(_("Executing SOSreport plugin..."));

    static const char command_default[] = "sosreport --batch --no-progressbar --only=anaconda --only=bootloader"
                                            " --only=devicemapper --only=filesys --only=hardware --only=kernel"
                                            " --only=libraries --only=memory --only=networking --only=nfsserver"
                                            " --only=pam --only=process --only=rpm -k rpm.rpmva=off --only=ssh"
                                            " --only=startup --only=yum 2>&1";
    static const char command_prefix[] = "sosreport --batch --no-progressbar";
    std::string command;

    vector_string_t args;
    ParseArgs(pArgs, args);

    if (args.size() == 0 || args[0] == "")
    {
        command = command_default;
    }
    else
    {
        command = ssprintf("%s %s 2>&1", command_prefix, args[0].c_str());
    }

    update_client(_("running sosreport: %s"), command.c_str());
    FILE *fp = popen(command.c_str(), "r");
    if (fp == NULL)
    {
        throw CABRTException(EXCEP_PLUGIN, ssprintf("Can't execute '%s'", command.c_str()));
    }

//vda TODO: fix this mess
    std::ostringstream output_stream;
    __gnu_cxx::stdio_filebuf<char> command_output_buffer(fp, std::ios_base::in);

    output_stream << command << std::endl;
    output_stream << &command_output_buffer;

    pclose(fp);
    update_client(_("done running sosreport"));

    std::string output = output_stream.str();

    std::string sosreport_filename = ParseFilename(output);
    std::string sosreport_dd_filename = concat_path_file(pActionDir, "sosreport.tar.bz2");

    CDebugDump dd;
    dd.Open(pActionDir);
    //Not useful
    //dd.SaveText("sosreportoutput", output);
    if (copy_file(sosreport_filename.c_str(), sosreport_dd_filename.c_str()) < 0)
    {
        throw CABRTException(EXCEP_PLUGIN,
                ssprintf("Can't copy '%s' to '%s'",
                        sosreport_filename.c_str(),
                        sosreport_dd_filename.c_str()
                )
        );
    }
}

PLUGIN_INFO(ACTION,
            CActionSOSreport,
            "SOSreport",
            "0.0.2",
            "Run sosreport, save the output in the crash dump",
            "gavin@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
