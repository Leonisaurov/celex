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

INCLUDES = -I$(COMMON) -ISheet -ILang -I$(BUILD)

TUI = $(BUILD)/celex
SCRIPT = $(BUILD)/celexs
RUNNER = $(BUILD)/runner
EDITOR_TEST = $(BUILD)/editor_test

# El numero de compilacion va en la barra del editor, para saber siempre con
# que binario se esta trabajando.
VERSION_HEADER = $(BUILD)/version.h

.PHONY: FORCE
$(VERSION_HEADER): FORCE
	mkdir -p $(BUILD)
	@id="$$(git rev-parse --short HEAD 2>/dev/null || echo dev)"; \
	if [ -n "$$(git status --porcelain 2>/dev/null)" ]; then id="$$id+"; fi; \
	printf '// Generado: no editar.\n#define CELEX_BUILD "%s"\n' "$$id" > $@

# Las pruebas de edicion usan el gestor de celdas, pero no el programa.
EDITOR_SOURCES = $(filter-out TUI/main.c,$(TUI_SOURCES))

.PHONY: all build tui script run run-script test clean

all: build

build: tui script

tui: $(TUI)

script: $(SCRIPT)

$(TUI): $(TUI_SOURCES) $(SHARED_SOURCES) $(VERSION_HEADER)
	mkdir -p $(BUILD)
	$(CC) $(TUI_SOURCES) $(SHARED_SOURCES) -o $@ $(CFLAGS) $(SANITIZE) $(INCLUDES)

$(SCRIPT): $(SHEET_SOURCES) $(LANG_SOURCES) $(COMMON_SOURCES)
	mkdir -p $(BUILD)
	$(CC) $(SHEET_SOURCES) $(LANG_SOURCES) $(COMMON_SOURCES) -o $@ $(CFLAGS) $(SANITIZE) $(INCLUDES)

$(RUNNER): tests/runner.c
	mkdir -p $(BUILD)
	$(CC) tests/runner.c -o $@ $(CFLAGS) $(SANITIZE)

$(EDITOR_TEST): tests/editor_test.c $(EDITOR_SOURCES) $(SHARED_SOURCES) $(VERSION_HEADER)
	mkdir -p $(BUILD)
	$(CC) tests/editor_test.c $(EDITOR_SOURCES) $(SHARED_SOURCES) -o $@ $(CFLAGS) $(SANITIZE) $(INCLUDES) -ITUI

run: build
	$(TUI) sample.cle

run-script: build
	$(SCRIPT) sample.cle --values

# Las pruebas del lenguaje y de la hoja.
test: build $(RUNNER) $(EDITOR_TEST)
	$(RUNNER) $(SCRIPT) tests
	$(EDITOR_TEST)

clean:
	rm -rf $(BUILD)
