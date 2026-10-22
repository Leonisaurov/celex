#include "document.h"

#include "ref.h"
#include "theme.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// El buffer de texto que crece, para ir escribiendo el documento.
typedef struct {
    char *text;
    size_t length;
    size_t capacity;
} Text;

static int text_add(Text *text, const char *part) {
    size_t need = strlen(part);

    if (text->length + need + 1 > text->capacity) {
        size_t capacity = text->capacity == 0 ? 512 : text->capacity;

        while (capacity < text->length + need + 1) {
            capacity *= 2;
        }

        char *grown = (char *) realloc(text->text, capacity);
        if (grown == NULL) {
            return 0;
        }

        text->text = grown;
        text->capacity = capacity;
    }

    memcpy(text->text + text->length, part, need + 1);
    text->length += need;

    return 1;
}

// Escribe un texto entre comillas, escapando la comilla y la barra.
static int text_add_quoted(Text *text, const char *value) {
    char piece[3] = { '"', '\0', '\0' };

    if (!text_add(text, piece)) {
        return 0;
    }

    for (const char *at = value; *at != '\0'; at++) {
        piece[1] = '\0';

        if (*at == '"' || *at == '\\') {
            piece[0] = '\\';
            piece[1] = *at;
        } else if (*at == '\r' || *at == '\n') {
            // Una celda es una linea: lo que se pareciera a un salto se pone
            // como espacio.
            piece[0] = ' ';
        } else {
            piece[0] = *at;
        }

        if (!text_add(text, piece)) {
            return 0;
        }
    }

    piece[0] = '"';
    piece[1] = '\0';

    return text_add(text, piece);
}

// Las celdas de una hoja, ordenadas por columna y despues por fila, que es el
// orden en que se pueden calcular.
typedef struct {
    unsigned int x;
    unsigned int y;
    const char *content;
    CellFormat format;
} DocumentEntry;

typedef struct {
    DocumentEntry *items;
    size_t count;
    size_t capacity;
} DocumentEntries;

static int compare_entries(const void *a, const void *b) {
    const DocumentEntry *left = (const DocumentEntry *) a;
    const DocumentEntry *right = (const DocumentEntry *) b;

    if (left->x != right->x) {
        return left->x < right->x ? -1 : 1;
    }

    if (left->y != right->y) {
        return left->y < right->y ? -1 : 1;
    }

    return 0;
}

static int collect_entry(void *const context, struct hashmap_element_s *const element) {
    DocumentEntries *entries = (DocumentEntries *) context;
    MetaCell *meta = (MetaCell *) element->data;

    if (entries->count == entries->capacity) {
        size_t capacity = entries->capacity == 0 ? 16 : entries->capacity * 2;
        DocumentEntry *items = (DocumentEntry *) realloc(entries->items, capacity * sizeof(DocumentEntry));
        if (items == NULL) {
            return -1;
        }

        entries->items = items;
        entries->capacity = capacity;
    }

    entries->items[entries->count].x = meta->x;
    entries->items[entries->count].y = meta->y;
    entries->items[entries->count].content = cell_content(meta);
    entries->items[entries->count].format = meta->cell.format;
    entries->count++;

    return 0;
}

// Los nombres de la alineacion, por su numero. `none` es la que no se escribe:
// la de empezar, que es la de una hoja de calculo.
static const char *align_name(int align) {
    if (align == ALIGN_LEFT) {
        return "left";
    }

    if (align == ALIGN_CENTER) {
        return "center";
    }

    if (align == ALIGN_RIGHT) {
        return "right";
    }

    return NULL;
}

static int format_is_plain(CellFormat format) {
    return format.align == ALIGN_DEFAULT && format.bold == 0 && format.underline == 0 &&
           format.fg == 0 && format.bg == 0;
}

// La linea de formato de una celda, token por token. Los tokens nunca llevan
// texto libre, asi que la linea sale de una sola escritura y no hay comillas que
// escapar.
static int text_add_format(Text *text, const char *reference, CellFormat format) {
    char colors[32];

    if (format.fg != 0 && format.bg != 0) {
        snprintf(colors, sizeof(colors), " fg=%d bg=%d", format.fg, format.bg);
    } else if (format.fg != 0) {
        snprintf(colors, sizeof(colors), " fg=%d", format.fg);
    } else if (format.bg != 0) {
        snprintf(colors, sizeof(colors), " bg=%d", format.bg);
    } else {
        colors[0] = '\0';
    }

    const char *align = align_name(format.align);

    char line[128];
    int written = snprintf(line, sizeof(line), "  format %s%s%s%s%s%s\n",
                           reference,
                           align != NULL ? " align=" : "",
                           align != NULL ? align : "",
                           format.bold != 0 ? " bold" : "",
                           format.underline != 0 ? " underline" : "",
                           colors);

    if (written < 0 || (size_t) written >= sizeof(line)) {
        return 0;
    }

    return text_add(text, line);
}

