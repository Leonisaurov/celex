#include "paths.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#include <direct.h>
#include <windows.h>

static int make_one(const char *path) {
    return _mkdir(path) == 0;
}

#else

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

static int make_one(const char *path) {
    return mkdir(path, 0755) == 0;
}

#endif

// Comparacion ASCII sin distincion de mayusculas, para no depender de strcasecmp
// que no esta en todos lados.
static char lower_of(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char) (c - 'A' + 'a');
    }

    return c;
}

static int ci_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (lower_of(*a) != lower_of(*b)) {
            return 0;
        }

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

// Es un nombre de tema: acaba en ".theme" (da igual como este escrito) y no
// empieza por guion bajo ni por punto, que son la plantilla y los ocultos.
static int is_theme_name(const char *name) {
    if (name[0] == '_' || name[0] == '.') {
        return 0;
    }

    size_t length = strlen(name);

    if (length < 6) {
        return 0;
    }

    return ci_eq(name + length - 6, ".theme");
}

// La casa del usuario: HOME si esta, y si no, en Windows, USERPROFILE o la
// union de HOMEDRIVE y HOMEPATH.
static void home_dir(char *out, size_t size) {
    const char *home = getenv("HOME");

    if (home != NULL && home[0] != '\0') {
        snprintf(out, size, "%s", home);
        return;
    }

#ifdef _WIN32
    home = getenv("USERPROFILE");

    if (home != NULL && home[0] != '\0') {
        snprintf(out, size, "%s", home);
        return;
    }

    const char *drive = getenv("HOMEDRIVE");
    const char *path = getenv("HOMEPATH");

    if (drive != NULL && path != NULL && drive[0] != '\0' && path[0] != '\0') {
        snprintf(out, size, "%s%s", drive, path);
        return;
    }
#endif

    out[0] = '\0';
}

void conf_dir(char *out, size_t size) {
    char home[1024];
    home_dir(home, sizeof(home));

    if (home[0] == '\0') {
        snprintf(out, size, ".config/celex");
    } else {
        snprintf(out, size, "%s/.config/celex", home);
    }
}

void conf_themes_dir(char *out, size_t size) {
    char conf[1024];
    conf_dir(conf, sizeof(conf));

    snprintf(out, size, "%s/themes", conf);
}

void conf_preferences_file(char *out, size_t size) {
    char conf[1024];
    conf_dir(conf, sizeof(conf));

    snprintf(out, size, "%s/config", conf);
}

// Crea todos los tramos de un camino, de arriba abajo, ignorando los que ya
// existan. Se intenta en cada diagonal, y al final con el camino entero.
static void make_tree(char *path) {
    for (char *at = path + 1; *at != '\0'; at++) {
        if (*at == '/') {
            *at = '\0';
            make_one(path);
            *at = '/';
        }
    }

    make_one(path);
}

// Crea el camino y dice si queda disponible: vale con haberlo creado ahora o
// con que ya existiera.
static int ensure_leaf(char *path) {
    errno = 0;
    make_tree(path);

    if (make_one(path)) {
        return 1;
    }

    return errno == EEXIST;
}

int conf_ensure(void) {
    char conf[1024];
    conf_dir(conf, sizeof(conf));

    char themes[1024];
    conf_themes_dir(themes, sizeof(themes));

    return ensure_leaf(conf) && ensure_leaf(themes);
}

// La plantilla que se planta la primera vez. Empieza por guion bajo a proposito:
// el cargador la ignora, asi que no cambia los temas hasta que se renombre.
static const char *const EXAMPLE_THEME =
    "# _ejemplo.theme - plantilla para hacer tu propio tema.\n"
    "#\n"
    "# Renombra este archivo (por ejemplo `medianoche.theme`) para que el editor\n"
    "# lo cargue. Cada linea pinta una parte de la pantalla con un color:\n"
    "#\n"
    "#   <parte> <#fg> [on <#bg>] [bold] [underline] [reverse]\n"
    "#\n"
    "# Los colores van en hexadecimal, #rgb o #rrggbb, y se emiten en color\n"
    "# verdadero. `on` pone el fondo. Las tres palabras son atributos opcionales.\n"
    "# Lo que no escribas se queda sin color. Todo lo que empiece por # es un\n"
    "# comentario.\n"
    "\n"
    "name    medianoche\n"
    "\n"
    "background #cdd6f4 on #1e1e2e   # el vacio de la pantalla y su fondo\n"
    "grid       #6c7086              # las rayas de la rejilla\n"
    "header     #89b4fa bold         # letras de columnas y numeros de filas\n"
    "text       #cdd6f4              # el fuente de una celda\n"
    "value      #fab387              # el valor de una celda\n"
    "cursor     #1e1e2e on #89b4fa bold   # la celda seleccionada\n"
    "copied     #f5c2e7 bold         # el borde de la celda copiada\n"
    "status     #b4befe on #181825   # la barra de estado\n"
    "message    #a6e3a1 on #181825   # lo que la barra dice a la derecha\n"
    "error      #f38ba8 bold on #181825   # cuando algo salio mal\n"
    "tools      #89b4fa bold on #181825   # la barra de las herramientas\n";

int conf_seed_example(void) {
    char themes[1024];
    conf_themes_dir(themes, sizeof(themes));

    char file[1024];
    snprintf(file, sizeof(file), "%s/_ejemplo.theme", themes);

    // Si ya existe, no se toca: lo que el usuario escriba vale mas que la plantilla.
    FILE *existing = fopen(file, "r");

    if (existing != NULL) {
        fclose(existing);
        return 0;
    }

    FILE *out = fopen(file, "w");

    if (out == NULL) {
        return 0;
    }

    fputs(EXAMPLE_THEME, out);
    fclose(out);

    return 1;
}

#ifdef _WIN32

int conf_each_theme_file(const char *dir, ThemeFileCb cb, void *ctx) {
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s/*.theme", dir);

    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(pattern, &found);

    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int called = 0;

    do {
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        if (!is_theme_name(found.cFileName)) {
            continue;
        }

        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, found.cFileName);

        cb(full, ctx);
        called++;
    } while (FindNextFileA(handle, &found));

    FindClose(handle);

    return called;
}

#else

int conf_each_theme_file(const char *dir, ThemeFileCb cb, void *ctx) {
    DIR *open = opendir(dir);

    if (open == NULL) {
        return 0;
    }

    int called = 0;
    struct dirent *entry;

    while ((entry = readdir(open)) != NULL) {
        if (!is_theme_name(entry->d_name)) {
            continue;
        }

        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, entry->d_name);

        cb(full, ctx);
        called++;
    }

    closedir(open);

    return called;
}

#endif
