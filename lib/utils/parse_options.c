
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

unsigned parse_opts(int argc, char **argv, const struct options *opt,
                const char *usage)
{
    int help = 0;
    int size = parse_opt_size(opt);

    struct strbuf *shortopts = strbuf_new();

    struct option *longopts = xzalloc(sizeof(longopts[0]) * (size+2));
    struct option *curopt = longopts;
    int ii;
    for (ii = 0; ii < size; ++ii)
    {
        curopt->name = opt[ii].long_name;
        /*curopt->flag = 0; - xzalloc did it */
        curopt->val = opt[ii].short_name;

        switch (opt[ii].type)
        {
            case OPTION_BOOL:
                curopt->has_arg = no_argument;
                if (opt[ii].short_name)
                    strbuf_append_char(shortopts, opt[ii].short_name);
                break;
            case OPTION_INTEGER:
            case OPTION_STRING:
                curopt->has_arg = required_argument;
                if (opt[ii].short_name)
                    strbuf_append_strf(shortopts, "%c:", opt[ii].short_name);
                break;
            case OPTION_GROUP:
            case OPTION_END:
                break;
        }
        //log("curopt[%d].name:'%s' .has_arg:%d .flag:%p .val:%d", (int)(curopt-longopts),
        //      curopt->name, curopt->has_arg, curopt->flag, curopt->val);
        /*
         * getopt_long() thinks that NULL name marks the end of longopts.
         * Example:
         * [0] name:'verbose' val:'v'
         * [1] name:NULL      val:'c'
         * [2] name:'force'   val:'f'
         * ... ... ...
         * In this case, --force won't be accepted!
         * Therefore we can only advance if name is not NULL.
         */
        if (curopt->name)
            curopt++;
    }
    curopt->name = "help";
    curopt->has_arg = no_argument;
    curopt->flag = &help;
    curopt->val = 1;
    /* xzalloc did it already:
    curopt++;
    curopt->name = NULL;
    curopt->has_arg = 0;
    curopt->flag = NULL;
    curopt->val = 0;
    */

    unsigned retval = 0;
    while (1)
    {
        int c = getopt_long(argc, argv, shortopts->buf, longopts, NULL);

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
                if (ii < sizeof(retval)*8)
                    retval |= (1 << ii);

                if (opt[ii].value != NULL) switch (opt[ii].type)
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

    return retval;
}
