#include "preferences.h"

#include "cell.h"
#include "paths.h"
#include "popup.h"
#include "theme.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define PREFERENCES_MAGIC "celex-preferences"
#define PREFERENCES_VERSION 1

// Lo que puede pesar el nombre de un tema, con el mismo sitio que le da el
// cargador a la linea `name` de un archivo de tema.
#define THEME_NAME_SIZE 128

typedef struct {
    char theme[THEME_NAME_SIZE];
    int width;
    int height;
    bool tools;
    bool values;
} Preferences;

// Lo ultimo que se leyo del archivo, o que se escribio en el. Es lo que deja
// pasar los cambios de verdad y no escribir por escribir.
static Preferences saved;
static bool savedKnown = false;

static void read_current(const CellManager *manager, Preferences *into) {
    snprintf(into->theme, sizeof(into->theme), "%s", theme_name());
    into->width = cellWidth;
    into->height = cellHeight;
    into->tools = manager->showMenu;
    into->values = manager->showValues;
}

static bool same(const Preferences *a, const Preferences *b) {
    return strcmp(a->theme, b->theme) == 0 && a->width == b->width && a->height == b->height && a->tools == b->tools &&
           a->values == b->values;
}

static void remember(const CellManager *manager) {
    read_current(manager, &saved);
    savedKnown = true;
}

int preferences_load(CellManager *manager) {
    char path[1024];
    conf_preferences_file(path, sizeof(path));

    FILE *file = fopen(path, "r");

    if (file == NULL) {
        // No hay archivo: lo que trae el editor es lo que vale, y es lo que se
        // compara para escribir la primera vez que algo cambie.
        remember(manager);

        return 0;
    }

    char line[512];

    while (fgets(line, sizeof(line), file) != NULL) {
        char mode[16];
        int width = 0;
        int height = 0;

        // El nombre de un tema puede llevar espacios, como en la linea `name`
        // de su archivo, asi que se toma lo que queda de linea y no una palabra.
        char *at = line;

        while (*at == ' ' || *at == '\t') {
            at++;
        }

        if (strncmp(at, "theme", 5) == 0 && (at[5] == ' ' || at[5] == '\t')) {
            char *name = at + 5;

            while (*name == ' ' || *name == '\t') {
                name++;
            }

            size_t length = strlen(name);

            while (length > 0 && (name[length - 1] == '\r' || name[length - 1] == '\n' || name[length - 1] == ' ' ||
                                  name[length - 1] == '\t')) {
                name[--length] = '\0';
            }

            if (length > 0) {
                theme_set_by_name(name);
            }
        } else if (sscanf(at, "cell-size %d %d", &width, &height) == 2) {
            if (width >= 0 && height >= 0) {
                set_cell_size(width, height);
            }
        } else if (sscanf(at, "tools %15s", mode) == 1) {
            if (strcmp(mode, "off") == 0) {
                manager->showMenu = false;
            } else if (strcmp(mode, "on") == 0) {
                manager->showMenu = true;
            }
        } else if (sscanf(at, "values %15s", mode) == 1) {
            if (strcmp(mode, "off") == 0) {
                manager->showValues = false;
            } else if (strcmp(mode, "on") == 0) {
                manager->showValues = true;
            }
        }

        // Lo que no empieza por una de esas palabras es la cabecera, un
        // comentario de quien edite el archivo, o un campo que no se conoce.
    }

    fclose(file);
    remember(manager);

    return 1;
}

int preferences_save(const CellManager *manager) {
    Preferences now;
    read_current(manager, &now);

    if (savedKnown && same(&now, &saved)) {
        return 0;
    }

    if (!conf_ensure()) {
        return 0;
    }

    char path[1024];
    conf_preferences_file(path, sizeof(path));

    FILE *file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    fprintf(file, "%s %d\n", PREFERENCES_MAGIC, PREFERENCES_VERSION);
    fprintf(file, "theme %s\n", now.theme);
    fprintf(file, "cell-size %d %d\n", now.width, now.height);
    fprintf(file, "tools %s\n", now.tools ? "on" : "off");
    fprintf(file, "values %s\n", now.values ? "on" : "off");

    bool written = !ferror(file);

    if (fclose(file) != 0) {
        written = false;
    }

    if (written) {
        saved = now;
        savedKnown = true;
    }

    return written ? 1 : 0;
}

int preferences_sync(const CellManager *manager) {
    // La lista de temas prueba el tema al moverse, y solo lo fija al confirmar.
    // Guardar la prueba dejaria un tema que nadie eligio si se cierra a medias.
    if (manager->popup.active && manager->popup.target == POP_TARGET_THEME) {
        return 0;
    }

    return preferences_save(manager);
}
