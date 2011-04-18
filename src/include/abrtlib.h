/*
 * Utility routines.
 *
 * Licensed under GPLv2, see file COPYING in this tarball for details.
 */
#ifndef ABRTLIB_H_
#define ABRTLIB_H_

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h> /* sockaddr_in, sockaddr_in6 etc */
#include <termios.h>
#include <time.h>
#include <unistd.h>
/* Try to pull in PATH_MAX */
#include <limits.h>
#include <sys/param.h>
#ifndef PATH_MAX
# define PATH_MAX 256
#endif
#include <pwd.h>
#include <grp.h>
/* C++ bits */
#ifdef __cplusplus
# include <string>
#endif
#include <glib.h>

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

#undef ERR_PTR
#define ERR_PTR ((void*)(uintptr_t)1)

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))

#include "xfuncs.h"
#include "logging.h"
#include "read_write.h"
#include "strbuf.h"
#include "hash_sha1.h"

#include "abrt_crash_data.h"
#include "abrt_types.h"
#include "dump_dir.h"
#include "hooklib.h"
#include "run_event.h"
#include "event_config.h"


#ifdef __cplusplus
extern "C" {
#endif

#define prefixcmp abrt_prefixcmp
int prefixcmp(const char *str, const char *prefix);
#define suffixcmp abrt_suffixcmp
int suffixcmp(const char *str, const char *suffix);
#define strtrim abrt_strtrim
char *strtrim(char *str);
#define concat_path_file abrt_concat_path_file
char *concat_path_file(const char *path, const char *filename);
#define append_to_malloced_string abrt_append_to_malloced_string
char *append_to_malloced_string(char *mstr, const char *append);
#define skip_whitespace abrt_skip_whitespace
char* skip_whitespace(const char *s);
#define skip_non_whitespace abrt_skip_non_whitespace
char* skip_non_whitespace(const char *s);
/* Like strcpy but can copy overlapping strings. */
#define overlapping_strcpy abrt_overlapping_strcpy
void overlapping_strcpy(char *dst, const char *src);

/* A-la fgets, but malloced and of unlimited size */
#define xmalloc_fgets abrt_xmalloc_fgets
char *xmalloc_fgets(FILE *file);
/* Similar, but removes trailing \n */
#define xmalloc_fgetline abrt_xmalloc_fgetline
char *xmalloc_fgetline(FILE *file);

/* On error, copyfd_XX prints error messages and returns -1 */
enum {
	COPYFD_SPARSE = 1 << 0,
};
#define copyfd_eof abrt_copyfd_eof
off_t copyfd_eof(int src_fd, int dst_fd, int flags);
#define copyfd_size abrt_copyfd_size
off_t copyfd_size(int src_fd, int dst_fd, off_t size, int flags);
#define copyfd_exact_size abrt_copyfd_exact_size
void copyfd_exact_size(int src_fd, int dst_fd, off_t size);
#define copy_file abrt_copy_file
off_t copy_file(const char *src_name, const char *dst_name, int mode);
#define copy_file_recursive abrt_copy_file_recursive
int copy_file_recursive(const char *source, const char *dest);

/* Returns malloc'ed block */
#define encode_base64 abrt_encode_base64
char *encode_base64(const void *src, int length);

#define xatou abrt_xatou
unsigned xatou(const char *numstr);
#define xatoi abrt_xatoi
int xatoi(const char *numstr);
/* Using xatoi() instead of naive atoi() is not always convenient -
 * in many places people want *non-negative* values, but store them
 * in signed int. Therefore we need this one:
 * dies if input is not in [0, INT_MAX] range. Also will reject '-0' etc.
 * It should really be named xatoi_nonnegative (since it allows 0),
 * but that would be too long.
 */
#define xatoi_positive abrt_xatoi_positive
int xatoi_positive(const char *numstr);

//unused for now
//unsigned long long monotonic_ns(void);
//unsigned long long monotonic_us(void);
//unsigned monotonic_sec(void);

enum {
        /* on return, pipefds[1] is fd to which parent may write
         * and deliver data to child's stdin: */
        EXECFLG_INPUT      = 1 << 0,
        /* on return, pipefds[0] is fd from which parent may read
         * child's stdout: */
        EXECFLG_OUTPUT     = 1 << 1,
        /* open child's stdin to /dev/null: */
        EXECFLG_INPUT_NUL  = 1 << 2,
        /* open child's stdout to /dev/null: */
        EXECFLG_OUTPUT_NUL = 1 << 3,
        /* redirect child's stderr to stdout: */
        EXECFLG_ERR2OUT    = 1 << 4,
        /* open child's stderr to /dev/null: */
        EXECFLG_ERR_NUL    = 1 << 5,
        /* suppress perror_msg("Can't execute 'foo'") if exec fails */
        EXECFLG_QUIET      = 1 << 6,
        EXECFLG_SETGUID    = 1 << 7,
        EXECFLG_SETSID     = 1 << 8,
};
/*
 * env_vec: list of variables to set in environment (if string has
 * "VAR=VAL" form) or unset in environment (if string has no '=' char).
 *
 * Returns pid.
 */
#define fork_execv_on_steroids abrt_fork_execv_on_steroids
pid_t fork_execv_on_steroids(int flags,
                char **argv,
                int *pipefds,
                char **env_vec,
                const char *dir,
                uid_t uid);
/* Returns malloc'ed string. NULs are retained, and extra one is appended
 * after the last byte (this NUL is not accounted for in *size_p) */
#define run_in_shell_and_save_output abrt_run_in_shell_and_save_output
char *run_in_shell_and_save_output(int flags,
                const char *cmd,
                const char *dir,
                size_t *size_p);

/* Random utility functions */

/* Frees every element'd data using free(),
 * then frees list itself using g_list_free(list):
 */
#define list_free_with_free abrt_list_free_with_free
void list_free_with_free(GList *list);

#define get_dirsize abrt_get_dirsize
double get_dirsize(const char *pPath);
#define get_dirsize_find_largest_dir abrt_get_dirsize_find_largest_dir
double get_dirsize_find_largest_dir(
                const char *pPath,
                char **worst_dir, /* can be NULL */
                const char *excluded /* can be NULL */
);

/* Emit a string of hex representation of bytes */
#define bin2hex abrt_bin2hex
char* bin2hex(char *dst, const char *str, int count);
/* Convert "xxxxxxxx" hex string to binary, no more than COUNT bytes */
#define hex2bin abrt_hex2bin
char* hex2bin(char *dst, const char *str, int count);

/* Returns command line of running program.
 * Caller is responsible to free() the returned value.
 * If the pid is not valid or command line can not be obtained,
 * empty string is returned.
 */
#define get_cmdline abrt_get_cmdline
char* get_cmdline(pid_t pid);
#define get_environ abrt_get_environ
char* get_environ(pid_t pid);

/* Returns 1 if abrtd daemon is running, 0 otherwise. */
#define daemon_is_ok abrt_daemon_is_ok
int daemon_is_ok();

/* Takes ptr to time_t, or NULL if you want to use current time.
 * Returns "YYYY-MM-DD-hh:mm:ss" string.
 */
#define iso_date_string abrt_iso_date_string
char *iso_date_string(time_t *pt);

#define make_description_bz abrt_make_description_bz
char* make_description_bz(crash_data_t *crash_data);
#define make_description_comment abrt_make_description_comment
char* make_description_comment(crash_data_t *crash_data);
#define make_description_logger abrt_make_description_logger
char* make_description_logger(crash_data_t *crash_data);
#define make_description_mailx abrt_make_description_mailx
char* make_description_mailx(crash_data_t *crash_data);

#define parse_release_for_bz abrt_parse_release_for_bz
void parse_release_for_bz(const char *pRelease, char **product, char **version);
#define parse_release_for_rhts abrt_parse_release_for_rhts
void parse_release_for_rhts(const char *pRelease, char **product, char **version);

/**
 * Loads settings and stores it in second parameter. On success it
 * returns true, otherwise returns false.
 *
 * @param path A path of config file.
 *  Config file consists of "key=value" lines.
 * @param settings A read plugin's settings.
 * @param skipKeysWithoutValue
 *  If true, lines in format "key=" (without value) are skipped.
 *  Otherwise empty value "" is inserted into pSettings.
 * @return if it success it returns true, otherwise it returns false.
 */
#define load_conf_file abrt_load_conf_file
bool load_conf_file(const char *pPath, map_string_h *settings, bool skipKeysWithoutValue);

/* Tries to create a copy of dump_dir_name in base_dir, with same or similar basename.
 * Returns NULL if copying failed. In this case, logs a message before returning. */
#define steal_directory abrt_steal_directory
struct dump_dir *steal_directory(const char *base_dir, const char *dump_dir_name);

#ifdef __cplusplus
}
#endif

#endif
