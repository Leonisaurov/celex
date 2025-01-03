CC ?= clang

build:
	$(MAKE) -C TUI

run: build
	$(MAKE) -C TUI run

clean:
	$(MAKE) -C TUI clean
