CC ?= clang
CFLAGS ?= -Wall -Wextra

build: *.c *.h
	${CC} *.c -o celex ${CFLAGS} -fsanitize=address

run: build
	./celex ../sample.cle

clean:
	rm -f celex

.PHONY: build run clean
