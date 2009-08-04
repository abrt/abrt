/*
    dumpoops.cpp - dump oops(es) in a given file

    Copyright (C) 2009  Denys Vlasenko (dvlasenk@redhat.com)
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
#include "KerneloopsScanner.h"
#include <dlfcn.h>

int main(int argc, char **argv)
{
    if (!argv[1])
    {
        log("usage: %s FILE", argv[0]);
        return 1;
    }
    char *slash = strrchr(argv[0], '/');
    msg_prefix = xasprintf("%s: ", slash ? slash+1 : argv[0]);

    /* Load KerneloopsScanner plugin */
//    const plugin_info_t *plugin_info;
    CPlugin* (*plugin_newf)(void);
    int (*scan_syslog_file)(CKerneloopsScanner *This, const char *filename);
    void (*save_oops_to_debug_dump)(CKerneloopsScanner *This);
    void *handle;

    errno = 0;
    handle = dlopen(PLUGINS_LIB_DIR"/libKerneloopsScanner.so", RTLD_NOW);
    if (!handle)
        perror_msg_and_die("can't load %s", PLUGINS_LIB_DIR"/libKerneloopsScanner.so");
#define LOADSYM(fp, name) \
do { \
    fp = (typeof(fp)) (dlsym(handle, name)); \
    if (!fp) \
        perror_msg_and_die(PLUGINS_LIB_DIR"/libKerneloopsScanner.so has no %s", name); \
} while (0)
//    LOADSYM(plugin_info, "plugin_info");
    LOADSYM(plugin_newf, "plugin_new");
    LOADSYM(scan_syslog_file, "scan_syslog_file");
    LOADSYM(save_oops_to_debug_dump, "save_oops_to_debug_dump");
    CKerneloopsScanner* scanner = (CKerneloopsScanner*) plugin_newf();
    //scanner->Init();
    //scanner->LoadSettings(path);

    /* Use it: parse and dump the oops */
    int cnt = scan_syslog_file(scanner, argv[1]);
    log("found oopses: %d", cnt);

    if (cnt > 0)
    {
        log("dumping oopses");
        save_oops_to_debug_dump(scanner);
    }

    /*dlclose(handle); - why bother? */
    return 0;
}
