#include "theme.h"

#include "paths.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Apagar lo que hubiera: es el principio de toda secuencia de estilo, y lo que
// llevan los estilos que no pintan nada.
#define RESET "\x1b[0m"

typedef struct {
    const char *name;
    const char *sequences[STYLE_COUNT];
} Theme;

// Los temas de siempre, metidos en el programa. Los estilos que no se nombran se
// quedan sin color, que es el aspecto de siempre: la rejilla tal cual, y al reves
// la celda seleccionada y las dos barras.
static const Theme builtinThemes[] = {
    {
        "normal",
        {
            [STYLE_CURSOR] = "\x1b[0;7m",
            [STYLE_STATUS] = "\x1b[0;7m",
            [STYLE_TOOLS] = "\x1b[0;7m",
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
            [STYLE_TOOLS] = "\x1b[0;1;36;104m",
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
            [STYLE_TOOLS] = "\x1b[0;1;34;43m",
        },
    },
    {
        // Rose Pine: la paleta entera en color verdadero, con el fondo noche
        // tambien en el vacio de la pantalla. La rejilla apenas se ve, el
        // fuente va en blanco rosado, los valores en oro, y la celda marcada es
        // un bloque rosa con la letra oscura para que pegue a la vista.
        "rose-pine",
        {
            [STYLE_NONE] = "\x1b[0;38;2;224;222;244;48;2;25;23;36m",
            [STYLE_GRID] = "\x1b[0;38;2;82;79;103;48;2;25;23;36m",
            [STYLE_HEADER] = "\x1b[0;1;38;2;196;167;231;48;2;25;23;36m",
            [STYLE_TEXT] = "\x1b[0;38;2;224;222;244;48;2;25;23;36m",
            [STYLE_VALUE] = "\x1b[0;38;2;246;193;119;48;2;25;23;36m",
            [STYLE_CURSOR] = "\x1b[0;1;38;2;25;23;36;48;2;235;188;186m",
            [STYLE_COPIED] = "\x1b[0;1;38;2;235;111;146;48;2;25;23;36m",
            [STYLE_STATUS] = "\x1b[0;38;2;224;222;244;48;2;31;29;46m",
            [STYLE_MESSAGE] = "\x1b[0;38;2;156;207;216;48;2;31;29;46m",
            [STYLE_ERROR] = "\x1b[0;1;38;2;235;111;146;48;2;31;29;46m",
            [STYLE_TOOLS] = "\x1b[0;1;38;2;196;167;231;48;2;31;29;46m",
        },
    },
    {
        // Catppuccin Mocha: la misma idea en color verdadero sobre el azul noche
        // de la paleta. Cabeceras en azul, valores en melocoton, el borde copiado
        // en rosa, y la barra de estado un poco mas hundida que el resto para que
        // se sepa donde acaba la rejilla.
        "catppuccin",
        {
            [STYLE_NONE] = "\x1b[0;38;2;205;214;244;48;2;30;30;46m",
            [STYLE_GRID] = "\x1b[0;38;2;108;112;134;48;2;30;30;46m",
            [STYLE_HEADER] = "\x1b[0;1;38;2;137;180;250;48;2;30;30;46m",
            [STYLE_TEXT] = "\x1b[0;38;2;205;214;244;48;2;30;30;46m",
            [STYLE_VALUE] = "\x1b[0;38;2;250;179;135;48;2;30;30;46m",
            [STYLE_CURSOR] = "\x1b[0;1;38;2;30;30;46;48;2;137;180;250m",
            [STYLE_COPIED] = "\x1b[0;1;38;2;245;194;231;48;2;30;30;46m",
            [STYLE_STATUS] = "\x1b[0;38;2;180;190;254;48;2;24;24;37m",
            [STYLE_MESSAGE] = "\x1b[0;38;2;166;227;161;48;2;24;24;37m",
            [STYLE_ERROR] = "\x1b[0;1;38;2;243;139;168;48;2;24;24;37m",
            [STYLE_TOOLS] = "\x1b[0;1;38;2;137;180;250;48;2;24;24;37m",
        },
    },
};

