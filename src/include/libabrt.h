/*
 * Utility routines.
 *
 * Licensed under GPLv2, see file COPYING in this tarball for details.
 */
#ifndef LIBABRT_H_
#define LIBABRT_H_

/* libreport's internal functions we use: */
#include <internal_libreport.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Must be after #include "config.h" */
#if ENABLE_NLS
# include <libintl.h>
# define _(S) gettext(S)
#else
# define _(S) (S)
#endif

/* Some libc's forget to declare these, do it ourself */
extern char **environ;
#if defined(__GLIBC__) && __GLIBC__ < 2
int vdprintf(int d, const char *format, va_list ap);
#endif

#undef NORETURN
#define NORETURN __attribute__ ((noreturn))

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))

#ifdef __cplusplus
extern "C" {
#endif

#define check_free_space abrt_check_free_space
void check_free_space(unsigned setting_MaxCrashReportsSize, const char *dump_location);
#define trim_debug_dumps abrt_trim_debug_dumps
void trim_debug_dumps(const char *dirname, double cap_size, const char *exclude_path);


#define g_settings_nMaxCrashReportsSize abrt_g_settings_nMaxCrashReportsSize
extern unsigned int  g_settings_nMaxCrashReportsSize;
#define g_settings_sWatchCrashdumpArchiveDir abrt_g_settings_sWatchCrashdumpArchiveDir
extern char *        g_settings_sWatchCrashdumpArchiveDir;
#define g_settings_dump_location abrt_g_settings_dump_location
extern char *        g_settings_dump_location;

#define load_abrt_conf abrt_load_abrt_conf
int load_abrt_conf();
#define free_abrt_conf_data abrt_free_abrt_conf_data
void free_abrt_conf_data();


/* Returns 1 if abrtd daemon is running, 0 otherwise. */
#define daemon_is_ok abrt_daemon_is_ok
int daemon_is_ok();

/* Note: should be public since unit tests need to call it */
#define koops_extract_version abrt_koops_extract_version
char *koops_extract_version(const char *line);


#ifdef __cplusplus
}
#endif

#endif
