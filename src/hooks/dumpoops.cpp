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
        Denys Vlasenko <dvlasenk@redhat.com>
        Zdenek Prikryl <zprikryl@redhat.com>
*/

#include "abrtlib.h"
#include "KerneloopsScanner.h"
#include <dlfcn.h>
#include <glib.h>

#define LOADSYM(fp, name) \
do { \
    fp = (typeof(fp)) (dlsym(handle, name)); \
    if (!fp) \
        perror_msg_and_die(PLUGINS_LIB_DIR"/libKerneloopsScanner.so has no %s", name); \
} while (0)


int main(int argc, char **argv)
{
    char *program_name = strrchr(argv[0], '/');
    program_name = program_name ? program_name + 1 : argv[0];

    /* Parse options */
    bool opt_d = 0, opt_s = 0;
    int opt;
    while ((opt = getopt(argc, argv, "dsv")) != -1) {
        switch (opt) {
            case 'd':
                opt_d = 1;
                break;
            case 's':
                opt_s = 1;
                break;
            case 'v':
                /* Kerneloops code uses VERB3, thus: */
                g_verbose = 3;
                break;
            default:
usage:
                error_msg_and_die(
                        "Usage: %s [-dsv] FILE\n\n"
                        "Options:\n"
                        "\t-d\tCreate ABRT dump for every oops found\n"
                        "\t-s\tPrint found oopses on standard output\n"
                        "\t-v\tBe verbose\n"
                        , program_name
                        );
        }
    }
    argv += optind;
    if (!argv[0])
        goto usage;

    msg_prefix = program_name;

    /* Load KerneloopsScanner plugin */
    //	const plugin_info_t *plugin_info;
    CPlugin* (*plugin_newf)(void);
    int (*scan_syslog_file)(GList **oopsList, const char *filename, time_t *last_changed_p);
    int (*save_oops_to_debug_dump)(GList **oopsList);
    void *handle;

    errno = 0;
    //TODO: use it directly, not via dlopen?
    handle = dlopen(PLUGINS_LIB_DIR"/libKerneloopsScanner.so", RTLD_NOW);
    if (!handle)
        perror_msg_and_die("can't load %s", PLUGINS_LIB_DIR"/libKerneloopsScanner.so");

    //	LOADSYM(plugin_info, "plugin_info");
    LOADSYM(plugin_newf, "plugin_new");
    LOADSYM(scan_syslog_file, "scan_syslog_file");
    LOADSYM(save_oops_to_debug_dump, "save_oops_to_debug_dump");

    //	CKerneloopsScanner* scanner = (CKerneloopsScanner*) plugin_newf();
    //	scanner->Init();
    //	scanner->LoadSettings(path);

    /* Use it: parse and dump the oops */
    GList *oopsList = NULL;
    int cnt = scan_syslog_file(&oopsList, argv[0], NULL);
    log("found oopses: %d", cnt);

    if (cnt > 0) {
        if (opt_s) {
            int i = 0;
            int length = g_list_length(oopsList);
            while (i < length) {
                printf("\nVersion: %s", (char*)g_list_nth_data(oopsList, i));
                i++;
            }
        }
        if (opt_d) {
            log("dumping oopses");
            int errors = save_oops_to_debug_dump(&oopsList);
            if (errors > 0)
            {
                log("%d errors while dumping oopses", errors);
                return 1;
            }
        }
    }

    for (GList *li = oopsList; li != NULL; li = g_list_next(li))
        free((char*)li->data);

    g_list_free(oopsList);
    /*dlclose(handle); - why bother?
     * cos we are handsome and good lookin' guys :D
     */
    return 0;
}
