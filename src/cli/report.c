/*
    Copyright (C) 2009, 2010  Red Hat, Inc.

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
#include "run-command.h"
#include "report.h"

/* Field separator for the crash report file that is edited by user. */
#define FIELD_SEP "%----"

/*
 * Escapes the field content string to avoid confusion with file comments.
 * Returned field must be free()d by caller.
 */
static char *escape(const char *str)
{
    // Determine the size of resultant string.
    // Count the required number of escape characters.
    // 1. NEWLINE followed by #
    // 2. NEWLINE followed by \# (escaped version)
    const char *ptr = str;
    bool newline = true;
    int count = 0;
    while (*ptr)
    {
        if (newline)
        {
            if (*ptr == '#')
                ++count;
            if (*ptr == '\\' && ptr[1] == '#')
                ++count;
        }

        newline = (*ptr == '\n');
        ++ptr;
    }

    // Copy the input string to the resultant string, and escape all
    // occurences of \# and #.
    char *result = (char*)xmalloc(strlen(str) + 1 + count);

    const char *src = str;
    char *dest = result;
    newline = true;
    while (*src)
    {
        if (newline)
        {
            if (*src == '#')
                *dest++ = '\\';
            else if (*src == '\\' && *(src + 1) == '#')
                *dest++ = '\\';
        }

        newline = (*src == '\n');
        *dest++ = *src++;
    }
    *dest = '\0';
    return result;
}

/*
 * Removes all comment lines, and unescapes the string previously escaped
 * by escape(). Works in-place.
 */
static void remove_comments_and_unescape(char *str)
{
    char *src = str, *dest = str;
    bool newline = true;
    while (*src)
    {
        if (newline)
        {
            if (*src == '#')
            { // Skip the comment line!
                while (*src && *src != '\n')
                    ++src;

                if (*src == '\0')
                    break;

                ++src;
                continue;
            }
            if (*src == '\\'
                    && (src[1] == '#' || (src[1] == '\\' && src[2] == '#'))
               ) {
                ++src; // Unescape escaped char.
            }
        }

        newline = (*src == '\n');
        *dest++ = *src++;
    }
    *dest = '\0';
}

/*
 * Writes a field of crash report to a file.
 * Field must be writable.
 */
static void write_crash_report_field(FILE *fp, crash_data_t *crash_data,
        const char *field, const char *description)
{
    const struct crash_item *value = get_crash_data_item_or_NULL(crash_data, field);
    if (!value)
    {
        // exit silently, all fields are optional for now
        //error_msg("Field %s not found", field);
        return;
    }

    fprintf(fp, "%s%s\n", FIELD_SEP, field);

    fprintf(fp, "%s\n", description);
    if (!(value->flags & CD_FLAG_ISEDITABLE))
        fprintf(fp, _("# This field is read only\n"));

    char *escaped_content = escape(value->content);
    fprintf(fp, "%s\n", escaped_content);
    free(escaped_content);
}

/*
 * Saves the crash report to a file.
 * Parameter 'fp' must be opened before write_crash_report is called.
 * Returned value:
 *  If the report is successfully stored to the file, a zero value is returned.
 *  On failure, nonzero value is returned.
 */
static void write_crash_report(crash_data_t *report, FILE *fp)
{
    fprintf(fp, "# Please check this report. Lines starting with '#' will be ignored.\n"
            "# Lines starting with '%%----' separate fields, please do not delete them.\n\n");

    write_crash_report_field(fp, report, FILENAME_COMMENT,
            _("# Describe the circumstances of this crash below"));
    write_crash_report_field(fp, report, FILENAME_BACKTRACE,
            _("# Backtrace\n# Check that it does not contain any sensitive data (passwords, etc.)"));
    write_crash_report_field(fp, report, FILENAME_DUPHASH, "# DUPHASH");
    write_crash_report_field(fp, report, FILENAME_ARCHITECTURE, _("# Architecture"));
    write_crash_report_field(fp, report, FILENAME_CMDLINE, _("# Command line"));
    write_crash_report_field(fp, report, FILENAME_COMPONENT, _("# Component"));
    write_crash_report_field(fp, report, FILENAME_COREDUMP, _("# Core dump"));
    write_crash_report_field(fp, report, FILENAME_EXECUTABLE, _("# Executable"));
    write_crash_report_field(fp, report, FILENAME_KERNEL, _("# Kernel version"));
    write_crash_report_field(fp, report, FILENAME_PACKAGE, _("# Package"));
    write_crash_report_field(fp, report, FILENAME_REASON, _("# Reason of crash"));
    write_crash_report_field(fp, report, FILENAME_OS_RELEASE, _("# Release string of the operating system"));
}

