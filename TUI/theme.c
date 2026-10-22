#include "theme.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Apagar lo que hubiera: es el principio de toda secuencia de estilo, y lo que
// llevan los estilos que no pintan nada.
#define RESET "\x1b[0m"

typedef struct {
    const char *name;
    const char *sequences[STYLE_COUNT];
} Theme;

// Los temas. Los estilos que no se nombran se quedan sin color, que es el
// aspecto de siempre: la rejilla tal cual y la celda seleccionada al reves.
static const Theme themes[] = {
    {
        "normal",
        {
            [STYLE_CURSOR] = "\x1b[0;7m",
            [STYLE_STATUS] = "\x1b[0;7m",
        },
    },
    {
        // Para una terminal oscura: la rejilla gris para que no pese, los
        // valores en amarillo para distinguirlos del fuente, y la celda
        // seleccionada en azul.
        "oscuro",
        {
            [STYLE_GRID] = "\x1b[0;90m",
            [STYLE_HEADER] = "\x1b[0;1;36m",
            [STYLE_TEXT] = "\x1b[0;37m",
            [STYLE_VALUE] = "\x1b[0;33m",
            [STYLE_CURSOR] = "\x1b[0;44;97m",
            [STYLE_COPIED] = "\x1b[0;1;35m",
            [STYLE_STATUS] = "\x1b[0;104;97m",
            [STYLE_MESSAGE] = "\x1b[0;32m",
            [STYLE_ERROR] = "\x1b[0;1;31m",
        },
    },
    {
        // Para una terminal clara, con el azul y el verde oscuros para que se
        // lean sobre blanco.
        "claro",
        {
            [STYLE_GRID] = "\x1b[0;37m",
            [STYLE_HEADER] = "\x1b[0;1;34m",
            [STYLE_TEXT] = "\x1b[0;30m",
            [STYLE_VALUE] = "\x1b[0;1;32m",
            [STYLE_CURSOR] = "\x1b[0;46;30m",
            [STYLE_COPIED] = "\x1b[0;1;35m",
            [STYLE_STATUS] = "\x1b[0;43;30m",
            [STYLE_MESSAGE] = "\x1b[0;34m",
            [STYLE_ERROR] = "\x1b[0;1;31m",
        },
    },
};

static const int themeCount = (int) (sizeof(themes) / sizeof(themes[0]));

static int current = 0;

// Los estilos que pone el formato de una celda, despues de los del tema. Se
// guardan para siempre: el fotograma tiene números de estilo, y un numero tiene
// que significar lo mismo en el fotograma de ahora y en el de antes.
#define CUSTOM_MAX 64

static char *custom[CUSTOM_MAX];
static int customCount = 0;

const char *theme_name(void) {
    return themes[current].name;
}

const char *style_sequence(int style) {
    if (style < 0) {
        return RESET;
    }

    if (style >= STYLE_COUNT) {
        int slot = style - STYLE_COUNT;

        return slot < customCount && custom[slot] != NULL ? custom[slot] : RESET;
    }

    const char *sequence = themes[current].sequences[style];

    return sequence != NULL ? sequence : RESET;
}

int style_custom(const char *sequence) {
    if (sequence == NULL || sequence[0] == '\0') {
        return STYLE_TEXT;
    }

    for (int i = 0; i < customCount; i++) {
        if (strcmp(custom[i], sequence) == 0) {
            return STYLE_COUNT + i;
        }
    }

    if (customCount >= CUSTOM_MAX) {
        return STYLE_TEXT;
    }

    custom[customCount] = (char *) malloc(strlen(sequence) + 1);

    if (custom[customCount] == NULL) {
        return STYLE_TEXT;
    }

    strcpy(custom[customCount], sequence);

    return STYLE_COUNT + customCount++;
}

void theme_reset_custom(void) {
    for (int i = 0; i < customCount; i++) {
        free(custom[i]);
        custom[i] = NULL;
    }

    customCount = 0;
}

const char *theme_next(void) {
    current++;

    if (current >= themeCount) {
        current = 0;
    }

    theme_reset_custom();

    return themes[current].name;
}

int theme_set_by_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    for (int i = 0; i < themeCount; i++) {
        if (strcmp(themes[i].name, name) == 0) {
            current = i;
            theme_reset_custom();

            return 1;
        }
    }

    return 0;
}

int theme_count(void) {
    return themeCount;
}
