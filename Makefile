CC ?= gcc
CFLAGS ?= -Wall -Wextra
SANITIZE ?= -fsanitize=address

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

.PHONY: all build tui script run run-script clean

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

run: build
	$(TUI) sample.cle

run-script: build
	$(SCRIPT) sample.cle --values

clean:
	rm -rf $(BUILD)
