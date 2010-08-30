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
#include "abrtlib.h"
#include "RunApp.h"
#include "debug_dump.h"
#include "abrt_exception.h"
#include "comm_layer_inner.h"
#include "abrtlib.h"

#define COMMAND     0
#define FILENAME    1

using namespace std;

void CActionRunApp::Run(const char *pActionDir, const char *pArgs, int force)
{
    /* Don't update_client() - actions run at crash time, there is no client
     * to talk to at that point */
    log("RunApp('%s','%s')", pActionDir, pArgs);

    vector_string_t args;
    parse_args(pArgs, args, '"');

    if (args.size() <= COMMAND)
    {
        return;
    }
    const char *cmd = args[COMMAND].c_str();
    if (!cmd[0])
    {
        return;
    }

    /* NB: we chdir to the dump dir. Command can analyze component and such.
     * Example:
     * test x"`cat component`" = x"xorg-x11-apps" && cp /var/log/Xorg.0.log .
     */
    size_t cmd_out_size;
    char *cmd_out = run_in_shell_and_save_output(/*flags:*/ 0, cmd, pActionDir, &cmd_out_size);

    if (args.size() > FILENAME)
    {
        dump_dir_t *dd = dd_init();
        if (!dd_opendir(dd, pActionDir))
        {
            dd_close(dd);
            VERB1 log(_("Unable to open debug dump '%s'"), pActionDir);
            return;
        }

        dd_savebin(dd, args[FILENAME].c_str(), cmd_out, cmd_out_size);
        dd_close(dd);
    }

    free(cmd_out);
}

PLUGIN_INFO(ACTION,
            CActionRunApp,
            "RunApp",
            "0.0.1",
            _("Runs a command, saves its output"),
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
