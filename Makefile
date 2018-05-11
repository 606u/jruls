
PROG=	jruls
MAN=	
SRCS=	jruls.c

CFLAGS += -Wall -Wextra
CFLAGS += -g -O0
LDFLAGS += -g

LDADD+=	-ljail
DPADD+=	${LIBUTIL} ${LIBPROCSTAT} ${LIBKVM}

.include <bsd.prog.mk>
