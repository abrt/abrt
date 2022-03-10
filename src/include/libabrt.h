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


/**
  @brief Checks if there is enough free space to store the problem data

  @param setting_MaxCrashReportsSize Maximum data size
  @param dump_location Location to check for the available space
*/
int abrt_low_free_space(unsigned setting_MaxCrashReportsSize, const char *dump_location);

void abrt_trim_problem_dirs(const char *dirname, double cap_size, const char *exclude_path);
void abrt_ensure_writable_dir_uid_gid(const char *dir, mode_t mode, uid_t uid, gid_t gid);
void abrt_ensure_writable_dir(const char *dir, mode_t mode, const char *user);
void abrt_ensure_writable_dir_group(const char *dir, mode_t mode, const char *user, const char *group);
char *abrt_run_unstrip_n(const char *dump_dir_name, unsigned timeout_sec);
char *abrt_get_backtrace(struct dump_dir *dd, unsigned timeout_sec);

bool abrt_dir_is_in_dump_location(const char *dir_name);

enum {
    DD_PERM_EVENTS  = 1 << 0,
    DD_PERM_DAEMONS = 1 << 1,
};
bool abrt_dir_has_correct_permissions(const char *dir_name, int flags);
bool abrt_new_user_problem_entry_allowed(uid_t uid, const char *name, const char *value);

extern unsigned int  abrt_g_settings_nMaxCrashReportsSize;
extern char *        abrt_g_settings_sWatchCrashdumpArchiveDir;
extern char *        abrt_g_settings_dump_location;
extern bool          abrt_g_settings_delete_uploaded;
extern bool          abrt_g_settings_autoreporting;
extern char *        abrt_g_settings_autoreporting_event;
extern bool          abrt_g_settings_shortenedreporting;
extern bool          abrt_g_settings_explorechroots;
extern unsigned int  abrt_g_settings_debug_level;


int abrt_load_abrt_conf(void);
void abrt_free_abrt_conf_data(void);

int abrt_load_abrt_conf_file(const char *file, GHashTable *settings);

int abrt_load_abrt_plugin_conf_file(const char *file, GHashTable *settings);

int abrt_save_abrt_conf_file(const char *file, GHashTable *settings);

int abrt_save_abrt_plugin_conf_file(const char *file, GHashTable *settings);


void migrate_to_xdg_dirs(void);

int check_recent_crash_file(const char *filename, const char *executable);

/* Returns 1 if abrtd daemon is running, 0 otherwise. */
int abrt_daemon_is_ok(void);

/**
@brief Sends notification to abrtd that a new problem has been detected

@param[in] path Path to the problem directory containing the problem data
*/
void abrt_notify_new_path(const char *path);

/**
@brief Sends notification to abrtd that a new problem has been detected and
wait for the reply

@param path Path to the problem directory containing the problem data
@param message The abrtd reply
@return -errno on error otherwise return value of abrtd
*/
int abrt_notify_new_path_with_response(const char *path, char **message);

/* Note: should be public since unit tests need to call it */
char *abrt_koops_extract_version(const char *line);
char *abrt_kernel_tainted_short(const char *kernel_bt);
char *abrt_kernel_tainted_long(const char *tainted_short);
char *abrt_koops_hash_str_ext(const char *oops_buf, int frame_count, int duphas_flags);
char *abrt_koops_hash_str(const char *oops_buf);


int abrt_koops_line_skip_level(const char **c);
void abrt_koops_line_skip_jiffies(const char **c);

/*
 * extract_oops tries to find oops signatures in a log
 */
struct abrt_koops_line_info {
    char *ptr;
    int level;
};

void abrt_koops_extract_oopses_from_lines(GList **oops_list, const struct abrt_koops_line_info *lines_info, int lines_info_size);
void abrt_koops_extract_oopses(GList **oops_list, char *buffer, size_t buflen);
GList *abrt_koops_suspicious_strings_list(void);
GList *abrt_koops_suspicious_strings_blacklist(void);
void abrt_koops_print_suspicious_strings(void);
/**
 * Prints all suspicious strings that do not match any of the regular
 * expression in NULL terminated list.
 *
 * The regular expression should be compiled with REG_NOSUB flag.
 */
void abrt_koops_print_suspicious_strings_filtered(const regex_t **filterout);

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

#ifdef __cplusplus
}
#endif

#endif
