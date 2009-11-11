/*
    main.cpp - parses command line arguments

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
#include <argp.h>
#include <stdlib.h>
#include "config.h"
#include "backtrace.h"
#include "fallback.h"

const char *argp_program_version = "abrt-backtrace " VERSION;
const char *argp_program_bug_address = "<crash-catcher@lists.fedorahosted.org>";

static char doc[] = "abrt-backtrace -- backtrace analyzer";

/* A description of the arguments we accept. */
static char args_doc[] = "FILE";

static struct argp_option options[] = {
  {"independent"  , 'i', 0, 0, "Prints independent backtrace (fallback)" },
  {"tree"         , 't', 0, 0, "Prints backtrace analysis tree"},
  {"verbose"      , 'v', 0, 0, "Prints debug information"}, 
  {"debug-parser" , 'p', 0, 0, "Prints parser debug information"}, 
  {"debug-scanner", 's', 0, 0, "Prints scanner debug information"}, 
  { 0 }
};

struct arguments
{
  bool independent;
  bool tree;
  bool verbose;
  bool debug_parser;
  bool debug_scanner;
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
  case 'i': arguments->independent = true; break;
  case 't': arguments->tree = true; break;
  case 'v': arguments->verbose = true; break;
  case 'p': arguments->debug_parser = true; break;
  case 's': arguments->debug_scanner = true; break;

  case ARGP_KEY_ARG:
    if (arguments->filename)
    {
      /* Too many arguments. */
      argp_usage(state);
      exit(2);
    }
    arguments->filename = arg;
    break;

  case ARGP_KEY_END:
    if (!arguments->filename)
    {
      /* Not enough arguments. */
      argp_usage(state);
      exit(1);
    }
    break;
    
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };


int main(int argc, char **argv)
{
  /* Set options default values and parse program command line. */
  struct arguments arguments;
  arguments.independent = false;
  arguments.verbose = false;
  arguments.tree = false;
  arguments.debug_parser = false;
  arguments.debug_scanner = false;
  arguments.filename = 0;
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  /* Open input file and parse it. */
  FILE *fp = fopen(arguments.filename, "r");
  if (!fp)
  {
    printf("Unable to open '%s'.\n", arguments.filename);
    exit(3);
  }

  if (arguments.independent)
  {
    struct strbuf *ibt = independent_backtrace(fp);
    fclose(fp);
    puts(ibt->buf);
    strbuf_free(ibt);
    return;
  }

  struct backtrace *bt;
  bt = do_parse(fp, arguments.debug_parser, arguments.debug_scanner);
  fclose(fp);

  if (arguments.tree)
    backtrace_print_tree(bt);

  backtrace_free(bt);
  return 0;
}