#define BUILTIN_COUNT ((int) (sizeof(builtinThemes) / sizeof(builtinThemes[0])))

// Los temas que anade el usuario, leidos de la carpeta de configuracion. Viven
// detras de los integrados en la lista, y se sueltan al recargar o al cerrar.
static Theme *userThemes = NULL;
static int userCount = 0;
static int userCap = 0;

static const int builtinOnly = BUILTIN_COUNT;
static int themeCount = BUILTIN_COUNT;

static int current = 0;

// Los estilos que pone el formato de una celda, despues de los del tema. Se
// guardan para siempre: el fotograma tiene numeros de estilo, y un numero tiene
// que significar lo mismo en el fotograma de ahora y en el de antes.
#define CUSTOM_MAX 64

static char *custom[CUSTOM_MAX];
static int customCount = 0;

static const Theme *theme_at(int i) {
    if (i < BUILTIN_COUNT) {
        return &builtinThemes[i];
    }

    return &userThemes[i - BUILTIN_COUNT];
}

const char *theme_name(void) {
    return theme_at(current)->name;
}

const char *style_sequence(int style) {
    if (style < 0) {
        return RESET;
    }

    if (style >= STYLE_COUNT) {
        int slot = style - STYLE_COUNT;

        return slot < customCount && custom[slot] != NULL ? custom[slot] : RESET;
    }

    const char *sequence = theme_at(current)->sequences[style];

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

    return theme_at(current)->name;
}

int theme_set_by_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    for (int i = 0; i < themeCount; i++) {
        if (strcmp(theme_at(i)->name, name) == 0) {
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

const char *theme_name_at(int index) {
    if (index < 0 || index >= themeCount) {
        return "";
    }

    return theme_at(index)->name;
}

const char *theme_text_sequence(int index) {
    if (index < 0 || index >= themeCount) {
        return RESET;
    }

    const char *sequence = theme_at(index)->sequences[STYLE_TEXT];

    return sequence != NULL ? sequence : RESET;
}

// ---------------------------------------------------------------------------
// Temas definidos por el usuario: la lectura de los archivos *.theme.
// ---------------------------------------------------------------------------

// Copia una cadena en memoria nueva (evita strdup, que no esta en todos lados).
static char *own_copy(const char *s) {
    if (s == NULL) {
        return NULL;
    }

    size_t length = strlen(s);
    char *copy = (char *) malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, s, length + 1);

    return copy;
}

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

// El nombre de una parte de la pantalla, tal como se escribe en el archivo.
static int slot_of(const char *token) {
    static const char *const keys[STYLE_COUNT] = {
        "background", // STYLE_NONE
        "grid",
        "header",
        "text",
        "value",
        "cursor",
        "copied",
        "status",
        "message",
        "error",
        "tools",
    };

    for (int i = 0; i < STYLE_COUNT; i++) {
        if (ci_eq(token, keys[i])) {
            return i;
        }
    }

    return -1;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

// Un color escrito en hexadecimal: #rgb o #rrggbb, con la almohadilla delante.
static int parse_hex(const char *s, int *r, int *g, int *b) {
    if (s[0] != '#') {
        return 0;
    }

    const char *digits = s + 1;
    int vals[6];
    int count = 0;

    while (digits[count] != '\0') {
        if (count >= 6) {
            return 0;
        }

        int v = hex_val(digits[count]);

        if (v < 0) {
            return 0;
        }

        vals[count] = v;
        count++;
    }

    if (count == 3) {
        *r = vals[0] * 17;
        *g = vals[1] * 17;
        *b = vals[2] * 17;

        return 1;
    }

    if (count == 6) {
        *r = vals[0] * 16 + vals[1];
        *g = vals[2] * 16 + vals[3];
        *b = vals[4] * 16 + vals[5];

        return 1;
    }

    return 0;
}

// Anade un componente a la secuencia, poniendo el separador si hace falta. El
// principio "\x1b[0;" ya trae la diagonal, asi que el primero no la necesita.
static void append_component(char *buf, size_t size, const char *comp) {
    size_t length = strlen(buf);

    if (length + 1 >= size) {
        return;
    }

    if (buf[length - 1] != ';' && length + 2 < size) {
        buf[length] = ';';
        buf[length + 1] = '\0';
        length++;
    }

    strncat(buf, comp, size - length - 1);
}

// Monta la secuencia ANSI de una parte: siempre empieza apagando lo que hubiera,
// como los temas de siempre. Los colores van en color verdadero.
static void build_sequence(char *out, size_t size,
    int hasFg, int fr, int fg, int fb,
    int hasBg, int br, int bg, int bb,
    int bold, int underline, int reverse) {
    snprintf(out, size, "\x1b[0;");

    char comp[32];

    if (bold) {
        append_component(out, size, "1");
    }

    if (underline) {
        append_component(out, size, "4");
    }

    if (reverse) {
        append_component(out, size, "7");
    }

    if (hasFg) {
        snprintf(comp, sizeof(comp), "38;2;%d;%d;%d", fr, fg, fb);
        append_component(out, size, comp);
    }

    if (hasBg) {
        snprintf(comp, sizeof(comp), "48;2;%d;%d;%d", br, bg, bb);
        append_component(out, size, comp);
    }

    // La 'm' final va pegada al ultimo numero, sin separador.
    size_t length = strlen(out);

    if (length + 1 < size) {
        out[length] = 'm';
        out[length + 1] = '\0';
    }
}

static char *skip_blanks(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }

    return s;
}

// Coge el siguiente token: escribe el final en NUL, lo devuelve en *token y dice
// donde continuar, o NULL si el token era lo ultimo de la linea.
static char *take_token(char *s, char **token) {
    *token = s;

    while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') {
        s++;
    }

    if (*s == '\0') {
        return NULL;
    }

    *s = '\0';

    return s + 1;
}