/*
 * Updates appropriate field in the report from the text. The text can
 * contain multiple fields.
 * Returns:
 *  0 if no change to the field was detected.
 *  1 if the field was changed.
 *  Changes to read-only fields are ignored.
 */
static int read_crash_report_field(const char *text, crash_data_t *report,
        const char *field)
{
    char separator[sizeof("\n" FIELD_SEP)-1 + strlen(field) + 2]; // 2 = '\n\0'
    sprintf(separator, "\n%s%s\n", FIELD_SEP, field);
    const char *textfield = strstr(text, separator);
    if (!textfield)
        return 0; // exit silently because all fields are optional

    textfield += strlen(separator);
    int length = 0;
    const char *end = strstr(textfield, "\n" FIELD_SEP);
    if (!end)
        length = strlen(textfield);
    else
        length = end - textfield;

    struct crash_item *value = get_crash_data_item_or_NULL(report, field);
    if (!value)
    {
        error_msg("Field %s not found", field);
        return 0;
    }

    // Do not change noneditable fields.
    if (!(value->flags & CD_FLAG_ISEDITABLE))
        return 0;

    // Compare the old field contents with the new field contents.
    char newvalue[length + 1];
    strncpy(newvalue, textfield, length);
    newvalue[length] = '\0';
    strtrim(newvalue);

    char oldvalue[strlen(value->content) + 1];
    strcpy(oldvalue, value->content);
    strtrim(oldvalue);

    // Return if no change in the contents detected.
    if (strcmp(newvalue, oldvalue) == 0)
        return 0;

    free(value->content);
    value->content = xstrdup(newvalue);
    return 1;
}

/*
 * Updates the crash report 'report' from the text. The text must not contain
 * any comments.
 * Returns:
 *  0 if no field was changed.
 *  1 if any field was changed.
 *  Changes to read-only fields are ignored.
 */
static int read_crash_report(crash_data_t *report, const char *text)
{
    int result = 0;
    result |= read_crash_report_field(text, report, FILENAME_COMMENT);
    result |= read_crash_report_field(text, report, FILENAME_BACKTRACE);
    result |= read_crash_report_field(text, report, FILENAME_DUPHASH);
    result |= read_crash_report_field(text, report, FILENAME_ARCHITECTURE);
    result |= read_crash_report_field(text, report, FILENAME_CMDLINE);
    result |= read_crash_report_field(text, report, FILENAME_COMPONENT);
    result |= read_crash_report_field(text, report, FILENAME_COREDUMP);
    result |= read_crash_report_field(text, report, FILENAME_EXECUTABLE);
    result |= read_crash_report_field(text, report, FILENAME_KERNEL);
    result |= read_crash_report_field(text, report, FILENAME_PACKAGE);
    result |= read_crash_report_field(text, report, FILENAME_REASON);
    result |= read_crash_report_field(text, report, FILENAME_OS_RELEASE);
    return result;
}

/**
 * Ensures that the fields needed for editor are present in the crash data.
 * Fields: comments.
 */
static void create_fields_for_editor(crash_data_t *crash_data)
{
    if (!get_crash_data_item_or_NULL(crash_data, FILENAME_COMMENT))
        add_to_crash_data_ext(crash_data, FILENAME_COMMENT, "", CD_FLAG_TXT + CD_FLAG_ISEDITABLE);
}

/**
 * Runs external editor.
 * Returns:
 *  0 if the launch was successful
 *  1 if it failed. The error reason is logged using error_msg()
 */
static int launch_editor(const char *path)
{
    const char *editor, *terminal;

    editor = getenv("ABRT_EDITOR");
    if (!editor)
        editor = getenv("VISUAL");
    if (!editor)
        editor = getenv("EDITOR");

    terminal = getenv("TERM");
    if (!editor && (!terminal || strcmp(terminal, "dumb") == 0))
    {
        error_msg(_("Cannot run vi: $TERM, $VISUAL and $EDITOR are not set"));
        return 1;
    }

    if (!editor)
        editor = "vi";

    char *args[3];
    args[0] = (char*)editor;
    args[1] = (char*)path;
    args[2] = NULL;
    run_command(args);

    return 0;
}

/**
 * Returns:
 *  0 on success, crash data has been updated
 *  2 on failure, unable to create, open, or close temporary file
 *  3 on failure, cannot launch text editor
 */
