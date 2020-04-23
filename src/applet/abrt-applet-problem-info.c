#include "abrt-applet-problem-info.h"

#include <libabrt.h>
#include <libreport/internal_libreport.h>

struct _AbrtAppletProblemInfo
{
    GObject parent_instance;

    problem_data_t *problem_data;
    bool foreign;
    int count;
    bool packaged;
    char **envp;
    pid_t pid;
    bool known;
    bool reported;
    bool announced;
    bool writable;
    int time;
};

G_DEFINE_TYPE (AbrtAppletProblemInfo, abrt_applet_problem_info, G_TYPE_OBJECT)

static void
abrt_applet_problem_info_init (AbrtAppletProblemInfo *self)
{
    self->time = -1;
    self->pid = -1;
    self->count = -1;
    self->problem_data = problem_data_new ();
}

static void
abrt_applet_problem_info_finalize (GObject *object)
{
    AbrtAppletProblemInfo *self;

    self = ABRT_APPLET_PROBLEM_INFO (object);

    g_clear_pointer (&self->envp, g_strfreev);
    g_clear_pointer (&self->problem_data, g_hash_table_unref);
}

static void
abrt_applet_problem_info_class_init (AbrtAppletProblemInfoClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = abrt_applet_problem_info_finalize;
}

bool
abrt_applet_problem_info_is_announced (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), false);

    return self->announced;
}

bool
abrt_applet_problem_info_is_foreign (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), false);

    return self->foreign;
}

bool
abrt_applet_problem_info_is_packaged (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), false);

    return self->packaged;
}

bool
abrt_applet_problem_info_is_reported (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), false);

    return problem_data_get_content_or_NULL (self->problem_data, FILENAME_REPORTED_TO) != NULL;
}

const char *
abrt_applet_problem_info_get_command_line (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), NULL);

    return problem_data_get_content_or_NULL (self->problem_data, FILENAME_CMDLINE);
}

int
abrt_applet_problem_info_get_count (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), -1);

    if (self->count == -1)
    {
        const char *content;

        content = problem_data_get_content_or_NULL (self->problem_data, FILENAME_COUNT);

        self->count = content != NULL? atoi (content) : -1;
    }

    return self->count;
}

const char *
abrt_applet_problem_info_get_directory (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), NULL);

    return problem_data_get_content_or_NULL (self->problem_data, CD_DUMPDIR);
}

const char **
abrt_applet_problem_info_get_environment (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), NULL);

    if (self->envp == NULL)
    {
        const char *content;

        content = problem_data_get_content_or_NULL (self->problem_data, FILENAME_ENVIRON);

        self->envp = content != NULL? g_strsplit (content, "\n", -1) : NULL;
    }

    return (const char **) self->envp;
}

int
abrt_applet_problem_info_get_pid (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), -1);

    if (self->pid == -1)
    {
        const char *content;

        content = problem_data_get_content_or_NULL (self->problem_data, FILENAME_PID);

        self->pid = content != NULL? atoi (content) : -1;
    }

    return self->pid;
}

problem_data_t *
abrt_applet_problem_info_get_problem_data (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), NULL);

    return self->problem_data;
}

int
abrt_applet_problem_info_get_time (AbrtAppletProblemInfo *self)
{
    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), -1);

    if (self->time == -1)
    {
        const char *time_string;

        time_string = problem_data_get_content_or_NULL (self->problem_data, FILENAME_TIME);

        if (time_string == NULL)
        {
            error_msg_and_die ("BUG: Problem info has data without the element time");
        }

        self->time = atoi (time_string);
    }

    return self->time;
}

void
abrt_applet_problem_info_set_announced (AbrtAppletProblemInfo *self,
                                        bool                   announced)
{
    g_return_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self));

    self->announced = announced;
}

void
abrt_applet_problem_info_set_directory (AbrtAppletProblemInfo *self,
                                        const char            *directory)
{
    g_return_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self));

    problem_data_add_text_noteditable (self->problem_data, CD_DUMPDIR, directory);
}

void
abrt_applet_problem_info_set_foreign (AbrtAppletProblemInfo *self,
                                      bool                   foreign)
{
    g_return_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self));

    self->foreign = foreign;
}

void
abrt_applet_problem_info_set_known (AbrtAppletProblemInfo *self,
                                    bool                   known)
{
    g_return_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self));

    self->known = known;
}

void
abrt_applet_problem_info_set_packaged (AbrtAppletProblemInfo *self,
                                       bool                   packaged)
{
    g_return_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self));

    self->packaged = packaged;
}

void
abrt_applet_problem_info_set_reported (AbrtAppletProblemInfo *self,
                                       bool                   reported)
{
    g_return_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self));

    self->reported = reported;
}

bool
abrt_applet_problem_info_ensure_writable (AbrtAppletProblemInfo *self)
{
    const char *directory;
    int res;
    struct dump_dir *dump_directory;

    g_return_val_if_fail (ABRT_APPLET_IS_PROBLEM_INFO (self), false);

    if (self->writable)
    {
        return true;
    }

    directory = abrt_applet_problem_info_get_directory (self);
    /* chown the directory in any case, because kernel oopses are not foreign */
    /* but their dump directories are not writable without chowning them or */
    /* stealing them. The stealing is deprecated as it breaks the local */
    /* duplicate search and root cannot see them */
    res = chown_dir_over_dbus (directory);
    if (res != 0 && abrt_applet_problem_info_is_foreign (self))
    {
        error_msg (_("Can't take ownership of '%s'"), directory);

        return false;
    }

    abrt_applet_problem_info_set_foreign (self, false);

    dump_directory = libreport_open_directory_for_writing (directory, /* don't ask */ NULL);
    if (dump_directory == NULL)
    {
        error_msg (_("Can't open directory for writing '%s'"), directory);

        return false;
    }

    abrt_applet_problem_info_set_directory (self, dump_directory->dd_dirname);

    self->writable = true;

    dd_close (dump_directory);

    return true;
}

AbrtAppletProblemInfo *
abrt_applet_problem_info_new (const char *directory)
{
    AbrtAppletProblemInfo *self;

    self = g_object_new (ABRT_APPLET_TYPE_PROBLEM_INFO, NULL);

    problem_data_add_text_noteditable(self->problem_data, CD_DUMPDIR, directory);

    return self;
}