#define SEQ_LEN 160

typedef struct {
    char name[128];
    char seq[STYLE_COUNT][SEQ_LEN];
    unsigned char set[STYLE_COUNT];
    int ignored;
} ParsedTheme;

static void parse_line(char *line, ParsedTheme *parsed) {
    char *at = skip_blanks(line);

    // Linea vacia o comentario entero: no es una directiva, no se cuenta.
    if (*at == '\0' || *at == '#') {
        return;
    }

    char *token;
    char *rest = take_token(at, &token);

    if (*token == '\0') {
        return;
    }

    if (ci_eq(token, "name")) {
        if (rest == NULL) {
            return;
        }

        char *name = skip_blanks(rest);
        char *hash = strchr(name, '#');

        if (hash != NULL) {
            *hash = '\0';
        }

        size_t length = strlen(name);

        while (length > 0 && (name[length - 1] == ' ' || name[length - 1] == '\t'
            || name[length - 1] == '\r' || name[length - 1] == '\n')) {
            name[--length] = '\0';
        }

        if (length > 0) {
            snprintf(parsed->name, sizeof(parsed->name), "%s", name);
        }

        return;
    }

    int slot = slot_of(token);

    if (slot < 0) {
        parsed->ignored++;

        return;
    }

    int hasFg = 0, fr = 0, fg = 0, fb = 0;
    int hasBg = 0, br = 0, bg = 0, bb = 0;
    int bold = 0, underline = 0, reverse = 0;
    int expectBg = 0;
    char *cursor = rest;

    while (cursor != NULL) {
        cursor = skip_blanks(cursor);

        if (*cursor == '\0') {
            break;
        }

        cursor = take_token(cursor, &token);

        if (*token == '\0') {
            break;
        }

        if (ci_eq(token, "on")) {
            expectBg = 1;
            continue;
        }

        if (token[0] == '#') {
            int r, g, b;

            if (parse_hex(token, &r, &g, &b)) {
                if (expectBg) {
                    hasBg = 1;
                    br = r;
                    bg = g;
                    bb = b;
                } else {
                    hasFg = 1;
                    fr = r;
                    fg = g;
                    fb = b;
                }

                expectBg = 0;
            } else {
                // Un almohadilla que no es color abre un comentario de linea: se
                // para aqui y se ignora el resto.
                break;
            }

            continue;
        }

        if (ci_eq(token, "bold")) {
            bold = 1;
        } else if (ci_eq(token, "underline")) {
            underline = 1;
        } else if (ci_eq(token, "reverse")) {
            reverse = 1;
        }
        // Cualquier otra cosa se ignora.
    }

    if (!hasFg && !hasBg && !bold && !underline && !reverse) {
        return;
    }

    build_sequence(parsed->seq[slot], SEQ_LEN, hasFg, fr, fg, fb, hasBg, br, bg, bb,
        bold, underline, reverse);
    parsed->set[slot] = 1;
}

