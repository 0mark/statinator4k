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
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f s4k ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/s4k
	@sed "s#S4KSRC#${PREFIX}/share/s4k/src/#g" < s4km > ${DESTDIR}${PREFIX}/bin/s4km
	@chmod 755 ${DESTDIR}${PREFIX}/bin/s4km
	@#TODO: write tfm!
	@#@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@#@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@#@sed "s/VERSION/${VERSION}/g" < s4k.1 > ${DESTDIR}${MANPREFIX}/man1/s4k.1
	@#@chmod 644 ${DESTDIR}${MANPREFIX}/man1/s4k.1
	@echo installing src files to ${DESTDIR}${PREFIX}/share/s4k
	@mkdir -p ${DESTDIR}${PREFIX}/share/s4k/src
	@cp -f s4k.c notify.c notify.h config.def.h config.sprinkles.h config.mk formats_*.h Makefile   ${DESTDIR}${PREFIX}/share/s4k/src

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm ${DESTDIR}${PREFIX}/bin/s4k
	@rm ${DESTDIR}${PREFIX}/bin/s4km
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/s4k.1
	@echo removing src files from ${DESTDIR}${MANPREFIX}/share
	@rm -Rf ${DESTDIR}${PREFIX}/share/s4k
