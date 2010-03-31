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
#include "abrtlib.h"
#include "abrt_types.h"
#include "ABRTException.h"
#include "SOSreport.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

using namespace std;

static void ErrorCheck(int pos)
{
    if (pos < 0)
    {
        throw CABRTException(EXCEP_PLUGIN, "Can't find filename in sosreport output");
    }
}

static string ParseFilename(const string& pOutput)
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

    int line_end = pOutput.find_first_of('\n', filename_start);
    ErrorCheck(p);

    int filename_end = pOutput.find_last_not_of(" \n\t", line_end);
    ErrorCheck(p);

    return pOutput.substr(filename_start, filename_end - filename_start + 1);
}

void CActionSOSreport::Run(const char *pActionDir, const char *pArgs, int force)
{
    if (!force)
    {
        CDebugDump dd;
        dd.Open(pActionDir);
        bool bt_exists = dd.Exist("sosreport.tar.bz2");
        if (bt_exists)
        {
            VERB3 log("%s already exists, not regenerating", "sosreport.tar.bz2");
            return;
        }
    }

    static const char command_default[] = "nice sosreport --batch"
                                            " --only=anaconda --only=bootloader"
                                            " --only=devicemapper --only=filesys --only=hardware --only=kernel"
                                            " --only=libraries --only=memory --only=networking --only=nfsserver"
                                            " --only=pam --only=process --only=rpm -k rpm.rpmva=off --only=ssh"
                                            " --only=startup --only=yum 2>&1";
    static const char command_prefix[] = "nice sosreport --batch --no-progressbar";
    string command;

    vector_string_t args;
    parse_args(pArgs, args, '"');

    if (args.size() == 0 || args[0] == "")
    {
        command = command_default;
    }
    else
    {
        command = ssprintf("%s %s 2>&1", command_prefix, args[0].c_str());
    }

    update_client(_("Running sosreport: %s"), command.c_str());
    string output = command;
    output += '\n';
    char *command_out = run_in_shell_and_save_output(/*flags:*/ 0, command.c_str(), /*dir:*/ NULL, /*size_p:*/ NULL);
    output += command_out;
    free(command_out);
    update_client(_("Done running sosreport"));
    VERB3 log("sosreport output:'%s'", output.c_str());

    // Parse:
    // "Your sosreport has been generated and saved in:
    //  /tmp/sosreport-XXXX.tar.bz2"
    string sosreport_filename = ParseFilename(output);
    string sosreport_dd_filename = concat_path_file(pActionDir, "sosreport.tar.bz2");

    CDebugDump dd;
    dd.Open(pActionDir);
    //Not useful: dd.SaveText("sosreportoutput", output);
    off_t sz = copy_file(sosreport_filename.c_str(), sosreport_dd_filename.c_str(), 0644);
    unlink(sosreport_filename.c_str());            // don't want to leave sosreport-XXXX.tar.bz2 in /tmp
    unlink((sosreport_filename + ".md5").c_str()); // sosreport-XXXX.tar.bz2.md5 too
    if (sz < 0)
    {
        dd.Close();
        throw CABRTException(EXCEP_PLUGIN,
                "Can't copy '%s' to '%s'",
                sosreport_filename.c_str(),
                sosreport_dd_filename.c_str()
        );
    }
}

PLUGIN_INFO(ACTION,
            CActionSOSreport,
            "SOSreport",
            "0.0.2",
            "Runs sosreport, saves the output",
            "gavin@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
