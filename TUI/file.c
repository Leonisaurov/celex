#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "ref.h"

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }

    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' || text[length - 1] == '\n')) {
        text[length - 1] = '\0';
        length--;
    }

    return text;
}

// Como se marca una hoja en el archivo: una asignacion a la variable SHEET con
// el nombre entre comillas. El lenguaje la ve como una variable mas, asi que un
// archivo de varias hojas sigue siendo un programa valido.
#define SHEET_VARIABLE "SHEET"

// El nombre que estaba escrito en una linea `SHEET = "..."`.
static void sheet_line_name(char *value, char *into, size_t size) {
    char *first = strchr(value, '"');

    if (first == NULL) {
        snprintf(into, size, "%s", value);
        return;
    }

    first++;

    char *last = strrchr(first, '"');

    if (last == NULL || last < first) {
        snprintf(into, size, "%s", first);
        return;
    }

    *last = '\0';
    snprintf(into, size, "%s", first);
}

int load_file(const char *path, CellManager *manager, int *ignored) {
    int skipped = 0;
    bool named = false;

    if (ignored != NULL) {
        *ignored = 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return FILE_NOT_FOUND;
    }

    char line[4096];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *text = trim(line);

        if (text[0] == '\0' || text[0] == '#') {
            continue;
        }

        char *separator = strchr(text, '=');
        if (separator == NULL) {
            skipped++;
            continue;
        }

        *separator = '\0';
        char *reference = trim(text);
        char *value = trim(separator + 1);

        // El principio de una hoja.
        if (strcmp(reference, SHEET_VARIABLE) == 0) {
            char name[SHEET_NAME_SIZE];
            sheet_line_name(value, name, sizeof(name));

            if (!named) {
                // La primera linea le pone nombre a la hoja que ya habia, para
                // no dejar una hoja vacia delante.
                sheet_rename(manager, 0, name);
                named = true;
            } else {
                sheet_add(manager, name);
            }

            continue;
        }

        unsigned int x = 0;
        unsigned int y = 0;
        if (!ref_parse(reference, &x, &y)) {
            skipped++;
            continue;
        }

        set_cell_content(manager, x, y, value);
    }

    fclose(file);

    if (ignored != NULL) {
        *ignored = skipped;
    }

    // Se vuelve a la primera hoja, que es la que se espera al abrir un archivo.
    sheet_select(manager, 0);

    return FILE_LOADED;
}

typedef struct {
    unsigned int x;
    unsigned int y;
    const char *content;
} FileEntry;

typedef struct {
    FileEntry *items;
    size_t count;
    size_t capacity;
} FileEntries;

static int compare_entries(const void *a, const void *b) {
    const FileEntry *left = (const FileEntry *) a;
    const FileEntry *right = (const FileEntry *) b;

    // Columna y luego fila: el lenguaje no tiene referencias adelantadas,
    // asi que una celda tiene que venir despues de las celdas de las que
    // depende. Con este orden, una celda ve a las que estan encima en su
    // misma columna y a las de las columnas de la izquierda.
    if (left->x != right->x) {
        return left->x < right->x ? -1 : 1;
    }

    if (left->y != right->y) {
        return left->y < right->y ? -1 : 1;
    }

    return 0;
}

static int collect_entry(void *const context, struct hashmap_element_s *const element) {
    FileEntries *entries = (FileEntries *) context;
    MetaCell *meta = (MetaCell *) element->data;

    if (entries->count == entries->capacity) {
        size_t capacity = entries->capacity == 0 ? 16 : entries->capacity * 2;
        FileEntry *items = (FileEntry *) realloc(entries->items, capacity * sizeof(FileEntry));
        if (items == NULL) {
            return -1;
        }

        entries->items = items;
        entries->capacity = capacity;
    }

    entries->items[entries->count].x = meta->x;
    entries->items[entries->count].y = meta->y;
    entries->items[entries->count].content = cell_content(meta);
    entries->count++;

    return 0;
}

// El guion de una hoja: una linea por celda, en el orden en que se pueden
// calcular.
static char *state_source(SheetState *state) {
    FileEntries entries = {NULL, 0, 0};
    hashmap_iterate_pairs(&state->map, collect_entry, &entries);

    qsort(entries.items, entries.count, sizeof(FileEntry), compare_entries);

    size_t length = 1;
    for (size_t i = 0; i < entries.count; i++) {
        // Una celda sin contenido no necesita una linea en el archivo.
        if (entries.items[i].content[0] == '\0') {
            continue;
        }

        length += strlen(entries.items[i].content) + 16;
    }

    char *source = (char *) malloc(length);
    if (source == NULL) {
        free(entries.items);
        return NULL;
    }

    source[0] = '\0';
    for (size_t i = 0; i < entries.count; i++) {
        if (entries.items[i].content[0] == '\0') {
            continue;
        }

        char *reference = ref_to_string(entries.items[i].x, entries.items[i].y);
        strcat(source, reference);
        strcat(source, " = ");
        strcat(source, entries.items[i].content);
        strcat(source, "\n");
        free(reference);
    }

    free(entries.items);

    return source;
}

char *sheet_source(CellManager *manager) {
    return state_source(sheet_state(manager));
}

// El archivo entero: cada hoja empieza con su linea de nombre.
char *workbook_source(CellManager *manager) {
    size_t capacity = 1;

    for (int i = 0; i < sheet_count(manager); i++) {
        capacity += strlen(sheet_name_at(manager, i)) + 24;

        char *part = state_source(&sheet_at(manager, i)->state);

        if (part == NULL) {
            continue;
        }

        capacity += strlen(part);
        free(part);
    }

    char *source = (char *) malloc(capacity);
    if (source == NULL) {
        return NULL;
    }

    source[0] = '\0';

    for (int i = 0; i < sheet_count(manager); i++) {
        strcat(source, SHEET_VARIABLE " = \"");
        strcat(source, sheet_name_at(manager, i));
        strcat(source, "\"\n");

        char *part = state_source(&sheet_at(manager, i)->state);

        if (part != NULL) {
            strcat(source, part);
            free(part);
        }
    }

    return source;
}

static int write_source(const char *path, char *source) {
    if (source == NULL) {
        return FILE_ERROR;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        free(source);
        return FILE_ERROR;
    }

    fputs(source, file);
    fclose(file);
    free(source);

    return FILE_LOADED;
}

int save_file(const char *path, CellManager *manager) {
    return write_source(path, workbook_source(manager));
}

// Solo la hoja que se esta viendo, que es lo que evalua el interprete de fuera:
// las celdas de otras hojas no pueden verse desde aqui.
int save_sheet_file(const char *path, CellManager *manager) {
    return write_source(path, sheet_source(manager));
}