static int run_report_editor(crash_data_t *crash_data)
{
    /* Open a temporary file and write the crash report to it. */
    char filename[] = "/tmp/abrt-report.XXXXXX";
    int fd = mkstemp(filename);
    if (fd == -1) /* errno is set */
    {
        perror_msg("can't generate temporary file name");
        return 2;
    }

    FILE *fp = fdopen(fd, "w");
    if (!fp) /* errno is set */
    {
        die_out_of_memory();
    }

    write_crash_report(crash_data, fp);

    if (fclose(fp)) /* errno is set */
    {
        perror_msg("can't write '%s'", filename);
        return 2;
    }

    // Start a text editor on the temporary file.
    if (launch_editor(filename) != 0)
        return 3; /* exit with error */

    // Read the file back and update the report from the file.
    fp = fopen(filename, "r");
    if (!fp) /* errno is set */
    {
        perror_msg("can't open '%s' to read the crash report", filename);
        return 2;
    }

    fseek(fp, 0, SEEK_END);
    unsigned long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *text = (char*)xmalloc(size + 1);
    if (fread(text, 1, size, fp) != size)
    {
        error_msg("can't read '%s'", filename);
        fclose(fp);
        return 2;
    }
    text[size] = '\0';
    fclose(fp);

    // Delete the tempfile.
    if (unlink(filename) == -1) /* errno is set */
    {
        perror_msg("can't unlink %s", filename);
    }

    remove_comments_and_unescape(text);
    // Updates the crash report from the file text.
    int report_changed = read_crash_report(crash_data, text);
    free(text);
    if (report_changed)
        puts(_("\nThe report has been updated"));
    else
        puts(_("\nNo changes were detected in the report"));

    return 0;
}

/**
 * Asks user for a text response.
 * @param question
 *  Question displayed to user.
 * @param result
 *  Output array.
 * @param result_size
 *  Maximum byte count to be written.
 */
static void read_from_stdin(const char *question, char *result, int result_size)
{
    assert(result_size > 1);
    printf("%s", question);
    fflush(NULL);
    if (NULL == fgets(result, result_size, stdin))
        result[0] = '\0';
    // Remove the newline from the login.
    strchrnul(result, '\n')[0] = '\0';
}

/**
 * Asks a [y/n] question on stdin/stdout.
 * Returns true if the answer is yes, false otherwise.
 */
static bool ask_yesno(const char *question)
{
    /* The response might take more than 1 char in non-latin scripts. */
    const char *yes = _("y");
    const char *no = _("N");
    printf("%s [%s/%s]: ", question, yes, no);
    fflush(NULL);

    char answer[16];
    if (!fgets(answer, sizeof(answer), stdin))
        return false;
    /* Use strncmp here because the answer might contain a newline as
       the last char. */
    return 0 == strncmp(answer, yes, strlen(yes));
}

/* Returns true if echo has been changed from another state. */
static bool set_echo(bool enable)
{
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) < 0)
        return false;

    /* No change needed? */
    if ((bool)(t.c_lflag & ECHO) == enable)
        return false;

    t.c_lflag ^= ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) < 0)
        perror_msg_and_die("tcsetattr");

    return true;
}

/**
 *  Asks user for missing information
 */
static void ask_for_missing_settings(const char *event_name)
{
    for (int i = 0; i < 3; ++i)
    {
        GHashTable *error_table = validate_event(event_name);
        if (!error_table)
            return;

        event_config_t *event_config = get_event_config(event_name);

        GHashTableIter iter;
        char *opt_name, *err_msg;
        g_hash_table_iter_init(&iter, error_table);
        while (g_hash_table_iter_next(&iter, (void**)&opt_name, (void**)&err_msg))
        {
            event_option_t *opt = get_event_option_from_list(opt_name,
                                                             event_config->options);

            free(opt->value);
            opt->value = NULL;

            char result[512];

            char *question = xasprintf("%s: ", (opt->label) ? opt->label: opt->name);
            switch (opt->type) {
            case OPTION_TYPE_TEXT:
            case OPTION_TYPE_NUMBER:
                read_from_stdin(question, result, 512);
                opt->value = xstrdup(result);
                break;
            case OPTION_TYPE_PASSWORD:
            {
                bool changed = set_echo(false);
                read_from_stdin(question, result, 512);
                if (changed)
                    set_echo(true);

                opt->value = xstrdup(result);
                /* Newline was not added by pressing Enter because ECHO was
                   disabled, so add it now. */
                puts("");
                break;
            }
            case OPTION_TYPE_BOOL:
                if (ask_yesno(question))
                    opt->value = xstrdup("yes");
                else
                    opt->value = xstrdup("no");

                break;
            case OPTION_TYPE_INVALID:
                break;
            };

            free(question);
        }

        g_hash_table_destroy(error_table);

        error_table = validate_event(event_name);
        if (!error_table)
            return;

        log(_("Your input is not valid, because of:"));
        g_hash_table_iter_init(&iter, error_table);
        while (g_hash_table_iter_next(&iter, (void**)&opt_name, (void**)&err_msg))
            log(_("Bad value for '%s': %s"), opt_name, err_msg);

        g_hash_table_destroy(error_table);
    }

    /* we ask for 3 times and still don't have valid infromation */
    error_msg_and_die("Invalid input, program exiting...");
}

