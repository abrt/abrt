/*
 * Utility routines.
 *
 * Licensed under GPLv2, see file COPYING in this tarball for details.
 */
#include "libreport.h"

static char *concat_str_vector(char **strings)
{
	if (!strings[0])
		return xzalloc(1); // returns ""

	unsigned len = 0;
	char **spp = strings;
	while (*spp)
		len += strlen(*spp++) + 1;

	char *result = xmalloc(len);

	char *r = result;
	spp = strings;
	while (*spp) {
		r = stpcpy(r, *spp++);
		*r++ = ' ';
	}
	*--r = '\0';

	return result;
}

/* Returns pid */
pid_t fork_execv_on_steroids(int flags,
		char **argv,
		int *pipefds,
		char **env_vec,
		const char *dir,
		uid_t uid)
{
	pid_t child;
	/* Reminder: [0] is read end, [1] is write end */
	int pipe_to_child[2];
	int pipe_fm_child[2];

	/* Sanitize flags */
	if (!pipefds)
		flags &= ~(EXECFLG_INPUT | EXECFLG_OUTPUT);

	if (flags & EXECFLG_INPUT)
		xpipe(pipe_to_child);
	if (flags & EXECFLG_OUTPUT)
		xpipe(pipe_fm_child);

	fflush(NULL);
	child = fork();
	if (child == -1) {
		perror_msg_and_die("fork");
	}
	if (child == 0) {
		/* Child */

		if (dir)
			xchdir(dir);

		if (flags & EXECFLG_SETGUID) {
			struct passwd* pw = getpwuid(uid);
			gid_t gid = pw ? pw->pw_gid : uid;
			setgroups(1, &gid);
			xsetregid(gid, gid);
			xsetreuid(uid, uid);
		}

		if (env_vec) {
			/* Note: we use the glibc extension that putenv("var")
			 * *unsets* $var if "var" string has no '=' */
			while (*env_vec)
				putenv(*env_vec++);
		}

		/* Play with stdio descriptors */
		if (flags & EXECFLG_INPUT) {
			xmove_fd(pipe_to_child[0], STDIN_FILENO);
			close(pipe_to_child[1]);
		} else if (flags & EXECFLG_INPUT_NUL) {
			xmove_fd(xopen("/dev/null", O_RDWR), STDIN_FILENO);
		}
		if (flags & EXECFLG_OUTPUT) {
			xmove_fd(pipe_fm_child[1], STDOUT_FILENO);
			close(pipe_fm_child[0]);
		} else if (flags & EXECFLG_OUTPUT_NUL) {
			xmove_fd(xopen("/dev/null", O_RDWR), STDOUT_FILENO);
		}

		/* This should be done BEFORE stderr redirect */
		VERB1 {
			char *r = concat_str_vector(argv);
			log("Executing: %s", r);
			free(r);
		}

		if (flags & EXECFLG_ERR2OUT) {
			/* Want parent to see errors in the same stream */
			xdup2(STDOUT_FILENO, STDERR_FILENO);
		} else if (flags & EXECFLG_ERR_NUL) {
			xmove_fd(xopen("/dev/null", O_RDWR), STDERR_FILENO);
		}

		if (flags & EXECFLG_SETSID)
			setsid();

		execvp(argv[0], argv);
		if (!(flags & EXECFLG_QUIET))
			perror_msg("Can't execute '%s'", argv[0]);
		exit(127); /* shell uses this exit code in this case */
	}

	if (flags & EXECFLG_INPUT) {
		close(pipe_to_child[0]);
		pipefds[1] = pipe_to_child[1];
	}
	if (flags & EXECFLG_OUTPUT) {
		close(pipe_fm_child[1]);
		pipefds[0] = pipe_fm_child[0];
	}

	return child;
}

char *run_in_shell_and_save_output(int flags,
		const char *cmd,
		const char *dir,
		size_t *size_p)
{
	flags |= EXECFLG_OUTPUT;
	flags &= ~EXECFLG_INPUT;

	const char *argv[] = { "/bin/sh", "-c", cmd, NULL };
	int pipeout[2];
	pid_t child = fork_execv_on_steroids(flags, (char **)argv, pipeout,
		/*env_vec:*/ NULL, dir, /*uid (unused):*/ 0);

	size_t pos = 0;
	char *result = NULL;
	while (1) {
		result = (char*) xrealloc(result, pos + 4*1024 + 1);
		size_t sz = safe_read(pipeout[0], result + pos, 4*1024);
		if (sz <= 0) {
			break;
		}
		pos += sz;
	}
	result[pos] = '\0';
	if (size_p)
		*size_p = pos;
	close(pipeout[0]);
	waitpid(child, NULL, 0);

	return result;
}
