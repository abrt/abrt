/*
 * Utility routines.
 *
 * Licensed under GPLv2, see file COPYING in this tarball for details.
 */

/** @file libabrt.h */

#ifndef LIBABRT_H_
#define LIBABRT_H_

#include <regex.h>

#include <gio/gio.h> /* dbus */
#include "abrt-dbus.h"
/* libreport's internal functions we use: */
#include <libreport/internal_libreport.h>
#include "hooklib.h"

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))

#ifdef __cplusplus
extern "C" {
#endif

/* Some libc's forget to declare these, do it ourself */
extern char **environ;
#if defined(__GLIBC__) && __GLIBC__ < 2
int vdprintf(int d, const char *format, va_list ap);
#endif


#define low_free_space abrt_low_free_space
/**
  @brief Checks if there is enough free space to store the problem data

  @param setting_MaxCrashReportsSize Maximum data size
  @param dump_location Location to check for the available space
*/
int low_free_space(unsigned setting_MaxCrashReportsSize, const char *dump_location);

#define trim_problem_dirs abrt_trim_problem_dirs
void trim_problem_dirs(const char *dirname, double cap_size, const char *exclude_path);
#define ensure_writable_dir_id abrt_ensure_writable_dir_uid_git
void ensure_writable_dir_uid_gid(const char *dir, mode_t mode, uid_t uid, gid_t gid);
#define ensure_writable_dir abrt_ensure_writable_dir
void ensure_writable_dir(const char *dir, mode_t mode, const char *user);
#define ensure_writable_dir_group abrt_ensure_writable_dir_group
void ensure_writable_dir_group(const char *dir, mode_t mode, const char *user, const char *group);
#define run_unstrip_n abrt_run_unstrip_n
char *run_unstrip_n(const char *dump_dir_name, unsigned timeout_sec);
#define get_backtrace abrt_get_backtrace
char *get_backtrace(const char *dump_dir_name, unsigned timeout_sec, const char *debuginfo_dirs);

#define dir_is_in_dump_location abrt_dir_is_in_dump_location
bool dir_is_in_dump_location(const char *dir_name);

enum {
    DD_PERM_EVENTS  = 1 << 0,
    DD_PERM_DAEMONS = 1 << 1,
};
#define dir_has_correct_permissions abrt_dir_has_correct_permissions
bool dir_has_correct_permissions(const char *dir_name, int flags);
#define allowed_new_user_problem_entry abrt_allowed_new_user_problem_entry
bool allowed_new_user_problem_entry(uid_t uid, const char *name, const char *value);

#define g_settings_nMaxCrashReportsSize abrt_g_settings_nMaxCrashReportsSize
extern unsigned int  g_settings_nMaxCrashReportsSize;
#define g_settings_sWatchCrashdumpArchiveDir abrt_g_settings_sWatchCrashdumpArchiveDir
extern char *        g_settings_sWatchCrashdumpArchiveDir;
#define g_settings_dump_location abrt_g_settings_dump_location
extern char *        g_settings_dump_location;
#define g_settings_delete_uploaded abrt_g_settings_delete_uploaded
extern bool          g_settings_delete_uploaded;
#define g_settings_autoreporting abrt_g_settings_autoreporting
extern bool          g_settings_autoreporting;
#define g_settings_autoreporting_event abrt_g_settings_autoreporting_event
extern char *        g_settings_autoreporting_event;
#define g_settings_shortenedreporting abrt_g_settings_shortenedreporting
extern bool          g_settings_shortenedreporting;
#define g_settings_explorechroots abrt_g_settings_explorechroots
extern bool          g_settings_explorechroots;
#define g_settings_debug_level abrt_g_settings_debug_level
extern unsigned int  g_settings_debug_level;


#define load_abrt_conf abrt_load_abrt_conf
int load_abrt_conf(void);
#define free_abrt_conf_data abrt_free_abrt_conf_data
void free_abrt_conf_data(void);

#define load_abrt_conf_file abrt_load_abrt_conf_file
int load_abrt_conf_file(const char *file, map_string_t *settings);

#define load_abrt_plugin_conf_file abrt_load_abrt_plugin_conf_file
int load_abrt_plugin_conf_file(const char *file, map_string_t *settings);

#define save_abrt_conf_file abrt_save_abrt_conf_file
int save_abrt_conf_file(const char *file, map_string_t *settings);

#define save_abrt_plugin_conf_file abrt_save_abrt_plugin_conf_file
int save_abrt_plugin_conf_file(const char *file, map_string_t *settings);


void migrate_to_xdg_dirs(void);

int check_recent_crash_file(const char *filename, const char *executable);

/* Returns 1 if abrtd daemon is running, 0 otherwise. */
#define daemon_is_ok abrt_daemon_is_ok
int daemon_is_ok(void);

/**
@brief Sends notification to abrtd that a new problem has been detected

@param[in] path Path to the problem directory containing the problem data
*/
#define notify_new_path abrt_notify_new_path
void notify_new_path(const char *path);

/* Note: should be public since unit tests need to call it */
#define koops_extract_version abrt_koops_extract_version
char *koops_extract_version(const char *line);
#define kernel_tainted_short abrt_kernel_tainted_short
char *kernel_tainted_short(const char *kernel_bt);
#define kernel_tainted_long abrt_kernel_tainted_long
char *kernel_tainted_long(const char *tainted_short);
#define koops_hash_str_ext abrt_koops_hash_str_ext
int koops_hash_str_ext(char hash_str[SHA1_RESULT_LEN*2 + 1], const char *oops_buf, int frame_count, int duphas_flags);
#define koops_hash_str abrt_koops_hash_str
int koops_hash_str(char hash_str[SHA1_RESULT_LEN*2 + 1], const char *oops_buf);


#define koops_line_skip_level abrt_koops_line_skip_level
int koops_line_skip_level(const char **c);
#define koops_line_skip_jiffies abrt_koops_line_skip_jiffies
void koops_line_skip_jiffies(const char **c);

/*
 * extract_oops tries to find oops signatures in a log
 */
struct abrt_koops_line_info {
    char *ptr;
    int level;
};

#define koops_extract_oopses_from_lines abrt_koops_extract_oopses_from_lines
void koops_extract_oopses_from_lines(GList **oops_list, const struct abrt_koops_line_info *lines_info, int lines_info_size);
#define koops_extract_oopses abrt_koops_extract_oopses
void koops_extract_oopses(GList **oops_list, char *buffer, size_t buflen);
#define koops_suspicious_strings_list abrt_koops_suspicious_strings_list
GList *koops_suspicious_strings_list(void);
#define koops_print_suspicious_strings abrt_koops_print_suspicious_strings
void koops_print_suspicious_strings(void);
/**
 * Prints all suspicious strings that do not match any of the regular
 * expression in NULL terminated list.
 *
 * The regular expression should be compiled with REG_NOSUB flag.
 */
#define koops_print_suspicious_strings_filtered abrt_koops_print_suspicious_strings_filtered
void koops_print_suspicious_strings_filtered(const regex_t **filterout);

/* dbus client api */

/**
  @brief Changes the access rights of the problem specified by problem id

  Requires authorization

  @return 0 if successful; non-zero on failure
*/
int chown_dir_over_dbus(const char *problem_dir_path);

/**
  @brief Checks whether the given element name exists

  Might require authorization

  @return Positive number if such an element exist, 0 if doesn't and negative number if an error occurs.
 */
int test_exist_over_dbus(const char *problem_id, const char *element_name);

/**
  @brief Checks whether the problem corresponding to the given ID is complete

  Might require authorization

  @return Positive number if the problem is complete, 0 if doesn't and negative number if an error occurs.
 */
int dbus_problem_is_complete(const char *problem_id);

/**
  @ Returns value of the given element name

  Might require authorization

  @return malloced string or NULL if no such an element exists; ERR_PTR in case of any error.
 */
char *load_text_over_dbus(const char *problem_id, const char *element_name);

/**
 @brief Delets multiple problems specified by their id (as returned from problem_data_save)

 @param problem_dir_paths List of problem ids

 @return 0 if operation was successful, non-zero on failure
*/

int delete_problem_dirs_over_dbus(const GList *problem_dir_paths);

/**
  @brief Fetches given problem elements for specified problem id

  @return returns non-zero value on failures and prints error message
*/
int fill_problem_data_over_dbus(const char *problem_dir_path, const char **elements, problem_data_t *problem_data);

/**
  @brief Fetches problem information for specified problem id

  @return a valid pointer to problem_data_t or ERR_PTR on failure
*/
problem_data_t *get_problem_data_dbus(const char *problem_dir_path);

/**
  @brief Fetches full problem data for specified problem id

  @return a valid pointer to problem_data_t or ERR_PTR on failure
*/
problem_data_t *get_full_problem_data_over_dbus(const char *problem_dir_path);

/**
  @brief Fetches all problems from problem database

  @param authorize If set to true will try to fetch even problems owned by other users (will require root authorization over policy kit)

  @return List of problem ids or ERR_PTR on failure (NULL is an empty list)
*/
GList *get_problems_over_dbus(bool authorize);

/**
  @struct ignored_problems
  @brief An opaque structure holding a list of ignored problems
*/
typedef struct ignored_problems ignored_problems_t;

/**
  @brief Initializes a new instance of ignored problems

  @param file_path A malloced string holding a path to a file containing the list of ignored problems. Function takes ownership of the malloced memory, which will be freed in ignored_problems_free()
  @see ignored_problems_free()
  @return Fully initialized instance of ignored problems struct which must be destroyed by ignored_problems_free()
*/
ignored_problems_t *ignored_problems_new(char *file_path);

/**
  @brief Destroys an instance of ignored problems

  This function never fails. Supports the common behaviour where it accepts
  NULL pointers.

  @param set A destroyed instance
*/
void ignored_problems_free(ignored_problems_t *set);

/**
  @brief Adds a problem to the ignored problems

  This function never fails. All errors will be logged.

  @param set An instance of ignored problems to which the problem will be added
  @param problem_id An identifier of a problem which will be added to an ignored set
*/
void ignored_problems_add(ignored_problems_t *set, const char *problem_id);

/**
  @brief Removes a problem from the ignored problems

  This function never fails. All errors will be logged.

  @param set An instance of ignored problems from which the problem will be deleted
  @param problem_id An identifier of a problem which will be removed from an ignored problems struct
*/
void ignored_problems_remove(ignored_problems_t *set, const char *problem_id);

/**
  @brief Checks if a problem is in the ignored problems

  This function never fails. All errors will be logged. If any error occurs,
  the function returns 0 value.

  @param set An instance of ignored problems in which the problem will be searched
  @param problem_id An identifier of a problem
  @return Non 0 value if the problem is ignored; otherwise returns 0 value.
*/
bool ignored_problems_contains(ignored_problems_t *set, const char *problem_id);

/**
  @brief Adds a problem defined by its data to the ignored problems

  This function never fails. All errors will be logged.

  @param set An instance of ignored problems to which the problem will be added
  @param pd A data of a problem which will be added to an ignored set
*/
void ignored_problems_add_problem_data(ignored_problems_t *set, problem_data_t *pd);

/**
  @brief Removes a problem defined by its data from the ignored problems

  This function never fails. All errors will be logged.

  @param set An instance of ignored problems from which the problem will be deleted
  @param pd A data of a problem which will be removed from an ignored problems struct
*/
void ignored_problems_remove_problem_data(ignored_problems_t *set, problem_data_t *pd);

/**
  @brief Checks if a problem defined its data is in the ignored problems

  This function never fails. All errors will be logged. If any error occurs,
  the function returns 0 value.

  @param set An instance of ignored problems in which the problem will be searched
  @param pd A data of a problem
  @return Non 0 value if the problem is ignored; otherwise returns 0 value.
*/
bool ignored_problems_contains_problem_data(ignored_problems_t *set, problem_data_t *pd);

#ifdef __cplusplus
}
#endif

#endif
