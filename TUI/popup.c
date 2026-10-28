#include "popup.h"

#include "cell_manager.h"
#include "theme.h"
#include "tools.h"

#include <stdio.h>
#include <string.h>

// Cuantos bytes ocupa el caracter que empieza en text.
static size_t glyph_size(const char *text) {
    unsigned char first = (unsigned char) text[0];

    if (first < 0x80) return 1;
    if ((first & 0xE0) == 0xC0) return 2;
    if ((first & 0xF0) == 0xE0) return 3;
    if ((first & 0xF8) == 0xF0) return 4;

    return 1;
}

// Cuantas columnas de la pantalla ocupa un texto.
static int text_columns(const char *text) {
    int columns = 0;

    for (size_t i = 0; text[i] != '\0'; i += glyph_size(text + i)) {
        columns++;
    }

    return columns;
}

static void popup_reset(Popup *popup, PopupKind kind, PopupTarget target) {
    popup->active = true;
    popup->kind = kind;
    popup->target = target;
    popup->section = -1;
    popup->count = 0;
    popup->selected = 0;
    popup->hintWidth = 0;
    popup->restoreTo = -1;
    popup->text[0] = '\0';
}

// El rectangulo, recortado a la pantalla. Lo que no cabe ni en una rejilla de
// tres filas no se abre: no hay donde colgarlo.
void popup_place(Popup *popup, int width, int gridBottom) {
    if (width < 7 || gridBottom < 3) {
        popup->active = false;

        return;
    }

    if (popup->w > width - 2) {
        popup->w = width - 2;
    }

    if (popup->h > gridBottom - 1) {
        popup->h = gridBottom - 1;
    }

    if (popup->kind == POPUP_MENU) {
        // Cuelga de su seccion, y se arrima al borde derecho si no cabe.
        int starts[MENU_LIMIT];
        int fitted = menu_fit(width, starts);
        int x = 0;

        if (popup->section >= 0 && popup->section < fitted) {
            x = starts[popup->section];
        }

        popup->x = x;

        if (popup->x + popup->w > width - 1) {
            popup->x = width - 1 - popup->w;
        }

        if (popup->x < 0) {
            popup->x = 0;
        }

        popup->y = 1;

        return;
    }

    popup->x = (width - popup->w) / 2;
    popup->y = (gridBottom - popup->h) / 3;

    if (popup->x < 1) popup->x = 1;
    if (popup->y < 1) popup->y = 1;
}

void popup_open_menu(Popup *popup, int section, int width, int gridBottom) {
    MenuSection menu = menu_at(section);

    if (menu.items == NULL) {
        return;
    }

    popup_reset(popup, POPUP_MENU, POP_TARGET_NONE);

    popup->section = section;
    popup->count = menu.count;

    if (popup->count > POPUP_LIMIT) {
        popup->count = POPUP_LIMIT;
    }

    int longest = 0;
    int hints = 0;

    for (int i = 0; i < popup->count; i++) {
        MenuItem item = menu_item_at(section, i);

        snprintf(popup->labels[i], POPUP_ITEM, "%s", item.label);
        popup->keys[i] = item.key;

        int label = text_columns(popup->labels[i]);
        int hint = text_columns(menu_hint(item.key));

        if (label > longest) {
            longest = label;
        }

        if (hint > hints) {
            hints = hint;
        }
    }

    popup->hintWidth = hints;
    popup->w = longest + hints + 7;

    // El marco, el titulo y las filas: el titulo no comparte la fila de la
    // primera opcion, o la taparia.
    popup->h = popup->count + 3;

    popup_place(popup, width, gridBottom);
}

void popup_open_list(Popup *popup, PopupTarget target, const char *const *labels, int count, int current, int width, int gridBottom) {
    if (labels == NULL || count <= 0) {
        return;
    }

    popup_reset(popup, POPUP_LIST, target);

    popup->count = count;

    if (popup->count > POPUP_LIMIT) {
        popup->count = POPUP_LIMIT;
    }

    int longest = 0;

    for (int i = 0; i < popup->count; i++) {
        snprintf(popup->labels[i], POPUP_ITEM, "%s", labels[i]);

        int label = text_columns(popup->labels[i]);

        if (label > longest) {
            longest = label;
        }
    }

    if (current >= 0 && current < popup->count) {
        popup->selected = current;
    }

    // En la lista de los temas, lo puesto al abrir es lo que hay que devolver
    // si se cancela: la vista previa no manda por si sola.
    if (target == POP_TARGET_THEME) {
        popup->restoreTo = popup->selected;
    }

    popup->hintWidth = 0;
    popup->w = longest + 7;

    if (popup->w < 14) {
        popup->w = 14;
    }

    popup->h = popup->count + 3;

    popup_place(popup, width, gridBottom);
}

void popup_open_input(Popup *popup, PopupTarget target, const char *initial, int width, int gridBottom) {
    popup_reset(popup, POPUP_INPUT, target);

    if (initial != NULL) {
        snprintf(popup->text, POPUP_TEXT, "%s", initial);
    }

    popup->w = 44;

    // Marco, titulo, campo y marco: el campo necesita su propia fila.
    popup->h = 4;

    popup_place(popup, width, gridBottom);
}

void popup_close(Popup *popup) {
    popup->active = false;
    popup->kind = POPUP_NONE;
    popup->target = POP_TARGET_NONE;
    popup->text[0] = '\0';
}

void popup_cancel(Popup *popup) {
    if (popup->kind == POPUP_LIST && popup->target == POP_TARGET_THEME && popup->restoreTo >= 0 &&
        popup->restoreTo != popup->selected) {
        theme_set_by_name(theme_name_at(popup->restoreTo));
        invalidate_screen();
    }

    popup_close(popup);
}

int popup_at_cell(Popup *popup, int x, int y, int *item) {
    if (item != NULL) {
        *item = -1;
    }

    if (!popup->active) {
        return 0;
    }

    if (x < popup->x || x >= popup->x + popup->w || y < popup->y || y >= popup->y + popup->h) {
        return 0;
    }

    if (item != NULL && popup->kind != POPUP_INPUT) {
        // Bajo el titulo: la fila primera del interior no es de ninguna opcion.
        int row = y - popup->y - 2;

        // Con el popup recortado, las filas de mas no son de ninguna.
        if (row >= 0 && row < popup->count) {
            *item = row;
        }
    }

    return 1;
}
