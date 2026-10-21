CC ?= gcc
CFLAGS ?= -Wall -Wextra
SANITIZE ?= -fsanitize=address

# El codigo de depuracion del lenguaje se activa con DEBUG=1.
DEBUG ?= 0
ifeq ($(DEBUG),1)
CFLAGS += -DCELEX_DEBUG
endif

BUILD = build
COMMON = Common

TUI_SOURCES = $(wildcard TUI/*.c)
SHEET_SOURCES = $(wildcard Sheet/*.c)
LANG_SOURCES = $(wildcard Lang/*.c)
COMMON_SOURCES = $(wildcard $(COMMON)/*.c)

# La capa de modulo (Sheet) es la que habla con el lenguaje (Lang), y el
# editor (TUI) solo habla con la capa de modulo.
SHARED_SOURCES = $(filter-out $(COMMON)/fileread.c,$(COMMON_SOURCES)) $(filter-out Sheet/cli.c,$(SHEET_SOURCES)) $(LANG_SOURCES)

INCLUDES = -I$(COMMON) -ISheet -ILang

TUI = $(BUILD)/celex
SCRIPT = $(BUILD)/celexs
RUNNER = $(BUILD)/runner

.PHONY: all build tui script run run-script test clean

all: build

build: tui script

tui: $(TUI)

script: $(SCRIPT)

$(TUI): $(TUI_SOURCES) $(SHARED_SOURCES)
	mkdir -p $(BUILD)
	$(CC) $(TUI_SOURCES) $(SHARED_SOURCES) -o $@ $(CFLAGS) $(SANITIZE) $(INCLUDES)

$(SCRIPT): $(SHEET_SOURCES) $(LANG_SOURCES) $(COMMON_SOURCES)
	mkdir -p $(BUILD)
	$(CC) $(SHEET_SOURCES) $(LANG_SOURCES) $(COMMON_SOURCES) -o $@ $(CFLAGS) $(SANITIZE) $(INCLUDES)

$(RUNNER): tests/runner.c
	mkdir -p $(BUILD)
	$(CC) tests/runner.c -o $@ $(CFLAGS) $(SANITIZE)

run: build
	$(TUI) sample.cle

run-script: build
	$(SCRIPT) sample.cle --values

# Las pruebas del lenguaje y de la hoja.
test: build $(RUNNER)
	$(RUNNER) $(SCRIPT) tests

clean:
	rm -rf $(BUILD)