struct logging_state {
    char *last_line;
};
static char *do_log_and_save_line(char *log_line, void *param)
{
    struct logging_state *l_state = (struct logging_state *)param;
    log("%s", log_line);
    free(l_state->last_line);
    l_state->last_line = log_line;
    return NULL;
}
static int run_events(const char *dump_dir_name, GList *events)
{
    int error_cnt = 0;
    GList *env_list = NULL;

    // Run events
    struct logging_state l_state;
    l_state.last_line = NULL;
    struct run_event_state *run_state = new_run_event_state();
    run_state->logging_callback = do_log_and_save_line;
    run_state->logging_param = &l_state;
    for (GList *li = events; li; li = li->next)
    {
        char *event = (char *) li->data;

        // Export overridden settings as environment variables
        env_list = export_event_config(event);

        int r = run_event_on_dir_name(run_state, dump_dir_name, event);
        if (r == 0 && run_state->children_count == 0)
        {
            l_state.last_line = xasprintf("Error: no processing is specified for event '%s'",
                                          event);
            r = -1;
        }
        if (r == 0)
        {
            printf("%s: %s\n", event, (l_state.last_line ? : "Reporting succeeded"));
        }
        else
        {
            error_msg("Reporting via '%s' was not successful%s%s",
                    event,
                    l_state.last_line ? ": " : "",
                    l_state.last_line ? l_state.last_line : ""
            );
            error_cnt++;
        }
        free(l_state.last_line);
        l_state.last_line = NULL;

        // Unexport overridden settings
        unexport_event_config(env_list);
    }
    free_run_event_state(run_state);

    return error_cnt;
}

static char *do_log(char *log_line, void *param)
{
    log("%s", log_line);
    return log_line;
}

int run_analyze_event(const char *dump_dir_name, const char *analyzer)
{
    VERB2 log("run_analyze_event('%s')", dump_dir_name);

    struct run_event_state *run_state = new_run_event_state();
    run_state->logging_callback = do_log;
    int res = run_event_on_dir_name(run_state, dump_dir_name, analyzer);
    free_run_event_state(run_state);
    return res;
}

/* show even description? */
char *select_event_option(GList *list_options)
{
    if (!list_options)
        return NULL;

    unsigned count = g_list_length(list_options) - 1;
    if (!count)
        return NULL;

    int pos = -1;
    fprintf(stdout, _("Select how you would like to analyze the problem:\n"));
    for (GList *li = list_options; li; li = li->next)
    {
        char *opt = (char*)li->data;
        event_config_t *config = get_event_config(opt);
        if (config)
        {
            ++pos;
            printf(" %i) %s\n", pos, config->screen_name);
        }
    }

    unsigned picked;
    unsigned ii;
    for (ii = 0; ii < 3; ++ii)
    {
        fprintf(stdout, _("Choose option [0 - %u]: "), count);
        fflush(NULL);

        char answer[16];
        if (!fgets(answer, sizeof(answer), stdin))
            continue;

        answer[strlen(answer) - 1] = '\0';
        if (!*answer)
            continue;

        picked = xatou(answer);
        if (picked > count)
        {
            fprintf(stdout, _("You have chosen number out of range"));
            continue;
        }

        break;
    }

    if (ii == 3)
        error_msg_and_die(_("Invalid input, program exiting..."));

    GList *choosen = g_list_nth(list_options, picked);
    return xstrdup((char*)choosen->data);
}

GList *str_to_glist(char *str, int delim)
{
    GList *list = NULL;
    while (*str)
    {
        char *end = strchrnul(str, delim);
        char *tmp = xstrndup(str, end - str);
        if (*tmp)
            list = g_list_append(list, tmp);

        str = end;
        if (!*str)
            break;
        str++;
    }

    if (!list && !g_list_length(list))
        return NULL;

    return list;
}

