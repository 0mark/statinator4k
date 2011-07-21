include config.mk

SRC = s4k.c ${NOTIFY_CFILES}
OBJ = ${SRC:.c=.o}

all: options s4k

options:
	@echo statinator4k build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h formats*.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

s4k: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f s4k ${OBJ} dstat-${VERSION}.tar.gz

uberclean:
	@echo UBER cleaning
	@rm -f s4k ${OBJ} dstat-${VERSION}.tar.gz
	@rm -f config.h

install:
	cp s4k ${DESTDIR}${PREFIX}/bin

uninstall:
	rm ${DESTDIR}${PREFIX}/bin/s4k
