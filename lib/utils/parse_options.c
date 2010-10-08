
#include <getopt.h>
#include "abrtlib.h"
#include "parse_options.h"

#define USAGE_OPTS_WIDTH 24
#define USAGE_GAP         2

void parse_usage_and_die(const char *usage, const struct options *opt)
{
    fprintf(stderr, _("Usage: %s\n"), usage);

    if (opt->type != OPTION_GROUP)
        fputc('\n', stderr);

    for (; opt->type != OPTION_END; opt++)
    {
        size_t pos;
        int pad;

        if (opt->type == OPTION_GROUP)
        {
            fputc('\n', stderr);
            if (*opt->help)
                fprintf(stderr, "%s\n", opt->help);
            continue;
        }

        pos = fprintf(stderr, "    ");
        if (opt->short_name)
            pos += fprintf(stderr, "-%c", opt->short_name);

        if (opt->short_name && opt->long_name)
            pos += fprintf(stderr, ", ");

        if (opt->long_name)
            pos += fprintf(stderr, "--%s", opt->long_name);

        if (opt->argh)
            pos += fprintf(stderr, " %s", opt->argh);

        if (pos <= USAGE_OPTS_WIDTH)
            pad = USAGE_OPTS_WIDTH - pos;
        else
        {
            fputc('\n', stderr);
            pad = USAGE_OPTS_WIDTH;
        }
        fprintf(stderr, "%*s%s\n", pad + USAGE_GAP, "", opt->help);
    }
    fputc('\n', stderr);
    exit(1);
}

static int parse_opt_size(const struct options *opt)
{
    unsigned size = 0;
    for (; opt->type != OPTION_END; opt++)
        size++;

    return size;
}

void parse_opts(int argc, char **argv, const struct options *opt,
                const char *usage)
{
    int help = 0;
    int size = parse_opt_size(opt);

    struct strbuf *shortopts = strbuf_new();

    struct option *longopts = xzalloc(sizeof(longopts[0]) * (size+2));
    int ii;
    for (ii = 0; ii < size; ++ii)
    {
        longopts[ii].name = opt[ii].long_name;

        switch (opt[ii].type)
        {
            case OPTION_BOOL:
                longopts[ii].has_arg = no_argument;
                if (opt[ii].short_name)
                    strbuf_append_char(shortopts, opt[ii].short_name);
                break;
            case OPTION_INTEGER:
            case OPTION_STRING:
                longopts[ii].has_arg = required_argument;
                if (opt[ii].short_name)
                    strbuf_append_strf(shortopts, "%c:", opt[ii].short_name);
                break;
            case OPTION_GROUP:
            case OPTION_END:
                break;
        }
        /*longopts[ii].flag = 0; - xzalloc did it */
        longopts[ii].val = opt[ii].short_name;
    }
    longopts[ii].name = "help";
    /*longopts[ii].has_arg = 0; - xzalloc did it */
    longopts[ii].flag = &help;
    longopts[ii].val = 1;
    /* xzalloc did it already:
    ii++;
    longopts[ii].name = NULL;
    longopts[ii].has_arg = 0;
    longopts[ii].flag = NULL;
    longopts[ii].val = 0;
    */

    int option_index = 0;
    while (1)
    {
        int c = getopt_long(argc, argv, shortopts->buf, longopts, &option_index);

        if (c == -1)
            break;

        if (c == '?' || help)
        {
            free(longopts);
            strbuf_free(shortopts);
            parse_usage_and_die(usage, opt);
        }

        for (ii = 0; ii < size; ++ii)
        {
            if (opt[ii].short_name == c)
            {
                switch (opt[ii].type)
                {
                    case OPTION_BOOL:
                        *(int*)opt[ii].value += 1;
                        break;
                    case OPTION_INTEGER:
                        *(int*)opt[ii].value = xatoi(optarg);
                        break;
                    case OPTION_STRING:
                        if (optarg)
                            *(char**)opt[ii].value = (char*)optarg;
                        break;
                    case OPTION_GROUP:
                    case OPTION_END:
                        break;
                }
            }
        }
    }

    free(longopts);
    strbuf_free(shortopts);
}