// Escribe el bloque de una hoja: su nombre, donde estaba el cursor y lo deslizado
// que llevaba la vista, y sus celdas.
static int text_add_sheet(Text *text, CellManager *manager, int index) {
    Sheet *sheet = sheet_at(manager, index);
    char line[128];

    if (sheet == NULL) {
        return 0;
    }

    if (!text_add(text, "sheet ") || !text_add_quoted(text, sheet->name)) {
        return 0;
    }

    if (!text_add(text, "\n")) {
        return 0;
    }

    char *reference = ref_to_string(sheet->state.cursor.x, sheet->state.cursor.y);
    snprintf(line, sizeof(line), "  select %s\n  scroll %d %d\n", reference, sheet->state.originX, sheet->state.originY);
    free(reference);

    if (!text_add(text, line)) {
        return 0;
    }

    DocumentEntries entries = { NULL, 0, 0 };
    hashmap_iterate_pairs(&sheet->state.map, collect_entry, &entries);

    qsort(entries.items, entries.count, sizeof(DocumentEntry), compare_entries);

    for (size_t i = 0; i < entries.count; i++) {
        const char *content = entries.items[i].content;
        CellFormat format = entries.items[i].format;

        // Una celda sin contenido y sin formato no necesita una linea.
        if (content[0] != '\0') {
            reference = ref_to_string(entries.items[i].x, entries.items[i].y);

            snprintf(line, sizeof(line), "  cell %s ", reference);

            if (!text_add(text, line) || !text_add_quoted(text, content) || !text_add(text, "\n")) {
                free(reference);
                free(entries.items);

                return 0;
            }

            free(reference);
        }

        if (!format_is_plain(format)) {
            reference = ref_to_string(entries.items[i].x, entries.items[i].y);

            if (!text_add_format(text, reference, format)) {
                free(reference);
                free(entries.items);

                return 0;
            }

            free(reference);
        }
    }

    free(entries.items);

    return text_add(text, "end-sheet\n");
}

char *document_source(CellManager *manager) {
    Text text = { NULL, 0, 0 };
    char line[128];

    snprintf(line, sizeof(line), "%s %d\ntheme %s\ncell-size %d %d\n",
             DOCUMENT_MAGIC, DOCUMENT_VERSION, theme_name(), cellWidth, cellHeight);

    if (!text_add(&text, line)) {
        free(text.text);
        return NULL;
    }

    for (int i = 0; i < sheet_count(manager); i++) {
        if (!text_add_sheet(&text, manager, i)) {
            free(text.text);
            return NULL;
        }
    }

    if (text.text == NULL) {
        text.text = (char *) malloc(1);
        if (text.text != NULL) {
            text.text[0] = '\0';
        }
    }

    return text.text;
}

// El principio de una linea, sin espacios.
static char *skip_spaces(char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }

    return text;
}

// El final de una linea, con los espacios y el salto de ventana del fin quitados.
static void strip_end(char *text) {
    size_t length = strlen(text);

    while (length > 0 && (text[length - 1] == '\r' || text[length - 1] == '\n' || text[length - 1] == ' ' || text[length - 1] == '\t')) {
        text[--length] = '\0';
    }
}

// El numero que viene en la linea. `at` se pone despues de lo que se leyo, para
// poder leer dos seguidos. Devuelve -1 si no habia ninguno.
static int read_number(char **at) {
    char *start = skip_spaces(*at);

    char *end = NULL;
    long value = strtol(start, &end, 10);

    if (end == start) {
        return -1;
    }

    *at = end;

    return (int) value;
}

// Lo que hay entre comillas, desenredado en `into`. Devuelve donde se queda el
// texto, o NULL si no habia comilla.
static char *read_quoted(char *at, char *into, size_t size) {
    at = skip_spaces(at);

    if (*at != '"') {
        return NULL;
    }

    at++;

    size_t length = 0;

    while (*at != '\0') {
        char character = *at;

        if (character == '"') {
            break;
        }

        if (character == '\\' && at[1] != '\0') {
            at++;
            character = *at;
        }

        if (length + 1 >= size) {
            break;
        }

        into[length++] = character;
        at++;
    }

    into[length] = '\0';

    if (*at == '"') {
        at++;
    }

    return at;
}

