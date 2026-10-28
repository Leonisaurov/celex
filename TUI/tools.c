#include "tools.h"

#include "keys.h"

#include <stdio.h>
#include <string.h>

// Los items, por secciones. Delante lo que mas se usa, para que lo que no cabe
// en una pantalla corta sea lo de menos — dentro de una seccion y de la barra
// entera. La tecla de cada uno es la de siempre: el boton solo la muestra y la
// manda.
static const MenuItem docItems[] = {
    { "save", KEY_CTRL_S },
    { "save as", KEY_CTRL_G },
    { "open", KEY_CTRL_Q },
    { "new sheet", KEY_CTRL_A },
};

static const MenuItem editsItems[] = {
    { "copy", KEY_CTRL_C },
    { "paste", KEY_CTRL_V },
    { "undo", KEY_CTRL_Z },
    { "redo", KEY_CTRL_Y },
};

static const MenuItem styleItems[] = {
    { "bold", KEY_CTRL_B },
    { "line", KEY_CTRL_U },
    { "align", KEY_CTRL_O },
};

static const MenuItem viewItems[] = {
    { "value", 'v' },
    { "theme", KEY_CTRL_T },
};

static const MenuItem runItems[] = {
    { "run", KEY_CTRL_R },
};

static const MenuSection sections[] = {
    { "doc", docItems, (int) (sizeof(docItems) / sizeof(docItems[0])) },
    { "edits", editsItems, (int) (sizeof(editsItems) / sizeof(editsItems[0])) },
    { "style", styleItems, (int) (sizeof(styleItems) / sizeof(styleItems[0])) },
    { "view", viewItems, (int) (sizeof(viewItems) / sizeof(viewItems[0])) },
    { "run", runItems, (int) (sizeof(runItems) / sizeof(runItems[0])) },
};

#define MENU_COUNT (int) (sizeof(sections) / sizeof(sections[0]))

// Lo que mide una seccion en la barra: su nombre, con un aire detras.
#define MENU_WIDTH(label) ((int) strlen(label) + 1)

int menu_count(void) {
    return MENU_COUNT;
}

MenuSection menu_at(int index) {
    if (index < 0 || index >= MENU_COUNT) {
        MenuSection none = { NULL, NULL, 0 };

        return none;
    }

    return sections[index];
}

MenuItem menu_item_at(int section, int index) {
    if (section < 0 || section >= MENU_COUNT || index < 0 || index >= sections[section].count) {
        MenuItem none = { NULL, 0 };

        return none;
    }

    return sections[section].items[index];
}

// Las secciones van una detras de otra, con un aire en medio, y con el margen
// de la izquierda por delante.
int menu_fit(int width, int *starts) {
    int used = MENU_MARGIN;
    int fitted = 0;

    for (int i = 0; i < MENU_COUNT; i++) {
        int size = MENU_WIDTH(sections[i].label);

        if (used + size > width) {
            break;
        }

        if (starts != NULL) {
            starts[fitted] = used;
        }

        fitted++;
        used += size;
    }

    return fitted;
}

int menu_at_column(int width, int x) {
    int starts[MENU_COUNT];
    int fitted = menu_fit(width, starts);

    for (int i = 0; i < fitted; i++) {
        if (x >= starts[i] && x < starts[i] + MENU_WIDTH(sections[i].label)) {
            return i;
        }
    }

    return -1;
}

const char *menu_hint(int key) {
    if (key >= 1 && key <= 26) {
        static char hint[4];

        snprintf(hint, sizeof(hint), "^%c", (char) ('A' + key - 1));

        return hint;
    }

    if (key >= 32 && key < 127) {
        static char printable[2];

        printable[0] = (char) key;
        printable[1] = '\0';

        return printable;
    }

    return "";
}
