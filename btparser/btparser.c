/*
    btparser.c - backtrace parsing tool

    Copyright (C) 2010  Red Hat, Inc.

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
#include "lib/backtrace.h"
#include "lib/thread.h"
#include "lib/frame.h"
#include "lib/utils.h"
#include "lib/location.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <sysexits.h>
#include <assert.h>

const char *argp_program_version = "btparser " VERSION;
const char *argp_program_bug_address = "<kklic@redhat.com>";

static char doc[] = "btparser -- backtrace parser";

/* A description of the arguments we accept. */
static char args_doc[] = "FILE";

static struct argp_option options[] = {
    {"rate"            , 'r', 0, 0, "Prints the backtrace rating from 0 to 1"},
    {"crash-function"  , 'c', 0, 0, "Prints crash function"},
    {"duplication-hash", 'h', 0, 0, "Prints normalized string useful for hash calculation"},
    {"debug"           , 'd', 0, 0, "Prints parser debug information"},
    {"verbose"         , 'v', 0, 0, "Prints human-friendly superfluous output."},
    { 0 }
};

enum what_to_output
{
    BACKTRACE,
    RATE,
    CRASH_FUNCTION,
    DUPLICATION_HASH
};

struct arguments
{
    enum what_to_output output;
    bool debug;
    bool verbose;
    char *filename;
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
    /* Get the input argument from argp_parse, which we
       know is a pointer to our arguments structure. */
    struct arguments *arguments = (struct arguments*)state->input;

    switch (key)
    {
    case 'r': arguments->output = RATE; break;
    case 'c': arguments->output = CRASH_FUNCTION; break;
    case 'h': arguments->output = DUPLICATION_HASH; break;
    case 'd': arguments->debug = true; break;
    case 'v': arguments->verbose = true; break;
    case ARGP_KEY_ARG:
        if (arguments->filename)
        {
            /* Too many arguments. */
            argp_usage(state);
            exit(EX_USAGE); /* Invalid argument */
        }
        arguments->filename = arg;
        break;
    case ARGP_KEY_END:
        if (!arguments->filename)
        {
            /* Argument required */
            argp_usage(state);
            exit(EX_USAGE); /* Invalid argument */
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/** Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv)
{
    /* Set options default values and parse program command line. */
    struct arguments arguments;
    arguments.output = BACKTRACE;
    arguments.debug = false;
    arguments.verbose = false;
    arguments.filename = 0;
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    if (arguments.debug)
        btp_debug_parser = true;

    /* Load the input file to a string. */
    char *text = btp_file_to_string(arguments.filename);
    if (!text)
    {
        fprintf(stderr, "Failed to read the input file.\n");
        exit(1);
    }

    /* Parse the input string. */
    char *ptr = text;
    struct btp_location location;
    btp_location_init(&location);
    struct btp_backtrace *backtrace = btp_backtrace_parse(&ptr, &location);
    if (!backtrace)
    {
        fprintf(stderr,
                "Failed to parse the backtrace.\n  Line %d, column %d: %s\n",
                location.line,
                location.column,
                location.message);
        exit(1);
    }
    free(text);

    switch (arguments.output)
    {
    case BACKTRACE:
    {
        char *text_parsed = btp_backtrace_to_text(backtrace, false);
        puts(text_parsed);
        free(text_parsed);
        break;
    }
    case RATE:
    {
        float q = btp_backtrace_quality_complex(backtrace);
        printf("%.2f", q);
        break;
    }
    case CRASH_FUNCTION:
    {
        struct btp_frame *frame = btp_backtrace_get_crash_frame(backtrace);
        if (!frame)
        {
            fprintf(stderr, "Failed to find the crash function.");
            exit(1);
        }
        if (frame->function_name)
            puts(frame->function_name);
        else
        {
            assert(frame->signal_handler_called);
            puts("signal handler");
        }
        btp_frame_free(frame);
        break;
    }
    case DUPLICATION_HASH:
    {
        char *hash = btp_backtrace_get_duplication_hash(backtrace);
        puts(hash);
        free(hash);
        break;
    }
    default:
        fprintf(stderr, "Unexpected operation.");
        exit(1);
    }

    btp_backtrace_free(backtrace);
    return 0;
}
