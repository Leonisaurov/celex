CC ?= gcc
CFLAGS ?= -Wall -Wextra
SANITIZE ?= -fsanitize=address

BUILD = build
COMMON = Common

TUI_SOURCES = $(wildcard TUI/*.c)
LANG_SOURCES = $(wildcard Lang/*.c)
COMMON_SOURCES = $(wildcard $(COMMON)/*.c)
SHARED_SOURCES = $(filter-out $(COMMON)/fileread.c,$(COMMON_SOURCES)) $(filter-out Lang/cli.c,$(LANG_SOURCES))

INCLUDES = -I$(COMMON) -ILang

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

$(SCRIPT): $(LANG_SOURCES) $(COMMON_SOURCES)
	mkdir -p $(BUILD)
	$(CC) $(LANG_SOURCES) $(COMMON_SOURCES) -o $@ $(CFLAGS) $(SANITIZE) $(INCLUDES)

run: build
	$(TUI) sample.cle

run-script: build
	$(SCRIPT) sample.cle --values

clean:
	rm -rf $(BUILD)
