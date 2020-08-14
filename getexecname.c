/*
 * Copyright (c) 2020 Brian Callahan <bcallah@openbsd.org>
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

#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "getexecname.h"

/*
 * Get full path name of running executable.
 * On a best-guess basis.
 * Return string of full pathname, or NULL if something goes wrong.
 */
const char *
getexecname(void)
{
	static char bin[PATH_MAX];
	char *buf[1024]; /* Probably overkill */
	char *s;
	char t;
	int end = 0, i, mib[4];
	size_t len;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC_ARGS;
	mib[2] = getpid();
	mib[3] = KERN_PROC_ENV;

	len = sizeof(buf);

	if (sysctl(mib, 4, buf, &len, NULL, 0) == -1)
		return NULL;

	/*
	 * Some shells (ksh, bash) have a _ variable.
	 * But only use it if we have a full path.
	 */
	i = 0;
	while (buf[i] != NULL) {
		if (!strncmp(buf[i], "_=/", 3)) {
			++buf[i];
			++buf[i];
			if (!strcmp(basename(buf[i]), getprogname()))
				return buf[i];
		}
		++i;
	}

	/*
	 * Otherwise, grab PATH and return the first hit.
	 * This could be wrong if you have two executables
	 * of the same name in different PATH dirs.
	 */
	i = 0;
	while (buf[i] != NULL) {
		if (!strncmp(buf[i], "PATH=", 5)) {
			++buf[i];
			++buf[i];
			++buf[i];
			++buf[i];
			++buf[i];

check:
			s = buf[i];
			while ((t = *buf[i]) != ':') {
				if (t == '\0') {
					end = 1;
					break;
				}
				++buf[i];
			}
			*buf[i] = '\0';

			strlcpy(bin, s, sizeof(bin));
			strlcat(bin, "/", sizeof(bin));
			strlcat(bin, getprogname(), sizeof(bin));

			/* Success, probably.  */
			if (access(bin, X_OK) == 0)
				return bin;

			if (end || *++buf[i] == '\0')
				break;

			goto check;
		}
		++i;
	}

	/*
	 * Or maybe it's in PWD?
	 */
	i = 0;
	while (buf[i] != NULL) {
		if (!strncmp(buf[i], "PWD=", 4)) {
			++buf[i];
			++buf[i];
			++buf[i];
			++buf[i];

			s = buf[i];
			while ((t = *buf[i]) != ':') {
				if (t == '\0') {
					end = 1;
					break;
				}
				++buf[i];
			}
			*buf[i] = '\0';

			strlcpy(bin, s, sizeof(bin));
			strlcat(bin, "/", sizeof(bin));
			strlcat(bin, getprogname(), sizeof(bin));

			/* Success, probably.  */
			if (access(bin, X_OK) == 0)
				return bin;
		}
		++i;
	}

	/*
	 * Or maybe it's in OLDPWD?
	 */
	i = 0;
	while (buf[i] != NULL) {
		if (!strncmp(buf[i], "OLDPWD=", 7)) {
			++buf[i];
			++buf[i];
			++buf[i];
			++buf[i];
			++buf[i];
			++buf[i];
			++buf[i];

			s = buf[i];
			while ((t = *buf[i]) != ':') {
				if (t == '\0') {
					end = 1;
					break;
				}
				++buf[i];
			}
			*buf[i] = '\0';

			strlcpy(bin, s, sizeof(bin));
			strlcat(bin, "/", sizeof(bin));
			strlcat(bin, getprogname(), sizeof(bin));

			/* Success, probably.  */
			if (access(bin, X_OK) == 0)
				return bin;
		}
		++i;
	}

	/*
	 * If we still can't find anything... relative path it is.
	 */
	i = 0;
	while (buf[i] != NULL) {
		if (!strncmp(buf[i], "_=", 2)) {
			++buf[i];
			++buf[i];
			if (!strcmp(basename(buf[i]), getprogname()))
				return buf[i];
		}
		++i;
	}

	/*
	 * And if we still can't find anything... getprogname.
	 */
	return getprogname();
}
