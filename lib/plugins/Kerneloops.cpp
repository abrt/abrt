/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Authors:
       Anton Arapov <anton@redhat.com>
       Arjan van de Ven <arjan@linux.intel.com>
 */

#include "abrtlib.h"
#include "Kerneloops.h"
#include "debug_dump.h"
#include "abrt_exception.h"
#include "comm_layer_inner.h"

static unsigned hash_oops_str(const char *oops_ptr)
{
    unsigned char old_c;
    unsigned char c = 0;
    unsigned hash = 0;

    /* Special-case: if the first line is of form:
     * WARNING: at net/wireless/core.c:614 wdev_cleanup_work+0xe9/0x120 [cfg80211]() (Not tainted)
     * then hash only "file:line func+ofs/len" part.
     */
    if (strncmp(oops_ptr, "WARNING: at ", sizeof("WARNING: at ")-1) == 0)
    {
        const char *p = oops_ptr + sizeof("WARNING: at ")-1;
        p = strchr(p, ' '); /* skip filename:NNN */
        if (p)
        {
            p = strchrnul(p + 1, ' '); /* skip function_name+0xNN/0xNNN */
            oops_ptr += sizeof("WARNING: at ")-1;
            while (oops_ptr < p)
            {
                c = *oops_ptr++;
                hash = ((hash << 5) ^ (hash >> 27)) ^ c;
            }
            return hash;
        }
    }

    while (1)
    {
        old_c = c;
        c = *oops_ptr++;
        if (!c)
            break;
        if (c == '\n')
        {
            // Exclude some lines which have process name - in some oops classes
            // process name is irrelevant and changes with every oops.
            // Lines we filter out:
            // Pid: 8003, comm: Xorg Not tainted (2.6.27.9-159.fc10.i686 #1)
            // Process Xorg (pid: 8003, ti=f0a0c000 task=f2380000 task.ti=f0a0c000)
            if (strncmp(oops_ptr, "Pid: ", 5) == 0
             || strncmp(oops_ptr, "Process ", 8) == 0
            ) {
                while (*oops_ptr && *oops_ptr != '\n')
                    oops_ptr++;
                continue;
            }
        }
        if (!isalnum(old_c))
        {
            if (c >= '0' && c <= '9')
            {
                // Convert all (possibly hex) numbers to just one '0'
                if (c == '0' && *oops_ptr == 'x') // "0xSOMETHING"
                    oops_ptr++;
                while (isxdigit(*oops_ptr))
                    oops_ptr++;
                c = '0';
            }
            else if ((c|0x20) >= 'a' && (c|0x20) <= 'f')
            {
                // This *may be* a hex number without 0x prefix: "f0a0c000"
                // Check that it indeed is, and replace with '0'
                const char *oops_ptr2 = oops_ptr;
                while (isxdigit(*oops_ptr2))
                    oops_ptr2++;
                // Does it end in a letter which is not a hex digit?
                // (Example: "abcw" is not a hex number, "abc " is)
                if (!isalpha(*oops_ptr2))
                {
                    // It's "abc " case. Skip the "abc" string
                    oops_ptr = oops_ptr2;
                    c = '0';
                }
                // else: hash the string as-is
            }
        }
        // TODO: Drop call trace tail - in interrupt-driven oopses,
        // everything before interrupt is irrelevant.
        // Example of call trace part of oops:
        // Call Trace:
        // [<f88e11c7>] ? radeon_cp_resume+0x7d/0xbc [radeon]
        // [<f88745f8>] ? drm_ioctl+0x1b0/0x225 [drm]
        // [<f88e114a>] ? radeon_cp_resume+0x0/0xbc [radeon]
        // [<c049b1c0>] ? vfs_ioctl+0x50/0x69
        // [<c049b414>] ? do_vfs_ioctl+0x23b/0x247
        // [<c0460a56>] ? audit_syscall_entry+0xf9/0x123
        // [<c049b460>] ? sys_ioctl+0x40/0x5c
        // [<c0403c76>] ? syscall_call+0x7/0xb

        /* An algorithm proposed by Donald E. Knuth in The Art Of Computer
         * Programming Volume 3, under the topic of sorting and search
         * chapter 6.4.
         */
        hash = ((hash << 5) ^ (hash >> 27)) ^ c;
    }
    return hash;
}

std::string CAnalyzerKerneloops::GetLocalUUID(const char *pDebugDumpDir)
{
    VERB3 log("Getting local universal unique identification");

    dump_dir_t *dd = dd_init();
    if (!dd_opendir(dd, pDebugDumpDir))
    {
        dd_close(dd);
        VERB1 log(_("Unable to open debug dump '%s'"), pDebugDumpDir);
        return std::string("");
    }

    char *oops = dd_loadtxt(dd, FILENAME_BACKTRACE);
    unsigned hash = hash_oops_str(oops);
    free(oops);

    hash &= 0x7FFFFFFF;

    return to_string(hash);
}

std::string CAnalyzerKerneloops::GetGlobalUUID(const char *pDebugDumpDir)
{
    return GetLocalUUID(pDebugDumpDir);
}

PLUGIN_INFO(ANALYZER,
            CAnalyzerKerneloops,
            "Kerneloops",
            "0.0.2",
            _("Analyzes kernel oopses"),
            "anton@redhat.com",
            "https://people.redhat.com/aarapov",
            "");
