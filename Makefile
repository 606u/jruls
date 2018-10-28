
PROG=	jruls
MAN=	
SRCS=	jruls.c

CFLAGS += -Wall -Wextra
CFLAGS += -g -O0
LDFLAGS += -g

LDADD+=	-ljail -lcurses

.include <bsd.prog.mk>
