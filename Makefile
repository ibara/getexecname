#	$OpenBSD$

LIB=	getexecname
SRCS=	getexecname.c
MAN=	getexecname.3
HDRS=	getexecname.h

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${PREFIX}/include/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		-m 444 $$i ${PREFIX}/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
