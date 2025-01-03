CC ?= clang

build: *.c
	${CC} *.c -o celex -fsanitize=address

run: build
	./celex sample.cle
clean: ./celex
	rm celex
