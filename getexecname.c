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

/*
 * Couldn't get env via sysctl?
 * Try getenv(3).
 */
static char *
environment(void)
{
	char *env, *p;
	size_t len;

	/* "PATH=" + '\0' = 6  */
	p = getenv("PATH");
	len = strlen(p) + 6;

	if ((env = malloc(len)) == NULL)
		return NULL;

	strcpy(env, "PATH=");
	strcat(env, p);

	return env;
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
	char *s, t;
	int end = 0, mib[4];
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
	if (*argv[0] == '/' || *argv[0] == '.') {
		errno = 0;

		snprintf(execname, sizeof(execname), "%s", realpath(argv[0], NULL));

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

	if (sysctl(mib, 4, NULL, &len, NULL, 0) == -1) {
		if ((env = malloc(sizeof(char *))) == NULL) {
			free(argv);

			return NULL;
		}

		if ((env[0] = environment()) == NULL)
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
		if (!strncmp(env[i], "PATH=", 5)) {
			env[i] += 5;

			if (*env[i] == '\0')
				goto error;

check:
			s = env[i];
			while ((t = *env[i]) != ':') {
				if (t == '\0') {
					end = 1;
					break;
				}
				++env[i];
			}
			*env[i] = '\0';

			snprintf(execname, sizeof(execname), "%s/%s", s, argv[0]);

			/* Success, probably.  */
			if (access(execname, X_OK) == 0) {
				free(argv);
				free(env);

				return execname;
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
	free(argv);
	free(env);

	return NULL;
}
