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
#include "hash_md5.h"

#include "crash_types.h"
#include "dump_dir.h"
#include "abrt_types.h"
#include "abrt_packages.h"


#ifdef __cplusplus
extern "C" {
#endif

int prefixcmp(const char *str, const char *prefix);
int suffixcmp(const char *str, const char *suffix);
char *concat_path_file(const char *path, const char *filename);
char *append_to_malloced_string(char *mstr, const char *append);
char* skip_whitespace(const char *s);
char* skip_non_whitespace(const char *s);
/* Like strcpy but can copy overlapping strings. */
void overlapping_strcpy(char *dst, const char *src);

/* A-la fgets, but malloced and of unlimited size */
char *xmalloc_fgets(FILE *file);
/* Similar, but removes trailing \n */
char *xmalloc_fgetline(FILE *file);

/* On error, copyfd_XX prints error messages and returns -1 */
enum {
	COPYFD_SPARSE = 1 << 0,
};
off_t copyfd_eof(int src_fd, int dst_fd, int flags);
off_t copyfd_size(int src_fd, int dst_fd, off_t size, int flags);
void copyfd_exact_size(int src_fd, int dst_fd, off_t size);
off_t copy_file(const char *src_name, const char *dst_name, int mode);

/* Returns malloc'ed block */
char *encode_base64(const void *src, int length);

unsigned xatou(const char *numstr);
int xatoi(const char *numstr);
/* Using xatoi() instead of naive atoi() is not always convenient -
 * in many places people want *non-negative* values, but store them
 * in signed int. Therefore we need this one:
 * dies if input is not in [0, INT_MAX] range. Also will reject '-0' etc */
int xatoi_u(const char *numstr);

unsigned long long monotonic_ns(void);
unsigned long long monotonic_us(void);
unsigned monotonic_sec(void);

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
/* Returns pid */
pid_t fork_execv_on_steroids(int flags,
                char **argv,
                int *pipefds,
                char **unsetenv_vec,
                const char *dir,
                uid_t uid);
/* Returns malloc'ed string. NULs are retained, and extra one is appended
 * after the last byte (this NUL is not accounted for in *size_p) */
char *run_in_shell_and_save_output(int flags,
		const char *cmd,
		const char *dir,
		size_t *size_p);

/* Networking helpers */
typedef struct len_and_sockaddr {
	socklen_t len;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} u;
} len_and_sockaddr;
enum {
	LSA_LEN_SIZE = offsetof(len_and_sockaddr, u),
	LSA_SIZEOF_SA = sizeof(struct sockaddr) > sizeof(struct sockaddr_in6) ?
			sizeof(struct sockaddr) : sizeof(struct sockaddr_in6),
};
void setsockopt_reuseaddr(int fd);
int setsockopt_broadcast(int fd);
int setsockopt_bindtodevice(int fd, const char *iface);
len_and_sockaddr* get_sock_lsa(int fd);
void xconnect(int s, const struct sockaddr *s_addr, socklen_t addrlen);
unsigned lookup_port(const char *port, const char *protocol, unsigned default_port);
int get_nport(const struct sockaddr *sa);
void set_nport(len_and_sockaddr *lsa, unsigned port);
len_and_sockaddr* host_and_af2sockaddr(const char *host, int port, sa_family_t af);
len_and_sockaddr* xhost_and_af2sockaddr(const char *host, int port, sa_family_t af);
len_and_sockaddr* host2sockaddr(const char *host, int port);
len_and_sockaddr* xhost2sockaddr(const char *host, int port);
len_and_sockaddr* xdotted2sockaddr(const char *host, int port);
int xsocket_type(len_and_sockaddr **lsap, int family, int sock_type);
int xsocket_stream(len_and_sockaddr **lsap);
int create_and_bind_stream_or_die(const char *bindaddr, int port);
int create_and_bind_dgram_or_die(const char *bindaddr, int port);
int create_and_connect_stream_or_die(const char *peer, int port);
int xconnect_stream(const len_and_sockaddr *lsa);
char* xmalloc_sockaddr2host(const struct sockaddr *sa);
char* xmalloc_sockaddr2host_noport(const struct sockaddr *sa);
char* xmalloc_sockaddr2hostonly_noport(const struct sockaddr *sa);
char* xmalloc_sockaddr2dotted(const struct sockaddr *sa);
char* xmalloc_sockaddr2dotted_noport(const struct sockaddr *sa);

/* Random utility functions */

double get_dirsize(const char *pPath);
double get_dirsize_find_largest_dir(
                const char *pPath,
                char **worst_dir, /* can be NULL */
                const char *excluded /* can be NULL */
);

/* Returns command line of running program.
 * Caller is responsible to free() the returned value.
 * If the pid is not valid or command line can not be obtained,
 * empty string is returned.
 */
char* get_cmdline(pid_t pid);

/* Returns 1 if abrtd daemon is running, 0 otherwise. */
int daemon_is_ok();

struct run_event_state {
    int (*post_run_callback)(const char *dump_dir_name, void *param);
    void *post_run_param;
    char* (*logging_callback)(char *log_line, void *param);
    void *logging_param;
};
static inline struct run_event_state *new_run_event_state()
    { return (struct run_event_state*)xzalloc(sizeof(struct run_event_state)); }
static inline void free_run_event_state(struct run_event_state *state)
    { free(state); }
/* Returns exitcode of first failed action, or first nonzero return value
 * of post_run_callback. If all actions are successful, returns 0.
 * If no actions were run for the event, returns -1.
 */
int run_event(struct run_event_state *state, const char *dump_dir_name, const char *event);
char *list_possible_events(struct dump_dir *dd, const char *dump_dir_name, const char *pfx);

#ifdef __cplusplus
}
#endif


/* C++ style stuff */
#ifdef __cplusplus
std::string unsigned_to_string(unsigned long long x);
std::string signed_to_string(long long x);
template <class T> inline
std::string to_string(T x)
{
    if ((T)~(T)0 < (T)0) /* T is a signed type */
        return signed_to_string(x);
    return unsigned_to_string(x);
}

void parse_args(const char *psArgs, vector_string_t& pArgs, int quote = -1);
void parse_release(const char *pRelease, char **product, char **version);

// TODO: npajkovs: full rewrite ssprintf -> xasprintf
static inline std::string ssprintf(const char *format, ...)
{
    va_list p;
    char *string_ptr;

    va_start(p, format);
    string_ptr = xvasprintf(format, p);
    va_end(p);

    std::string res = string_ptr;
    free(string_ptr);
    return res;
}
#endif

#endif
