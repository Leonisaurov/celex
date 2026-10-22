#include "theme.h"

#include <stddef.h>
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

const char *theme_name(void) {
    return themes[current].name;
}

const char *style_sequence(ScreenStyle style) {
    if (style < 0 || style >= STYLE_COUNT) {
        return RESET;
    }

    const char *sequence = themes[current].sequences[style];

    return sequence != NULL ? sequence : RESET;
}

const char *theme_next(void) {
    current++;

    if (current >= themeCount) {
        current = 0;
    }

    return themes[current].name;
}

int theme_set_by_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    for (int i = 0; i < themeCount; i++) {
        if (strcmp(themes[i].name, name) == 0) {
            current = i;

            return 1;
        }
    }

    return 0;
}

int theme_count(void) {
    return themeCount;
}
