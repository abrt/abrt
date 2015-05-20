/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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
#include "builtin-cmd.h"

#define USAGE_OPTS_WIDTH 16
#define USAGE_GAP         2

/* TODO: add --pager(default) and --no-pager */

#define CMD(NAME, ABBREV, help) { #NAME, ABBREV, cmd_##NAME , (help) }
struct cmd_struct {
    const char *cmd;
    const char *abbrev;
    int (*fn)(int, const char **);
    const char *help;
};

static void list_cmds_help(const struct cmd_struct *commands)
{
    for (const struct cmd_struct *p = commands; p->cmd; ++p)
    {
        size_t pos;
        int pad;

        pos = fprintf(stderr, "    ");
        pos += fprintf(stderr, "%s", p->cmd);
        pos += fprintf(stderr, ", %s", p->abbrev);

        if (pos <= USAGE_OPTS_WIDTH)
            pad = USAGE_OPTS_WIDTH - pos;
        else
        {
            fputc('\n', stderr);
            pad = USAGE_OPTS_WIDTH;
        }

        fprintf(stderr, "%*s%s\n", pad + USAGE_GAP, "", p->help);
    }
}

static unsigned handle_internal_options(int argc, const char **argv, const char *usage)
{
    unsigned skip = 0;

    while (*argv)
    {
        const char *cmd = *argv;
        if (cmd[0] != '-')
            break;

        if (strcmp(cmd, "--version") == 0)
        {
            printf("%s version %s\n", g_progname, PACKAGE_VERSION);
            exit(0);
        }
        if (strcmp(cmd, "--help") == 0)
        {
            return skip + argc;
        }
        else
            error_msg_and_die("%s", usage);

        argv++;
        argc--;
        skip++;
    }

    return skip;
}

static void handle_internal_command(int argc, const char **argv,
                                    const struct cmd_struct *commands)
{
    const char *cmd = argv[0];

    for (const struct cmd_struct *p = commands; p->cmd; ++p)
    {
        if (strcmp(p->cmd, cmd) != 0 && strcmp(p->abbrev, cmd) != 0)
            continue;

        exit(p->fn(argc, argv));
    }
}

int main(int argc, const char **argv)
{
    setlocale(LC_ALL, "");
    /* Hack:
     * Right-to-left scripts don't work properly in many terminals.
     * Hebrew speaking people say he_IL.utf8 looks so mangled
     * they prefer en_US.utf8 instead.
     */
    const char *msg_locale = setlocale(LC_MESSAGES, NULL);
    if (msg_locale && strcmp(msg_locale, "he_IL.utf8") == 0)
        setlocale(LC_MESSAGES, "en_US.utf8");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init((char **)argv);

    argv++;
    argc--;

    const char *abrt_cli_usage_string = _(
        "Usage: abrt-cli [--version] COMMAND [DIR]..."
        );

    const struct cmd_struct commands[] = {
        CMD(list, "ls", _("List problems [in DIRs]")),
        CMD(remove, "rm", _("Remove problem directory DIR")),
        CMD(report, "e",_("Analyze and report problem data in DIR")),
        CMD(info, "i", _("Print information about DIR")),
        CMD(status, "st",_("Print the count of the recent crashes")),
        CMD(process, "p",_("Process multiple problems")),
        {NULL, NULL, NULL, NULL}
    };

    migrate_to_xdg_dirs();

    unsigned skip = handle_internal_options(argc, argv, abrt_cli_usage_string);
    argc -= skip;
    argv += skip;
    if (argc > 0)
        handle_internal_command(argc, argv, commands);

    /* user didn't specify command; print out help */
    printf("%s\n\n", abrt_cli_usage_string);
    list_cmds_help(commands);
    printf("\n%s\n", _("See 'abrt-cli COMMAND --help' for more information"));

    return 0;
}
