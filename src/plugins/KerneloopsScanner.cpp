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

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    Authors:
    Anton Arapov <anton@redhat.com>
    Arjan van de Ven <arjan@linux.intel.com>
*/
#include <syslog.h>
#include <asm/unistd.h> /* __NR_syslog */
#include <glib.h>
#include "abrtlib.h"
#include "comm_layer_inner.h"
#include "KerneloopsSysLog.h"
#include "KerneloopsScanner.h"

// TODO: https://fedorahosted.org/abrt/ticket/78

static int scan_dmesg(GList **oopsList)
{
    VERB1 log("Scanning dmesg");

    /* syslog(3) - read the last len bytes from the log buffer
     * (non-destructively), but dont read more than was written
     * into the buffer since the last"clear ring buffer" cmd.
     * Returns the number of bytes read.
     */
    char *buffer = (char*)xzalloc(16*1024);
    syscall(__NR_syslog, 3, buffer, 16*1024 - 1); /* always NUL terminated */
    int cnt_FoundOopses = extract_oopses(oopsList, buffer, strlen(buffer));
    free(buffer);

    return cnt_FoundOopses;
}


/* "dumpoops" tool uses these two functions too */
extern "C" {

int scan_syslog_file(GList **oopsList, const char *filename, time_t *last_changed_p)
{
    VERB1 log("Scanning syslog file '%s'", filename);

    char *buffer;
    struct stat statb;
    int fd;
    int cnt_FoundOopses;
    ssize_t sz;
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        return 0;
    statb.st_size = 0; /* paranoia */
    if (fstat(fd, &statb) != 0 || statb.st_size < 1)
    {
        close(fd);
        return 0;
    }

    if (last_changed_p != NULL)
    {
        if (*last_changed_p == statb.st_mtime)
        {
            VERB1 log("Syslog file '%s' hasn't changed since last scan, skipping", filename);
            close(fd);
            return 0;
        }
        *last_changed_p = statb.st_mtime;
    }

    /*
     * In theory we have a race here, since someone could spew
     * to /var/log/messages before we read it in... we try to
     * deal with it by reading at most 10kbytes extra. If there's
     * more than that.. any oops will be in dmesg anyway.
     * Do not try to allocate an absurd amount of memory; ignore
     * older log messages because they are unlikely to have
     * sufficiently recent data to be useful.  32MB is more
     * than enough; it's not worth looping through more log
     * if the log is larger than that.
     */
    sz = statb.st_size + 10*1024;
    if (statb.st_size > (32*1024*1024 - 10*1024))
    {
        xlseek(fd, statb.st_size - (32*1024*1024 - 10*1024), SEEK_SET);
        sz = 32*1024*1024;
    }
    buffer = (char*)xzalloc(sz);
    sz = full_read(fd, buffer, sz);
    close(fd);

    cnt_FoundOopses = 0;
    if (sz > 0)
        cnt_FoundOopses = extract_oopses(oopsList, buffer, sz);
    free(buffer);

    return cnt_FoundOopses;
}

/* returns number of errors */
int save_oops_to_debug_dump(GList **oopsList)
{
    unsigned countdown = 16; /* do not report hundreds of oopses */
    unsigned idx = g_list_length(*oopsList);
    time_t t = time(NULL);
    pid_t my_pid = getpid();

    VERB1 log("Saving %u oopses as crash dump dirs", idx >= countdown ? countdown-1 : idx);

    char *tainted_str = NULL;
    /* once tainted flag is set to 1, only restart can reset the flag to 0 */
    FILE *tainted_fd = fopen("/proc/sys/kernel/tainted", "r");
    if (tainted_fd)
    {
        tainted_str = xmalloc_fgetline(tainted_fd);
        fclose(tainted_fd);
    }
    else
        error_msg("/proc/sys/kernel/tainted does not exist");

    int errors = 0;

    while (idx != 0 && --countdown != 0)
    {
        char path[sizeof(DEBUG_DUMPS_DIR"/kerneloops-%lu-%lu-%lu") + 3 * sizeof(long)*3];
        sprintf(path, DEBUG_DUMPS_DIR"/kerneloops-%lu-%lu-%lu", (long)t, (long)my_pid, (long)idx);

        char *first_line = (char*)g_list_nth_data(*oopsList,--idx);
        char *second_line = (char*)strchr(first_line, '\n'); /* never NULL */
        *second_line++ = '\0';

        struct dump_dir *dd = dd_create(path, /*uid:*/ 0);
        if (dd)
        {
            dd_save_text(dd, FILENAME_ANALYZER, "Kerneloops");
            dd_save_text(dd, FILENAME_EXECUTABLE, "kernel");
            dd_save_text(dd, FILENAME_KERNEL, first_line);
            dd_save_text(dd, FILENAME_CMDLINE, "not_applicable");
            dd_save_text(dd, FILENAME_BACKTRACE, second_line);
            /* Optional, makes generated bz more informative */
            strchrnul(second_line, '\n')[0] = '\0';
            dd_save_text(dd, FILENAME_REASON, second_line);

            if (tainted_str && tainted_str[0] != '0')
                dd_save_text(dd, FILENAME_TAINTED, tainted_str);

            free(tainted_str);
            dd_close(dd);
        }
        else
            errors++;
    }

    return errors;
}

} /* extern "C" */


CKerneloopsScanner::CKerneloopsScanner()
{
    int cnt_FoundOopses;
    m_syslog_last_change = 0;

    /* Scan dmesg, on first call only */
    GList *oopsList = NULL;
    cnt_FoundOopses = scan_dmesg(&oopsList);
    if (cnt_FoundOopses > 0)
    {
        int errors = save_oops_to_debug_dump(&oopsList);
        if (errors > 0)
            log("%d errors while dumping oopses", errors);
    }
}

void CKerneloopsScanner::Run(const char *pActionDir, const char *pArgs, int force)
{
    const char *syslog_file = "/var/log/messages";
    map_plugin_settings_t::const_iterator it = m_pSettings.find("SysLogFile");
    if (it != m_pSettings.end())
        syslog_file = it->second.c_str();

    GList *oopsList = NULL;
    int cnt_FoundOopses = scan_syslog_file(&oopsList, syslog_file, &m_syslog_last_change);
    if (cnt_FoundOopses > 0)
    {
        int errors = save_oops_to_debug_dump(&oopsList);
        if (errors > 0)
            log("%d errors while dumping oopses", errors);
        /*
         * This marker in syslog file prevents us from
         * re-parsing old oopses (any oops before it is
         * ignored by scan_syslog_file()). The only problem
         * is that we can't be sure here that syslog_file
         * is the file where syslog(xxx) stuff ends up.
         */
        openlog("abrt", 0, LOG_KERN);
        syslog(LOG_WARNING,
               "Kerneloops: Reported %u kernel oopses to Abrt",
               cnt_FoundOopses);
        closelog();
    }

    for (GList *li = oopsList; li != NULL; li = g_list_next(li))
        free((char*)li->data);
    g_list_free(oopsList);
}

PLUGIN_INFO(ACTION,
            CKerneloopsScanner,
            "KerneloopsScanner",
            "0.0.1",
            _("Periodically scans for and saves kernel oopses"),
            "anton@redhat.com",
            "http://people.redhat.com/aarapov",
            "");