/* Report the crash */
int report(const char *dump_dir_name, int flags)
{
    /* Load crash_data from (possibly updated by analyze) dump dir */
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
	return -1;

    char *analyze_events_as_lines = list_possible_events(dd, NULL, "analyze");
    dd_close(dd);

    if (analyze_events_as_lines && *analyze_events_as_lines)
    {
        GList *list_analyze_events = str_to_glist(analyze_events_as_lines, '\n');
        free(analyze_events_as_lines);

        char *event = select_event_option(list_analyze_events);
        list_free_with_free(list_analyze_events);

        int analyzer_result = run_analyze_event(dump_dir_name, event);
        free(event);

        if (analyzer_result != 0)
            return 1;
    }

    /* Load crash_data from (possibly updated by analyze) dump dir */
    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
	return -1;

    char *report_events_as_lines = list_possible_events(dd, NULL, "report");
    crash_data_t *crash_data = create_crash_data_from_dump_dir(dd);
    dd_close(dd);

    if (!(flags & CLI_REPORT_BATCH))
    {
        /* Open text editor and give a chance to review the backtrace etc */
        create_fields_for_editor(crash_data);
        int result = run_report_editor(crash_data);
        if (result != 0)
        {
            free_crash_data(crash_data);
            free(report_events_as_lines);
            return 1;
        }
        /* Save comment, backtrace */
        dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (dd)
        {
//TODO: we should iterate through crash_data and modify all modifiable fields
            const char *comment = get_crash_item_content_or_NULL(crash_data, FILENAME_COMMENT);
            const char *backtrace = get_crash_item_content_or_NULL(crash_data, FILENAME_BACKTRACE);
            if (comment)
                dd_save_text(dd, FILENAME_COMMENT, comment);
            if (backtrace)
                dd_save_text(dd, FILENAME_BACKTRACE, backtrace);
            dd_close(dd);
        }
    }

    /* Get possible reporters associated with this particular crash */
    /* TODO: npajkovs: remove this annoying c++ vector_string_t */
    GList *report_events = NULL;
    if (report_events_as_lines && *report_events_as_lines)
        report_events = str_to_glist(report_events_as_lines, '\n');

    free(report_events_as_lines);

    if (!report_events)
    {
        free_crash_data(crash_data);
        error_msg_and_die("The dump directory '%s' has no defined reporters",
                          dump_dir_name);
    }

    /* Get settings */
    load_event_config_data();

    int errors = 0;
    int plugins = 0;
    if (flags & CLI_REPORT_BATCH)
    {
        puts(_("Reporting..."));
        errors += run_events(dump_dir_name, report_events);
        plugins += g_list_length(report_events);
    }
    else
    {
        const char *rating_str = get_crash_item_content_or_NULL(crash_data, FILENAME_RATING);
        unsigned rating = rating_str ? xatou(rating_str) : 4;

        /* For every reporter, ask if user really wants to report using it. */
        for (GList *li = report_events; li; li = li->next)
        {
            char *reporter_name = (char *) li->data;
            event_config_t *config = get_event_config(reporter_name);
            char question[255];
            char *show_reporter_name;
            if (config)
                show_reporter_name = (config->screen_name) ? config->screen_name : reporter_name;
            else
                show_reporter_name = reporter_name;

            snprintf(question, sizeof(question), _("Report using %s?"), show_reporter_name);

            if (!config)
                VERB1 log("No configuration file found for '%s' reporter", reporter_name);

            if (!ask_yesno(question))
            {
                puts(_("Skipping..."));
                continue;
            }

            /* TODO: npajkovs; not implemented yet */
            //const char *rating_required = get_map_string_item_or_NULL(single_plugin_settings, "RatingRequired");
            //if (rating_required
            //    && string_to_bool(rating_required) == true
            if (rating < 3)
            {
                puts(_("Reporting disabled because the backtrace is unusable"));

                const char *package = get_crash_item_content_or_NULL(crash_data, FILENAME_PACKAGE);
                if (package && package[0])
                    printf(_("Please try to install debuginfo manually using the command: \"debuginfo-install %s\" and try again\n"), package);

                plugins++;
                errors++;
                continue;
            }
            ask_for_missing_settings(reporter_name);

            /*
             * to avoid creating list with one item, we probably should
             * provide something like
             * run_event(char*, char*)
             */
            GList *cur_event = NULL;
            cur_event = g_list_append(cur_event, reporter_name);
            errors += run_events(dump_dir_name, cur_event);
            g_list_free(cur_event);

            plugins++;
        }
    }

    printf(_("Crash reported via %d report events (%d errors)\n"), plugins, errors);
    free_crash_data(crash_data);
    list_free_with_free(report_events);
    return errors;
}