// Una palabra suelta, sin comillas: un nombre de tema o una referencia de celda.
// Devuelve donde se queda el texto, o NULL si no habia nada.
static char *read_word(char *at, char *into, size_t size) {
    at = skip_spaces(at);

    size_t length = 0;

    while (at[length] != '\0' && !isspace((unsigned char) at[length])) {
        if (length + 1 >= size) {
            break;
        }

        into[length] = at[length];
        length++;
    }

    into[length] = '\0';

    if (length == 0) {
        return NULL;
    }

    return at + length;
}

// El numero de una alineacion por su nombre, o -1 si no es ninguno.
static int align_by_name(const char *name) {
    if (strcmp(name, "default") == 0) {
        return ALIGN_DEFAULT;
    }

    if (strcmp(name, "left") == 0) {
        return ALIGN_LEFT;
    }

    if (strcmp(name, "center") == 0) {
        return ALIGN_CENTER;
    }

    if (strcmp(name, "right") == 0) {
        return ALIGN_RIGHT;
    }

    return -1;
}

// El color de un token `fg=3` o `bg=12`, o -1 si no es un numero de la paleta.
static int color_by_token(const char *token) {
    char *end = NULL;
    long value = strtol(token + 3, &end, 10);

    if (end == token + 3 || *end != '\0' || value < 1 || value > 16) {
        return -1;
    }

    return (int) value;
}

