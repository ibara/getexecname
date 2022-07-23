/*
 * Copyright (c) 2020-2021 Brian Callahan <bcallah@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "getexecname.h"

extern char **environ;

/*
 * Couldn't get env via sysctl?
 * Try environ variable.
 */
static char *
environment(int condition)
{
	for (char **cur = environ; *cur; ++cur)
		if (!strncmp(*cur, (condition) ? "PATH=" : "PWD=", 4 + condition))
			return *cur;
	return NULL;
}

/*
 * Get full path name of running executable.
 * Return string of full pathname, or NULL if error.
 */
const char *
getexecname(void)
{
	static char execname[PATH_MAX];
	char **argv, **env;
	char *s = NULL, t;
	int end = 0, mib[4], condition = 0;
	size_t i, len;

	(void) memset(execname, 0, sizeof(execname));

	/*
	 * Option 1:
	 * Get argv[0] and run it against realpath(3).
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC_ARGS;
	mib[2] = getpid();
	mib[3] = KERN_PROC_ARGV;

	if (sysctl(mib, 4, NULL, &len, NULL, 0) == -1)
		return NULL;

	if ((argv = malloc(sizeof(char *) * (len - 1))) == NULL)
		return NULL;

	if (sysctl(mib, 4, argv, &len, NULL, 0) == -1)
		return NULL;

	/* Can be resolved with realpath(3)?  Do that and done.  */
	if (*argv[0] == '/') {
cwd:
		errno = 0;

		if (realpath(argv[0], execname) != NULL) {
			snprintf(execname, sizeof(execname), "%s", execname);
		}

		free(argv);

		if (errno != 0)
			return NULL;

		return execname;
	}

	/*
	 * Option 2:
	 * Get the program's PATH and try each in order.
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC_ARGS;
	mib[2] = getpid();
	mib[3] = KERN_PROC_ENV;

        condition = (*argv[0] == '.');

	if (sysctl(mib, 4, NULL, &len, NULL, 0) == -1) {
		if ((env = malloc(sizeof(char *))) == NULL) {
			free(argv);

			return NULL;
		}

		if ((env[0] = environment((condition) ? 0 : 1)) == NULL)
			goto error;
	} else {
		if ((env = malloc(sizeof(char *) * (len - 1))) == NULL) {
			free(argv);

			return NULL;
		}

		if (sysctl(mib, 4, env, &len, NULL, 0) == -1)
			goto error;
	}

	/* PATH resolution.  */
	for (i = 0; env[i] != NULL; i++) {
		if (!strncmp(env[i], (condition) ? "PWD=" : "PATH=", (condition) ? 4 : 5)) {
			env[i] += (condition) ? 4 : 5;

			if (*env[i] == '\0')
				goto error;

check:
			if (condition) 
				goto pwd;

			s = env[i];
			while ((t = *env[i]) != ':') {
				if (t == '\0') {
					end = 1;
					break;
				}
				++env[i];
			}
			*env[i] = '\0';

pwd:
			if (s == NULL) 
				s = env[i];

			snprintf(execname, sizeof(execname), "%s/%s", s, argv[0]);
			if (realpath(execname, execname) != NULL) {
				snprintf(execname, sizeof(execname), "%s", execname);
				if (access(execname, X_OK) == 0) {
					free(argv);
					free(env);

					return execname;
				}
			}

			if (end || *++env[i] == '\0')
				break;

			goto check;
		}
	}

	/*
	 * Out of options.
	 */
error:
	free(env);

	if (condition) 
		goto cwd;

	free(argv);

	return NULL;
}