// El nombre por defecto: el del archivo, sin el camino y sin el ".theme".
static void name_from_path(const char *path, char *out, size_t size) {
    const char *base = path;

    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }

    size_t length = strlen(base);

    if (length > 6 && ci_eq(base + length - 6, ".theme")) {
        length -= 6;
    }

    if (length >= size) {
        length = size - 1;
    }

    memcpy(out, base, length);
    out[length] = '\0';
}

static int name_is_taken(const char *name) {
    for (int i = 0; i < themeCount; i++) {
        if (strcmp(theme_at(i)->name, name) == 0) {
            return 1;
        }
    }

    return 0;
}

static Theme *reserve_slot(void) {
    if (userCount >= userCap) {
        int next = userCap > 0 ? userCap * 2 : 8;
        Theme *grown = (Theme *) realloc(userThemes, (size_t) next * sizeof(Theme));

        if (grown == NULL) {
            return NULL;
        }

        userThemes = grown;
        userCap = next;
    }

    Theme *slot = &userThemes[userCount];
    userCount++;

    return slot;
}

typedef struct {
    int added;
    int skipped;
} LoadCtx;

static void on_theme_file(const char *path, void *ctx) {
    FILE *file = fopen(path, "r");

    if (file == NULL) {
        return;
    }

    ParsedTheme parsed;
    memset(&parsed, 0, sizeof(parsed));

    char line[256];

    while (fgets(line, sizeof(line), file) != NULL) {
        parse_line(line, &parsed);
    }

    fclose(file);

    if (parsed.name[0] == '\0') {
        name_from_path(path, parsed.name, sizeof(parsed.name));
    }

    if (parsed.name[0] == '\0') {
        return;
    }

    LoadCtx *context = (LoadCtx *) ctx;

    if (name_is_taken(parsed.name)) {
        context->skipped++;

        return;
    }

    Theme *theme = reserve_slot();

    if (theme == NULL) {
        return;
    }

    theme->name = own_copy(parsed.name);

    for (int s = 0; s < STYLE_COUNT; s++) {
        theme->sequences[s] = parsed.set[s] ? own_copy(parsed.seq[s]) : NULL;
    }

    themeCount++;
    context->added++;
}

void theme_free_user(void) {
    for (int i = 0; i < userCount; i++) {
        Theme *theme = &userThemes[i];
        free((void *) theme->name);

        for (int s = 0; s < STYLE_COUNT; s++) {
            free((void *) theme->sequences[s]);
        }
    }

    free(userThemes);
    userThemes = NULL;
    userCount = 0;
    userCap = 0;

    themeCount = builtinOnly;

    if (current >= themeCount) {
        current = 0;
    }
}

int theme_load_themes_dir(const char *dir) {
    if (dir == NULL || dir[0] == '\0') {
        return 0;
    }

    theme_free_user();

    LoadCtx context = { 0, 0 };
    conf_each_theme_file(dir, on_theme_file, &context);

    return context.added;
}
