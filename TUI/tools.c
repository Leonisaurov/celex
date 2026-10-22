#include "tools.h"

#include "keys.h"

#include <string.h>

// Lo que se puede tocar, en el orden en que se pone. Delante lo que mas se usa,
// para que lo que no cabe en una pantalla corta sea lo de menos.
static const Tool tools[] = {
    { "bold", KEY_CTRL_B },
    { "line", KEY_CTRL_U },
    { "align", KEY_CTRL_O },
    { "copy", KEY_CTRL_C },
    { "paste", KEY_CTRL_V },
    { "undo", KEY_CTRL_Z },
    { "redo", KEY_CTRL_Y },
    { "save", KEY_CTRL_S },
    { "hide", KEY_CTRL_K },
    { "run", KEY_CTRL_R },
    { "value", 'v' },
    { "sheet", KEY_CTRL_N },
    { "theme", KEY_CTRL_T },
};

#define TOOL_COUNT (int) (sizeof(tools) / sizeof(tools[0]))

// El nombre de la fila, para que se sepa que eso que se puede tocar son las
// herramientas.
#define TOOLS_LABEL "tools:"

// Lo que ocupa una botonera: un espacio, el nombre, y otro espacio.
#define TOOL_WIDTH(name) ((int) strlen(name) + 2)

int tools_label_width(void) {
    return (int) strlen(TOOLS_LABEL) + 1;
}

Tool tool_at(int index) {
    if (index < 0 || index >= TOOL_COUNT) {
        Tool none = { "", 0 };

        return none;
    }

    return tools[index];
}

int tool_count(void) {
    return TOOL_COUNT;
}

// Las botones van una detras de otra, con una barra de separacion en medio.
int tools_fit(int width, int *starts) {
    int used = tools_label_width();
    int fitted = 0;

    for (int i = 0; i < TOOL_COUNT; i++) {
        int size = TOOL_WIDTH(tools[i].name);

        if (used + size > width) {
            break;
        }

        if (starts != NULL) {
            starts[fitted] = used;
        }

        fitted++;
        used += size + 1;
    }

    return fitted;
}

int tool_at_column(int width, int x) {
    int starts[TOOL_COUNT];
    int fitted = tools_fit(width, starts);

    for (int i = 0; i < fitted; i++) {
        if (x >= starts[i] && x < starts[i] + TOOL_WIDTH(tools[i].name)) {
            return i;
        }
    }

    return -1;
}
