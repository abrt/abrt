#pragma once

#include <glib-object.h>
#include <libreport/problem_data.h>
#include <stdbool.h>

#define ABRT_APPLET_TYPE_PROBLEM_INFO abrt_applet_problem_info_get_type ()
G_DECLARE_FINAL_TYPE (AbrtAppletProblemInfo, abrt_applet_problem_info,
                      ABRT_APPLET, PROBLEM_INFO, GObject)

bool                    abrt_applet_problem_info_is_announced     (AbrtAppletProblemInfo *problem_info);
bool                    abrt_applet_problem_info_is_foreign       (AbrtAppletProblemInfo *problem_info);
bool                    abrt_applet_problem_info_is_packaged      (AbrtAppletProblemInfo *problem_info);
bool                    abrt_applet_problem_info_is_reported      (AbrtAppletProblemInfo *problem_info);

const char             *abrt_applet_problem_info_get_command_line (AbrtAppletProblemInfo *problem_info);
int                     abrt_applet_problem_info_get_count        (AbrtAppletProblemInfo *problem_info);
const char *            abrt_applet_problem_info_get_directory    (AbrtAppletProblemInfo *problem_info);
const char            **abrt_applet_problem_info_get_environment  (AbrtAppletProblemInfo *problem_info);
int                     abrt_applet_problem_info_get_pid          (AbrtAppletProblemInfo *problem_info);
int                     abrt_applet_problem_info_get_time         (AbrtAppletProblemInfo *problem_info);

void                    abrt_applet_problem_info_set_announced    (AbrtAppletProblemInfo *problem_info,
                                                                   bool                   announced);
void                    abrt_applet_problem_info_set_directory    (AbrtAppletProblemInfo *problem_info,
                                                                   const char            *directory);
void                    abrt_applet_problem_info_set_foreign      (AbrtAppletProblemInfo *problem_info,
                                                                   bool                   foreign);
void                    abrt_applet_problem_info_set_known        (AbrtAppletProblemInfo *problem_info,
                                                                   bool                   known);
void                    abrt_applet_problem_info_set_reported     (AbrtAppletProblemInfo *problem_info,
                                                                   bool                   reported);

bool                    abrt_applet_problem_info_ensure_writable  (AbrtAppletProblemInfo *problem_info);

bool                    abrt_applet_problem_info_load_over_dbus   (AbrtAppletProblemInfo *problem_info);

AbrtAppletProblemInfo  *abrt_applet_problem_info_new              (const char            *directory);