// Los campos del documento: cada linea empieza con una palabra que dice que es,
// y lo demas se lee con los lectores de arriba, que saltan los espacios por su
// cuenta. Lo que no se reconoce no rompe el archivo: se ignora y se cuenta.
int document_load(const char *content, CellManager *manager, int *ignored) {
    if (content == NULL || strncmp(skip_spaces((char *) content), DOCUMENT_MAGIC, strlen(DOCUMENT_MAGIC)) != 0) {
        return 0;
    }

    if (ignored != NULL) {
        *ignored = 0;
    }

    // El documento es la verdad: lo que hubiera antes se olvida.
    workbook_clear(manager);

    char *text = (char *) malloc(strlen(content) + 1);
    if (text == NULL) {
        return 0;
    }

    strcpy(text, content);

    bool header = false;
    bool inSheet = false;
    bool firstSheet = true;
    int ignoredLines = 0;

    char *line = text;

    while (line != NULL) {
        char *next = strchr(line, '\n');

        if (next != NULL) {
            *next = '\0';
            next++;
        }

        strip_end(line);

        char *at = skip_spaces(line);

        if (*at == '\0') {
            line = next;
            continue;
        }

        if (!header) {
            // La primera linea tiene que ser la del documento, y dice la
            // version.
            if (strncmp(at, DOCUMENT_MAGIC, strlen(DOCUMENT_MAGIC)) != 0) {
                ignoredLines++;
            } else {
                header = true;

                char *after = at + strlen(DOCUMENT_MAGIC);
                int version = read_number(&after);

                if (version > DOCUMENT_VERSION) {
                    // Un formato mas nuevo del que se conoce: se leen lo que se
                    // entienda, que es para lo que se ignoran las lineas sueltas.
                    ignoredLines++;
                }
            }

            line = next;
            continue;
        }

        if (strncmp(at, "theme", 5) == 0 && isspace((unsigned char) at[5])) {
            char name[SHEET_NAME_SIZE];

            // El nombre del tema va suelto, sin comillas.
            at = read_word(at + 5, name, sizeof(name));

            if (at == NULL || !theme_set_by_name(name)) {
                ignoredLines++;
            }
        } else if (strncmp(at, "cell-size", 9) == 0 && isspace((unsigned char) at[9])) {
            char *after = at + 9;

            int width = read_number(&after);
            int height = read_number(&after);

            if (width < 0 || height < 0) {
                ignoredLines++;
            } else {
                set_cell_size(width, height);
            }
        } else if (strncmp(at, "sheet", 5) == 0 && isspace((unsigned char) at[5])) {
            char name[SHEET_NAME_SIZE];

            at = read_quoted(at + 5, name, sizeof(name));

            if (at == NULL) {
                ignoredLines++;
            } else if (firstSheet) {
                // La primera hoja del documento se queda con el nombre de la que
                // ya estaba, para no tener una hoja de mas.
                sheet_rename(manager, 0, name);
                sheet_select(manager, 0);
                firstSheet = false;
                inSheet = true;
            } else if (sheet_add(manager, name) < 0) {
                ignoredLines++;
            } else {
                inSheet = true;
            }
        } else if (strncmp(at, "end-sheet", 9) == 0) {
            inSheet = false;
        } else if (strncmp(at, "cell", 4) == 0 && isspace((unsigned char) at[4])) {
            char reference[32];
            char *after = read_word(at + 4, reference, sizeof(reference));

            char cellContent[1024];

            at = after != NULL ? read_quoted(after, cellContent, sizeof(cellContent)) : NULL;

            unsigned int x = 0;
            unsigned int y = 0;

            if (at == NULL || !inSheet || !ref_parse(reference, &x, &y)) {
                ignoredLines++;
            } else {
                set_cell_content(manager, x, y, cellContent);
            }
        } else if (strncmp(at, "format", 6) == 0 && isspace((unsigned char) at[6])) {
            char reference[32];
            char *after = read_word(at + 6, reference, sizeof(reference));

            unsigned int x = 0;
            unsigned int y = 0;

            if (after == NULL || !inSheet || !ref_parse(reference, &x, &y)) {
                ignoredLines++;
            } else {
                // Un formato puede estar puesto en una celda vacia: la celda
                // aparece para llevarlo.
                MetaCell *meta = get_or_create_cell(manager, x, y);

                if (meta == NULL) {
                    ignoredLines++;
                } else {
                    CellFormat format = meta->cell.format;
                    char *cursor = skip_spaces(after);
                    int bad = 0;

                    while (*cursor != '\0') {
                        char token[32];

                        cursor = read_word(cursor, token, sizeof(token));

                        if (cursor == NULL) {
                            bad = 1;
                            break;
                        }

                        cursor = skip_spaces(cursor);

                        if (strncmp(token, "align=", 6) == 0) {
                            int align = align_by_name(token + 6);

                            if (align < 0) {
                                bad = 1;
                                break;
                            }

                            format.align = (unsigned char) align;
                        } else if (strcmp(token, "bold") == 0) {
                            format.bold = 1;
                        } else if (strcmp(token, "underline") == 0) {
                            format.underline = 1;
                        } else if (strncmp(token, "fg=", 3) == 0 || strncmp(token, "bg=", 3) == 0) {
                            int color = color_by_token(token);

                            if (color < 0) {
                                bad = 1;
                                break;
                            }

                            if (token[0] == 'f') {
                                format.fg = (unsigned char) color;
                            } else {
                                format.bg = (unsigned char) color;
                            }
                        } else {
                            bad = 1;
                            break;
                        }
                    }

                    if (bad) {
                        ignoredLines++;
                    } else {
                        set_cell_format(meta, format);
                    }
                }
            }
        } else if (strncmp(at, "select", 6) == 0 && inSheet) {
            char reference[32];

            if (read_word(at + 6, reference, sizeof(reference)) == NULL) {
                ignoredLines++;
            } else {
                unsigned int x = 0;
                unsigned int y = 0;

                if (!ref_parse(reference, &x, &y)) {
                    ignoredLines++;
                } else {
                    sheet_state(manager)->cursor.x = (uint16_t) x;
                    sheet_state(manager)->cursor.y = (uint16_t) y;
                }
            }
        } else if (strncmp(at, "scroll", 6) == 0 && inSheet) {
            char *after = at + 6;

            int x = read_number(&after);
            int y = read_number(&after);

            if (x < 0 || y < 0) {
                ignoredLines++;
            } else {
                sheet_state(manager)->originX = x;
                sheet_state(manager)->originY = y;
            }
        } else {
            ignoredLines++;
        }

        line = next;
    }

    // Las hojas se guardan con la primera puesta, como al abrir un archivo.
    sheet_select(manager, 0);

    free(text);

    if (ignored != NULL) {
        *ignored = ignoredLines;
    }

    invalidate_screen();

    return 1;
}

int document_is_path(const char *path) {
    if (path == NULL) {
        return 0;
    }

    size_t length = strlen(path);
    const char *extension = ".clex";
    size_t extensionLength = strlen(extension);

    if (length < extensionLength) {
        return 0;
    }

    return strcmp(path + length - extensionLength, extension) == 0;
}

int document_change_extension(const char *path, const char *extension, char *into, size_t size) {
    if (path == NULL || extension == NULL || into == NULL || size == 0) {
        return 0;
    }

    const char *dot = strrchr(path, '.');

    // Si el punto viene antes que el ultimo separador, no es una extension: es
    // parte de un directorio, y el nombre no tiene extension que cambiar.
    const char *separator = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last = separator;

    if (backslash != NULL && (last == NULL || backslash > last)) {
        last = backslash;
    }

    if (dot != NULL && last != NULL && dot < last) {
        dot = NULL;
    }

    size_t stem = dot != NULL ? (size_t) (dot - path) : strlen(path);

    if (stem + strlen(extension) + 1 > size) {
        into[0] = '\0';

        return 0;
    }

    memcpy(into, path, stem);
    into[stem] = '\0';
    strcat(into, extension);

    return 1;
}
