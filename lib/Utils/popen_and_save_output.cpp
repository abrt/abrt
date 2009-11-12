/*
 * Utility routines.
 *
 * Licensed under GPLv2 or later, see file COPYING in this tarball for details.
 */
#include "abrtlib.h"

using namespace std;

string popen_and_save_output(const char *cmd)
{
    string result;

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) /* fork or pipe failed; or out-of-mem */
    {
        return result;
    }

    size_t sz;
    char buf[BUFSIZ + 1];
    while ((sz = fread(buf, 1, sizeof(buf)-1, fp)) > 0)
    {
        buf[sz] = '\0';
        result += buf;
    }
    pclose(fp);

    return result;
}
