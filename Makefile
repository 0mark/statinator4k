include config.mk

SRC = s4k.c notify.c
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

install:
	cp s4k ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/s4k
