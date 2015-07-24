/*
    Copyright (C) 2013  Red Hat, Inc.

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
#include "libabrt.h"

struct oops_text {
    unsigned panic_no;
    unsigned part_no;
    const char *filename;
    char *text;
};

static
struct oops_text *parse_file(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        return NULL;

    char buffer[16 * 1024];

    struct oops_text *ot = NULL;

    if (!fgets(buffer, sizeof(buffer), fp))
        goto ret;
    unsigned n1, n2;
    int n = sscanf(buffer, "Panic#%u Part%u\n", &n1, &n2);
    if (n != 2)
        goto ret;

    ot = xzalloc(sizeof(*ot));
    ot->filename = filename;
    ot->panic_no = n1;
    ot->part_no = n2;

    size_t sz = fread(buffer, 1, sizeof(buffer), fp);
    ot->text = strndup(buffer, sz);

 ret:
    fclose(fp);
    return ot;
}

static
int compare_oops_texts(const void *a, const void *b)
{
    struct oops_text *aa = *(struct oops_text **)a;
    struct oops_text *bb = *(struct oops_text **)b;
    if (aa->panic_no < bb->panic_no)
        return -1;
    if (aa->panic_no > bb->panic_no)
        return 1;
    if (aa->part_no > bb->part_no)
        return -1;
    return (aa->part_no < bb->part_no);
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] [-od] FILE...\n"
        "\n"
        "Scans files for split oops message. Can print and/or delete them."
    );
    enum {
        OPT_v = 1 << 0,
        OPT_o = 1 << 1,
        OPT_d = 1 << 2,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL('o', NULL, NULL, _("Print found oopses")),
        OPT_BOOL('d', NULL, NULL, _("Delete files with found oopses")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    struct oops_text **v = xzalloc(sizeof(v[0]));
    int i = 0;

    while (*argv)
    {
        v[i] = parse_file(*argv);
        if (v[i])
        {
            v = xrealloc(v, (++i + 1) * sizeof(v[0]));
            v[i] = NULL;
        }
        argv++;
    }

    if (i == 0) /* nothing was found */
        return 0;

    qsort(v, i, sizeof(v[0]), compare_oops_texts);

    if (opts & OPT_o)
    {
        struct oops_text **vv = v;
        while (*vv)
        {
            struct oops_text *cur_oops = *vv;
            fputs(cur_oops->text, stdout);
            vv++;
        }
    }

    if (opts & OPT_d)
    {
        struct oops_text **vv = v;
        while (*vv)
        {
            struct oops_text *cur_oops = *vv;
            if (unlink(cur_oops->filename) != 0)
                perror_msg("Can't unlink '%s'", cur_oops->filename);
            vv++;
        }
    }

    return 0;
}
